#include "pegs.hpp"
#include "gridGameView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {
// Rules reference: Simon Tatham's Portable Puzzle Collection (MIT).
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/pegs.html
// Presets and the reverse-move random generator follow pegs.c, snapshot
// 20260923.616da16. See SIMON-TATHAM-LICENSE.txt.
// -1 is an obstacle, 0 an empty hole, and 1 a peg.
int pegNeighbor (int cell, int direction, int width, int height) {
    if (cell < 0 || cell >= width * height) return -1;
    switch (direction) {
        case 0: return cell >= width ? cell - width : -1;
        case 1: return cell % width + 1 < width ? cell + 1 : -1;
        case 2: return cell + width < width * height ? cell + width : -1;
        case 3: return cell % width > 0 ? cell - 1 : -1;
        default: return -1;
    }
}

int jumpedPeg (const std::vector<int> &board, int width, int height, int from, int to) {
    if (from < 0 || to < 0 || from >= width * height || to >= width * height || board[from] != 1 || board[to] != 0) return -1;
    const int dx = to % width - from % width, dy = to / width - from / width;
    if (!((std::abs(dx) == 2 && dy == 0) || (std::abs(dy) == 2 && dx == 0))) return -1;
    const int middle = from + (to - from) / 2;
    return board[middle] == 1 ? middle : -1;
}

std::vector<std::pair<int, int>> pegMoves (const std::vector<int> &board, int width, int height) {
    std::vector<std::pair<int, int>> moves;
    for (int from = 0; from < width * height; ++from) {
        if (board[from] != 1) continue;
        for (int d = 0; d < 4; ++d) {
            const int to = pegNeighbor(pegNeighbor(from, d, width, height), d, width, height);
            if (jumpedPeg(board, width, height, from, to) >= 0) moves.emplace_back(from, to);
        }
    }
    return moves;
}

enum class PegsShape { Cross, Octagon, Random };
struct PegsPreset { int width, height; PegsShape shape; };
constexpr std::array<PegsPreset, 9> pegPresets{{
    {5, 7, PegsShape::Cross}, {7, 7, PegsShape::Cross},
    {5, 9, PegsShape::Cross}, {7, 9, PegsShape::Cross},
    {9, 9, PegsShape::Cross}, {7, 7, PegsShape::Octagon},
    {5, 5, PegsShape::Random}, {7, 7, PegsShape::Random},
    {9, 9, PegsShape::Random}
}};

std::vector<int> standardPegs (const PegsPreset &preset) {
    const int width = preset.width, height = preset.height;
    std::vector<int> board(width * height, -1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int dx = std::abs(x - width / 2), dy = std::abs(y - height / 2);
            if (preset.shape == PegsShape::Cross) {
                if (dx <= 1 || dy <= 1) board[y * width + x] = 1;
            } else if (dx + dy <= 1 + std::max(width, height) / 2) {
                board[y * width + x] = 1;
            }
        }
    }
    if (preset.shape == PegsShape::Cross) {
        board[(height / 2) * width + width / 2] = 0;
    } else {
        const int choice = QRandomGenerator::global()->bounded(3);
        int dx = 0, dy = 0;
        if (choice == 0) {
            dx = 2 * QRandomGenerator::global()->bounded(2) - 1;
            dy = 2 * QRandomGenerator::global()->bounded(2) - 1;
            if (QRandomGenerator::global()->bounded(2)) dy *= 3;
            else dx *= 3;
        } else {
            const int distance = choice == 1 ? 2 : 1;
            const int sign = 2 * QRandomGenerator::global()->bounded(2) - 1;
            if (QRandomGenerator::global()->bounded(2)) dx = distance * sign;
            else dy = distance * sign;
        }
        board[(height / 2 + dy) * width + width / 2 + dx] = 0;
    }
    return board;
}

struct PegsPuzzle {
    std::vector<int> board;
};

