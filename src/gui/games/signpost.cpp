#include "signpost.hpp"
#include "squareGameView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <cstring>

class SignpostView : public SquareGameView {
    public:
        explicit SignpostView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("signpostView"));
            setFocusPolicy(Qt::StrongFocus);
            setMouseTracking(true);
            setAccessibleName(tr("路标棋盘"));
        }
        QSize sizeHint () const override { return {320, 320}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        QRectF boardRect () const {
            const qreal tile = std::max(0.0, std::min((width() - 16.0) / columns,
                                                     (height() - 16.0) / rows));
            return {(width() - tile * columns) / 2, (height() - tile * rows) / 2,
                    tile * columns, tile * rows};
        }
        int cellAt (QPointF point) const {
            const auto rect = boardRect();
            if (rect.width() <= 0 || point.x() < rect.left() || point.x() >= rect.right() ||
                point.y() < rect.top() || point.y() >= rect.bottom()) return -1;
            const qreal tile = rect.width() / columns;
            return static_cast<int>((point.y() - rect.top()) / tile) * columns
                 + static_cast<int>((point.x() - rect.left()) / tile);
        }
        QPointF cellCentre (int cell) const {
            const auto rect = boardRect();
            const qreal tile = rect.width() / columns;
            return rect.topLeft() + QPointF((cell % columns + 0.5) * tile,
                                            (cell / columns + 0.5) * tile);
        }
        void setState (const SignpostBackend *value, SignpostConfig config,
                       int selected, bool showCursor, int origin, bool reverse) {
            state = value; columns = config.width; rows = config.height;
            cursor = selected; cursorVisible = showCursor; dragStart = origin; backwards = reverse;
            update();
        }
        void setDragPoint (QPointF point) { dragPoint = point; update(); }
    protected:
        void paintEvent (QPaintEvent *) override {
            if (!state) return;
            QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
            const auto rect = boardRect();
            const qreal tile = rect.width() / columns;
            const int n = signpost_cells(state);
            const bool dark = palette().color(QPalette::Base).lightness() < 128;
            const QColor text = palette().color(QPalette::Text);
            const QColor muted = palette().color(QPalette::PlaceholderText);
            for (int cell = 0; cell < n; ++cell) {
                const auto centre = cellCentre(cell);
                const QRectF square(centre - QPointF(tile/2, tile/2), QSizeF(tile, tile));
                const int number = signpost_number(state, cell);
                const int group = number > 0 ? number / (n+1) : 0;
                const bool fixed = signpost_fixed(state, cell);
                bool available = false;
                if (dragStart >= 0)
                    available = backwards ? signpost_can_link(state, cell, dragStart, true)
                                          : signpost_can_link(state, dragStart, cell, true);
                QColor background = palette().color(QPalette::Base);
                if (group > 0) background = QColor::fromHsv((group * 67) % 360, dark ? 110 : 55, dark ? 90 : 248);
                if (available) background = palette().color(QPalette::Highlight);
                painter.setBrush(background); painter.setPen(QPen(palette().color(QPalette::Mid), 0.8));
                painter.drawRect(square);
                const QColor ink = available ? palette().color(QPalette::HighlightedText) : text;
                char label[64]; signpost_label(state, cell, label, sizeof(label));
                auto numberFont = font(); numberFont.setBold(fixed);
                const int pixels = std::max(1, static_cast<int>(std::min(tile * 0.34,
                    tile * 1.35 / std::max<size_t>(4, std::strlen(label)))));
                numberFont.setPixelSize(pixels); painter.setFont(numberFont);
                painter.setPen(signpost_error(state, cell) && !fixed ? QColor("#e46b78") : ink);
                painter.drawText(QRectF(square.left() + tile * 0.08, square.top() + tile * 0.03,
                                        tile * 0.84, tile * 0.4), Qt::AlignLeft | Qt::AlignVCenter,
                                 QString::fromLatin1(label));
                const QPointF arrowCentre = square.topLeft() + QPointF(tile * 0.72, tile * 0.72);
                painter.setPen(QPen(cell == dragStart ? palette().color(QPalette::Highlight)
                    : signpost_next(state, cell) >= 0 ? muted : ink, std::max(1.0, tile * 0.045),
                    Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                if (fixed && number == n) {
                    QPolygonF star;
                    for (int i = 0; i < 10; ++i) {
                        const qreal angle = i * 3.141592653589793 / 5 - 3.141592653589793 / 2;
                        const qreal radius = tile * (i % 2 ? 0.08 : 0.19);
                        star << arrowCentre + QPointF(std::cos(angle), std::sin(angle)) * radius;
                    }
                    painter.setBrush(ink); painter.drawPolygon(star);
                } else {
                    const qreal angle = signpost_direction(state, cell) * 3.141592653589793 / 4
                                      - 3.141592653589793 / 2;
                    const QPointF unit(std::cos(angle), std::sin(angle)), side(-unit.y(), unit.x());
                    const QPointF tip = arrowCentre + unit * tile * 0.18;
                    painter.drawLine(arrowCentre - unit * tile * 0.18, tip);
                    painter.drawPolyline(QPolygonF{tip - unit * tile * 0.13 + side * tile * 0.11,
                                                   tip, tip - unit * tile * 0.13 - side * tile * 0.11});
                }
                if (number != 1 && signpost_prev(state, cell) < 0) {
                    painter.setPen(Qt::NoPen); painter.setBrush(ink);
                    painter.drawEllipse(square.topLeft() + QPointF(tile * 0.25, tile * 0.72), tile * 0.035, tile * 0.035);
                }
                if ((cell == cursor && cursorVisible && hasFocus()) || cell == dragStart) {
                    painter.setPen(QPen(ink, 2, Qt::DashLine)); painter.setBrush(Qt::NoBrush);
                    painter.drawRect(square.adjusted(2, 2, -2, -2));
                }
            }
            if (dragStart >= 0) {
                const auto start = cellCentre(dragStart);
                const auto end = dragPoint;
                painter.setPen(QPen(palette().color(QPalette::Highlight), 2));
                painter.drawLine(start, end);
                const QPointF vector = backwards ? start - end : end - start;
                const qreal length = std::hypot(vector.x(), vector.y());
                if (length > 1) {
                    const QPointF unit = vector / length, side(-unit.y(), unit.x());
                    const QPointF tip = backwards ? start : end;
                    painter.drawPolyline(QPolygonF{tip - unit * 10 + side * 5, tip, tip - unit * 10 - side * 5});
                }
            }
        }
        void focusInEvent (QFocusEvent *event) override { SquareGameView::focusInEvent(event); update(); }
        void focusOutEvent (QFocusEvent *event) override { SquareGameView::focusOutEvent(event); update(); }
    private:
        const SignpostBackend *state{};
        int columns{4}, rows{4}, cursor{}, dragStart{-1};
        bool backwards{}, cursorVisible{};
        QPointF dragPoint;
};

SignpostGame::State SignpostGame::take (SignpostBackend *value) { return State(value, signpost_free); }

SignpostGame::SignpostGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new SignpostView(ui->signpostBoard); ui->signpostBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < signpost_preset_count(); ++i) {
        const auto p = signpost_preset(i);
        ui->signpostPreset->addItem(tr("%1×%2 · %3").arg(p.width).arg(p.height)
            .arg(p.cornerEnds ? tr("端点固定在角落") : tr("自由端点")));
    }
    ui->signpostPreset->addItem(tr("自定义…"));
    ui->signpostPreset->setMinimumContentsLength(5);
    ui->signpostPreset->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    expandComboBox(ui->signpostPreset);
    for (auto *dimension : {ui->signpostWidth, ui->signpostHeight}) {
        dimension->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed); dimension->setMinimumWidth(48);
    }
    connect(ui->signpostPreset, &QComboBox::currentIndexChanged, this, &SignpostGame::selectPreset);
    connect(ui->signpostApplyButton, &QToolButton::clicked, this, &SignpostGame::applyCustom);
    connect(ui->signpostNewButton, &QToolButton::clicked, this, &SignpostGame::reset);
    connect(ui->signpostRetryButton, &QToolButton::clicked, this, &SignpostGame::retry);
    connect(ui->signpostUndoButton, &QToolButton::clicked, this, &SignpostGame::undo);
    connect(ui->signpostRedoButton, &QToolButton::clicked, this, &SignpostGame::redo);
    connect(ui->signpostSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy) adopt(take(signpost_solve(initial.get(), state.get())));
    });
    selectPreset();
}

