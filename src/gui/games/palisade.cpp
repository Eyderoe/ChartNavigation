#include "palisade.hpp"
#include "rectPuzzleView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>

class PalisadeView : public RectPuzzleView {
  public:
    explicit PalisadeView(QWidget *parent) : RectPuzzleView(parent) {
        setObjectName("palisadeView");
        setAccessibleName(tr("围栏棋盘"));
    }
    void setState(const PalisadeBackend *s, const PalisadeInput *i, PalisadeConfig c, bool full) {
        state = s;
        input = i;
        columns = c.width;
        rows = c.height;
        fullCursor = full;
        flags.clear();
        if (s) {
            flags.resize(columns * rows);
            palisade_display(s, i, flags.data());
        }
        update();
    }
    QPointF edgePoint(int cell, int direction) const {
        const qreal tile = boardRect().width() / columns;
        const QPointF offsets[] = {{0, -0.5}, {0.5, 0}, {0, 0.5}, {-0.5, 0}};
        return cellCentre(cell) + offsets[direction] * tile;
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        if (!state)
            return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto rect = boardRect();
        const qreal tile = rect.width() / columns;
        const QColor ink = palette().color(QPalette::Text), error("#e46b78"), uncertain("#d8b44a");
        painter.fillRect(rect, palette().color(QPalette::Base));
        for (int cell = 0; cell < columns * rows; cell++) {
            const auto centre = cellCentre(cell);
            const QRectF square(centre - QPointF(tile / 2, tile / 2), QSizeF(tile, tile));
            const int clue = palisade_clue(state, cell);
            if (clue >= 0) {
                auto f = font();
                f.setPixelSize(std::max(1, int(tile * 0.5)));
                painter.setFont(f);
                painter.setPen(flags[cell] & (1 << 12) ? error : ink);
                painter.drawText(square, Qt::AlignCenter, QString::number(clue));
            }
            for (int direction : {1, 2}) {
                if ((direction == 1 && cell % columns == columns - 1) ||
                    (direction == 2 && cell / columns == rows - 1))
                    continue;
                const int bit = 1 << direction;
                const bool on = flags[cell] & bit, off = flags[cell] & (bit << 4),
                           bad = flags[cell] & (bit << 8);
                painter.setPen(QPen(bad   ? error
                                    : on  ? ink
                                    : off ? palette().color(QPalette::Mid)
                                          : uncertain,
                                    on    ? std::max(2.0, tile * 0.075)
                                    : off ? 0.8
                                          : std::max(1.5, tile * 0.045),
                                    off ? Qt::DotLine : Qt::SolidLine, Qt::RoundCap));
                if (direction == 1)
                    painter.drawLine(square.topRight(), square.bottomRight());
                else
                    painter.drawLine(square.bottomLeft(), square.bottomRight());
            }
        }
        painter.setPen(QPen(ink, std::max(2.0, tile * 0.075)));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect);
        if (palisade_input_visible(input)) {
            int x, y;
            palisade_cursor(input, &x, &y);
            const auto point = rect.topLeft() + QPointF(x * tile / 2, y * tile / 2);
            painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
            if (fullCursor)
                painter.drawRect(
                    QRectF(point - QPointF(tile * 0.36, tile * 0.36), QSizeF(tile * 0.72, tile * 0.72)));
            else
                painter.drawEllipse(point, tile * 0.12, tile * 0.12);
        }
    }

  private:
    const PalisadeBackend *state{};
    const PalisadeInput *input{};
    bool fullCursor{};
    std::vector<int> flags;
};
PalisadeGame::State PalisadeGame::take(PalisadeBackend *value) { return State(value, palisade_free); }
PalisadeGame::PalisadeGame(Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new PalisadeView(ui->palisadeBoard);
    ui->palisadeBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < palisade_preset_count(); i++) {
        auto c = palisade_preset(i);
        ui->palisadePreset->addItem(tr("%1×%2 · 每区 %3 格").arg(c.width).arg(c.height).arg(c.region));
    }
    ui->palisadePreset->addItem(tr("自定义…"));
    for (auto *combo : {ui->palisadePreset, ui->palisadeCursorMode}) {
        combo->setMinimumContentsLength(5);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        expandComboBox(combo);
    }
    for (auto *spin : {ui->palisadeWidth, ui->palisadeHeight, ui->palisadeRegion}) {
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(48);
    }
    connect(ui->palisadePreset, &QComboBox::currentIndexChanged, this, &PalisadeGame::selectPreset);
    connect(ui->palisadeApplyButton, &QToolButton::clicked, this, &PalisadeGame::applyCustom);
    connect(ui->palisadeNewButton, &QToolButton::clicked, this, &PalisadeGame::reset);
    connect(ui->palisadeRetryButton, &QToolButton::clicked, this, &PalisadeGame::retry);
    connect(ui->palisadeUndoButton, &QToolButton::clicked, this, &PalisadeGame::undo);
    connect(ui->palisadeRedoButton, &QToolButton::clicked, this, &PalisadeGame::redo);
    connect(ui->palisadeSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy)
            adopt(take(palisade_solve(initial.get(), state.get())));
    });
    connect(ui->palisadeCursorMode, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(ui->palisadeClearRegions, &QCheckBox::toggled, this, [this] { refresh(); });
    selectPreset();
}
void PalisadeGame::fillCustom() {
    ui->palisadeWidth->setValue(config.width);
    ui->palisadeHeight->setValue(config.height);
    ui->palisadeRegion->setValue(config.region);
}
void PalisadeGame::selectPreset() {
    const int index = ui->palisadePreset->currentIndex();
    const bool custom = index == palisade_preset_count();
    ui->palisadeCustomSettings->setVisible(custom);
    if (!custom)
        config = palisade_preset(index);
    fillCustom();
    if (!custom)
        reset();
}
void PalisadeGame::applyCustom() {
    PalisadeConfig next{ui->palisadeWidth->value(), ui->palisadeHeight->value(), ui->palisadeRegion->value()};
    if (const auto *error = palisade_validate(next)) {
        ui->palisadeFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error)));
        return;
    }
    config = next;
    reset();
}
void PalisadeGame::reset() {
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
        initial = take(palisade_copy(state.get()));
        if (state)
            input.reset(palisade_input_new(state.get()));
        refresh();
    });
    watcher->setFuture(
        QtConcurrent::run([parameters, seed] { return take(palisade_new(parameters, seed.constData())); }));
}
void PalisadeGame::retry() {
    if (!busy && initial)
        adopt(take(palisade_copy(initial.get())));
}
void PalisadeGame::adopt(State next) {
    if (next) {
        history.push_back(state);
        future.clear();
        state = std::move(next);
    }
    refresh();
}
void PalisadeGame::undo() {
    if (busy || history.empty())
        return;
    future.push_back(state);
    state = std::move(history.back());
    history.pop_back();
    refresh();
}
void PalisadeGame::redo() {
    if (busy || future.empty())
        return;
    history.push_back(state);
    state = std::move(future.back());
    future.pop_back();
    refresh();
}
void PalisadeGame::send(int button, int x, int y, bool control, bool shift) {
    if (!busy && state)
        adopt(take(palisade_input(input.get(), state.get(), x, y, button, control, shift)));
}
void PalisadeGame::refresh() {
    if (input)
        palisade_input_mode(input.get(), ui->palisadeCursorMode->currentIndex() == 1,
                            ui->palisadeClearRegions->isChecked());
    view->setState(state.get(), input.get(), config, ui->palisadeCursorMode->currentIndex() == 1);
    ui->palisadeStatus->setText(
        tr("%1×%2 · 每个区域 %3 格").arg(config.width).arg(config.height).arg(config.region));
    ui->palisadeFeedback->setText(
        busy                            ? tr("正在生成棋盘并校验可解性…")
        : !state                        ? tr("未能生成棋盘，请开新一局。")
        : palisade_cheated(state.get()) ? tr("已显示答案，可撤销返回。")
        : palisade_won(state.get())
            ? tr("区域大小与数字条件全部满足了！")
            : tr("黄色边表示未定，实线是围栏，灰色虚线表示无围栏。数字包含棋盘外边界。"));
    ui->palisadeRetryButton->setEnabled(!busy && bool(state));
    ui->palisadeSolveButton->setEnabled(!busy && bool(state));
    ui->palisadeUndoButton->setEnabled(!busy && !history.empty());
    ui->palisadeRedoButton->setEnabled(!busy && !future.empty());
    QStringList clues, borders;
    if (state)
        for (int cell = 0; cell < config.width * config.height; cell++) {
            clues << QString::number(palisade_clue(state.get(), cell));
            borders << QString::number(palisade_border(state.get(), cell));
        }
    view->setAccessibleDescription(ui->palisadeStatus->text() +
                                   tr("；线索 %1；边界 %2").arg(clues.join(','), borders.join(',')));
}
bool PalisadeGame::eventFilter(QObject *watched, QEvent *event) {
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
        case Qt::Key_Return:
        case Qt::Key_Enter:
            button = PuzzleEnter;
            break;
        case Qt::Key_Space:
            button = PuzzleSpace;
            break;
        case Qt::Key_U:
            undo();
            return true;
        case Qt::Key_R:
            redo();
            return true;
        default:
            return Game::eventFilter(watched, event);
        }
        send(button, -100, -100, key->modifiers().testFlag(Qt::ControlModifier),
             key->modifiers().testFlag(Qt::ShiftModifier));
        return true;
    }
    return Game::eventFilter(watched, event);
}
