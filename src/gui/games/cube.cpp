#include "cube.hpp"
#include "gridGameView.hpp"
#include "ui_note_widget.h"
#include "ui/theme.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QVariantAnimation>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <numeric>
#include <ranges>

namespace {
// Cube game rules, presets, grid enumeration, random placement and polyhedron
// rolling are adapted from Simon Tatham's cube.c, snapshot 20260923.616da16.
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/cube.html
// See SIMON-TATHAM-LICENSE.txt.
enum SolidType { Tetrahedron, Cube, Octahedron, Icosahedron };
enum Direction { Left, Right, Up, Down, UpLeft, UpRight, DownLeft, DownRight };

struct Vec3 { qreal x{}, y{}, z{}; };
struct Face { std::array<int, 4> vertices{}; };
struct Solid {
    int order{};
    std::vector<Vec3> vertices;
    std::vector<Face> faces;
    std::vector<Vec3> normals;
    qreal shear{}, border{};
};

Vec3 subtract (Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 cross (Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
qreal dot (Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 normalized (Vec3 v) {
    const qreal length = std::sqrt(dot(v, v));
    return length > 0 ? Vec3{v.x / length, v.y / length, v.z / length} : Vec3{};
}

Solid makeSolid (int order, std::initializer_list<Vec3> vertices,
                 std::initializer_list<std::array<int, 4>> faces,
                 qreal shear, qreal border) {
    Solid solid;
    solid.order = order;
    solid.vertices = vertices;
    solid.faces.reserve(faces.size());
    for (const auto &indices : faces) solid.faces.push_back({indices});
    solid.shear = shear;
    solid.border = border;
    for (const auto &face : solid.faces) {
        const auto a = solid.vertices[face.vertices[0]];
        const auto b = solid.vertices[face.vertices[1]];
        const auto c = solid.vertices[face.vertices[2]];
        solid.normals.push_back(normalized(cross(subtract(b, a), subtract(c, b))));
    }
    return solid;
}

const Solid &solidFor (int type) {
    // Vertex, face and shear data are copied from the official source.
    static const std::array<Solid, 4> solids{{
        makeSolid(3,
            {{0.0, -0.57735026919, -0.20412414523}, {-0.5, 0.28867513459, -0.20412414523},
             {0.0, 0.0, 0.6123724357}, {0.5, 0.28867513459, -0.20412414523}},
            {{{0,2,1,0}}, {{3,1,2,0}}, {{2,0,3,0}}, {{1,3,0,0}}}, 0.0, 0.3),
        makeSolid(4,
            {{-0.5,-0.5,-0.5}, {-0.5,-0.5,0.5}, {-0.5,0.5,-0.5}, {-0.5,0.5,0.5},
             {0.5,-0.5,-0.5}, {0.5,-0.5,0.5}, {0.5,0.5,-0.5}, {0.5,0.5,0.5}},
            {{{0,1,3,2}}, {{1,5,7,3}}, {{5,4,6,7}}, {{4,0,2,6}}, {{0,4,5,1}}, {{3,7,6,2}}},
            0.3, 0.5),
        makeSolid(3,
            {{-0.5,-0.28867513459472505,0.4082482904638664}, {0.5,0.28867513459472505,-0.4082482904638664},
             {-0.5,0.28867513459472505,-0.4082482904638664}, {0.5,-0.28867513459472505,0.4082482904638664},
             {0.0,-0.57735026918945009,-0.4082482904638664}, {0.0,0.57735026918945009,0.4082482904638664}},
            {{{4,0,2,0}}, {{0,5,2,0}}, {{0,4,3,0}}, {{5,0,3,0}},
             {{1,4,2,0}}, {{5,1,2,0}}, {{4,1,3,0}}, {{1,5,3,0}}}, 0.0, 0.5),
        makeSolid(3,
            {{0.0,0.57735026919,0.75576131408}, {0.0,-0.93417235896,0.17841104489},
             {0.0,0.93417235896,-0.17841104489}, {0.0,-0.57735026919,-0.75576131408},
             {-0.5,-0.28867513459,0.75576131408}, {-0.5,0.28867513459,-0.75576131408},
             {0.5,-0.28867513459,0.75576131408}, {0.5,0.28867513459,-0.75576131408},
             {-0.80901699437,0.46708617948,0.17841104489}, {0.80901699437,0.46708617948,0.17841104489},
             {-0.80901699437,-0.46708617948,-0.17841104489}, {0.80901699437,-0.46708617948,-0.17841104489}},
            {{{8,0,2,0}}, {{0,9,2,0}}, {{1,10,3,0}}, {{11,1,3,0}}, {{0,4,6,0}},
             {{4,1,6,0}}, {{5,2,7,0}}, {{3,5,7,0}}, {{4,8,10,0}}, {{8,5,10,0}},
             {{9,6,11,0}}, {{7,9,11,0}}, {{0,8,4,0}}, {{9,0,6,0}}, {{10,1,4,0}},
             {{1,11,6,0}}, {{8,2,5,0}}, {{2,9,7,0}}, {{3,10,5,0}}, {{11,3,7,0}}},
            0.0, 0.8)
    }};
    return solids.at(static_cast<size_t>(type));
}

struct GridSquare {
    qreal x{}, y{};
    std::vector<QPointF> points;
    std::array<int, 8> directions{};
    bool flip{};
    int tetraClass{};
};

struct Preset { int solid, d1, d2; };
constexpr std::array<Preset, 4> presets{{
    {Cube, 4, 4}, {Tetrahedron, 1, 2}, {Octahedron, 2, 2}, {Icosahedron, 3, 3}
}};

int gridArea (int solid, int d1, int d2) {
    return solid == Cube ? d1 * d2 : d1 * d1 + d2 * d2 + 4 * d1 * d2;
}

std::vector<GridSquare> enumerateGrid (int solid, int d1, int d2) {
    std::vector<GridSquare> grid;
    grid.reserve(static_cast<size_t>(gridArea(solid, d1, d2)));
    if (solid == Cube) {
        for (int y = 0; y < d2; ++y) for (int x = 0; x < d1; ++x) {
            GridSquare square;
            square.x = x;
            square.y = y;
            square.points = {{x - 0.5, y - 0.5}, {x - 0.5, y + 0.5},
                             {x + 0.5, y + 0.5}, {x + 0.5, y - 0.5}};
            square.directions = {{0x03, 0x0c, 0x09, 0x06, 0, 0, 0, 0}};
            grid.push_back(square);
        }
        return grid;
    }

    const qreal triangleHeight = std::sqrt(3.0) / 2.0;
    int firstIndex = -1;
    for (int row = 0; row < d1 + d2; ++row) {
        const int other = row < d2 ? 1 : -1;
        const int rowLength = row < d2 ? row + d1 : 2 * d2 + d1 - row;
        const qreal y = triangleHeight * row;

        // Down-pointing triangles, followed by up-pointing triangles, as in cube.c.
        for (int i = 0; i < rowLength; ++i) {
            int ix = 2 * i - (rowLength - 1);
            const qreal x = ix * 0.5;
            GridSquare square;
            square.x = x;
            square.y = y + triangleHeight / 3.0;
            square.points = {{x - 0.5, y}, {x, y + triangleHeight}, {x + 0.5, y}};
            square.directions[Left] = 0x03;
            square.directions[Right] = 0x06;
            square.directions[Up] = 0x05;
            square.directions[UpLeft] = square.directions[UpRight] = square.directions[Up];
            square.directions[DownLeft] = square.directions[Left];
            square.directions[DownRight] = square.directions[Right];
            square.flip = true;
            if (firstIndex < 0) firstIndex = ix & 3;
            ix -= firstIndex;
            square.tetraClass = ((row + (ix & 1)) & 2) ^ (ix & 3);
            grid.push_back(square);
        }
        for (int i = 0; i < rowLength + other; ++i) {
            int ix = 2 * i - (rowLength + other - 1);
            const qreal x = ix * 0.5;
            GridSquare square;
            square.x = x;
            square.y = y + 2.0 * triangleHeight / 3.0;
            square.points = {{x + 0.5, y + triangleHeight}, {x, y}, {x - 0.5, y + triangleHeight}};
            square.directions[Left] = 0x06;
            square.directions[Right] = 0x03;
            square.directions[Down] = 0x05;
            square.directions[DownLeft] = square.directions[DownRight] = square.directions[Down];
            square.directions[UpLeft] = square.directions[Left];
            square.directions[UpRight] = square.directions[Right];
            if (firstIndex < 0) firstIndex = (ix - 1) & 3;
            ix -= firstIndex;
            square.tetraClass = ((row + (ix & 1)) & 2) ^ (ix & 3);
            grid.push_back(square);
        }
    }
    return grid;
}

CubeState generateGame (Preset preset) {
    CubeState state;
    state.solid = preset.solid;
    state.d1 = preset.d1;
    state.d2 = preset.d2;
    const auto grid = enumerateGrid(state.solid, state.d1, state.d2);
    const auto &solid = solidFor(state.solid);
    state.board.assign(grid.size(), 0);
    state.faces.assign(solid.faces.size(), 0);

    const int classCount = state.solid == Tetrahedron ? 4 : state.solid == Octahedron ? 2 : 1;
    const int facesPerClass = static_cast<int>(solid.faces.size()) / classCount;
    std::vector<std::vector<int>> classSquares(static_cast<size_t>(classCount));
    for (int i = 0; i < static_cast<int>(grid.size()); ++i) {
        const int group = classCount == 4 ? grid[i].tetraClass : classCount == 2 ? int(grid[i].flip) : 0;
        classSquares[group].push_back(i);
    }
    for (auto &squares : classSquares) {
        for (int n = 0; n < facesPerClass; ++n) {
            const int selected = QRandomGenerator::global()->bounded(static_cast<int>(squares.size()));
            state.board[squares[selected]] = 1;
            squares.erase(squares.begin() + selected);
        }
    }
    std::vector<int> emptySquares;
    for (int i = 0; i < static_cast<int>(state.board.size()); ++i)
        if (!state.board[i]) emptySquares.push_back(i);
    state.position = emptySquares[QRandomGenerator::global()->bounded(static_cast<int>(emptySquares.size()))];
    return state;
}

int edgeForDirection (const GridSquare &square, int direction) {
    if (direction < 0 || direction >= static_cast<int>(square.directions.size())) return 0;
    return square.directions[direction];
}

int resolvedDirection (const GridSquare &square, int requested) {
    const int mask = edgeForDirection(square, requested);
    if (!mask) return -1;
    if (requested > Down) {
        for (int direction = Left; direction <= Down; ++direction)
            if (square.directions[direction] == mask) return direction;
        return -1;
    }
    return requested;
}

struct RollInfo {
    int destination{-1};
    std::array<int, 2> sourceCorners{};
    std::array<int, 2> destinationCorners{};
    std::array<int, 2> polyKeys{};
    int direction{-1};
    qreal angle{};
};

bool findDestination (const std::vector<GridSquare> &grid, int current, int direction,
                      RollInfo &roll) {
    const auto &square = grid[current];
    const int mask = square.directions[direction];
    if (!mask) return false;
    int key = 0;
    for (int i = 0; i < static_cast<int>(square.points.size()); ++i)
        if (mask & (1 << i)) {
            if (key >= 2) return false;
            roll.sourceCorners[key++] = i;
        }
    if (key != 2) return false;

    for (int candidate = 0; candidate < static_cast<int>(grid.size()); ++candidate) {
        if (candidate == current) continue;
        std::array<int, 2> found{{-1, -1}};
        int matches = 0;
        for (int j = 0; j < static_cast<int>(grid[candidate].points.size()); ++j) {
            for (int k = 0; k < 2; ++k) {
                if (QLineF(grid[candidate].points[j], square.points[roll.sourceCorners[k]]).length() < 0.01) {
                    if (matches < 2) found[matches] = j;
                    ++matches;
                }
            }
        }
        if (matches == 2) {
            roll.destination = candidate;
            roll.destinationCorners = found;
            roll.direction = direction;
            return true;
        }
    }
    return false;
}

qreal lowestZ (const Solid &solid) {
    qreal z = 0;
    for (const auto &vertex : solid.vertices) z = std::min(z, vertex.z);
    return z;
}

bool alignPoly (const Solid &solid, const GridSquare &square, std::array<int, 4> &keys) {
    const qreal zmin = lowestZ(solid);
    const qreal flip = square.flip ? -1.0 : 1.0;
    for (int point = 0; point < static_cast<int>(square.points.size()); ++point) {
        int matches = 0, match = -1;
        for (int vertex = 0; vertex < static_cast<int>(solid.vertices.size()); ++vertex) {
            const auto &v = solid.vertices[vertex];
            const qreal dx = v.x * flip - square.points[point].x() + square.x;
            const qreal dy = v.y * flip - square.points[point].y() + square.y;
            const qreal dz = v.z - zmin;
            if (dx * dx + dy * dy + dz * dz < 0.1) { ++matches; match = vertex; }
        }
        if (matches != 1 || match < 0) return false;
        keys[point] = match;
    }
    return true;
}

void flipPoly (Solid &solid, bool flip) {
    if (!flip) return;
    for (auto &v : solid.vertices) { v.x = -v.x; v.y = -v.y; }
    for (auto &n : solid.normals) { n.x = -n.x; n.y = -n.y; }
}

Vec3 matrixMultiply (const std::array<qreal, 9> &matrix, Vec3 v) {
    return {matrix[0] * v.x + matrix[3] * v.y + matrix[6] * v.z,
            matrix[1] * v.x + matrix[4] * v.y + matrix[7] * v.z,
            matrix[2] * v.x + matrix[5] * v.y + matrix[8] * v.z};
}

Solid transformPoly (const Solid &source, bool flip, int key0, int key1, qreal angle) {
    Solid result = source;
    flipPoly(result, flip);
    const qreal vx = result.vertices[key1].x - result.vertices[key0].x;
    const qreal vy = result.vertices[key1].y - result.vertices[key0].y;
    const std::array<qreal, 9> align{{vx, -vy, 0, vy, vx, 0, 0, 0, 1}};
    const qreal cosine = std::cos(angle), sine = std::sin(angle);
    const std::array<qreal, 9> rotate{{1, 0, 0, 0, cosine, -sine, 0, sine, cosine}};
    const std::array<qreal, 9> unalign{{vx, vy, 0, -vy, vx, 0, 0, 0, 1}};
    for (auto &v : result.vertices)
        v = matrixMultiply(unalign, matrixMultiply(rotate, matrixMultiply(align, v)));
    for (auto &n : result.normals)
        n = matrixMultiply(unalign, matrixMultiply(rotate, matrixMultiply(align, n)));
    return result;
}

int lowestFace (const Solid &solid) {
    int best = 0;
    qreal lowest = 0;
    for (int i = 0; i < static_cast<int>(solid.faces.size()); ++i) {
        qreal z = 0;
        for (int j = 0; j < solid.order; ++j)
            z += solid.vertices[solid.faces[i].vertices[j]].z;
        if (i == 0 || z < lowest) { lowest = z; best = i; }
    }
    return best;
}

bool isComplete (const CubeState &state) {
    return std::ranges::all_of(state.faces, [](int color) { return color != 0; });
}

bool roll (CubeState &state, const std::vector<GridSquare> &grid, int requested,
           RollInfo *rollOutput = nullptr) {
    if (state.position < 0 || state.position >= static_cast<int>(grid.size())) return false;
    RollInfo info;
    info.direction = resolvedDirection(grid[state.position], requested);
    if (info.direction < 0 || !findDestination(grid, state.position, info.direction, info)) return false;

    const auto &solid = solidFor(state.solid);
    std::array<int, 4> allKeys{};
    if (!alignPoly(solid, grid[state.position], allKeys)) return false;
    info.polyKeys = {{allKeys[info.sourceCorners[0]], allKeys[info.sourceCorners[1]]}};

    int adjacentFaces[2] = {-1, -1}, faceCount = 0;
    for (int face = 0; face < static_cast<int>(solid.faces.size()); ++face) {
        bool first = false, second = false;
        for (int j = 0; j < solid.order; ++j) {
            const int vertex = solid.faces[face].vertices[j];
            first |= vertex == info.polyKeys[0];
            second |= vertex == info.polyKeys[1];
        }
        if (first && second && faceCount < 2) adjacentFaces[faceCount++] = face;
    }
    if (faceCount != 2) return false;
    info.angle = std::acos(std::clamp(dot(solid.normals[adjacentFaces[0]],
                                          solid.normals[adjacentFaces[1]]), -1.0, 1.0));
    if (solid.order == 4 && info.direction == Up) info.angle = -info.angle;

    Solid transformed = transformPoly(solid, grid[state.position].flip,
                                      info.polyKeys[0], info.polyKeys[1], info.angle);
    flipPoly(transformed, grid[info.destination].flip);
    std::array<int, 4> destinationKeys{};
    if (!alignPoly(transformed, grid[info.destination], destinationKeys)) {
        info.angle = -info.angle;
        transformed = transformPoly(solid, grid[state.position].flip,
                                    info.polyKeys[0], info.polyKeys[1], info.angle);
        flipPoly(transformed, grid[info.destination].flip);
        if (!alignPoly(transformed, grid[info.destination], destinationKeys)) return false;
    }

    std::vector<int> newColors(state.faces.size(), -1);
    for (int face = 0; face < static_cast<int>(solid.faces.size()); ++face) {
        qreal bestDistance = std::numeric_limits<qreal>::max();
        int sourceFace = -1;
        for (int candidate = 0; candidate < static_cast<int>(transformed.normals.size()); ++candidate) {
            const auto delta = subtract(transformed.normals[candidate], solid.normals[face]);
            const qreal distance = dot(delta, delta);
            if (distance < bestDistance) { bestDistance = distance; sourceFace = candidate; }
        }
        if (sourceFace < 0 || bestDistance > 0.1) return false;
        newColors[face] = state.faces[sourceFace];
    }

    const bool wasComplete = isComplete(state);
    state.faces = std::move(newColors);
    state.position = info.destination;
    ++state.moves;
    if (!wasComplete) {
        const int bottom = lowestFace(solid);
        std::swap(state.faces[bottom], state.board[state.position]);
        if (isComplete(state)) state.completedMoves = state.moves;
    }
    if (rollOutput) *rollOutput = info;
    return true;
}

bool canRoll (const CubeState &state, const std::vector<GridSquare> &grid, int direction) {
    CubeState copy = state;
    return roll(copy, grid, direction);
}

bool faceVisible (const QPolygonF &polygon) {
    if (polygon.size() < 3) return false;
    const auto a = polygon[1] - polygon[0];
    const auto b = polygon[2] - polygon[1];
    return a.x() * b.y() - a.y() * b.x() > 1e-8;
}
}

class CubeView : public GridGameView {
    public:
        explicit CubeView (QWidget *parent) : GridGameView(parent) {
            setObjectName(QStringLiteral("cubeView"));
            setAccessibleName(tr("多面体棋盘"));
            animation.setDuration(130);
            connect(&animation, &QVariantAnimation::valueChanged, this, [this] { update(); });
            connect(&animation, &QVariantAnimation::finished, this, [this] { update(); });
        }

        void setState (const CubeState &value) {
            animation.stop();
            state = value;
            grid = enumerateGrid(state.solid, state.d1, state.d2);
            update();
        }

        void animateRoll (const CubeState &from, const RollInfo &rollInfo) {
            animation.stop();
            if (!isVisible()) return;
            source = from;
            rollInfoForAnimation = rollInfo;
            animation.setStartValue(0.0);
            animation.setEndValue(1.0);
            animation.start();
        }

        int directionAt (QPointF point) const {
            if (state.position < 0 || state.position >= static_cast<int>(grid.size())) return -1;
            const QPointF mouse = screenToWorld(point);
            const auto &square = grid[state.position];
            const qreal dx = mouse.x() - square.x, dy = mouse.y() - square.y;
            if (std::abs(dx) < 1e-8 && std::abs(dy) < 1e-8) return -1;
            const qreal angle = std::atan2(dy, dx);
            if (square.points.size() == 4) {
                if (std::abs(angle) > 3 * std::numbers::pi / 4) return Left;
                if (std::abs(angle) < std::numbers::pi / 4) return Right;
                return angle > 0 ? Down : Up;
            }
            if (square.directions[Up] == 0) {
                if (angle < -std::numbers::pi / 2 || angle > 5 * std::numbers::pi / 6) return Left;
                if (angle > std::numbers::pi / 6) return Down;
                return Right;
            }
            if (angle > std::numbers::pi / 2 || angle < -5 * std::numbers::pi / 6) return Left;
            if (angle < -std::numbers::pi / 6) return Up;
            return Right;
        }

    protected:
        void hideEvent (QHideEvent *event) override {
            animation.stop();
            GridGameView::hideEvent(event);
        }

        void paintEvent (QPaintEvent *) override {
            if (grid.empty()) return;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            const QColor blue("#2474b5"), blank = palette().color(QPalette::Base);
            const QColor border = palette().color(QPalette::Text);
            const bool rolling = animation.state() == QAbstractAnimation::Running;
            const CubeState &shown = rolling ? source : state;
            const auto &shownGrid = grid;
            for (int i = 0; i < static_cast<int>(shownGrid.size()); ++i) {
                QPolygonF polygon;
                for (const auto &point : shownGrid[i].points) polygon << worldToScreen(point);
                painter.setPen(QPen(border, 1));
                painter.setBrush(shown.board[i] ? blue : blank);
                painter.drawPolygon(polygon);
                if (hasFocus() && i == shown.position) {
                    painter.setBrush(Qt::NoBrush);
                    painter.setPen(QPen(palette().color(QPalette::Text), 2, Qt::DashLine));
                    painter.drawPolygon(polygon);
                }
            }

            if (shown.position < 0 || shown.position >= static_cast<int>(shownGrid.size())) return;
            const auto &square = shownGrid[shown.position];
            const auto &base = solidFor(shown.solid);
            Solid poly = base;
            std::array<int, 4> keys{};
            if (rolling) {
                poly = transformPoly(base, square.flip, rollInfoForAnimation.polyKeys[0],
                                     rollInfoForAnimation.polyKeys[1],
                                     rollInfoForAnimation.angle * animation.currentValue().toReal());
                const auto &edge = rollInfoForAnimation.sourceCorners;
                const auto &g0 = square.points[edge[0]], &g1 = square.points[edge[1]];
                const auto &v0 = poly.vertices[rollInfoForAnimation.polyKeys[0]];
                const auto &v1 = poly.vertices[rollInfoForAnimation.polyKeys[1]];
                const Vec3 translation{((g0.x() - v0.x) + (g1.x() - v1.x)) / 2,
                                       ((g0.y() - v0.y) + (g1.y() - v1.y)) / 2,
                                       (-(v0.z + v1.z)) / 2};
                for (auto &v : poly.vertices) { v.x += translation.x; v.y += translation.y; v.z += translation.z; }
            } else {
                flipPoly(poly, square.flip);
                if (alignPoly(base, square, keys)) {
                    const auto &v0 = poly.vertices[keys[0]], &v1 = poly.vertices[keys[1]];
                    const auto &g0 = square.points[0], &g1 = square.points[1];
                    const Vec3 translation{((g0.x() - v0.x) + (g1.x() - v1.x)) / 2,
                                           ((g0.y() - v0.y) + (g1.y() - v1.y)) / 2,
                                           (-(v0.z + v1.z)) / 2};
                    for (auto &v : poly.vertices) { v.x += translation.x; v.y += translation.y; v.z += translation.z; }
                }
            }

            for (int face = 0; face < static_cast<int>(base.faces.size()); ++face) {
                QPolygonF polygon;
                for (int i = 0; i < base.order; ++i) {
                    const int vertex = base.faces[face].vertices[i];
                    const auto &v = poly.vertices[vertex];
                    polygon << worldToScreen({v.x - v.z * base.shear, v.y - v.z * base.shear});
                }
                if (!faceVisible(polygon)) continue;
                painter.setPen(QPen(border, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.setBrush(shown.faces[face] ? blue : blank);
                painter.drawPolygon(polygon);
            }
        }

    private:
        std::pair<QPointF, qreal> transform () const {
            if (grid.empty()) return {{width() / 2.0, height() / 2.0}, 1};
            const auto &solid = solidFor(state.solid);
            qreal left = std::numeric_limits<qreal>::max(), right = -left;
            qreal top = left, bottom = -left;
            const auto &boundsGrid = grid;
            for (const auto &square : boundsGrid) for (const auto &p : square.points) {
                left = std::min(left, p.x()); right = std::max(right, p.x());
                top = std::min(top, p.y()); bottom = std::max(bottom, p.y());
            }
            left -= solid.border; right += solid.border;
            top -= solid.border; bottom += solid.border;
            const QRectF available = QRectF(rect()).adjusted(12, 12, -12, -12);
            const qreal scale = std::max<qreal>(0.1, std::min(available.width() / (right - left),
                                                              available.height() / (bottom - top)));
            const QPointF center((left + right) / 2, (top + bottom) / 2);
            return {QPointF(width() / 2.0, height() / 2.0) - center * scale, scale};
        }

        QPointF worldToScreen (QPointF point) const {
            const auto [offset, scale] = transform();
            return offset + point * scale;
        }

        QPointF screenToWorld (QPointF point) const {
            const auto [offset, scale] = transform();
            return (point - offset) / scale;
        }

        CubeState state, source;
        std::vector<GridSquare> grid;
        RollInfo rollInfoForAnimation;
        QVariantAnimation animation;
};

CubeGame::CubeGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    expandComboBox(ui->cubeType);
    view = new CubeView(ui->cubeBoard);
    ui->cubeBoardLayout->addWidget(view);
    view->installEventFilter(this);
    connect(ui->cubeType, &QComboBox::currentIndexChanged, this, &CubeGame::reset);
    connect(ui->cubeNewButton, &QToolButton::clicked, this, &CubeGame::reset);
    connect(ui->cubeRetryButton, &QToolButton::clicked, this, [this] {
        state = initial;
        history.clear();
        refresh();
    });
    connect(ui->cubeUndoButton, &QToolButton::clicked, this, [this] {
        if (history.empty()) return;
        state = std::move(history.back());
        history.pop_back();
        refresh();
    });
    const std::array<std::pair<QToolButton*, int>, 4> buttons{{
        {ui->cubeLeftButton, Left}, {ui->cubeRightButton, Right},
        {ui->cubeUpButton, Up}, {ui->cubeDownButton, Down}
    }};
    const std::array<QString, 4> labels{tr("向左滚动"), tr("向右滚动"), tr("向上滚动"), tr("向下滚动")};
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
        auto *button = buttons[i].first;
        const int direction = buttons[i].second;
        button->setAccessibleName(labels[i]);
        button->setToolTip(labels[i]);
        connect(button, &QToolButton::clicked, this, [this, direction] {
            move(direction);
            view->setFocus(Qt::OtherFocusReason);
        });
    }
    reset();
}

void CubeGame::reset () {
    const int index = std::clamp(ui->cubeType->currentIndex(), 0, static_cast<int>(presets.size()) - 1);
    state = generateGame(presets[index]);
    initial = state;
    history.clear();
    refresh();
}

void CubeGame::move (int direction) {
    const auto grid = enumerateGrid(state.solid, state.d1, state.d2);
    CubeState next = state;
    RollInfo rollInfo;
    if (!roll(next, grid, direction, &rollInfo)) return;
    history.push_back(state);
    state = std::move(next);
    refresh();
    view->animateRoll(history.back(), rollInfo);
}

void CubeGame::refresh () {
    const int collected = static_cast<int>(std::accumulate(state.faces.begin(), state.faces.end(), 0));
    const bool won = isComplete(state);
    view->setState(state);
    ui->cubeStatus->setText(tr("已收集 %1 / %2 面 · %3 步")
        .arg(collected).arg(state.faces.size()).arg(state.completedMoves ? state.completedMoves : state.moves));
    ui->cubeFeedback->setText(won ? tr("所有蓝色面都已收集完成。")
                                  : tr("每次滚动都会交换落点格子与多面体底面的颜色。"));
    ui->cubeUndoButton->setEnabled(!history.empty());
    const auto grid = enumerateGrid(state.solid, state.d1, state.d2);
    const std::array<std::pair<QToolButton*, int>, 4> buttons{{
        {ui->cubeLeftButton, Left}, {ui->cubeRightButton, Right},
        {ui->cubeUpButton, Up}, {ui->cubeDownButton, Down}
    }};
    for (const auto &[button, direction] : buttons)
        button->setEnabled(canRoll(state, grid, direction));
    QStringList tiles, faces;
    for (int color : state.board) tiles << QString::number(color);
    for (int color : state.faces) faces << QString::number(color);
    view->setAccessibleDescription(tr("位置 %1；棋盘格子 %2；各面状态 %3")
        .arg(state.position + 1).arg(tiles.join(','), faces.join(',')));
}

bool CubeGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched != view) return Game::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        const auto *mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton) return false;
        view->setFocus(Qt::MouseFocusReason);
        const int direction = view->directionAt(mouse->position());
        if (direction >= 0) move(direction);
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Up: case Qt::Key_W: move(Up); return true;
            case Qt::Key_Right: case Qt::Key_D: move(Right); return true;
            case Qt::Key_Down: case Qt::Key_S: move(Down); return true;
            case Qt::Key_Left: case Qt::Key_A: move(Left); return true;
            case Qt::Key_7: move(UpLeft); return true;
            case Qt::Key_9: move(UpRight); return true;
            case Qt::Key_1: move(DownLeft); return true;
            case Qt::Key_3: move(DownRight); return true;
        }
    }
    return Game::eventFilter(watched, event);
}
