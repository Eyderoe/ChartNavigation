#include "flood.hpp"
#include "ui_note_widget.h"
#include "colorPalette.hpp"
#include "squareGameView.hpp"
#include "ui/theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>

using GameColors::floodPalette;

namespace {
// Rules reference: Simon Tatham's Portable Puzzle Collection (MIT).
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/flood.html
// Presets and the depth-3 move chooser follow flood.c, snapshot
// 20260923.616da16. See SIMON-TATHAM-LICENSE.txt.

std::vector<int> floodRegion (const std::vector<int> &board, int side) {
    std::vector<int> region{0};
    std::vector<bool> visited(board.size());
    visited[0] = true;
    for (size_t i = 0; i < region.size(); ++i) {
        const int cell = region[i];
        const auto visit = [&](int next) {
            if (!visited[next] && board[next] == board[0]) {
                visited[next] = true;
                region.push_back(next);
            }
        };
        if (cell % side > 0) visit(cell - 1);
        if (cell % side + 1 < side) visit(cell + 1);
        if (cell >= side) visit(cell - side);
        if (cell + side < static_cast<int>(board.size())) visit(cell + side);
    }
    return region;
}

bool floodComplete (const std::vector<int> &board) {
    return std::ranges::all_of(board, [&](int color) { return color == board[0]; });
}

struct FloodSearchResult {
    int distance{};
    int farthestCount{};
    int controlled{};
};

// This is the same distance measure as flood.c's search(): moving through a
// same-colour cell costs zero; crossing to another colour costs one.
FloodSearchResult searchFlood (const std::vector<int> &board, int side) {
    constexpr int infinity = std::numeric_limits<int>::max();
    std::vector<int> distance(board.size(), infinity);
    std::deque<int> queue{0};
    distance[0] = 0;
    while (!queue.empty()) {
        const int cell = queue.front();
        queue.pop_front();
        const int x = cell % side, y = cell / side;
        const auto relax = [&](int next) {
            const int cost = board[next] == board[cell] ? 0 : 1;
            if (distance[next] <= distance[cell] + cost)
                return;
            distance[next] = distance[cell] + cost;
            if (cost == 0) queue.push_front(next);
            else queue.push_back(next);
        };
        if (x > 0) relax(cell - 1);
        if (x + 1 < side) relax(cell + 1);
        if (y > 0) relax(cell - side);
        if (y + 1 < side) relax(cell + side);
    }
    const int maximum = *std::ranges::max_element(distance);
    return {maximum, static_cast<int>(std::ranges::count(distance, maximum)),
            static_cast<int>(std::ranges::count(distance, 0))};
}

void floodFill (std::vector<int> &board, int color, int side) {
    const auto region = floodRegion(board, side);
    for (int cell : region)
        board[cell] = color;
}

struct FloodChoice {
    int color{-1};
    FloodSearchResult score{std::numeric_limits<int>::max(), 0, 0};
};

FloodChoice chooseFloodMove (const std::vector<int> &board, int side, int colors, int depth) {
    FloodChoice best;
    for (int color = 0; color < colors; ++color) {
        if (color == board[0])
            continue;
        auto candidate = board;
        floodFill(candidate, color, side);
        FloodSearchResult score;
        if (floodComplete(candidate)) {
            score = {-1, depth, static_cast<int>(board.size())};
        } else if (depth < 2) {
            score = chooseFloodMove(candidate, side, colors, depth + 1).score;
        } else {
            score = searchFlood(candidate, side);
        }
        if (score.distance < best.score.distance ||
            (score.distance == best.score.distance &&
             (score.farthestCount < best.score.farthestCount ||
              (score.farthestCount == best.score.farthestCount &&
               score.controlled > best.score.controlled)))) {
            best = {color, score};
        }
    }
    return best;
}

int floodSolutionLength (std::vector<int> board, int side, int colors) {
    int moves = 0;
    while (!floodComplete(board)) {
        const auto choice = chooseFloodMove(board, side, colors, 0);
        Q_ASSERT(choice.color >= 0);
        floodFill(board, choice.color, side);
        ++moves;
    }
    return moves;
}

struct FloodPreset { int side, colors, extraMoves; };
constexpr std::array<FloodPreset, 7> floodPresets{{
    {12, 6, 5}, {12, 6, 2}, {12, 6, 0},
    {16, 6, 2}, {16, 6, 0}, {12, 3, 0}, {12, 4, 0}
}};
}

