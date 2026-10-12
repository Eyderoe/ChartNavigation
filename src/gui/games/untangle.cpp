#include "untangle.hpp"
#include "ui_note_widget.h"
#include "squareGameView.hpp"
#include "ui/theme.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>

namespace {
// Generator adapted from untangle.c, Simon Tatham's Portable Puzzle
// Collection (MIT), snapshot 20260923.616da16. See SIMON-TATHAM-LICENSE.txt.
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/doc/untangle.html
constexpr qreal untangleEpsilon = 1e-7;
using UntangleEdge = std::array<int, 2>;

qreal crossProduct (QPointF a, QPointF b) {
    return a.x() * b.y() - a.y() * b.x();
}

bool pointOnSegment (QPointF p, QPointF a, QPointF b) {
    return std::abs(crossProduct(b - a, p - a)) <= untangleEpsilon
        && QPointF::dotProduct(p - a, p - b) <= untangleEpsilon;
}

// Shared endpoints are legal; overlapping edges, endpoint-on-edge touches and
// intersections between unrelated edges are not.
bool untangleEdgesIntersect (const std::vector<QPointF> &nodes,
                            UntangleEdge first, UntangleEdge second) {
    for (int shared : first)
        for (int other : second)
            if (shared == other) {
                const auto a = nodes[first[0] == shared ? first[1] : first[0]] - nodes[shared];
                const auto b = nodes[second[0] == shared ? second[1] : second[0]] - nodes[shared];
                return std::abs(crossProduct(a, b)) <= untangleEpsilon
                    && QPointF::dotProduct(a, b) > 0;
            }
    const auto a = nodes[first[0]], b = nodes[first[1]];
    const auto c = nodes[second[0]], d = nodes[second[1]];
    const qreal abC = crossProduct(b - a, c - a), abD = crossProduct(b - a, d - a);
    const qreal cdA = crossProduct(d - c, a - c), cdB = crossProduct(d - c, b - c);
    const auto opposite = [](qreal x, qreal y) {
        return (x > untangleEpsilon && y < -untangleEpsilon)
            || (x < -untangleEpsilon && y > untangleEpsilon);
    };
    return (opposite(abC, abD) && opposite(cdA, cdB))
        || pointOnSegment(a, c, d) || pointOnSegment(b, c, d)
        || pointOnSegment(c, a, b) || pointOnSegment(d, a, b);
}

bool untangleCrossingsChanged (const std::vector<QPointF> &before,
                              const std::vector<QPointF> &after,
                              const std::vector<UntangleEdge> &edges) {
    for (size_t i = 0; i < edges.size(); ++i)
        for (size_t j = i + 1; j < edges.size(); ++j)
            if (untangleEdgesIntersect(before, edges[i], edges[j])
                != untangleEdgesIntersect(after, edges[i], edges[j]))
                return true;
    return false;
}

struct UntangleConflicts {
    int count{};
    std::vector<bool> edges;
};

UntangleConflicts untangleConflicts (const std::vector<QPointF> &nodes,
                                   const std::vector<UntangleEdge> &edges) {
    UntangleConflicts result{0, std::vector<bool>(edges.size())};
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto edge = edges[i];
        if (QLineF(nodes[edge[0]], nodes[edge[1]]).length() <= untangleEpsilon) {
            ++result.count;
            result.edges[i] = true;
        }
        for (size_t j = i + 1; j < edges.size(); ++j)
            if (untangleEdgesIntersect(nodes, edge, edges[j])) {
                ++result.count;
                result.edges[i] = result.edges[j] = true;
            }
    }
    return result;
}

template<typename T>
void shuffleUntangle (std::vector<T> &values) {
    for (int i = static_cast<int>(values.size()) - 1; i > 0; --i)
        std::swap(values[i], values[QRandomGenerator::global()->bounded(i + 1)]);
}

struct UntanglePuzzle {
    std::vector<QPointF> solution;
    std::vector<UntangleEdge> edges;
};