void SignpostGame::selectPreset () {
    const int index = ui->signpostPreset->currentIndex();
    const bool custom = index == signpost_preset_count();
    ui->signpostCustomSettings->setVisible(custom);
    if (!custom) config = signpost_preset(index);
    ui->signpostWidth->setValue(config.width); ui->signpostHeight->setValue(config.height);
    ui->signpostCornerEnds->setChecked(config.cornerEnds);
    if (!custom) reset();
}
void SignpostGame::applyCustom () {
    const SignpostConfig candidate{ui->signpostWidth->value(), ui->signpostHeight->value(),
                                   ui->signpostCornerEnds->isChecked()};
    if (const auto *error = signpost_validate(candidate)) {
        ui->signpostFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error))); return;
    }
    config = candidate; reset();
}
void SignpostGame::reset () {
    const unsigned ticket = ++generation;
    cancelDrag(); cursor = 0; cursorVisible = false; busy = true;
    state.reset(); initial.reset(); history.clear(); future.clear(); refresh();
    const auto parameters = config;
    const auto seed = QByteArray::number(QRandomGenerator::global()->generate64());
    auto *watcher = new QFutureWatcher<State>(this);
    connect(watcher, &QFutureWatcher<State>::finished, this, [this, watcher, ticket] {
        auto result = watcher->result(); watcher->deleteLater();
        if (ticket != generation) return;
        busy = false; state = std::move(result); initial = take(signpost_copy(state.get())); refresh();
    });
    watcher->setFuture(QtConcurrent::run([parameters, seed] { return take(signpost_new(parameters, seed.constData())); }));
}
void SignpostGame::retry () {
    if (!busy && initial) adopt(take(signpost_copy(initial.get())));
}
void SignpostGame::adopt (State next) {
    cancelDrag();
    if (!next) { refresh(); return; }
    history.push_back(state); future.clear(); state = std::move(next); refresh();
}
void SignpostGame::undo () {
    if (busy || history.empty()) return;
    cancelDrag(); future.push_back(state); state = std::move(history.back()); history.pop_back(); refresh();
}
void SignpostGame::redo () {
    if (busy || future.empty()) return;
    cancelDrag(); history.push_back(state); state = std::move(future.back()); future.pop_back(); refresh();
}
void SignpostGame::cancelDrag () { dragStart = -1; keyboardDrag = false; }
void SignpostGame::beginDrag (int cell, bool reverse, bool keyboard) {
    if (busy || !state || cell < 0 || cell >= signpost_cells(state.get())) return;
    const int number = signpost_number(state.get(), cell);
    if (signpost_fixed(state.get(), cell) &&
        ((reverse && number == 1) || (!reverse && number == signpost_cells(state.get())))) return;
    dragStart = cell; backwards = reverse; keyboardDrag = keyboard;
    view->setDragPoint(view->cellCentre(cell)); refresh();
}
void SignpostGame::finishDrag (int cell) {
    const int origin = dragStart;
    if (origin < 0) return;
    const bool reverse = backwards;
    cancelDrag();
    if (cell == origin) { refresh(); return; }
    if (cell < 0) adopt(take(signpost_unlink(state.get(), origin, reverse)));
    else adopt(take(signpost_link(state.get(), reverse ? cell : origin, reverse ? origin : cell)));
}
void SignpostGame::refresh () {
    view->setState(state.get(), config, cursor, cursorVisible, dragStart, backwards);
    int links = 0; QStringList knowledge;
    for (int cell = 0; cell < signpost_cells(state.get()); ++cell) {
        links += signpost_next(state.get(), cell) >= 0;
        char label[64]; signpost_label(state.get(), cell, label, sizeof(label));
        knowledge << tr("%1:%2,方向%3,后继%4").arg(cell+1).arg(QString::fromLatin1(label))
            .arg(signpost_direction(state.get(), cell)).arg(signpost_next(state.get(), cell)+1);
    }
    ui->signpostStatus->setText(tr("%1×%2 · 已连接 %3 / %4").arg(config.width).arg(config.height)
        .arg(links).arg(std::max(0, signpost_cells(state.get())-1)));
    ui->signpostFeedback->setText(busy ? tr("正在生成棋盘并校验可解性…")
        : !state ? tr("未能生成棋盘，请开新一局。")
        : signpost_used_solve(state.get()) ? tr("已显示答案，可撤销返回。")
        : signpost_won(state.get()) ? tr("整条路径连起来了！")
        : dragStart >= 0 ? (backwards ? tr("选择前一个格子；拖到棋盘外可断开整条未编号链。")
                                      : tr("选择后一个格子；拖到棋盘外可断开这个格子的连线。"))
        : tr("沿箭头连接 1 到 %1；同方向可以跨越多个格子。粗体数字是固定线索。").arg(signpost_cells(state.get())));
    ui->signpostRetryButton->setEnabled(!busy && bool(initial));
    ui->signpostUndoButton->setEnabled(!busy && !history.empty());
    ui->signpostRedoButton->setEnabled(!busy && !future.empty());
    ui->signpostSolveButton->setEnabled(!busy && bool(state) && !signpost_used_solve(state.get()));
    view->setAccessibleDescription(ui->signpostStatus->text() + QStringLiteral("；") + knowledge.join(';'));
}