class FloodView : public SquareGameView {
    public:
        explicit FloodView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("floodView"));
            setFocusPolicy(Qt::StrongFocus);
            setAccessibleName(tr("洪水棋盘"));
        }
        QSize sizeHint () const override { return {280, 280}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        QRectF boardRect () const {
            const qreal side = std::min(width(), height());
            return QRectF((width() - side) / 2.0, 0, side, side).adjusted(2, 2, -2, -2);
        }
        int cellAt (QPointF point) const {
            const auto rect = boardRect();
            if (side <= 0 || rect.width() <= 0 || !rect.contains(point))
                return -1;
            const int column = static_cast<int>((point.x() - rect.left()) * side / rect.width());
            const int row = static_cast<int>((point.y() - rect.top()) * side / rect.height());
            return column < side && row < side ? row * side + column : -1;
        }
        void setBoard (const std::vector<int> &values, int size, int selected) {
            board = values;
            side = size;
            cursor = selected;
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
            const auto rect = boardRect();
            if (side <= 0 || rect.width() <= 0)
                return;
            QPainter painter(this);
            const qreal cellSide = rect.width() / side;
            auto cellFont = font();
            cellFont.setPixelSize(std::max(1, static_cast<int>(cellSide * 0.48)));
            cellFont.setBold(true);
            painter.setFont(cellFont);
            for (int i = 0; i < static_cast<int>(board.size()); ++i) {
                const QRectF cell(rect.left() + i % side * cellSide,
                                  rect.top() + i / side * cellSide, cellSide, cellSide);
                painter.fillRect(cell, QColor(floodPalette[board[i]]));
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(QColor("#ffffff"), 0.5));
                painter.drawRect(cell);
                painter.setPen(board[i] == 2 ? QColor("#30291b") : QColor("#ffffff"));
                painter.drawText(cell, Qt::AlignCenter, QString::number(board[i] + 1));
                if (i == 0) {
                    painter.setBrush(Qt::white);
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(cell.topLeft() + QPointF(cellSide * 0.17, cellSide * 0.17),
                                        cellSide * 0.07, cellSide * 0.07);
                }
                if (i == cursor && hasFocus()) {
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(Qt::white, 3));
                    painter.drawRect(cell.adjusted(2, 2, -2, -2));
                    painter.setPen(QPen(Qt::black, 1));
                    painter.drawRect(cell.adjusted(2, 2, -2, -2));
                }
            }
        }
    private:
        std::vector<int> board;
        int side{};
        int cursor{};
};

FloodGame::FloodGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initFlood();
}

void FloodGame::initFlood () {
    expandComboBox(ui->floodDifficulty);
    floodView = new FloodView(ui->floodBoard);
    ui->floodBoardLayout->addWidget(floodView);
    floodView->installEventFilter(this);
    for (int color = 0; color < static_cast<int>(floodColors.size()); ++color) {
        auto *button = new QToolButton(ui->pageFlood);
        floodColors[color] = button;
        button->setObjectName(QStringLiteral("floodColor%1").arg(color + 1));
        button->setText(QString::number(color + 1));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAccessibleName(tr("颜色 %1").arg(color + 1));
        button->setToolTip(tr("选择颜色 %1（数字键 %1）").arg(color + 1));
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        // Use an icon so native and themed button styles retain their focus,
        // hover and disabled indicators while sharing the board's colours.
        QPixmap swatch(24, 24);
        swatch.fill(QColor(floodPalette[color]));
        button->setIcon(QIcon(swatch));
        ui->floodColorsLayout->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, color] {
            moveFlood(color);
            floodView->setFocus(Qt::OtherFocusReason);
        });
    }
    connect(ui->floodDifficulty, &QComboBox::currentIndexChanged, this, &FloodGame::resetFlood);
    connect(ui->floodNewButton, &QToolButton::clicked, this, &FloodGame::resetFlood);
    connect(ui->floodRetryButton, &QToolButton::clicked, this, [this] {
        floodBoard = floodInitialBoard;
        floodHistory.clear();
        floodCursor = 0;
        updateFlood();
    });
    connect(ui->floodUndoButton, &QToolButton::clicked, this, [this] {
        if (floodHistory.empty())
            return;
        floodBoard = std::move(floodHistory.back());
        floodHistory.pop_back();
        updateFlood();
    });
    resetFlood();
}