UntanglePuzzle generateUntangle (int count) {
    // Direct adaptation of untangle.c's generator: choose distinct points
    // from a sqrt(3n) integer grid, then greedily add non-crossing edges from
    // the currently lowest-degree vertex, capped at the upstream MAXDEGREE.
    UntanglePuzzle puzzle;
    const int side = std::max(2, static_cast<int>(std::sqrt(count * 3)));
    std::vector<int> positions(side * side);
    std::iota(positions.begin(), positions.end(), 0);
    shuffleUntangle(positions);
    puzzle.solution.reserve(count);
    for (int i = 0; i < count; ++i)
        puzzle.solution.emplace_back((positions[i] % side) / static_cast<qreal>(side),
                                     (positions[i] / side) / static_cast<qreal>(side));

    std::vector<int> degree(count);
    while (true) {
        std::vector<int> vertices(count);
        std::iota(vertices.begin(), vertices.end(), 0);
        std::ranges::sort(vertices, [&](int a, int b) {
            return degree[a] == degree[b] ? a < b : degree[a] < degree[b];
        });
        bool added = false;
        for (size_t i = 0; i < vertices.size() && !added; ++i) {
            const int from = vertices[i];
            if (degree[from] >= 4)
                break;
            std::vector<std::pair<qreal, int>> candidates;
            for (size_t j = i + 1; j < vertices.size(); ++j) {
                const int to = vertices[j];
                if (degree[to] >= 4
                    || std::ranges::any_of(puzzle.edges, [from, to](const auto &edge) {
                        return (edge[0] == from && edge[1] == to)
                            || (edge[0] == to && edge[1] == from);
                    }))
                    continue;
                const auto delta = puzzle.solution[to] - puzzle.solution[from];
                candidates.emplace_back(QPointF::dotProduct(delta, delta), to);
            }
            std::ranges::sort(candidates, [](const auto &a, const auto &b) {
                return a.first == b.first ? a.second < b.second : a.first < b.first;
            });
            for (const auto &[distance, to] : candidates) {
                Q_UNUSED(distance);
                const UntangleEdge edge{from, to};
                bool blocked = false;
                for (int point = 0; point < count; ++point)
                    if (point != from && point != to
                        && pointOnSegment(puzzle.solution[point], puzzle.solution[from], puzzle.solution[to])) {
                        blocked = true;
                        break;
                    }
                if (blocked || std::ranges::any_of(puzzle.edges, [&](const auto &existing) {
                    return untangleEdgesIntersect(puzzle.solution, edge, existing);
                }))
                    continue;
                puzzle.edges.push_back(edge);
                ++degree[from];
                ++degree[to];
                added = true;
                break;
            }
        }
        if (!added)
            break;
    }
    return puzzle;
}
}