PegsPuzzle generateRandomPegs (int width, int height) {
    while (true) {
        PegsPuzzle puzzle;
        puzzle.board.assign(width * height, -1);
        puzzle.board[(height / 2) * width + width / 2] = 1;
        int moves = 0;
        while (true) {
            struct ReverseMove { int start, middle, end, cost; };
            std::array<std::vector<ReverseMove>, 3> choices;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    const int start = y * width + x;
                    if (puzzle.board[start] != 1) continue;
                    for (const auto [dx, dy] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
                        const int mx = x + dx, my = y + dy, ex = x + 2 * dx, ey = y + 2 * dy;
                        if (mx < 0 || mx >= width || ex < 0 || ex >= width || my < 0 || my >= height || ey < 0 || ey >= height)
                            continue;
                        const int middle = my * width + mx, end = ey * width + ex;
                        if (puzzle.board[middle] == 1 || puzzle.board[end] == 1) continue;
                        const int cost = (puzzle.board[middle] < 0) + (puzzle.board[end] < 0);
                        choices[cost].push_back({start, middle, end, cost});
                    }
                }
            }
            const int maxCost = moves < width * height / 2 ? 2 : 1;
            int cost = 0;
            while (cost <= maxCost && choices[cost].empty()) ++cost;
            if (cost > maxCost) break;
            const auto &move = choices[cost][QRandomGenerator::global()->bounded(static_cast<int>(choices[cost].size()))];
            puzzle.board[move.start] = 0;
            puzzle.board[move.middle] = puzzle.board[move.end] = 1;
            ++moves;
        }
        int extremes = 0;
        for (int y = 0; y < height; ++y) {
            if (puzzle.board[y * width] >= 0) extremes |= 1;
            if (puzzle.board[y * width + width - 1] >= 0) extremes |= 2;
        }
        for (int x = 0; x < width; ++x) {
            if (puzzle.board[x] >= 0) extremes |= 4;
            if (puzzle.board[(height - 1) * width + x] >= 0) extremes |= 8;
        }
        if (extremes == 15) return puzzle;
    }
}
}

class PegsView : public GridGameView {
    public:
        explicit PegsView (QWidget *parent) : GridGameView(parent) {
            setObjectName(QStringLiteral("pegsView"));
            setAccessibleName(tr("孔明棋棋盘"));
        }
        QRectF boardRect () const override {
            if (gridWidth <= 0 || gridHeight <= 0) return {};
            constexpr qreal margin = 4;
            const qreal unit = std::max(0.0, std::min((width() - 2 * margin) / gridWidth,
                                                      (height() - 2 * margin) / gridHeight));
            const QSizeF size(unit * gridWidth, unit * gridHeight);
            return QRectF(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        }
        QRectF cellRect (int cell) const {
            const auto rect = boardRect();
            const qreal unit = rect.width() / gridWidth;
            return {rect.left() + cell % gridWidth * unit, rect.top() + cell / gridWidth * unit, unit, unit};
        }
        int cellAt (QPointF point) const {
            const auto rect = boardRect();
            if (gridWidth <= 0 || gridHeight <= 0 || rect.width() <= 0 || !rect.contains(point)) return -1;
            const int x = static_cast<int>((point.x() - rect.left()) * gridWidth / rect.width());
            const int y = static_cast<int>((point.y() - rect.top()) * gridHeight / rect.height());
            return x < gridWidth && y < gridHeight ? y * gridWidth + x : -1;
        }
        void setBoard (const std::vector<int> &values, int width, int height, int focus, int picked) {
            board = values;
            gridWidth = width;
            gridHeight = height;
            cursor = focus;
            selected = picked;
            update();
        }
    protected:
        void paintEvent (QPaintEvent *) override {
            if (gridWidth <= 0 || gridHeight <= 0 || boardRect().width() <= 0) return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor border = palette().color(QPalette::Text), blue("#2474b5");
            for (int cell = 0; cell < gridWidth * gridHeight; ++cell) {
                if (board[cell] < 0) continue;
                const auto rect = cellRect(cell);
                painter.setPen(Qt::NoPen);
                painter.setBrush(palette().color(QPalette::AlternateBase));
                painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4);
                const qreal inset = rect.width() * (board[cell] == 1 ? 0.18 : 0.33);
                painter.setPen(QPen(border, 1));
                painter.setBrush(board[cell] == 1 ? blue : palette().color(QPalette::Base));
                painter.drawEllipse(rect.adjusted(inset, inset, -inset, -inset));
                if (cell == selected) {
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(QColor("#e6b83d"), 3));
                    painter.drawEllipse(rect.adjusted(3, 3, -3, -3));
                }
                if (selected >= 0 && jumpedPeg(board, gridWidth, gridHeight, selected, cell) >= 0) {
                    painter.setBrush(QColor("#33805a"));
                    painter.setPen(Qt::NoPen);
                    painter.drawEllipse(rect.center(), rect.width() * 0.08, rect.height() * 0.08);
                }
            }
            if (hasFocus() && cursor >= 0) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
                painter.drawRect(cellRect(cursor).adjusted(3, 3, -3, -3));
            }
        }
    private:
        std::vector<int> board;
        int gridWidth{}, gridHeight{};
        int cursor{}, selected{-1};
};

