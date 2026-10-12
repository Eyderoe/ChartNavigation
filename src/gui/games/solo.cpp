#include "solo.hpp"
#include "rectPuzzleView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>

class SoloView : public RectPuzzleView {
  public:
    explicit SoloView(QWidget *parent) : RectPuzzleView(parent) {
        setObjectName("soloView");
        setAccessibleName(tr("数独棋盘"));
    }
    void setState(const SoloBackend *s, const SoloInput *i, SoloConfig c) {
        state = s;
        input = i;
        config = c;
        columns = rows = s ? solo_order(s) : c.columns * c.rows;
        if (s) {
            errors = std::make_unique<bool[]>(columns * columns);
            cageErrors = std::make_unique<bool[]>(columns * columns);
            solo_errors(s, errors.get(), cageErrors.get());
        }
        update();
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        if (!state)
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto rect = boardRect();
        const qreal tile = rect.width() / columns;
        const int selected = solo_cursor(input, columns);
        const bool pencil = solo_cursor_pencil(input);
        const bool show = solo_cursor_visible(input);
        const QColor ink = palette().color(QPalette::Text);
        for (int cell = 0; cell < columns * columns; cell++) {
            const int x = cell % columns, y = cell / columns;
            const auto centre = cellCentre(cell);
            const QRectF square(centre - QPointF(tile / 2, tile / 2), QSizeF(tile, tile));
            QColor bg = palette().color(QPalette::Base);
            if (config.diagonal && (x == y || x + y == columns - 1)) {
                const auto tint = palette().color(QPalette::Highlight);
                bg.setRgb((bg.red() * 5 + tint.red()) / 6, (bg.green() * 5 + tint.green()) / 6,
                          (bg.blue() * 5 + tint.blue()) / 6);
            }
            painter.setPen(QPen(palette().color(QPalette::Mid), 0.6));
            painter.setBrush(bg);
            painter.drawRect(square);
            if (show && selected == cell) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(palette().color(QPalette::Highlight));
                if (pencil)
                    painter.drawPolygon(QPolygonF{square.topLeft(), square.topLeft() + QPointF(tile * 0.5, 0),
                                                  square.topLeft() + QPointF(0, tile * 0.5)});
                else
                    painter.drawRect(square.adjusted(1, 1, -1, -1));
            }
            const int cage = solo_cage(state, cell);
            if (cage >= 0) {
                const bool l = x == 0 || solo_cage(state, cell - 1) != cage,
                           r = x == columns - 1 || solo_cage(state, cell + 1) != cage;
                const bool t = y == 0 || solo_cage(state, cell - columns) != cage,
                           b = y == columns - 1 || solo_cage(state, cell + columns) != cage;
                const qreal inset = tile * 0.1;
                const qreal left = square.left() + (l ? inset : 0), right = square.right() - (r ? inset : 0);
                const qreal top = square.top() + (t ? inset : 0), bottom = square.bottom() - (b ? inset : 0);
                painter.setPen(
                    QPen(cageErrors[cell] ? QColor("#e46b78") : palette().color(QPalette::PlaceholderText), 1,
                         Qt::DashLine));
                if (l)
                    painter.drawLine(QPointF(left, top), QPointF(left, bottom));
                if (r)
                    painter.drawLine(QPointF(right, top), QPointF(right, bottom));
                if (t)
                    painter.drawLine(QPointF(left, top), QPointF(right, top));
                if (b)
                    painter.drawLine(QPointF(left, bottom), QPointF(right, bottom));
                if (int sum = solo_cage_sum(state, cell)) {
                    auto f = font();
                    f.setPixelSize(std::max(6, int(tile * 0.24)));
                    painter.setFont(f);
                    painter.setPen(show && selected == cell && !pencil
                                       ? palette().color(QPalette::HighlightedText)
                                       : ink);
                    painter.drawText(square.adjusted(tile * 0.13, tile * 0.07, -tile * 0.1, -tile * 0.65),
                                     Qt::AlignLeft | Qt::AlignTop | Qt::TextDontClip, QString::number(sum));
                }
            }
            const int value = solo_value(state, cell);
            if (value) {
                auto f = font();
                f.setBold(solo_fixed(state, cell));
                f.setPixelSize(std::max(1, int(tile * (cage >= 0 ? 0.48 : 0.6))));
                painter.setFont(f);
                painter.setPen(errors[cell] ? QColor("#e46b78")
                               : show && selected == cell && !pencil
                                   ? palette().color(QPalette::HighlightedText)
                               : solo_fixed(state, cell) ? ink
                                                         : palette().color(QPalette::Link));
                painter.drawText(square.adjusted(0, cage >= 0 ? tile * 0.12 : 0, 0, 0), Qt::AlignCenter,
                                 QString(QChar::fromLatin1(solo_symbol(value))));
            } else {
                const int across = int(std::ceil(std::sqrt(columns))), down = (columns + across - 1) / across;
                auto f = font();
                f.setPixelSize(std::max(1, int(tile * 0.7 / across)));
                painter.setFont(f);
                painter.setPen(ink);
                for (int number = 1; number <= columns; number++)
                    if (solo_pencil(state, cell, number)) {
                        const int index = number - 1;
                        const qreal top = cage >= 0 ? tile * 0.25 : tile * 0.04, avail = tile - top;
                        const QRectF mark(square.left() + (index % across) * tile / across,
                                          square.top() + top + (index / across) * avail / down, tile / across,
                                          avail / down);
                        painter.drawText(mark, Qt::AlignCenter,
                                         QString(QChar::fromLatin1(solo_symbol(number))));
                    }
            }
            painter.setPen(QPen(ink, std::max(1.5, tile * 0.055)));
            const int block = solo_block(state, cell);
            if (x == 0 || solo_block(state, cell - 1) != block)
                painter.drawLine(square.topLeft(), square.bottomLeft());
            if (y == 0 || solo_block(state, cell - columns) != block)
                painter.drawLine(square.topLeft(), square.topRight());
            if (x == columns - 1)
                painter.drawLine(square.topRight(), square.bottomRight());
            if (y == columns - 1)
                painter.drawLine(square.bottomLeft(), square.bottomRight());
        }
    }

  private:
    const SoloBackend *state{};
    const SoloInput *input{};
    SoloConfig config{};
    std::unique_ptr<bool[]> errors, cageErrors;
};

