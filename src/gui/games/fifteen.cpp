#include "fifteen.hpp"
#include "squareGameView.hpp"
#include "ui_note_widget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>

class FifteenView : public SquareGameView {
    public:
        explicit FifteenView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("fifteenView"));
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(tr("十五数码棋盘"));
        }
        QSize sizeHint () const override { return {280, 280}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        QRectF boardRect () const {
            constexpr qreal margin = 4;
            const qreal unit = std::max(0.0, std::min((width() - 2 * margin) / state.w,
                                                     (height() - 2 * margin) / state.h));
            const QSizeF size(unit * state.w, unit * state.h);
            return {QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size};
        }
        QRectF cellRect (int cell) const {
            const auto rect = boardRect();
            const qreal unit = rect.width() / state.w;
            return {rect.left() + cell % state.w * unit, rect.top() + cell / state.w * unit, unit, unit};
        }
        int cellAt (QPointF point) const {
            const auto rect = boardRect();
            if (rect.width() <= 0 || !rect.contains(point)) return -1;
            const int x = static_cast<int>((point.x() - rect.left()) * state.w / rect.width());
            const int y = static_cast<int>((point.y() - rect.top()) * state.h / rect.height());
            return x < state.w && y < state.h ? y * state.w + x : -1;
        }
        void setState (const FifteenPuzzle::State &value) {
            state = value;
            update();
        }
    protected:
        void focusInEvent (QFocusEvent *event) override {
            SquareGameView::focusInEvent(event);
            update();
        }
        void focusOutEvent (QFocusEvent *event) override {
            SquareGameView::focusOutEvent(event);
            update();
        }
        void paintEvent (QPaintEvent *) override {
            if (state.tiles.empty() || boardRect().width() <= 0) return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.fillRect(boardRect(), palette().color(QPalette::Mid));
            auto tileFont = font();
            tileFont.setBold(true);
            tileFont.setPixelSize(std::max(1, static_cast<int>(cellRect(0).width() * 0.36)));
            painter.setFont(tileFont);
            for (int i = 0; i < static_cast<int>(state.tiles.size()); ++i) {
                const auto rect = cellRect(i).adjusted(2, 2, -2, -2);
                if (state.tiles[i] != 0) {
                    painter.setPen(QPen(palette().color(QPalette::Light), 1));
                    painter.setBrush(palette().color(QPalette::Button));
                    painter.drawRoundedRect(rect, 3, 3);
                    painter.setPen(palette().color(QPalette::ButtonText));
                    painter.drawText(rect, Qt::AlignCenter, QString::number(state.tiles[i]));
                } else if (hasFocus()) {
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
                    painter.drawRect(rect.adjusted(2, 2, -2, -2));
                }
            }
        }
    private:
        FifteenPuzzle::State state;
};

FifteenGame::FifteenGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new FifteenView(ui->fifteenBoard);
    ui->fifteenBoardLayout->addWidget(view);
    view->installEventFilter(this);
    connect(ui->fifteenWidth, &QSpinBox::valueChanged, this, &FifteenGame::reset);
    connect(ui->fifteenHeight, &QSpinBox::valueChanged, this, &FifteenGame::reset);
    connect(ui->fifteenNewButton, &QToolButton::clicked, this, &FifteenGame::reset);
    connect(ui->fifteenRetryButton, &QToolButton::clicked, this, [this] {
        state = initial;
        history.clear();
        future.clear();
        refresh();
    });
    connect(ui->fifteenUndoButton, &QToolButton::clicked, this, &FifteenGame::undo);
    connect(ui->fifteenRedoButton, &QToolButton::clicked, this, &FifteenGame::redo);
    connect(ui->fifteenHintButton, &QToolButton::clicked, this, &FifteenGame::hint);
    connect(ui->fifteenSolveButton, &QToolButton::clicked, this, [this] {
        history.push_back(state);
        future.clear();
        FifteenPuzzle::solve(state);
        refresh();
    });
    reset();
}

void FifteenGame::reset () {
    state = FifteenPuzzle::generate(ui->fifteenWidth->value(), ui->fifteenHeight->value(),
                                  [](int bound) { return QRandomGenerator::global()->bounded(bound); });
    initial = state;
    history.clear();
    future.clear();
    refresh();
}

void FifteenGame::move (int cell) {
    auto next = state;
    if (!FifteenPuzzle::slide(next, cell)) return;
    history.push_back(state);
    future.clear();
    state = std::move(next);
    refresh();
}

void FifteenGame::hint () {
    if (!state.completed) move(FifteenPuzzle::hintCell(state));
}

void FifteenGame::undo () {
    if (history.empty()) return;
    future.push_back(state);
    state = std::move(history.back());
    history.pop_back();
    refresh();
}

void FifteenGame::redo () {
    if (future.empty()) return;
    history.push_back(state);
    state = std::move(future.back());
    future.pop_back();
    refresh();
}

void FifteenGame::refresh () {
    view->setState(state);
    ui->fifteenStatus->setText(state.used_solve
        ? tr("%1×%2 · 自动完成后移动 %3 步").arg(state.w).arg(state.h).arg(state.movecount - state.completed)
        : tr("%1×%2 · 已移动 %3 步").arg(state.w).arg(state.h).arg(state.completed ? state.completed : state.movecount));
    ui->fifteenFeedback->setText(state.used_solve ? tr("已自动完成，可以继续滑动练习，或撤销返回刚才的棋盘。")
        : state.completed ? tr("数字归位了！完成时共移动 %1 步。").arg(state.completed)
        : tr("将数字从左上起按顺序排列，空格留在右下角。提示会直接移动一块，连续使用可完成本局。"));
    ui->fifteenUndoButton->setEnabled(!history.empty());
    ui->fifteenRedoButton->setEnabled(!future.empty());
    ui->fifteenHintButton->setEnabled(!state.completed);
    QStringList tiles;
    for (int tile : state.tiles) tiles << QString::number(tile);
    view->setAccessibleDescription(tr("%1 列 %2 行；棋盘 %3（0 为空格）；空格第 %4 行第 %5 列；移动 %6 步")
        .arg(state.w).arg(state.h).arg(tiles.join(',')).arg(state.gap_pos / state.w + 1)
        .arg(state.gap_pos % state.w + 1).arg(state.movecount));
}

bool FifteenGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return Game::eventFilter(watched, event);
        view->setFocus(Qt::MouseFocusReason);
        move(view->cellAt(mouse->position()));
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Undo)) { undo(); return true; }
        if (key->matches(QKeySequence::Redo)) { redo(); return true; }
        int dx = 0, dy = 0;
        switch (key->key()) {
            case Qt::Key_Up: dy = -1; break;
            case Qt::Key_Right: dx = 1; break;
            case Qt::Key_Down: dy = 1; break;
            case Qt::Key_Left: dx = -1; break;
            case Qt::Key_H: hint(); return true;
            case Qt::Key_U: undo(); return true;
            case Qt::Key_R: redo(); return true;
            default: return Game::eventFilter(watched, event);
        }
        // Arrow keys move the tile; the gap moves in the opposite direction.
        const int x = state.gap_pos % state.w - dx, y = state.gap_pos / state.w - dy;
        if (x >= 0 && x < state.w && y >= 0 && y < state.h) move(y * state.w + x);
        return true;
    }
    return Game::eventFilter(watched, event);
}
