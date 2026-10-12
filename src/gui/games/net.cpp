#include "net.hpp"
#include "gridGameView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <array>
#include <bit>
#include <numeric>
#include <ranges>

namespace {
// Rules reference: Simon Tatham's Portable Puzzle Collection (MIT).
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/net.html
// The ambiguity solver and boundary perturbation follow net.c, snapshot
// 20260923.616da16. See SIMON-TATHAM-LICENSE.txt.
// Four port bits run clockwise from north: 1, 2, 4, 8.
int rotatePorts (int ports, int turns) {
    turns = (turns % 4 + 4) % 4;
    return ((ports << turns) | (ports >> (4 - turns))) & 15;
}

struct NetPreset { int width, height; bool wrapping; };
constexpr std::array<NetPreset, 10> netPresets{{
    {5, 5, false}, {7, 7, false}, {9, 9, false}, {11, 11, false}, {13, 11, false},
    {5, 5, true}, {7, 7, true}, {9, 9, true}, {11, 11, true}, {13, 11, true}
}};

int netNeighbor (int cell, int direction, int width, int height, bool wrapping) {
    const int x = cell % width, y = cell / width;
    switch (direction) {
        case 0: return y > 0 ? cell - width : wrapping ? cell + width * (height - 1) : -1;
        case 1: return x + 1 < width ? cell + 1 : wrapping ? cell - x : -1;
        case 2: return y + 1 < height ? cell + width : wrapping ? cell % width : -1;
        case 3: return x > 0 ? cell - 1 : wrapping ? cell + width - 1 : -1;
        default: return -1;
    }
}

struct NetAnalysis {
    std::vector<bool> lit;
    int connected{}, dangling{}, loops{};
    bool complete () const { return connected == static_cast<int>(lit.size()) && dangling == 0 && loops == 0; }
};

NetAnalysis analyzeNet (const std::vector<int> &ports, int width, int height, bool wrapping) {
    NetAnalysis result;
    result.lit.assign(ports.size(), false);
    std::vector<int> parent(ports.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](int cell) {
        while (parent[cell] != cell) cell = parent[cell];
        return cell;
    };
    for (int cell = 0; cell < static_cast<int>(ports.size()); ++cell) {
        for (int d = 0; d < 4; ++d) {
            if (!(ports[cell] & (1 << d))) continue;
            const int next = netNeighbor(cell, d, width, height, wrapping);
            if (next < 0 || !(ports[next] & (1 << ((d + 2) % 4)))) ++result.dangling;
            else if (next > cell) {
                const int a = root(cell), b = root(next);
                if (a == b) ++result.loops;
                else parent[a] = b;
            }
        }
    }
    std::vector<int> queue{(height / 2) * width + width / 2};
    result.lit[queue.front()] = true;
    for (size_t i = 0; i < queue.size(); ++i) {
        const int cell = queue[i];
        for (int d = 0; d < 4; ++d) {
            const int next = netNeighbor(cell, d, width, height, wrapping);
            if (next >= 0 && !result.lit[next] && (ports[cell] & (1 << d)) && (ports[next] & (1 << ((d + 2) % 4)))) {
                result.lit[next] = true;
                queue.push_back(next);
            }
        }
    }
    result.connected = queue.size();
    return result;
}

// A domain is a bitset of possible port masks (not a bitset of directions).
constexpr std::array<int, 4> withPort{0xaaaa, 0xcccc, 0xf0f0, 0xff00};
bool propagateNet (std::vector<int> &domains, int width, int height, bool wrapping) {
    bool changed;
    do {
        changed = false;
        for (int cell = 0; cell < static_cast<int>(domains.size()); ++cell) {
            const int before = domains[cell];
            for (int d = 0; d < 4; ++d) {
                const int next = netNeighbor(cell, d, width, height, wrapping), opposite = (d + 2) % 4;
                if (next < 0 || !(domains[next] & withPort[opposite])) domains[cell] &= ~withPort[d];
                else if (!(domains[next] & ~withPort[opposite])) domains[cell] &= withPort[d];
            }
            if (!domains[cell]) return false;
            changed |= before != domains[cell];
        }
    } while (changed);
    // Any loop made of already-forced connections cannot be repaired later.
    std::vector<int> parent(domains.size());
    std::iota(parent.begin(), parent.end(), 0);
    const auto root = [&](int cell) {
        while (parent[cell] != cell) cell = parent[cell];
        return cell;
    };
    for (int cell = 0; cell < static_cast<int>(domains.size()); ++cell) {
        for (int d : {1, 2}) {
            const int next = netNeighbor(cell, d, width, height, wrapping);
            if (next < 0 || (domains[cell] & ~withPort[d]) || (domains[next] & ~withPort[(d + 2) % 4])) continue;
            const int a = root(cell), b = root(next);
            if (a == b) return false;
            parent[a] = b;
        }
    }
    return true;
}

int countNetSolutions (const std::vector<int> &ports, int width, int height, bool wrapping,
                       int forcedCell = -1, int forcedPorts = -1) {
    std::vector<int> domains(ports.size());
    for (size_t cell = 0; cell < ports.size(); ++cell) {
        for (int d = 0; d < 4; ++d) domains[cell] |= 1 << rotatePorts(ports[cell], d);
        if (static_cast<int>(cell) == forcedCell)
            domains[cell] &= 1 << forcedPorts;
    }
    int count = 0, budget = 25000;
    const auto search = [&](auto &&self, std::vector<int> current) -> void {
        if (count >= 2) return;
        if (--budget < 0) { count = 2; return; } // Treat an unproven orientation as possible.
        if (!propagateNet(current, width, height, wrapping)) return;
        int chosen = -1, smallest = 5;
        for (int cell = 0; cell < static_cast<int>(current.size()); ++cell) {
            const int size = std::popcount(static_cast<unsigned>(current[cell]));
            if (size > 1 && size < smallest) { smallest = size; chosen = cell; }
        }
        if (chosen < 0) {
            std::vector<int> candidate(current.size());
            for (size_t i = 0; i < current.size(); ++i) candidate[i] = std::countr_zero(static_cast<unsigned>(current[i]));
            if (analyzeNet(candidate, width, height, wrapping).complete()) ++count;
            return;
        }
        for (int mask = 1; mask < 16 && count < 2; ++mask) {
            if (!(current[chosen] & (1 << mask))) continue;
            auto branch = current;
            branch[chosen] = 1 << mask;
            self(self, std::move(branch));
        }
    };
    search(search, std::move(domains));
    return count;
}

bool determineNetLocks (const std::vector<int> &ports, int width, int height, bool wrapping,
                        std::vector<bool> &locked) {
    std::vector<int> domains(ports.size());
    for (size_t cell = 0; cell < ports.size(); ++cell)
        for (int turn = 0; turn < 4; ++turn)
            domains[cell] |= 1 << rotatePorts(ports[cell], turn);
    if (!propagateNet(domains, width, height, wrapping)) return false;

    locked.assign(ports.size(), false);
    for (int cell = 0; cell < static_cast<int>(ports.size()); ++cell) {
        if (!(domains[cell] & (1 << ports[cell]))) return false;
        if (std::popcount(static_cast<unsigned>(domains[cell])) == 1) {
            locked[cell] = true;
            continue;
        }

        bool hasAlternative = false;
        for (int candidate = 0; candidate < 16; ++candidate) {
            if (candidate == ports[cell] || !(domains[cell] & (1 << candidate))) continue;
            // A tile is fixed only after the solver proves that every other
            // orientation has no complete, connected, loop-free solution.
            if (countNetSolutions(ports, width, height, wrapping, cell, candidate) != 0) {
                hasAlternative = true;
                break;
            }
        }
        locked[cell] = !hasAlternative;
    }
    return true;
}

struct NetBoundaryEdge { int x{}, y{}, direction{}; };

bool netCrossesBoundary (int cell, int direction, int width, int height) {
    return netNeighbor(cell, direction, width, height, false) < 0;
}

void severNetEdge (std::vector<int> &ports, int x, int y, int direction,
                   int width, int height) {
    const int cell = y * width + x;
    const int next = netNeighbor(cell, direction, width, height, true);
    ports[cell] &= ~(1 << direction);
    ports[next] &= ~(1 << ((direction + 2) % 4));
}

// Perturb one ambiguous region using the upstream boundary-join method:
// add a legal edge on its perimeter, trace the resulting cycle, cut a random
// different edge on that cycle, then mark the region as resolved for this pass.
void perturbNet (std::vector<int> &ports, std::vector<bool> &locked,
                 int width, int height, bool wrapping,
                 int startX, int startY, int startDirection) {
    const int area = width * height;
    const auto cellAt = [width](int x, int y) { return y * width + x; };
    const auto neighborXY = [width, height](int x, int y, int direction) {
        const int cell = y * width + x;
        const int next = netNeighbor(cell, direction, width, height, true);
        return std::pair{next % width, next / width};
    };
    std::vector<NetBoundaryEdge> perimeter;
    int x = startX, y = startY, direction = startDirection;
    const auto startingEdge = NetBoundaryEdge{x, y, direction};
    for (int steps = 0; steps < area * 16 + 64; ++steps) {
        perimeter.push_back({x, y, direction});
        const int turnLeft = (direction + 3) % 4;
        const auto [leftX, leftY] = neighborXY(x, y, turnLeft);
        if ((!wrapping && netCrossesBoundary(cellAt(x, y), turnLeft, width, height)) ||
            locked[cellAt(leftX, leftY)]) {
            direction = turnLeft;
        } else {
            x = leftX;
            y = leftY;
            const auto [forwardX, forwardY] = neighborXY(x, y, direction);
            if ((wrapping || !netCrossesBoundary(cellAt(x, y), direction, width, height)) &&
                !locked[cellAt(forwardX, forwardY)]) {
                x = forwardX;
                y = forwardY;
                direction = (direction + 1) % 4;
            }
        }
        if (x == startingEdge.x && y == startingEdge.y && direction == startingEdge.direction)
            break;
    }
    if (perimeter.empty() || perimeter.size() >= static_cast<size_t>(area * 16 + 64) ||
        x != startingEdge.x || y != startingEdge.y || direction != startingEdge.direction)
        return;

    auto shuffled = perimeter;
    for (int i = static_cast<int>(shuffled.size()) - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(shuffled[i], shuffled[j]);
    }

    bool joined = false;
    int joinedX = -1, joinedY = -1, joinedDirection = -1;
    for (const auto &edge : shuffled) {
        const int cell = cellAt(edge.x, edge.y);
        const auto [nextX, nextY] = neighborXY(edge.x, edge.y, edge.direction);
        const int next = cellAt(nextX, nextY);
        if (!wrapping && netCrossesBoundary(cell, edge.direction, width, height)) continue;
        if (ports[cell] & (1 << edge.direction)) continue;
        if (((ports[cell] | (1 << edge.direction)) & 15) == 15) continue;
        const int opposite = (edge.direction + 2) % 4;
        if (((ports[next] | (1 << opposite)) & 15) == 15) continue;
        ports[cell] |= 1 << edge.direction;
        ports[next] |= 1 << opposite;
        x = edge.x;
        y = edge.y;
        direction = edge.direction;
        joinedX = x;
        joinedY = y;
        joinedDirection = direction;
        joined = true;
        break;
    }
    if (!joined) return;

    std::array<std::vector<NetBoundaryEdge>, 2> paths;
    std::array<NetBoundaryEdge, 2> positions{{
        {x, y, direction}, {x, y, direction}
    }};
    bool cut = false;
    for (int step = 0; step < area * 16 + 64 && !cut; ++step) {
        for (int hand = 0; hand < 2 && !cut; ++hand) {
            x = positions[hand].x;
            y = positions[hand].y;
            direction = positions[hand].direction;
            const auto [nextX, nextY] = neighborXY(x, y, direction);
            auto &path = paths[hand];
            if (!path.empty() && path.back().x == nextX && path.back().y == nextY &&
                path.back().direction == (direction + 2) % 4) {
                path.pop_back();
            } else {
                path.push_back({x, y, direction});
            }

            direction = (direction + 2) % 4;
            bool foundNext = false;
            for (int turn = 0; turn < 4; ++turn) {
                direction = (direction + (hand == 0 ? 3 : 1)) % 4;
                if (ports[cellAt(nextX, nextY)] & (1 << direction)) {
                    positions[hand] = {nextX, nextY, direction};
                    foundNext = true;
                    break;
                }
            }
            if (!foundNext || path.empty()) {
                severNetEdge(ports, joinedX, joinedY, joinedDirection, width, height);
                return;
            }

            if (positions[hand].x == path.front().x && positions[hand].y == path.front().y &&
                positions[hand].direction == path.front().direction) {
                if (path.size() < 2) {
                    severNetEdge(ports, joinedX, joinedY, joinedDirection, width, height);
                    return;
                }
                const int cutAt = 1 + QRandomGenerator::global()->bounded(static_cast<int>(path.size()) - 1);
                severNetEdge(ports, path[cutAt].x, path[cutAt].y, path[cutAt].direction,
                             width, height);
                cut = true;
            }
        }
    }
    if (!cut) {
        severNetEdge(ports, joinedX, joinedY, joinedDirection, width, height);
        return;
    }

    // Match cube.c's column sweep over sorted perimeter edges to mark every
    // tile inside the disputed area, preventing a second perturb in this pass.
    const auto sourceDirectionOrder = [](int d) {
        constexpr std::array<int, 4> order{{2, 1, 8, 4}}; // U, R, D, L source bit values
        return order[d];
    };
    std::sort(perimeter.begin(), perimeter.end(), [&](const auto &a, const auto &b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return sourceDirectionOrder(a.direction) < sourceDirectionOrder(b.direction);
    });
    int sweepX = -1, sweepY = -1;
    for (int i = 0; i <= static_cast<int>(perimeter.size()); ++i) {
        if (i == static_cast<int>(perimeter.size()) || perimeter[i].x > sweepX) {
            if (sweepX != -1) {
                while (sweepY < height) locked[cellAt(sweepX, sweepY++)] = true;
                sweepX = sweepY = -1;
            }
            if (i == static_cast<int>(perimeter.size())) break;
            sweepX = perimeter[i].x;
            sweepY = 0;
        }
        if (perimeter[i].direction == 0) {
            sweepX = perimeter[i].x;
            sweepY = perimeter[i].y;
        } else if (perimeter[i].direction == 2) {
            while (sweepY <= perimeter[i].y) locked[cellAt(sweepX, sweepY++)] = true;
            sweepX = sweepY = -1;
        }
    }
}

std::vector<int> makeNetTree (int width, int height, bool wrapping) {
    const int size = width * height;
    std::vector<int> ports(size);
    std::vector<bool> visited(size);
    struct Edge { int from, direction; };
    std::vector<Edge> frontier;
    const auto add = [&](int cell) {
        visited[cell] = true;
        for (int d = 0; d < 4; ++d) {
            const int next = netNeighbor(cell, d, width, height, wrapping);
            if (next >= 0 && !visited[next] && std::popcount(static_cast<unsigned>(ports[cell])) < 3)
                frontier.push_back({cell, d});
        }
    };
    add((height / 2) * width + width / 2);
    while (!frontier.empty()) {
        const int index = QRandomGenerator::global()->bounded(static_cast<int>(frontier.size()));
        const auto edge = frontier[index];
        frontier[index] = frontier.back();
        frontier.pop_back();
        const int next = netNeighbor(edge.from, edge.direction, width, height, wrapping);
        if (visited[next]) continue;
        if (std::popcount(static_cast<unsigned>(ports[edge.from])) >= 3) continue;
        ports[edge.from] |= 1 << edge.direction;
        ports[next] |= 1 << ((edge.direction + 2) % 4);
        add(next);
    }
    return ports;
}

std::vector<int> generateNet (int width, int height, bool wrapping) {
    for (;;) {
        auto ports = makeNetTree(width, height, wrapping);
        if (countNetSolutions(ports, width, height, wrapping) == 1) return ports;
        int previousBoundaryCount = -1;
        for (;;) {
            std::vector<bool> locked;
            if (!determineNetLocks(ports, width, height, wrapping, locked)) break;
            if (std::ranges::all_of(locked, [](bool value) { return value; })) return ports;

            int boundaryCount = 0;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
                const int cell = y * width + x;
                if (x + 1 < width && locked[cell] != locked[cell + 1]) {
                    ++boundaryCount;
                    if (locked[cell])
                        perturbNet(ports, locked, width, height, wrapping, x + 1, y, 3);
                    else
                        perturbNet(ports, locked, width, height, wrapping, x, y, 1);
                }
                if (y + 1 < height && locked[cell] != locked[cell + width]) {
                    ++boundaryCount;
                    if (locked[cell])
                        perturbNet(ports, locked, width, height, wrapping, x, y + 1, 0);
                    else
                        perturbNet(ports, locked, width, height, wrapping, x, y, 2);
                }
            }
            if (previousBoundaryCount != -1 && previousBoundaryCount <= boundaryCount) break;
            previousBoundaryCount = boundaryCount;
        }
    }
}
}