SoloGame::State SoloGame::take(SoloBackend *value) { return State(value, solo_free); }
SoloGame::SoloGame(Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new SoloView(ui->soloBoard);
    ui->soloBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < solo_preset_count(); i++) {
        char name[100];
        solo_preset_name(i, name, sizeof(name));
        ui->soloPreset->addItem(QString::fromLatin1(name));
    }
    ui->soloPreset->addItem(tr("自定义…"));
    for (auto pair : {std::pair{ui->soloDifficulty, true}, std::pair{ui->soloSymmetry, false}}) {
        char names[256];
        solo_choices(pair.second, names, sizeof(names));
        pair.first->addItems(QString::fromLatin1(names).split(':', Qt::SkipEmptyParts));
    }
    for (auto *combo : {ui->soloPreset, ui->soloDifficulty, ui->soloSymmetry}) {
        combo->setMinimumContentsLength(5);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        expandComboBox(combo);
    }
    for (auto *spin : {ui->soloColumns, ui->soloRows}) {
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(48);
    }
    ui->soloPreset->setCurrentIndex(2);
    connect(ui->soloPreset, &QComboBox::currentIndexChanged, this, &SoloGame::selectPreset);
    connect(ui->soloApplyButton, &QToolButton::clicked, this, &SoloGame::applyCustom);
    connect(ui->soloNewButton, &QToolButton::clicked, this, &SoloGame::reset);
    connect(ui->soloRetryButton, &QToolButton::clicked, this, &SoloGame::retry);
    connect(ui->soloUndoButton, &QToolButton::clicked, this, &SoloGame::undo);
    connect(ui->soloRedoButton, &QToolButton::clicked, this, &SoloGame::redo);
    connect(ui->soloSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy)
            adopt(take(solo_solve(initial.get(), state.get())));
    });
    connect(ui->soloMarksButton, &QToolButton::clicked, this, [this] {
        if (!busy)
            adopt(take(solo_marks(state.get())));
    });
    connect(ui->soloEraseButton, &QToolButton::clicked, this, [this] { send('\b'); });
    connect(ui->soloPencilButton, &QToolButton::toggled, this, [this](bool checked) {
        if (input && state) {
            solo_input_select(input.get(), solo_cursor(input.get(), solo_order(state.get())), checked,
                              solo_order(state.get()));
            refresh();
        }
    });
    connect(ui->soloKeepHighlight, &QCheckBox::toggled, this, [this] { refresh(); });
    selectPreset();
}
void SoloGame::fillCustom() {
    ui->soloColumns->setValue(config.columns);
    ui->soloRows->setValue(config.rows);
    ui->soloJigsaw->setChecked(config.rows == 1);
    ui->soloDiagonal->setChecked(config.diagonal);
    ui->soloKiller->setChecked(config.killer);
    ui->soloSymmetry->setCurrentIndex(config.symmetry);
    ui->soloDifficulty->setCurrentIndex(config.difficulty);
}
void SoloGame::selectPreset() {
    const int index = ui->soloPreset->currentIndex();
    const bool custom = index == solo_preset_count();
    ui->soloCustomSettings->setVisible(custom);
    if (!custom)
        config = solo_preset(index);
    fillCustom();
    if (!custom)
        reset();
}
void SoloGame::applyCustom() {
    SoloConfig next{ui->soloColumns->value(),
                    ui->soloRows->value(),
                    ui->soloSymmetry->currentIndex(),
                    ui->soloDifficulty->currentIndex(),
                    3,
                    ui->soloDiagonal->isChecked(),
                    ui->soloKiller->isChecked()};
    if (ui->soloJigsaw->isChecked()) {
        next.columns *= next.rows;
        next.rows = 1;
    }
    if (const auto *error = solo_validate(next)) {
        ui->soloFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error)));
        return;
    }
    config = next;
    reset();
}
void SoloGame::reset() {
    const unsigned ticket = ++generation;
    busy = true;
    input.reset();
    state.reset();
    initial.reset();
    history.clear();
    future.clear();
    refresh();
    const auto parameters = config;
    const auto seed = QByteArray::number(QRandomGenerator::global()->generate64());
    auto *watcher = new QFutureWatcher<State>(this);
    connect(watcher, &QFutureWatcher<State>::finished, this, [this, watcher, ticket] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (ticket != generation)
            return;
        busy = false;
        state = std::move(result);
        initial = take(solo_copy(state.get()));
        if (state)
            input.reset(solo_input_new(state.get()));
        buildButtons();
        refresh();
    });
    watcher->setFuture(
        QtConcurrent::run([parameters, seed] { return take(solo_new(parameters, seed.constData())); }));
}
void SoloGame::retry() {
    if (!busy && initial)
        adopt(take(solo_copy(initial.get())));
}
void SoloGame::adopt(State next) {
    if (next) {
        history.push_back(state);
        future.clear();
        state = std::move(next);
        solo_input_changed(input.get(), state.get());
    }
    refresh();
}
void SoloGame::undo() {
    if (busy || history.empty())
        return;
    future.push_back(state);
    state = std::move(history.back());
    history.pop_back();
    solo_input_changed(input.get(), state.get());
    refresh();
}
void SoloGame::redo() {
    if (busy || future.empty())
        return;
    history.push_back(state);
    state = std::move(future.back());
    future.pop_back();
    solo_input_changed(input.get(), state.get());
    refresh();
}
void SoloGame::send(int button, int x, int y) {
    if (!busy && state)
        adopt(take(solo_input(input.get(), state.get(), x, y, button)));
}
void SoloGame::buildButtons() {
    const int n = solo_order(state.get());
    if (n == buttonsOrder)
        return;
    buttonsOrder = n;
    while (auto *item = ui->soloNumbersLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (int value = 1; value <= n; value++) {
        auto *button = new QToolButton(ui->soloNumbers);
        button->setObjectName(QStringLiteral("soloNumber%1").arg(value));
        button->setText(QString(QChar::fromLatin1(solo_symbol(value))));
        button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        ui->soloNumbersLayout->addWidget(button, (value - 1) / 5, (value - 1) % 5);
        connect(button, &QToolButton::clicked, this, [this, value] { send(solo_symbol(value)); });
    }
}
void SoloGame::refresh() {
    QString rules = tr("每行、每列和粗线宫内的数字都不重复。");
    if (config.diagonal)
        rules += tr("两条有底色的主对角线也不能重复。");
    if (config.killer)
        rules += tr("虚线笼内数字不重复，且总和等于角标。");
    rules += tr("左键选格后输入数字，右键或 "
                "Control＋左键添加候选；回车切换填写／候选，空格或退格清除。方向键移动，10 起使用 "
                "a、b、c…；也可点击下方数字按钮。");
    ui->soloRules->setText(rules);
    if (input)
        solo_input_keep(input.get(), ui->soloKeepHighlight->isChecked());
    view->setState(state.get(), input.get(), config);
    int filled = 0;
    QStringList values;
    for (int cell = 0; cell < solo_order(state.get()) * solo_order(state.get()); cell++) {
        filled += solo_value(state.get(), cell) != 0;
        values << QString::number(solo_value(state.get(), cell));
    }
    const int order = config.columns * config.rows;
    ui->soloStatus->setText(tr("%1×%1 · 已填 %2 / %3").arg(order).arg(filled).arg(order * order));
    ui->soloFeedback->setText(
        busy                        ? tr("正在按所选难度生成棋盘…")
        : !state                    ? tr("未能生成棋盘，请开新一局。")
        : solo_cheated(state.get()) ? tr("已显示答案，可撤销返回。")
        : solo_won(state.get())
            ? tr("数独完成了！")
            : tr("粗体数字为固定线索；右键选择候选模式。10 起使用字母 a、b、c…；M 填入全部候选。"));
    ui->soloUndoButton->setEnabled(!busy && !history.empty());
    ui->soloRedoButton->setEnabled(!busy && !future.empty());
    for (auto *button : {ui->soloRetryButton, ui->soloSolveButton, ui->soloMarksButton, ui->soloPencilButton,
                         ui->soloEraseButton})
        button->setEnabled(!busy && bool(state));
    ui->soloNumbers->setEnabled(!busy && bool(state));
    {
        QSignalBlocker blocker(ui->soloPencilButton);
        ui->soloPencilButton->setChecked(solo_cursor_pencil(input.get()));
    }
    view->setAccessibleDescription(ui->soloStatus->text() + QStringLiteral("；格子：") + values.join(','));
}
bool SoloGame::eventFilter(QObject *watched, QEvent *event) {
    if (watched != view)
        return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
            return false;
        view->setFocus(Qt::MouseFocusReason);
        const auto p = view->canonical(mouse->position());
        send(mouse->button() == Qt::RightButton || mouse->modifiers().testFlag(Qt::ControlModifier)
                 ? PuzzleRight
                 : PuzzleLeft,
             p.x(), p.y());
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->matches(QKeySequence::Undo)) {
            undo();
            return true;
        }
        if (key->matches(QKeySequence::Redo)) {
            redo();
            return true;
        }
        int button = 0;
        switch (key->key()) {
        case Qt::Key_Up:
            button = PuzzleUp;
            break;
        case Qt::Key_Down:
            button = PuzzleDown;
            break;
        case Qt::Key_Left:
            button = PuzzleArrowLeft;
            break;
        case Qt::Key_Right:
            button = PuzzleArrowRight;
            break;
        case Qt::Key_Enter:
        case Qt::Key_Return:
            button = PuzzleEnter;
            break;
        case Qt::Key_Space:
            button = PuzzleSpace;
            break;
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
            button = '\b';
            break;
        default:
            if (key->text().size() == 1)
                button = key->text()[0].toLatin1();
            else
                return Game::eventFilter(watched, event);
        }
        send(button);
        return true;
    }
    return Game::eventFilter(watched, event);
}