PegsGame::PegsGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    expandComboBox(ui->pegsPreset);
    view = new PegsView(ui->pegsBoard);
    ui->pegsBoardLayout->addWidget(view);
    view->installEventFilter(this);
    connect(ui->pegsPreset, &QComboBox::currentIndexChanged, this, &PegsGame::reset);
    connect(ui->pegsNewButton, &QToolButton::clicked, this, &PegsGame::reset);
    connect(ui->pegsRetryButton, &QToolButton::clicked, this, [this] {
        board = initial;
        history.clear();
        selected = dragSource = -1;
        cursor = (height / 2) * width + width / 2;
        refresh();
    });
    connect(ui->pegsUndoButton, &QToolButton::clicked, this, [this] {
        if (history.empty()) return;
        board = std::move(history.back());
        history.pop_back();
        selected = dragSource = -1;
        refresh();
    });
    reset();
}

void PegsGame::reset () {
    const auto &preset = pegPresets[std::clamp(ui->pegsPreset->currentIndex(), 0,
                                               static_cast<int>(pegPresets.size()) - 1)];
    width = preset.width;
    height = preset.height;
    board = preset.shape == PegsShape::Random ? generateRandomPegs(width, height).board
                                               : standardPegs(preset);
    initial = board;
    history.clear();
    cursor = (height / 2) * width + width / 2;
    selected = dragSource = -1;
    refresh();
}

bool PegsGame::jump (int from, int to) {
    const int middle = jumpedPeg(board, width, height, from, to);
    if (middle < 0) {
        ui->pegsFeedback->setText(tr("需要跳过相邻棋子，落到两个格子外的空孔。"));
        return false;
    }
    history.push_back(board);
    board[from] = board[middle] = 0;
    board[to] = 1;
    cursor = to;
    selected = -1;
    refresh();
    return true;
}

void PegsGame::click (int cell) {
    if (cell < 0 || board[cell] < 0) return;
    cursor = cell;
    if (board[cell] == 1) {
        selected = cell == selected ? -1 : cell;
        refresh();
    } else if (selected >= 0) jump(selected, cell);
    else refresh();
}

void PegsGame::refresh () {
    const int remaining = std::ranges::count(board, 1);
    const auto moves = pegMoves(board, width, height);
    view->setBoard(board, width, height, cursor, selected);
    ui->pegsStatus->setText(tr("剩余 %1 颗 · %2 步 · %3 种可走跳法").arg(remaining).arg(history.size()).arg(moves.size()));
    ui->pegsFeedback->setText(remaining == 1 ? tr("只剩一颗，成功！") : moves.empty() ? tr("没有可走的跳法了，可以撤销或重试本局。") : tr("选中棋子后，绿色圆点标出可落下的空孔。"));
    ui->pegsUndoButton->setEnabled(!history.empty());
    QStringList cells;
    for (int value : board) cells << QString::number(value);
    view->setAccessibleDescription(tr("棋盘（-1 为障碍，0 为空孔，1 为棋子）：%1；光标 %2；选中 %3").arg(cells.join(',')).arg(cursor + 1).arg(selected + 1));
}

bool PegsGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        view->setFocus(Qt::MouseFocusReason);
        const int cell = view->cellAt(mouse->position());
        dragSource = cell >= 0 && board[cell] == 1 ? cell : -1;
        dragStart = mouse->position();
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        const int cell = view->cellAt(mouse->position());
        if (dragSource >= 0 && (mouse->position() - dragStart).manhattanLength() > 4) jump(dragSource, cell);
        else click(cell);
        dragSource = -1;
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent*>(event)->key();
        int direction = -1;
        switch (key) {
            case Qt::Key_Up: direction = 0; break;
            case Qt::Key_Right: direction = 1; break;
            case Qt::Key_Down: direction = 2; break;
            case Qt::Key_Left: direction = 3; break;
            case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space: click(cursor); return true;
            case Qt::Key_Escape: selected = -1; refresh(); return true;
        }
        if (direction >= 0) {
            if (selected >= 0)
                jump(selected, pegNeighbor(pegNeighbor(selected, direction, width, height), direction, width, height));
            else {
                const int next = pegNeighbor(cursor, direction, width, height);
                if (next >= 0) cursor = next;
                refresh();
            }
            return true;
        }
    }
    return Game::eventFilter(watched, event);
}