class NetView : public GridGameView {
    public:
        explicit NetView (QWidget *parent) : GridGameView(parent) {
            setObjectName(QStringLiteral("netView"));
            setAccessibleName(tr("管网棋盘"));
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
        void setState (const NetState &value, int width, int height, bool wrapping, int focus,
                       const NetAnalysis &result) {
            state = value;
            gridWidth = width;
            gridHeight = height;
            this->wrapping = wrapping;
            cursor = focus;
            analysis = result;
            update();
        }
    protected:
        void paintEvent (QPaintEvent *) override {
            if (gridWidth <= 0 || gridHeight <= 0 || boardRect().width() <= 0) return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor live("#33805a"), inactive = palette().color(QPalette::Text), bad("#c43d4d");
            for (int cell = 0; cell < gridWidth * gridHeight; ++cell) {
                const auto rect = cellRect(cell);
                const qreal unit = rect.width();
                const auto center = rect.center();
                painter.setPen(QPen(palette().color(QPalette::Mid), 1));
                painter.setBrush(state.locked[cell] ? palette().color(QPalette::AlternateBase) : palette().color(QPalette::Base));
                painter.drawRect(rect);
                const QColor color = analysis.lit[cell] ? live : inactive;
                painter.setPen(QPen(color, std::max(2.0, unit * 0.09), Qt::SolidLine, Qt::RoundCap));
                const std::array<QPointF, 4> ends{QPointF(center.x(), rect.top()), QPointF(rect.right(), center.y()),
                                               QPointF(center.x(), rect.bottom()), QPointF(rect.left(), center.y())};
                for (int d = 0; d < 4; ++d) {
                    if (!(state.ports[cell] & (1 << d))) continue;
                    painter.setPen(QPen(color, std::max(2.0, unit * 0.09), Qt::SolidLine, Qt::RoundCap));
                    painter.drawLine(center, ends[d]);
                    const int next = netNeighbor(cell, d, gridWidth, gridHeight, wrapping);
                    if (next < 0 || !(state.ports[next] & (1 << ((d + 2) % 4)))) {
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(bad);
                        const QPointF tip = center + (ends[d] - center) * 0.85;
                        painter.drawEllipse(tip, unit * 0.045, unit * 0.045);
                    }
                }
                painter.setPen(QPen(color, 1));
                const int source = (gridHeight / 2) * gridWidth + gridWidth / 2;
                painter.setBrush(cell == source ? QColor("#e6b83d") : color);
                const qreal radius = unit * (cell == source ? 0.15 : std::popcount(static_cast<unsigned>(state.ports[cell])) == 1 ? 0.12 : 0.055);
                painter.drawEllipse(center, radius, radius);
                if (state.locked[cell]) {
                    const QRectF lock(rect.right() - unit * 0.28, rect.top() + unit * 0.13, unit * 0.16, unit * 0.14);
                    painter.setPen(QPen(inactive, 1.2));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawArc(lock.translated(0, -unit * 0.08), 0, 180 * 16);
                    painter.setBrush(inactive);
                    painter.drawRoundedRect(lock, 1, 1);
                }
            }
            if (hasFocus() && cursor >= 0) {
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
                painter.drawRect(cellRect(cursor).adjusted(3, 3, -3, -3));
            }
        }
    private:
        NetState state;
        NetAnalysis analysis;
        int gridWidth{}, gridHeight{};
        bool wrapping{};
        int cursor{};
};

NetGame::NetGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    expandComboBox(ui->netPreset);
    view = new NetView(ui->netBoard);
    ui->netBoardLayout->addWidget(view);
    view->installEventFilter(this);
    connect(ui->netPreset, &QComboBox::currentIndexChanged, this, &NetGame::reset);
    connect(ui->netNewButton, &QToolButton::clicked, this, &NetGame::reset);
    connect(ui->netRetryButton, &QToolButton::clicked, this, [this] {
        state = initial;
        history.clear();
        cursor = (height / 2) * width + width / 2;
        refresh();
    });
    connect(ui->netUndoButton, &QToolButton::clicked, this, [this] {
        if (history.empty()) return;
        state = std::move(history.back());
        history.pop_back();
        refresh();
    });
    reset();
}