void FloodGame::resetFlood () {
    const int presetIndex = std::clamp(ui->floodDifficulty->currentIndex(), 0,
                                       static_cast<int>(floodPresets.size()) - 1);
    const auto preset = floodPresets[presetIndex];
    floodSide = preset.side;
    floodColorCount = preset.colors;
    floodBoard.resize(floodSide * floodSide);
    do {
        for (int &color : floodBoard)
            color = QRandomGenerator::global()->bounded(floodColorCount);
    } while (floodComplete(floodBoard));
    floodInitialBoard = floodBoard;
    floodHistory.clear();
    floodCursor = 0;
    floodMoveLimit = floodSolutionLength(floodBoard, floodSide, floodColorCount) + preset.extraMoves;
    updateFlood();
}

void FloodGame::moveFlood (int color) {
    if (color < 0 || color >= floodColorCount || color == floodBoard[0]
        || floodComplete(floodBoard) || static_cast<int>(floodHistory.size()) >= floodMoveLimit)
        return;
    const auto region = floodRegion(floodBoard, floodSide);
    floodHistory.push_back(floodBoard);
    for (int cell : region)
        floodBoard[cell] = color;
    updateFlood();
}

void FloodGame::updateFlood () {
    const int moves = static_cast<int>(floodHistory.size());
    const bool solved = floodComplete(floodBoard);
    const bool finished = solved || moves >= floodMoveLimit;
    floodView->setBoard(floodBoard, floodSide, floodCursor);
    ui->floodStatus->setText(tr("步数：%1 / %2　已淹没：%3 / %4")
        .arg(moves).arg(floodMoveLimit).arg(floodRegion(floodBoard, floodSide).size()).arg(floodBoard.size()));
    ui->floodUndoButton->setEnabled(!floodHistory.empty());
    for (int color = 0; color < static_cast<int>(floodColors.size()); ++color) {
        floodColors[color]->setVisible(color < floodColorCount);
        floodColors[color]->setEnabled(!finished && color != floodBoard[0]);
    }
    ui->floodFeedback->setText(solved
        ? tr("全盘同色！共用了 %1 步。点击新一局继续。").arg(moves)
        : finished ? tr("步数已用完。可以撤销，或重试本局。") : QString{});
    floodView->setAccessibleDescription(ui->floodStatus->text() + ui->floodFeedback->text());
}

bool FloodGame::handleFloodEvent (QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton)
            return false;
        const int cell = floodView->cellAt(mouse->position());
        if (cell < 0)
            return false;
        floodCursor = cell;
        floodView->setFocus(Qt::MouseFocusReason);
        moveFlood(floodBoard[cell]);
        updateFlood();
        return true;
    }
    if (event->type() != QEvent::KeyPress)
        return false;
    const auto *key = static_cast<QKeyEvent*>(event);
    if ((key->modifiers() & ~Qt::KeypadModifier) != Qt::NoModifier)
        return false;
    const int row = floodCursor / floodSide, column = floodCursor % floodSide;
    switch (key->key()) {
        case Qt::Key_Left: if (column > 0) --floodCursor; break;
        case Qt::Key_Right: if (column + 1 < floodSide) ++floodCursor; break;
        case Qt::Key_Up: if (row > 0) floodCursor -= floodSide; break;
        case Qt::Key_Down: if (row + 1 < floodSide) floodCursor += floodSide; break;
        case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space:
            moveFlood(floodBoard[floodCursor]); break;
        default:
            if (key->key() < Qt::Key_1 || key->key() >= Qt::Key_1 + floodColorCount)
                return false;
            moveFlood(key->key() - Qt::Key_1);
            break;
    }
    updateFlood();
    return true;
}

bool FloodGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched == floodView && handleFloodEvent(event))
        return true;
    return QObject::eventFilter(watched, event);
}