class UntangleView : public SquareGameView {
    public:
        explicit UntangleView (QWidget *parent) : SquareGameView(parent) {
            setObjectName(QStringLiteral("untangleView"));
            setMouseTracking(true);
            setAccessibleName(tr("解绳棋盘"));
        }
        QSize sizeHint () const override { return {280, 280}; }
        QSize minimumSizeHint () const override { return {100, 100}; }
        QRectF boardRect () const {
            const qreal side = std::min(width(), height());
            return {(width() - side) / 2.0, 0, side, side};
        }
        QRectF nodeRect () const { return boardRect().adjusted(16, 16, -16, -16); }
        QPointF toNormalized (QPointF point) const {
            const auto rect = nodeRect();
            return {(point.x() - rect.left()) / std::max(1.0, rect.width()),
                    (point.y() - rect.top()) / std::max(1.0, rect.height())};
        }
        QPointF toWidget (QPointF point) const {
            const auto rect = nodeRect();
            return rect.topLeft() + QPointF(point.x() * rect.width(), point.y() * rect.height());
        }
        int nodeAt (QPointF point) const {
            int hit = -1;
            qreal distance = 15;
            for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                const qreal candidate = QLineF(point, toWidget(nodes[i])).length();
                if (candidate < distance) {
                    distance = candidate;
                    hit = i;
                }
            }
            return hit;
        }
        void setBoard (const std::vector<QPointF> &points, const std::vector<UntangleEdge> &lines,
                       const std::vector<bool> &conflicts, int selected, bool complete) {
            nodes = points;
            edges = lines;
            crossing = conflicts;
            dragged = selected;
            solved = complete;
            update();
        }
    protected:
        void paintEvent (QPaintEvent *) override {
            const auto rect = boardRect();
            if (rect.width() <= 0)
                return;
            const qreal ratio = devicePixelRatioF();
            QPixmap image(QSize(static_cast<int>(std::ceil(rect.width() * ratio)),
                                static_cast<int>(std::ceil(rect.height() * ratio))));
            image.setDevicePixelRatio(ratio);
            image.fill(Qt::transparent);
            {
                QPainter painter(&image);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.setBrush(palette().color(QPalette::Base));
                painter.setPen(QPen(palette().color(QPalette::Mid), 1));
                painter.drawRoundedRect(QRectF(0.5, 0.5, rect.width() - 1, rect.height() - 1), 10, 10);
                const QColor normal = solved ? QColor("#37ad83") : palette().color(QPalette::Text);
                const QColor conflict("#e76664");
                // Draw crossing edges last so their highlight stays visible.
                for (int pass = 0; pass < 2; ++pass)
                    for (size_t i = 0; i < edges.size(); ++i) {
                        if (crossing[i] != (pass == 1))
                            continue;
                        painter.setPen(QPen(crossing[i] ? conflict : normal, 2, Qt::SolidLine, Qt::RoundCap));
                        painter.drawLine(toWidget(nodes[edges[i][0]]) - rect.topLeft(),
                                         toWidget(nodes[edges[i][1]]) - rect.topLeft());
                    }
                for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
                    const auto center = toWidget(nodes[i]) - rect.topLeft();
                    if (i == dragged) {
                        auto halo = palette().color(QPalette::Highlight);
                        halo.setAlpha(65);
                        painter.setPen(Qt::NoPen);
                        painter.setBrush(halo);
                        painter.drawEllipse(center, 10, 10);
                    }
                    painter.setPen(QPen(solved ? normal : palette().color(QPalette::Highlight), 2));
                    painter.setBrush(i == dragged ? palette().color(QPalette::Highlight) : palette().color(QPalette::Base));
                    painter.drawEllipse(center, 5, 5);
                }
            }
            QPainter painter(this);
            painter.drawPixmap(rect.topLeft(), image);
        }
    private:
        std::vector<QPointF> nodes;
        std::vector<UntangleEdge> edges;
        std::vector<bool> crossing;
        int dragged{-1};
        bool solved{};
};

UntangleGame::UntangleGame (Ui::note_widget *ui, QObject *parent) : Game(ui, parent) {
    initUntangle();
}

void UntangleGame::initUntangle () {
    expandComboBox(ui->untanglePreset);
    untangleView = new UntangleView(ui->untangleBoard);
    ui->untangleBoardLayout->addWidget(untangleView);
    untangleView->installEventFilter(this);
    connect(ui->untanglePreset, &QComboBox::currentIndexChanged, this, &UntangleGame::resetUntangle);
    connect(ui->untangleNewButton, &QToolButton::clicked, this, &UntangleGame::resetUntangle);
    connect(ui->untangleRetryButton, &QToolButton::clicked, this, [this] {
        untangleDraggedNode = -1;
        untangleView->unsetCursor();
        untangleNodes = untangleInitialNodes;
        untangleHistory.clear();
        untangleMoves = 0;
        updateUntangle();
    });
    connect(ui->untangleUndoButton, &QToolButton::clicked, this, [this] {
        if (untangleHistory.empty())
            return;
        untangleNodes = std::move(untangleHistory.back().nodes);
        if (untangleHistory.back().counted)
            --untangleMoves;
        untangleHistory.pop_back();
        updateUntangle();
    });
    resetUntangle();
}

void UntangleGame::resetUntangle () {
    static constexpr std::array<int, 5> sizes{6, 10, 15, 20, 25};
    const int count = sizes[std::clamp(ui->untanglePreset->currentIndex(), 0,
                                       static_cast<int>(sizes.size()) - 1)];
    auto puzzle = generateUntangle(count);
    untangleEdges = std::move(puzzle.edges);
    std::vector<int> order(count);
    untangleNodes.resize(count);
    do {
        std::iota(order.begin(), order.end(), 0);
        shuffleUntangle(order);
        for (int i = 0; i < count; ++i) {
            const qreal angle = 2 * std::numbers::pi * i / count - std::numbers::pi / 2;
            // map.c repeatedly shuffles the vertices onto a circle until at
            // least one pair of nonincident edges crosses.
            untangleNodes[order[i]] = {0.5 + (3.0 / 7.0) * std::cos(angle),
                                       0.5 + (3.0 / 7.0) * std::sin(angle)};
        }
    } while (untangleConflicts(untangleNodes, untangleEdges).count == 0);
    untangleInitialNodes = untangleNodes;
    untangleHistory.clear();
    untangleDraggedNode = -1;
    untangleView->unsetCursor();
    untangleMoves = 0;
    updateUntangle();
}