void NetGame::reset () {
    const auto &preset = netPresets[std::clamp(ui->netPreset->currentIndex(), 0,
                                               static_cast<int>(netPresets.size()) - 1)];
    width = preset.width;
    height = preset.height;
    wrapping = preset.wrapping;
    const auto solution = generateNet(width, height, wrapping);
    do {
        state.ports = solution;
        for (int &ports : state.ports) ports = rotatePorts(ports, QRandomGenerator::global()->bounded(4));
    } while (analyzeNet(state.ports, width, height, wrapping).loops != 0 ||
             analyzeNet(state.ports, width, height, wrapping).complete());
    state.locked.assign(width * height, false);
    state.moves = 0;
    initial = state;
    history.clear();
    cursor = (height / 2) * width + width / 2;
    refresh();
}

void NetGame::rotate (int cell, int turns) {
    if (cell < 0 || state.locked[cell] || analyzeNet(state.ports, width, height, wrapping).complete()) return;
    const int rotated = rotatePorts(state.ports[cell], turns);
    if (rotated == state.ports[cell]) return;
    history.push_back(state);
    state.ports[cell] = rotated;
    ++state.moves;
    refresh();
}

void NetGame::toggleLock (int cell) {
    if (cell < 0) return;
    history.push_back(state);
    state.locked[cell] = !state.locked[cell];
    refresh();
}