bool SignpostGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::Hide) { cancelDrag(); refresh(); }
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton && mouse->button() != Qt::RightButton) return false;
        cancelDrag(); cursorVisible = false; view->setFocus(Qt::MouseFocusReason);
        const int cell = view->cellAt(mouse->position());
        beginDrag(cell, mouse->button() == Qt::RightButton || mouse->modifiers().testFlag(Qt::ControlModifier), false);
        refresh();
        return true;
    }
    if (event->type() == QEvent::MouseMove && dragStart >= 0 && !keyboardDrag) {
        view->setDragPoint(static_cast<QMouseEvent*>(event)->position()); return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && dragStart >= 0 && !keyboardDrag) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton || mouse->button() == Qt::RightButton)
            finishDrag(view->cellAt(mouse->position()));
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Undo)) { undo(); return true; }
        if (key->matches(QKeySequence::Redo)) { redo(); return true; }
        if (busy || !state) return false;
        const int x = cursor % config.width, y = cursor / config.width;
        switch (key->key()) {
            case Qt::Key_Left: if (x > 0) --cursor; break;
            case Qt::Key_Right: if (x < config.width-1) ++cursor; break;
            case Qt::Key_Up: if (y > 0) cursor -= config.width; break;
            case Qt::Key_Down: if (y < config.height-1) cursor += config.width; break;
            case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
                if (!cursorVisible) { cursorVisible = true; refresh(); }
                else if (keyboardDrag) finishDrag(cursor);
                else beginDrag(cursor, key->key() == Qt::Key_Space, true);
                return true;
            case Qt::Key_Escape: cancelDrag(); refresh(); return true;
            case Qt::Key_Delete: case Qt::Key_Backspace: case Qt::Key_X:
                if (cursorVisible) adopt(take(signpost_unlink(state.get(), cursor, key->modifiers().testFlag(Qt::ShiftModifier))));
                return true;
            case Qt::Key_U: undo(); return true;
            case Qt::Key_R: redo(); return true;
            default: return Game::eventFilter(watched, event);
        }
        cursorVisible = true;
        if (dragStart >= 0) view->setDragPoint(view->cellCentre(cursor));
        refresh(); return true;
    }
    return Game::eventFilter(watched, event);
}
