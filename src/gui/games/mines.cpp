#include "mines.hpp"
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
#include <array>
#include <cmath>

class MinesView : public SquareGameView {
    public:
        explicit MinesView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("minesView"));
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(tr("扫雷棋盘"));
        }
        QSize sizeHint () const override { return {320, 320}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        void setGeometry (const MinesBackend *state) {
            polygons.clear(); centres.clear(); radii.clear(); bounds = {};
            for (int cell = 0; cell < mines_cells(state); ++cell) {
                QPolygonF polygon;
                for (int corner = 0; corner < mines_corners(state, cell); ++corner) {
                    double x{}, y{};
                    mines_corner(state, cell, corner, &x, &y);
                    polygon << QPointF(x, y);
                }
                double x{}, y{}, radius{};
                mines_centre(state, cell, &x, &y, &radius);
                bounds = bounds.isNull() ? polygon.boundingRect() : bounds.united(polygon.boundingRect());
                polygons.push_back(polygon);
                centres.emplace_back(x, y);
                radii.push_back(radius);
            }
        }
        QTransform boardTransform () const {
            if (bounds.width() <= 0 || bounds.height() <= 0) return {};
            const qreal scale = std::max(0.0, std::min((width() - 8) / bounds.width(),
                                                       (height() - 8) / bounds.height()));
            QTransform result;
            result.translate((width() - bounds.width() * scale) / 2,
                             (height() - bounds.height() * scale) / 2);
            result.scale(scale, scale);
            result.translate(-bounds.left(), -bounds.top());
            return result;
        }
        int cellAt (QPointF point) const {
            bool invertible{};
            const auto inverse = boardTransform().inverted(&invertible);
            if (!invertible) return -1;
            const auto gridPoint = inverse.map(point);
            for (int cell = 0; cell < static_cast<int>(polygons.size()); ++cell)
                if (polygons[cell].containsPoint(gridPoint, Qt::OddEvenFill)) return cell;
            return -1;
        }
        QPointF cellCentre (int cell) const { return boardTransform().map(centres[cell]); }
        void setState (const MinesBackend *state, int selected) {
            board.clear();
            for (int i = 0; i < mines_cells(state); ++i) board.push_back(mines_cell(state, i));
            cursor = selected;
            start = mines_start_cell(state);
            update();
        }
    protected:
        void focusInEvent (QFocusEvent *event) override {
            SquareGameView::focusInEvent(event); update();
        }
        void focusOutEvent (QFocusEvent *event) override {
            SquareGameView::focusOutEvent(event); update();
        }
        void paintEvent (QPaintEvent *) override {
            if (board.size() != polygons.size()) return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const auto transform = boardTransform();
            const bool dark = palette().color(QPalette::Base).lightness() < 128;
            const QColor base = palette().color(QPalette::Base);
            const QColor covered = dark ? base.lighter(185) : base.darker(115);
            const std::array<const char*, 8> lightColors{"#1767b2", "#247348", "#bd3846", "#483b9a", "#975022", "#137d86", "#19232d", "#697582"};
            const std::array<const char*, 8> darkColors{"#73b9ff", "#84d6a3", "#ff8895", "#b3a0ff", "#edba85", "#81d5df", "#eff0f1", "#aab8c7"};
            for (int cell = 0; cell < static_cast<int>(board.size()); ++cell) {
                const int value = board[cell];
                const auto polygon = transform.map(polygons[cell]);
                const auto centre = transform.map(centres[cell]);
                const qreal radius = std::max(1.0, radii[cell] * transform.m11());
                painter.setPen(QPen(palette().color(QPalette::Mid), 0.8));
                painter.setBrush(value == 65 ? QColor("#da6873")
                    : value < 0 ? covered : base);
                painter.drawPolygon(polygon);
                if (value >= 1 && value <= 16) {
                    auto numberFont = font();
                    numberFont.setBold(true);
                    numberFont.setPixelSize(std::max(1, static_cast<int>(radius * 1.4)));
                    painter.setFont(numberFont);
                    painter.setPen(value <= 8 ? QColor((dark ? darkColors : lightColors)[value - 1]) : palette().color(QPalette::Text));
                    painter.drawText(QRectF(centre - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius)),
                                     Qt::AlignCenter, QString::number(value));
                } else if (value == -1) {
                    painter.setPen(QPen(palette().color(QPalette::Text), std::max(1.0, radius * 0.1)));
                    painter.drawLine(centre + QPointF(-radius * 0.3, -radius * 0.6), centre + QPointF(-radius * 0.3, radius * 0.6));
                    painter.setBrush(QColor("#e66d78"));
                    painter.setPen(Qt::NoPen);
                    painter.drawPolygon(QPolygonF{centre + QPointF(-radius * 0.3, -radius * 0.6),
                        centre + QPointF(radius * 0.65, -radius * 0.15), centre + QPointF(-radius * 0.3, radius * 0.1)});
                } else if (value >= 64) {
                    const QColor color = palette().color(QPalette::Text);
                    painter.setPen(QPen(color, std::max(1.0, radius * 0.12)));
                    for (int direction = 0; direction < 4; ++direction) {
                        const qreal angle = direction * 3.141592653589793 / 4;
                        const QPointF arm(std::cos(angle) * radius * 0.65, std::sin(angle) * radius * 0.65);
                        painter.drawLine(centre - arm, centre + arm);
                    }
                    painter.setBrush(color); painter.setPen(Qt::NoPen);
                    painter.drawEllipse(centre, radius * 0.4, radius * 0.4);
                    if (value == 66) {
                        painter.setPen(QPen(QColor("#e66d78"), std::max(1.0, radius * 0.2)));
                        painter.drawLine(centre - QPointF(radius * 0.65, radius * 0.65), centre + QPointF(radius * 0.65, radius * 0.65));
                        painter.drawLine(centre + QPointF(-radius * 0.65, radius * 0.65), centre + QPointF(radius * 0.65, -radius * 0.65));
                    }
                }
                if (cell == start && value == -2) {
                    painter.setPen(Qt::NoPen); painter.setBrush(QColor("#33805a"));
                    painter.drawEllipse(centre, radius * 0.16, radius * 0.16);
                }
                if (cell == cursor && hasFocus()) {
                    painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
                    painter.setBrush(Qt::NoBrush); painter.drawPolygon(polygon);
                }
            }
        }
    private:
        std::vector<QPolygonF> polygons;
        std::vector<QPointF> centres;
        std::vector<qreal> radii;
        std::vector<int> board;
        QRectF bounds;
        int cursor{}, start{-1};
};

