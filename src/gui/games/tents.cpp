#include "tents.hpp"
#include "rectPuzzleView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrentRun>
#include <array>
class TentsView : public RectPuzzleView {
  public:
    explicit TentsView(QWidget *parent) : RectPuzzleView(parent) {
        setObjectName("tentsView");
        setAccessibleName(tr("帐篷棋盘"));
        setMouseTracking(true);
    }
    void setState(const TentsBackend *s, const TentsInput *i, TentsConfig c) {
        state = s;
        input = i;
        config = c;
        columns = c.width + 1;
        rows = c.height + 1;
        values.clear();
        errors.clear();
        if (s) {
            values.resize(c.width * c.height);
            errors.resize(c.width * c.height + c.width + c.height);
            tents_display(s, i, values.data(), errors.data());
        }
        update();
    }
    QPointF gridPoint(int cell) const {
        return cellCentre(cell % config.width + cell / config.width * columns);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        if (!state)
            return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto rect = boardRect();
        qreal t = rect.width() / columns;
        const QColor base = palette().color(QPalette::Base);
        const bool dark = base.lightness() < 128;
        const QColor grid = dark ? QColor("#738795") : QColor("#8b98a5");
        QColor ink = palette().color(QPalette::Text), error("#e46b78"), grass("#79ae77"),
               leaf = dark ? QColor("#48b982") : QColor("#228457"),
               tent("#df9a48");
        QColor ground = base;
        ground.setRed((ground.red() * 4 + grass.red()) / 5);
        ground.setGreen((ground.green() * 4 + grass.green()) / 5);
        ground.setBlue((ground.blue() * 4 + grass.blue()) / 5);
        auto f = font();
        f.setPixelSize(std::max(1, int(t * .48)));
        p.setFont(f);
        const int area = config.width * config.height;
        for (int cell = 0; cell < area; cell++) {
            auto c = gridPoint(cell);
            QRectF box(c - QPointF(t / 2, t / 2), QSizeF(t, t));
            int v = values[cell];
            p.fillRect(box, v == TentsBlank ? base : ground);
            const bool bad = errors[cell] & (1 << 12);
            if (v == TentsTree) {
                p.fillRect(QRectF(c + QPointF(-t * .045, -t * .1), QSizeF(t * .09, t * .43)),
                           bad ? error : QColor("#9a6b45"));
                p.setPen(Qt::NoPen);
                p.setBrush(bad ? error : leaf);
                p.drawEllipse(c + QPointF(0, -t * .16), t * .24, t * .23);
                p.drawEllipse(c + QPointF(-t * .2, -t * .03), t * .12, t * .12);
                p.drawEllipse(c + QPointF(t * .2, -t * .03), t * .12, t * .12);
            } else if (v == TentsTent) {
                p.setPen(QPen(bad ? error : ink, 1));
                p.setBrush(bad ? error : tent);
                p.drawPolygon(QPolygonF{c + QPointF(-t * .33, t * .3), c + QPointF(t * .33, t * .3),
                                        c + QPointF(0, -t * .33)});
                p.setBrush(ground);
                p.drawPolygon(QPolygonF{c + QPointF(-t * .09, t * .3), c + QPointF(t * .09, t * .3),
                                        c + QPointF(0, -t * .04)});
            } else if (v == TentsGrass) {
                p.setPen(QPen(leaf, std::max(.8, t * .04)));
                p.drawLine(c + QPointF(-t * .08, t * .06), c + QPointF(0, t * .15));
                p.drawLine(c + QPointF(0, t * .15), c + QPointF(t * .13, -t * .12));
            }
            if (errors[cell] & 0xff0) {
                p.setPen(error);
                p.drawText(box.adjusted(t * .1, 0, -t * .05, 0), Qt::AlignRight | Qt::AlignTop, "!");
            }
            if (tents_cursor(input, config.width) == cell) {
                p.setPen(QPen(palette().color(QPalette::Highlight), 2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(box.adjusted(t * .12, t * .12, -t * .12, -t * .12));
            }
        }
        // Draw each grid line once, after all cell fills, so neighbouring cells
        // cannot cover half of a border. Keep thin lines crisp at small sizes.
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(QPen(grid, 1));
        const QRectF gridRect(rect.topLeft(), QSizeF(t * config.width, t * config.height));
        for (int x = 1; x < config.width; x++) {
            const qreal px = gridRect.left() + x * t;
            p.drawLine(QPointF(px, gridRect.top()), QPointF(px, gridRect.bottom()));
        }
        for (int y = 1; y < config.height; y++) {
            const qreal py = gridRect.top() + y * t;
            p.drawLine(QPointF(gridRect.left(), py), QPointF(gridRect.right(), py));
        }
        p.setPen(QPen(grid, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawRect(gridRect);
        p.setRenderHint(QPainter::Antialiasing);
        for (int x = 0; x < config.width; x++) {
            auto c = cellCentre(x + config.height * columns);
            p.setPen(errors[area + x] ? error : ink);
            p.drawText(QRectF(c - QPointF(t / 2, t / 2), QSizeF(t, t)), Qt::AlignCenter,
                       QString::number(tents_clue(state, x)));
        }
        for (int y = 0; y < config.height; y++) {
            auto c = cellCentre(config.width + y * columns);
            p.setPen(errors[area + config.width + y] ? error : ink);
            p.drawText(QRectF(c - QPointF(t / 2, t / 2), QSizeF(t, t)), Qt::AlignCenter,
                       QString::number(tents_clue(state, config.width + y)));
        }
    }

  private:
    const TentsBackend *state{};
    const TentsInput *input{};
    TentsConfig config{};
    std::vector<int> values, errors;
};
TentsGame::State TentsGame::take(TentsBackend *value) { return State(value, tents_free); }
TentsGame::TentsGame(Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new TentsView(ui->tentsBoard);
    ui->tentsBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < tents_preset_count(); i++) {
        auto c = tents_preset(i);
        ui->tentsPreset->addItem(tr("%1×%2 · %3")
                                     .arg(c.width)
                                     .arg(c.height)
                                     .arg(QString::fromUtf8(tents_difficulty_name(c.difficulty))));
    }
    ui->tentsPreset->addItem(tr("自定义…"));
    for (int i = 0; i < 2; i++)
        ui->tentsDifficulty->addItem(QString::fromUtf8(tents_difficulty_name(i)));
    for (auto *combo : {ui->tentsPreset, ui->tentsDifficulty}) {
        combo->setMinimumContentsLength(5);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        expandComboBox(combo);
    }
    for (auto *spin : {ui->tentsWidth, ui->tentsHeight}) {
        spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        spin->setMinimumWidth(48);
    }
    connect(ui->tentsPreset, &QComboBox::currentIndexChanged, this, &TentsGame::selectPreset);
    connect(ui->tentsApplyButton, &QToolButton::clicked, this, &TentsGame::applyCustom);
    connect(ui->tentsNewButton, &QToolButton::clicked, this, &TentsGame::reset);
    connect(ui->tentsRetryButton, &QToolButton::clicked, this, &TentsGame::retry);
    connect(ui->tentsUndoButton, &QToolButton::clicked, this, &TentsGame::undo);
    connect(ui->tentsRedoButton, &QToolButton::clicked, this, &TentsGame::redo);
    connect(ui->tentsSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy)
            adopt(take(tents_reveal(initial.get(), state.get())));
    });

    selectPreset();
}
void TentsGame::fillCustom() {
    ui->tentsWidth->setValue(config.width);
    ui->tentsHeight->setValue(config.height);
    ui->tentsDifficulty->setCurrentIndex(config.difficulty);
}
void TentsGame::selectPreset() {
    const int index = ui->tentsPreset->currentIndex();
    const bool custom = index == tents_preset_count();
    ui->tentsCustomSettings->setVisible(custom);
    if (!custom)
        config = tents_preset(index);
    fillCustom();
    if (!custom)
        reset();
}
void TentsGame::applyCustom() {
    TentsConfig next{ui->tentsWidth->value(), ui->tentsHeight->value(), ui->tentsDifficulty->currentIndex()};
    if (const auto *error = tents_validate(next)) {
        ui->tentsFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error)));
        return;
    }
    config = next;
    reset();
}
void TentsGame::reset() {
    const unsigned ticket = ++generation;
    busy = true;
    dragButton = 0;
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
        initial = take(tents_copy(state.get()));
        if (state)
            input.reset(tents_input_new(state.get()));
        refresh();
    });
    watcher->setFuture(
        QtConcurrent::run([parameters, seed] { return take(tents_new(parameters, seed.constData())); }));
}
void TentsGame::retry() {
    if (!busy && initial)
        adopt(take(tents_copy(initial.get())));
}
void TentsGame::adopt(State next) {
    if (next) {
        history.push_back(state);
        future.clear();
        state = std::move(next);
    }
    refresh();
}
void TentsGame::undo() {
    if (busy || history.empty())
        return;
    future.push_back(state);
    state = std::move(history.back());
    history.pop_back();
    refresh();
}
void TentsGame::redo() {
    if (busy || future.empty())
        return;
    history.push_back(state);
    state = std::move(future.back());
    future.pop_back();
    refresh();
}
void TentsGame::send(int button, int x, int y, bool control, bool shift) {
    if (!busy && state)
        adopt(take(tents_input(input.get(), state.get(), x, y, button, control, shift)));
}
void TentsGame::refresh() {
    view->setState(state.get(), input.get(), config);
    ui->tentsStatus->setText(tr("%1×%2 · %3")
                                 .arg(config.width)
                                 .arg(config.height)
                                 .arg(QString::fromUtf8(tents_difficulty_name(config.difficulty))));
    ui->tentsFeedback->setText(busy                         ? tr("正在生成棋盘并校验可解性…")
                               : !state                     ? tr("未能生成棋盘，请开新一局。")
                               : tents_cheated(state.get()) ? tr("已显示答案，可撤销返回。")
                               : tents_won(state.get())
                                   ? tr("帐篷与树木都配对成功，完成啦！")
                                   : tr("绿格表示已排除；红色数字或感叹号提示数量、相邻或配对冲突。"));
    ui->tentsRetryButton->setEnabled(!busy && bool(state));
    ui->tentsSolveButton->setEnabled(!busy && bool(state));
    ui->tentsUndoButton->setEnabled(!busy && !history.empty());
    ui->tentsRedoButton->setEnabled(!busy && !future.empty());
    QStringList cells, clues;
    if (state) {
        for (int c = 0; c < config.width * config.height; c++)
            cells << QString::number(tents_cell(state.get(), c));
        for (int c = 0; c < config.width + config.height; c++)
            clues << QString::number(tents_clue(state.get(), c));
    }
    view->setAccessibleDescription(ui->tentsStatus->text() +
                                   tr("；格子 %1；行列线索 %2").arg(cells.join(','), clues.join(',')));
}
bool TentsGame::eventFilter(QObject *watched, QEvent *event) {
    if (watched != view)
        return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        int button = 0;
        if (event->type() == QEvent::MouseButtonPress) {
            if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton)
                return false;
            dragButton =
                mouse->button() == Qt::RightButton || mouse->modifiers().testFlag(Qt::ControlModifier)
                    ? PuzzleRight
                    : PuzzleLeft;
            button = dragButton;
            view->setFocus(Qt::MouseFocusReason);
        } else {
            if (!dragButton)
                return false;
            bool right = dragButton == PuzzleRight;
            button = event->type() == QEvent::MouseMove ? (right ? PuzzleRightDrag : PuzzleLeftDrag)
                                                        : (right ? PuzzleRightRelease : PuzzleLeftRelease);
            if (event->type() == QEvent::MouseButtonRelease)
                dragButton = 0;
        }
        const auto p = view->canonical(mouse->position());
        send(button, p.x(), p.y());
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
        case Qt::Key_T:
            button = 'T';
            break;
        case Qt::Key_N:
            button = 'N';
            break;
        case Qt::Key_B:
            button = 'B';
            break;
        default:
            return Game::eventFilter(watched, event);
        }
        send(button, -100, -100, key->modifiers().testFlag(Qt::ControlModifier),
             key->modifiers().testFlag(Qt::ShiftModifier));
        return true;
    }
    return Game::eventFilter(watched, event);
}