void NetGame::refresh () {
    const auto result = analyzeNet(state.ports, width, height, wrapping);
    view->setState(state, width, height, wrapping, cursor, result);
    ui->netStatus->setText(tr("已连通 %1 / %2 格 · %3 次旋转 · %4 格锁定").arg(result.connected).arg(width * height).arg(state.moves).arg(std::ranges::count(state.locked, true)));
    ui->netFeedback->setText(result.complete() ? tr("管网全部连通，没有环路！") : tr("未接合端口 %1 个 · 环路 %2 个；红点表示未接合端口。").arg(result.dangling).arg(result.loops));
    ui->netUndoButton->setEnabled(!history.empty());
    QStringList ports, locked;
    for (int value : state.ports) ports << QString::number(value);
    for (bool value : state.locked) locked << QString::number(value);
    view->setAccessibleDescription(tr("管线（北、东、南、西端口位值为 1、2、4、8）：%1；锁定 %2；光标 %3").arg(ports.join(','), locked.join(',')).arg(cursor + 1));
}

bool NetGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        const int cell = view->cellAt(mouse->position());
        if (cell < 0) return true;
        cursor = cell;
        view->setFocus(Qt::MouseFocusReason);
        if (mouse->button() == Qt::MiddleButton || (mouse->button() == Qt::LeftButton && mouse->modifiers().testFlag(Qt::ShiftModifier))) toggleLock(cell);
        else if (mouse->button() == Qt::LeftButton) rotate(cell, -1);
        else if (mouse->button() == Qt::RightButton) rotate(cell, 1);
        // Locked or symmetric tiles still update the cursor without a move.
        refresh();
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        int direction = -1;
        switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Up: direction = 0; break;
            case Qt::Key_Right: direction = 1; break;
            case Qt::Key_Down: direction = 2; break;
            case Qt::Key_Left: direction = 3; break;
            case Qt::Key_A: rotate(cursor, -1); return true;
            case Qt::Key_D: case Qt::Key_Return: case Qt::Key_Enter: case Qt::Key_Space: rotate(cursor, 1); return true;
            case Qt::Key_F: rotate(cursor, 2); return true;
            case Qt::Key_S: toggleLock(cursor); return true;
        }
        if (direction >= 0) {
            const int next = netNeighbor(cursor, direction, width, height, wrapping);
            if (next >= 0) cursor = next;
            refresh();
            return true;
        }
    }
    return Game::eventFilter(watched, event);
}