MinesGame::State MinesGame::take (MinesBackend *value) { return State(value, mines_free); }

MinesGame::MinesGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    view = new MinesView(ui->minesBoard);
    ui->minesBoardLayout->addWidget(view);
    view->installEventFilter(this);
    for (int i = 0; i < mines_preset_count(); ++i) {
        const auto preset = mines_preset(i);
        char count[32]; mines_count_text(preset, count, sizeof(count));
        auto title = tr("%1×%2 · %3 雷 · %4").arg(preset.width).arg(preset.height)
            .arg(QString::fromLatin1(count), QString::fromLatin1(mines_grid_name(preset.type)));
        const QString group = QString::fromLatin1(mines_preset_group(i));
        if (!group.isEmpty()) title.prepend(group + QStringLiteral(" · "));
        ui->minesPreset->addItem(title);
    }
    ui->minesPreset->addItem(tr("自定义…"));
    for (int type = 0; type < mines_grid_count(); ++type)
        ui->minesGridType->addItem(QString::fromLatin1(mines_grid_name(type)));
    for (auto *combo : {ui->minesPreset, ui->minesGridType}) {
        combo->setMinimumContentsLength(5);
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    }
    for (auto *dimension : {ui->minesWidth, ui->minesHeight}) {
        dimension->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        dimension->setMinimumWidth(48);
    }
    expandComboBox(ui->minesPreset); expandComboBox(ui->minesGridType);
    connect(ui->minesPreset, &QComboBox::currentIndexChanged, this, &MinesGame::selectPreset);
    connect(ui->minesApplyButton, &QToolButton::clicked, this, &MinesGame::applyCustom);
    connect(ui->minesNewButton, &QToolButton::clicked, this, &MinesGame::reset);
    connect(ui->minesRetryButton, &QToolButton::clicked, this, &MinesGame::retry);
    connect(ui->minesUndoButton, &QToolButton::clicked, this, &MinesGame::undo);
    connect(ui->minesRedoButton, &QToolButton::clicked, this, &MinesGame::redo);
    connect(ui->minesSolveButton, &QToolButton::clicked, this, [this] {
        if (!busy) adopt(take(mines_solve(state.get())));
    });
    selectPreset();
}