void UntangleGame::updateUntangle () {
    const auto conflicts = untangleConflicts(untangleNodes, untangleEdges);
    untangleCrossings = conflicts.count;
    const bool solved = untangleCrossings == 0;
    untangleView->setBoard(untangleNodes, untangleEdges, conflicts.edges, untangleDraggedNode, solved);
    ui->untangleStatus->setText(tr("交叉：%1　移动：%2").arg(untangleCrossings).arg(untangleMoves));
    ui->untangleUndoButton->setEnabled(!untangleHistory.empty() && untangleDraggedNode == -1);
    ui->untangleFeedback->setText(solved && untangleDraggedNode == -1
        ? tr("解开了！共移动 %1 次。点击新一局继续。").arg(untangleMoves) : QString{});
    untangleView->setAccessibleDescription(ui->untangleStatus->text() + ui->untangleFeedback->text());
}

void UntangleGame::finishUntangleDrag () {
    if (untangleDraggedNode == -1)
        return;
    untangleDraggedNode = -1;
    bool moved = false;
    for (size_t i = 0; i < untangleNodes.size(); ++i)
        moved |= QLineF(untangleNodes[i], untangleDragStart[i]).length() > untangleEpsilon;
    if (moved) {
        // Compare pairs, since the crossing count can stay the same even when
        // different ropes cross. Uncounted position changes remain undoable.
        const bool counted = untangleCrossingsChanged(untangleDragStart, untangleNodes, untangleEdges);
        untangleHistory.push_back({std::move(untangleDragStart), counted});
        if (counted)
            ++untangleMoves;
    } else
        untangleNodes = untangleDragStart;
    untangleView->unsetCursor();
    updateUntangle();
}

bool UntangleGame::handleUntangleEvent (QEvent *event) {
    if (event->type() == QEvent::Hide || event->type() == QEvent::UngrabMouse) {
        finishUntangleDrag();
        return false;
    }
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseMove
        && event->type() != QEvent::MouseButtonRelease)
        return false;
    const auto *mouse = static_cast<QMouseEvent*>(event);
    if (event->type() == QEvent::MouseButtonPress) {
        if (mouse->button() != Qt::LeftButton || untangleCrossings == 0)
            return false;
        const int node = untangleView->nodeAt(mouse->position());
        if (node == -1)
            return false;
        untangleDraggedNode = node;
        untangleDragStart = untangleNodes;
        untangleDragOffset = untangleNodes[node] - untangleView->toNormalized(mouse->position());
        untangleView->setCursor(Qt::ClosedHandCursor);
        updateUntangle();
        return true;
    }
    if (untangleDraggedNode != -1) {
        if (event->type() == QEvent::MouseButtonRelease && mouse->button() != Qt::LeftButton)
            return false;
        // Qt's implicit mouse grab keeps delivering drags outside the widget;
        // clamp normalized coordinates so nodes always remain reachable.
        const auto point = untangleView->toNormalized(mouse->position()) + untangleDragOffset;
        untangleNodes[untangleDraggedNode] = {std::clamp(point.x(), 0.0, 1.0),
                                            std::clamp(point.y(), 0.0, 1.0)};
        if (event->type() == QEvent::MouseButtonRelease)
            finishUntangleDrag();
        else
            updateUntangle();
        return true;
    }
    untangleView->setCursor(untangleCrossings != 0 && untangleView->nodeAt(mouse->position()) != -1
        ? Qt::OpenHandCursor : Qt::ArrowCursor);
    return false;
}

bool UntangleGame::eventFilter (QObject *watched, QEvent *event) {
    if (watched == untangleView && handleUntangleEvent(event))
        return true;
    return QObject::eventFilter(watched, event);
}