void MinesGame::fillCustom () {
    ui->minesWidth->setValue(config.width); ui->minesHeight->setValue(config.height);
    ui->minesGridType->setCurrentIndex(config.type); ui->minesSoluble->setChecked(config.soluble);
    char count[32]; mines_count_text(config, count, sizeof(count));
    ui->minesCount->setText(QString::fromLatin1(count));
}

void MinesGame::selectPreset () {
    const int index = ui->minesPreset->currentIndex();
    const bool custom = index == mines_preset_count();
    ui->minesCustomSettings->setVisible(custom);
    if (custom) { fillCustom(); return; }
    config = mines_preset(index);
    fillCustom(); reset();
}

void MinesGame::applyCustom () {
    MinesConfig candidate{ui->minesWidth->value(), ui->minesHeight->value(),
                          ui->minesGridType->currentIndex(), false, 0, ui->minesSoluble->isChecked()};
    if (!mines_parse_count(&candidate, ui->minesCount->text().trimmed().toLatin1().constData())) {
        ui->minesFeedback->setText(tr("雷数请输入正整数，或 0～100 之间的百分比，例如 10 或 25%。"));
        return;
    }
    if (const auto *error = mines_validate(candidate)) {
        ui->minesFeedback->setText(tr("参数无效：%1").arg(QString::fromUtf8(error))); return;
    }
    config = candidate; reset();
}

void MinesGame::reset () {
    ++generation; busy = false; pressedCell = -1;
    seed = QByteArray::number(QRandomGenerator::global()->generate64());
    state = take(mines_new(config, seed.constData()));
    initial = take(mines_copy(state.get()));
    history.clear(); future.clear(); deaths = 0; cursor = 0;
    view->setGeometry(state.get()); refresh();
}

void MinesGame::retry () {
    ++generation; busy = false; pressedCell = -1;
    cursor = std::max(0, mines_start_cell(initial.get()));
    adopt(take(mines_copy(initial.get())));
}

void MinesGame::action (int cell, bool flag) {
    if (busy || cell < 0 || cell >= mines_cells(state.get())) return;
    cursor = cell;
    if (!flag && !mines_generated(state.get()) && mines_cell(state.get(), cell) == -2) {
        busy = true;
        const unsigned ticket = ++generation;
        const auto parameters = config;
        const auto randomSeed = seed;
        std::vector<int> board;
        for (int i = 0; i < mines_cells(state.get()); ++i) board.push_back(mines_cell(state.get(), i));
        auto *watcher = new QFutureWatcher<State>(this);
        connect(watcher, &QFutureWatcher<State>::finished, this, [this, watcher, ticket] {
            auto next = watcher->result(); watcher->deleteLater();
            if (ticket != generation) return;
            busy = false;
            if (!next) { refresh(); return; }
            // The worker's grid/layout is isolated until now. Rebase every old
            // snapshot so Undo and Restart preserve the first-click minefield.
            initial = take(mines_rebase(initial.get(), next.get()));
            // Upstream Restart restores the opening area after the first click.
            initial = take(mines_action(initial.get(), mines_start_cell(next.get()), false));
            for (auto &old : history) old = take(mines_rebase(old.get(), next.get()));
            state = take(mines_rebase(state.get(), next.get()));
            adopt(std::move(next));
        });
        watcher->setFuture(QtConcurrent::run([parameters, randomSeed, board = std::move(board), cell] {
            return take(mines_first_open(parameters, randomSeed.constData(), board.data(), cell));
        }));
        refresh(); return;
    }
    adopt(take(mines_action(state.get(), cell, flag)));
    refresh();
}

void MinesGame::adopt (State next) {
    if (!next) return;
    if (!mines_dead(state.get()) && mines_dead(next.get())) ++deaths;
    history.push_back(state); future.clear(); state = std::move(next); refresh();
}

void MinesGame::undo () {
    if (busy || history.empty()) return;
    future.push_back(state); state = std::move(history.back()); history.pop_back(); refresh();
}

void MinesGame::redo () {
    if (busy || future.empty()) return;
    history.push_back(state); state = std::move(future.back()); future.pop_back(); refresh();
}

void MinesGame::refresh () {
    view->setState(state.get(), cursor);
    int flags = 0, opened = 0;
    QStringList knowledge;
    for (int i = 0; i < mines_cells(state.get()); ++i) {
        const int value = mines_cell(state.get(), i);
        flags += value == -1; opened += value >= 0 && value <= 16;
        knowledge << QString::number(value);
    }
    char count[32]; mines_count_text(config, count, sizeof(count));
    const QString target = mines_generated(state.get()) ? QString::number(mines_nominal_count(state.get()))
                                                       : QString::fromLatin1(count);
    ui->minesStatus->setText(tr("雷数 %1 · 标记 %2 · 已打开 %3 / %4 · 踩雷 %5 次")
        .arg(target).arg(flags).arg(opened).arg(mines_cells(state.get())).arg(deaths));
    ui->minesFeedback->setText(busy ? tr("正在为首次点击生成棋盘并校验可解性…")
        : mines_used_solve(state.get()) ? tr("已显示本局布局，可撤销返回。")
        : mines_dead(state.get()) ? tr("踩到雷了！可以撤销继续；踩雷次数会保留。")
        : mines_won(state.get()) ? tr("安全格全部打开了！本局踩雷 %1 次。").arg(deaths)
        : !mines_generated(state.get()) ? tr("请选择第一格：它和相邻格都不会有雷。")
        : config.soluble ? tr("这局可以通过推理完成，无需猜测。撤销首次点击后，绿点标出原来的安全起点。")
        : tr("已关闭确保可解性，本局可能需要猜测。"));
    ui->minesUndoButton->setEnabled(!busy && !history.empty());
    ui->minesRedoButton->setEnabled(!busy && !future.empty());
    ui->minesSolveButton->setEnabled(!busy && mines_generated(state.get()) && !mines_used_solve(state.get()));
    view->setAccessibleDescription(tr("%1；格子状态 %2（-2 未打开，-1 插旗，0～16 邻雷数，65 踩雷）；光标 %3")
        .arg(ui->minesStatus->text(), knowledge.join(',')).arg(cursor + 1));
}

bool MinesGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        const int cell = view->cellAt(mouse->position());
        if (cell < 0) return true;
        cursor = cell; view->setFocus(Qt::MouseFocusReason);
        if (mouse->button() == Qt::RightButton ||
            (mouse->button() == Qt::LeftButton && mouse->modifiers().testFlag(Qt::ControlModifier))) {
            pressedCell = -1; action(cell, true);
        } else if (mouse->button() == Qt::LeftButton) pressedCell = cell;
        refresh(); return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        const int cell = view->cellAt(mouse->position());
        if (mouse->button() == Qt::LeftButton && cell == pressedCell && cell >= 0) action(cell, false);
        pressedCell = -1; return true;
    }
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent*>(event);
        if (key->matches(QKeySequence::Undo)) { undo(); return true; }
        if (key->matches(QKeySequence::Redo)) { redo(); return true; }
        int direction = -1;
        switch (key->key()) {
            case Qt::Key_Up: direction = 0; break;
            case Qt::Key_Right: direction = 1; break;
            case Qt::Key_Down: direction = 2; break;
            case Qt::Key_Left: direction = 3; break;
            case Qt::Key_Return: case Qt::Key_Enter: action(cursor, false); return true;
            case Qt::Key_Space: action(cursor, true); return true;
            case Qt::Key_U: undo(); return true;
            case Qt::Key_R: redo(); return true;
            default: return Game::eventFilter(watched, event);
        }
        const int next = mines_cursor(state.get(), cursor, direction);
        if (next >= 0) cursor = next;
        refresh(); return true;
    }
    return Game::eventFilter(watched, event);
}
