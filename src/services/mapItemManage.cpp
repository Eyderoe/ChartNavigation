#include "mapItemManage.hpp"

#include "mapItemTemplate.hpp"

#include <QColor>
#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QRegion>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>


qreal vorCenterPointWidth (const qreal size) {
    return size > 14.0 ? size / 4.0 : size / 3.0;
}

qreal ndbCenterPointWidth (const qreal size) {
    return size > 12.0 ? size / 4.0 : size / 3.0;
}

bool isNdb (const MapItemData &data) {
    const auto *navaid = std::get_if<MapNavData>(&data);
    return navaid && navaid->navType == NavaidType::ndb;
}

void configureNdbPen (QPen &pen) {
    // LNM's NDB mark is two broken rings.  Keep the pattern in pen space so
    // it stays the same size for screen-fixed symbols at every map zoom.
    pen.setStyle(Qt::CustomDashLine);
    pen.setDashPattern({2.0, 2.0});
}

constexpr qreal airwayLineWidth{1.0};
// Path items use projected metres as their local coordinates, while the LNM
// arrow size is specified in screen pixels.  Keep a small local margin for the
// device-space arrow without making every route's scene-index region enormous.
constexpr qreal airwayBoundingMargin{2000.0};

bool isOneWayAirway (const char direct) {
    return direct == 'F' || direct == 'R';
}

QPolygonF airwayArrow (const QLineF &segment, const char direct, const qreal lineWidth) {
    if (!isOneWayAirway(direct) || segment.length() <= 1.0e-9)
        return {};

    QPointF direction = segment.p2() - segment.p1();
    if (direct == 'R')
        direction = -direction;
    direction /= segment.length();
    const QPointF right{-direction.y(), direction.x()};
    const QPointF center = segment.pointAt(0.5);
    const qreal size = 2.5 * lineWidth;

    // LNM 的箭头以线段中点为中心，先在屏幕局部坐标中画出这个凹口形：
    //       (0, -s)
    //      /       \
    //   (s, s)   (-s, s)
    //        \   /
    //       (0, s/2)
    // 局部 +y 指向线段的反方向；因此用 -direction 映射局部 y 轴。
    const QPolygonF localArrow{
        QPointF{0.0, -size}, QPointF{size, size},
        QPointF{0.0, size / 2.0}, QPointF{-size, size}
    };
    QPolygonF arrow;
    arrow.reserve(localArrow.size());
    for (const QPointF &point : localArrow)
        arrow.emplace_back(center + right * point.x() - direction * point.y());
    return arrow;
}


namespace
{
constexpr int mapItemTypeDataKey{0};
constexpr int mapItemIdDataKey{1};

struct NormalizedBound {
    double top{};
    double bottom{};
    double left{};
    double right{};
    bool valid{false};
};

NormalizedBound normalizeBound (const Rect2D &rect) {
    const auto &[topLeft, bottomRight] = rect;
    if (!std::isfinite(topLeft.first) || !std::isfinite(topLeft.second)
        || !std::isfinite(bottomRight.first) || !std::isfinite(bottomRight.second))
        return {};

    double left = normalizeLongitude(topLeft.second);
    double right = normalizeLongitude(bottomRight.second);
    if (getLongiSpan(topLeft.second, bottomRight.second) >= 360.0) {
        left = -180.0;
        right = 180.0;
    }
    return NormalizedBound(std::clamp(std::max(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
                           std::clamp(std::min(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat), left,
                           right, true);
}

bool contains (const NormalizedBound &outer, const NormalizedBound &inner) {
    if (!outer.valid || !inner.valid || outer.top < inner.top || outer.bottom > inner.bottom)
        return false;

    const auto outerRanges = getLongiRanges(outer.left, outer.right);
    for (const auto &[innerLeft, innerRight] : getLongiRanges(inner.left, inner.right)) {
        const bool rangeContained = std::ranges::any_of(outerRanges, [innerLeft, innerRight](const LongiRange &range) {
            return range.first <= innerLeft && range.second >= innerRight;
        });
        if (!rangeContained)
            return false;
    }
    return true;
}

Rect2D expandForItems (const NormalizedBound &viewport) {
    // 以视口中心为基准，将 m×n 的可见范围扩大为 2m×2n 的图元缓存。
    constexpr double maximumLatitudeSpan{maxSupportLat * 2.0};
    const double requestedLatitudeSpan = viewport.top - viewport.bottom;
    const double latitudeSpan = std::min(maximumLatitudeSpan, requestedLatitudeSpan * 2.0);
    const double latitudeCenter = (viewport.top + viewport.bottom) / 2.0;
    double top = latitudeCenter + latitudeSpan / 2.0;
    double bottom = latitudeCenter - latitudeSpan / 2.0;
    if (top > maxSupportLat) {
        bottom -= top - maxSupportLat;
        top = maxSupportLat;
    }
    if (bottom < -maxSupportLat) {
        top += -maxSupportLat - bottom;
        bottom = -maxSupportLat;
    }

    const double longitudeSpan = std::min(360.0, getLongiSpan(viewport.left, viewport.right) * 2.0);
    if (longitudeSpan >= 360.0)
        return {{top, -180.0}, {bottom, 180.0}};

    const double longitudeCenter = getLongiRangeCenter(viewport.left, viewport.right);
    const double left = normalizeLongitude(longitudeCenter - longitudeSpan / 2.0);
    const double right = normalizeLongitude(longitudeCenter + longitudeSpan / 2.0);
    return {{top, left}, {bottom, right}};
}

MapItemType dataType (const MapItemData &data) {
    return std::visit([](const auto &item) { return item.type; }, data);
}

int dataId (const MapItemData &data) {
    return std::visit([](const auto &item) { return item.id; }, data);
}

QString dataLabel (const MapItemData &data) {
    return std::visit([](const auto &item) -> QString {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, MapApData>)
            return item.icao;
        else if constexpr (std::is_same_v<T, MapAwyData> || std::is_same_v<T, MapFirData>
            || std::is_same_v<T, MapNavData>)
            return item.ident;
        else
            return QString::number(item.alt / 100);
    }, data);
}

bool isPathData (const MapItemData &data) {
    return std::holds_alternative<MapAwyData>(data) || std::holds_alternative<MapFirData>(data);
}

bool isPointData (const MapItemData &data) {
    return !isPathData(data);
}

bool dataTypeMatchesVariant (const MapItemData &data) {
    return std::visit([](const auto &item) {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, MapApData>)
            return item.type == MapItemType::airport || item.type == MapItemType::fix;
        else if constexpr (std::is_same_v<T, MapAwyData>)
            return item.type == MapItemType::awy;
        else if constexpr (std::is_same_v<T, MapFirData>)
            return item.type == MapItemType::fir;
        else if constexpr (std::is_same_v<T, MapMoraData>)
            return item.type == MapItemType::mora;
        else
            return item.type == MapItemType::navaid;
    }, data);
}

QPointF toQPoint (const Point2D &point) {
    return {point.first, point.second};
}

bool finitePoint (const Point2D &point) {
    return std::isfinite(point.first) && std::isfinite(point.second);
}

QPen defaultPen (const MapItemType type) {
    QPen pen;
    pen.setCosmetic(true);
    switch (type) {
        case MapItemType::airport:
            pen.setWidthF(12.0 / 5.0);
            break;
        case MapItemType::fix:
            pen.setWidthF(std::max(10.0 / 6.0, 1.5));
            break;
        case MapItemType::fir:
            pen.setWidthF(5.0);
            pen.setCapStyle(Qt::FlatCap);
            pen.setDashPattern({2.0, 2.0});
            break;
        default:
            pen.setWidthF(1.0);
            break;
    }
    switch (type) {
        case MapItemType::airport:
            pen.setColor(QColor(QStringLiteral("#005A9C")));
            break;
        case MapItemType::awy:
            pen.setColor(QColor(90, 150, 255));
            break;
        case MapItemType::fir:
            pen.setColor(QColor(QStringLiteral("#606080")));
            break;
        case MapItemType::fix:
            pen.setColor(QColor(230, 230, 230));
            break;
        case MapItemType::mora:
            pen.setColor(QColor(115, 115, 115));
            break;
        case MapItemType::navaid:
            pen.setColor(QColor(210, 110, 230));
            break;
    }
    return pen;
}

QBrush defaultBrush (const MapItemType type) {
    if (type == MapItemType::airport)
        return QBrush(QColor(QStringLiteral("#005A9C")));
    return Qt::NoBrush;
}

qreal defaultZValue (const MapItemType type) {
    switch (type) {
        case MapItemType::airport:
            return 3.0;
        case MapItemType::navaid:
            return 2.0;
        case MapItemType::fix:
            return 1.0;
        case MapItemType::fir:
            return -19.0;
        case MapItemType::mora:
            return -20.0;
        case MapItemType::awy:
            return -10.0;
    }
    return 0.0;
}

int displayPriority (const MapItemType type) {
    switch (type) {
        case MapItemType::airport:
            return 0;
        case MapItemType::navaid:
            return 1;
        case MapItemType::fix:
            return 2;
        case MapItemType::awy:
            return 3;
        case MapItemType::fir:
            return 4;
        case MapItemType::mora:
            return 5;
    }
    return 5;
}

QRectF deviceBounds (const QGraphicsItem &item, const QTransform &sceneToDevice) {
    return item.deviceTransform(sceneToDevice).mapRect(item.boundingRect());
}

std::array<QPointF, 8> pointLabelOffsets (const QRectF &symbolBounds,
                                         const QRectF &labelBounds,
                                         const QPointF &anchor) {
    constexpr qreal gap{4.0};
    const qreal left = symbolBounds.left() - anchor.x() - gap - labelBounds.right();
    const qreal right = symbolBounds.right() - anchor.x() + gap - labelBounds.left();
    const qreal top = symbolBounds.top() - anchor.y() - gap - labelBounds.bottom();
    const qreal bottom = symbolBounds.bottom() - anchor.y() + gap - labelBounds.top();
    const qreal centeredX = -labelBounds.center().x();
    const qreal centeredY = -labelBounds.center().y();
    return {
        QPointF{right, centeredY}, QPointF{left, centeredY},
        QPointF{centeredX, top}, QPointF{centeredX, bottom},
        QPointF{right, top}, QPointF{right, bottom},
        QPointF{left, top}, QPointF{left, bottom}
    };
}

void configureLabel (QGraphicsSimpleTextItem &labelItem, const QPointF &anchor) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    labelItem.setBrush(QColor(225, 225, 225));
    const QRectF labelRect = labelItem.boundingRect();
    labelItem.setPos(anchor);
    labelItem.setTransform(QTransform::fromTranslate(5.0, -labelRect.height() / 2.0));
}

void configureCenteredLabel (QGraphicsSimpleTextItem &labelItem, const QPointF &anchor) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    labelItem.setBrush(QColor(225, 225, 225));
    labelItem.setPos(anchor);
    const QPointF center = labelItem.boundingRect().center();
    labelItem.setTransform(QTransform::fromTranslate(-center.x(), -center.y()));
}

void configureMoraLabel (QGraphicsSimpleTextItem &labelItem, const QPointF &anchor,
                         const QRectF &frame) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, false);
    labelItem.setBrush(QColor(225, 225, 225));

    // QGraphicsSimpleTextItem uses the font pixel size as its local-coordinate
    // height when the item follows the map transform.  Scale a reference font
    // so the rendered label occupies 20% of the projected MORA frame.
    constexpr int referencePixelSize{100};
    QFont moraFont = labelItem.font();
    moraFont.setPixelSize(referencePixelSize);
    moraFont.setWeight(QFont::DemiBold);
    labelItem.setFont(moraFont);

    const qreal referenceHeight = labelItem.boundingRect().height();
    const qreal targetHeight = frame.height() * 0.20;
    if (referenceHeight > 0.0 && targetHeight > 0.0) {
        const qreal requestedSize = referencePixelSize * targetHeight / referenceHeight;
        moraFont.setPixelSize(std::max(1, static_cast<int>(std::lround(requestedSize))));
        labelItem.setFont(moraFont);
    }
    labelItem.setPos(anchor - labelItem.boundingRect().center());
}

void configureAirwayLabel (QGraphicsSimpleTextItem &labelItem, const QLineF &segment,
                           const qreal position = 0.5) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    qreal angle = std::atan2(segment.dy(), segment.dx()) * 180.0 / std::numbers::pi;
    if (angle > 90.0)
        angle -= 180.0;
    else if (angle <= -90.0)
        angle += 180.0;

    const QRectF labelRect = labelItem.boundingRect();
    // ItemIgnoresTransformations 下 pos 仍是地图坐标，不能在这里减去像素尺寸。
    // 把地图锚点放在线段候选位置，再用单一变换将文字中心旋转到原点，
    // 避免独立 setRotation() 让居中平移也绕原点旋转而偏离航路。
    labelItem.setPos(segment.pointAt(position));
    labelItem.setTransformOriginPoint(QPointF{});
    labelItem.setRotation(0.0);
    const qreal radians = angle * std::numbers::pi / 180.0;
    const qreal cosine = std::cos(radians);
    const qreal sine = std::sin(radians);
    const QPointF center = labelRect.center();
    // Explicitly map the local text center to (0, 0), then rotate around it.
    // Writing the matrix coefficients avoids QTransform's chained-operation order ambiguity.
    labelItem.setTransform(QTransform{
        cosine, sine, -sine, cosine,
        -cosine * center.x() + sine * center.y(),
        -sine * center.x() - cosine * center.y()
    });
}

std::vector<QLineF> lineSegments (const QPainterPath &path) {
    std::vector<QLineF> result;
    QPointF previous;
    bool hasPrevious{};
    for (int index = 0; index < path.elementCount(); ++index) {
        const auto element = path.elementAt(index);
        const QPointF point{element.x, element.y};
        if (element.type == QPainterPath::LineToElement && hasPrevious) {
            const QLineF line(previous, point);
            if (line.length() > 1.0e-9)
                result.emplace_back(line);
        }
        previous = point;
        hasPrevious = true;
        if (element.type == QPainterPath::MoveToElement)
            previous = point;
    }
    return result;
}

struct ProjectedPathSegment {
    MapItemData data;
    QPointF first;
    QPointF second;
};

bool samePoint (const QPointF &first, const QPointF &second) {
    constexpr qreal joinTolerance{0.01};
    return QLineF(first, second).length() <= joinTolerance;
}

struct ProjectedPointKey {
    std::int64_t x;
    std::int64_t y;

    bool operator== (const ProjectedPointKey &) const = default;
};

struct ProjectedPointKeyHash {
    size_t operator() (const ProjectedPointKey &key) const noexcept {
        const size_t first = std::hash<std::int64_t>{}(key.x);
        const size_t second = std::hash<std::int64_t>{}(key.y);
        return first ^ (second + 0x9e3779b9U + (first << 6U) + (first >> 2U));
    }
};

bool pointKeyLess (const ProjectedPointKey &first, const ProjectedPointKey &second) {
    return first.x < second.x || (first.x == second.x && first.y < second.y);
}

struct ProjectedSegmentKey {
    ProjectedPointKey first;
    ProjectedPointKey second;

    bool operator== (const ProjectedSegmentKey &) const = default;
};

struct ProjectedSegmentKeyHash {
    size_t operator() (const ProjectedSegmentKey &key) const noexcept {
        const size_t first = ProjectedPointKeyHash{}(key.first);
        const size_t second = ProjectedPointKeyHash{}(key.second);
        return first ^ (second + 0x9e3779b9U + (first << 6U) + (first >> 2U));
    }
};

struct FirLineKey {
    std::int64_t directionX;
    std::int64_t directionY;
    std::int64_t offset;

    bool operator== (const FirLineKey &) const = default;
};

struct FirLineKeyHash {
    size_t operator() (const FirLineKey &key) const noexcept {
        size_t result = std::hash<std::int64_t>{}(key.directionX);
        const auto combine = [&result](const std::int64_t value) {
            const size_t hash = std::hash<std::int64_t>{}(value);
            result ^= hash + 0x9e3779b9U + (result << 6U) + (result >> 2U);
        };
        combine(key.directionY);
        combine(key.offset);
        return result;
    }
};

struct FirLineEvent {
    double position;
    Point2D point;
    int coverageDelta;
};

struct FirLineGroup {
    MapItemData representative;
    std::vector<FirLineEvent> events;
};

ProjectedPointKey pointKey (const QPointF &point) {
    constexpr qreal joinTolerance{0.01};
    return {
        std::llround(point.x() / joinTolerance),
        std::llround(point.y() / joinTolerance)
    };
}

ProjectedPointKey firPointKey (const QPointF &point) {
    // 相邻 FIR 的同一条边可能来自两条独立记录，允许亚米级坐标舍入误差。
    constexpr qreal duplicateTolerance{1.0};
    return {
        std::llround(point.x() / duplicateTolerance),
        std::llround(point.y() / duplicateTolerance)
    };
}

struct FirGraph {
    std::unordered_map<ProjectedPointKey, std::vector<size_t>, ProjectedPointKeyHash> adjacentSegments;
};

struct FirPathState {
    qreal distance;
    ProjectedPointKey point;
};

struct FirPathStateGreater {
    bool operator() (const FirPathState &first, const FirPathState &second) const noexcept {
        return first.distance > second.distance;
    }
};

struct FirPathPrevious {
    ProjectedPointKey point;
    size_t segmentIndex;
};

std::optional<std::vector<size_t>> shortestFirPath (
    const FirGraph &graph, const std::vector<ProjectedPathSegment> &segments,
    const std::vector<bool> &removed, const ProjectedPointKey start,
    const ProjectedPointKey target, const qreal maximumLength) {
    std::priority_queue<FirPathState, std::vector<FirPathState>, FirPathStateGreater> pending;
    std::unordered_map<ProjectedPointKey, qreal, ProjectedPointKeyHash> distances;
    std::unordered_map<ProjectedPointKey, FirPathPrevious, ProjectedPointKeyHash> previous;
    distances.emplace(start, 0.0);
    pending.push({0.0, start});

    while (!pending.empty()) {
        const FirPathState current = pending.top();
        pending.pop();
        const auto knownDistance = distances.find(current.point);
        if (knownDistance == distances.end() || current.distance > knownDistance->second)
            continue;
        if (current.point == target)
            break;

        const auto adjacent = graph.adjacentSegments.find(current.point);
        if (adjacent == graph.adjacentSegments.end())
            continue;
        for (const size_t segmentIndex : adjacent->second) {
            if (removed[segmentIndex])
                continue;
            const auto &segment = segments[segmentIndex];
            const ProjectedPointKey first = firPointKey(segment.first);
            const ProjectedPointKey second = firPointKey(segment.second);
            const ProjectedPointKey next = first == current.point ? second : first;
            const qreal distance = current.distance + QLineF(segment.first, segment.second).length();
            if (distance > maximumLength)
                continue;
            const auto found = distances.find(next);
            if (found != distances.end() && found->second <= distance)
                continue;
            distances[next] = distance;
            previous[next] = {current.point, segmentIndex};
            pending.push({distance, next});
        }
    }

    if (!distances.contains(target))
        return std::nullopt;
    std::vector<size_t> path;
    for (ProjectedPointKey point = target; point != start;) {
        const auto found = previous.find(point);
        if (found == previous.end())
            return std::nullopt;
        path.emplace_back(found->second.segmentIndex);
        point = found->second.point;
    }
    std::ranges::reverse(path);
    return path;
}

qreal distanceToLine (const QPointF &point, const QLineF &line) {
    const QPointF direction = line.p2() - line.p1();
    const qreal length = line.length();
    if (length <= 1.0e-9)
        return std::numeric_limits<qreal>::infinity();
    const QPointF relative = point - line.p1();
    return std::abs(direction.x() * relative.y() - direction.y() * relative.x()) / length;
}

std::vector<ProjectedPathSegment> collapseApproximateFirDuplicates (
    const std::vector<ProjectedPathSegment> &segments) {
    // 相邻 FIR 可能用“一条直线”和“多段折线”分别描述同一条共边。
    // 只有两套描述共享首尾点时才比较，普通 FIR 曲线不会被主动拉直。
    std::map<QString, FirGraph> graphs;
    for (size_t index = 0; index < segments.size(); ++index) {
        const auto *fir = std::get_if<MapFirData>(&segments[index].data);
        if (!fir)
            continue;
        auto &adjacent = graphs[fir->ident].adjacentSegments;
        adjacent[firPointKey(segments[index].first)].emplace_back(index);
        adjacent[firPointKey(segments[index].second)].emplace_back(index);
    }

    std::vector<bool> removed(segments.size());
    for (size_t index = 0; index < segments.size(); ++index) {
        if (removed[index])
            continue;
        const auto *fir = std::get_if<MapFirData>(&segments[index].data);
        if (!fir)
            continue;
        const QLineF directLine(segments[index].first, segments[index].second);
        const qreal directLength = directLine.length();
        if (directLength <= 1.0e-9)
            continue;

        const ProjectedPointKey start = firPointKey(directLine.p1());
        const ProjectedPointKey target = firPointKey(directLine.p2());
        constexpr qreal relativeDeviationLimit{0.05};
        constexpr qreal absoluteDeviationLimit{5000.0};
        constexpr qreal pathLengthFactor{1.10};
        const qreal maximumDeviation = std::min(directLength * relativeDeviationLimit,
                                                absoluteDeviationLimit);
        for (const auto &[ident, graph] : graphs) {
            if (ident == fir->ident || !graph.adjacentSegments.contains(start)
                || !graph.adjacentSegments.contains(target))
                continue;
            const auto path = shortestFirPath(graph, segments, removed, start, target,
                                              directLength * pathLengthFactor);
            if (!path || path->size() < 2)
                continue;

            // 相对 5% 与绝对 5 km 必须同时满足；额外限制路径长度，避免误选
            // 同一 FIR 绕边界另一侧抵达目标点的长路径。
            qreal maximumPathDeviation{};
            for (const size_t pathSegment : *path) {
                maximumPathDeviation = std::max({maximumPathDeviation,
                                                 distanceToLine(segments[pathSegment].first, directLine),
                                                 distanceToLine(segments[pathSegment].second, directLine)});
            }
            if (maximumPathDeviation > maximumDeviation)
                continue;
            for (const size_t pathSegment : *path)
                removed[pathSegment] = true;
        }
    }

    std::vector<ProjectedPathSegment> result;
    result.reserve(segments.size());
    for (size_t index = 0; index < segments.size(); ++index) {
        if (!removed[index])
            result.emplace_back(segments[index]);
    }
    return result;
}

std::vector<ProjectedPathSegment> uniqueFirSegments (const std::vector<ProjectedPathSegment> &segments,
                                                     const DynamicLCC &projection) {
    constexpr double lineKeyTolerance{1.0e-6};
    constexpr double eventTolerance{1.0e-10};
    std::unordered_map<FirLineKey, FirLineGroup, FirLineKeyHash> groups;
    std::vector<ProjectedPathSegment> ungrouped;
    const std::vector<ProjectedPathSegment> displaySegments = collapseApproximateFirDuplicates(segments);
    groups.reserve(displaySegments.size());
    ungrouped.reserve(displaySegments.size());

    // 公共边界在不同 FIR 中可能采用不同分段；先按经纬度直线归组，不能只比较整段端点。
    for (const auto &segment : displaySegments) {
        const auto *fir = std::get_if<MapFirData>(&segment.data);
        if (!fir || std::abs(fir->p2.second - fir->p1.second) > 180.0) {
            ungrouped.emplace_back(segment);
            continue;
        }

        double directionX = fir->p2.second - fir->p1.second;
        double directionY = fir->p2.first - fir->p1.first;
        const double length = std::hypot(directionX, directionY);
        if (length <= eventTolerance) {
            ungrouped.emplace_back(segment);
            continue;
        }
        directionX /= length;
        directionY /= length;
        if (directionX < 0.0 || (std::abs(directionX) <= eventTolerance && directionY < 0.0)) {
            directionX = -directionX;
            directionY = -directionY;
        }

        const double offset = -directionY * fir->p1.second + directionX * fir->p1.first;
        const FirLineKey key{
            std::llround(directionX / lineKeyTolerance),
            std::llround(directionY / lineKeyTolerance),
            std::llround(offset / lineKeyTolerance)
        };
        auto group = groups.try_emplace(key, FirLineGroup{segment.data, {}}).first;
        const double firstPosition = directionX * fir->p1.second + directionY * fir->p1.first;
        const double secondPosition = directionX * fir->p2.second + directionY * fir->p2.first;
        if (firstPosition <= secondPosition) {
            group->second.events.push_back({firstPosition, fir->p1, 1});
            group->second.events.push_back({secondPosition, fir->p2, -1});
        } else {
            group->second.events.push_back({secondPosition, fir->p2, 1});
            group->second.events.push_back({firstPosition, fir->p1, -1});
        }
    }

    std::vector<ProjectedPathSegment> splitSegments;
    splitSegments.reserve(segments.size());
    for (auto &entry : groups) {
        auto &group = entry.second;
        std::ranges::sort(group.events, {}, &FirLineEvent::position);
        // 在每个端点处切开，用覆盖计数保证任一最小区间最多生成一次绘制几何。
        int coverage{};
        size_t eventIndex{};
        while (eventIndex < group.events.size()) {
            const double position = group.events[eventIndex].position;
            const Point2D point = group.events[eventIndex].point;
            int delta{};
            size_t nextIndex = eventIndex;
            while (nextIndex < group.events.size()
                   && std::abs(group.events[nextIndex].position - position) <= eventTolerance) {
                delta += group.events[nextIndex].coverageDelta;
                ++nextIndex;
            }
            coverage += delta;
            if (coverage > 0 && nextIndex < group.events.size()) {
                const auto projected = projection.trans({point, group.events[nextIndex].point});
                if (projected.size() == 2 && finitePoint(projected[0]) && finitePoint(projected[1]))
                    splitSegments.push_back({group.representative,
                                             toQPoint(projected[0]), toQPoint(projected[1])});
            }
            eventIndex = nextIndex;
        }
    }

    splitSegments.insert(splitSegments.end(), ungrouped.begin(), ungrouped.end());
    std::vector<ProjectedPathSegment> unique;
    unique.reserve(splitSegments.size());
    std::unordered_set<ProjectedSegmentKey, ProjectedSegmentKeyHash> seen;
    seen.reserve(splitSegments.size());
    for (const auto &segment : splitSegments) {
        ProjectedPointKey first = firPointKey(segment.first);
        ProjectedPointKey second = firPointKey(segment.second);
        if (pointKeyLess(second, first))
            std::swap(first, second);
        if (seen.emplace(ProjectedSegmentKey{first, second}).second)
            unique.emplace_back(segment);
    }
    return unique;
}

QPainterPath combineSegments (const std::vector<ProjectedPathSegment> &segments) {
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    std::vector<bool> consumed(segments.size());
    std::unordered_map<ProjectedPointKey, std::vector<size_t>, ProjectedPointKeyHash> endpoints;
    endpoints.reserve(segments.size() * 2);
    for (size_t index = 0; index < segments.size(); ++index) {
        endpoints[pointKey(segments[index].first)].emplace_back(index);
        endpoints[pointKey(segments[index].second)].emplace_back(index);
    }

    const auto connectedSegment = [&segments, &consumed, &endpoints](const QPointF &endpoint)
        -> std::optional<std::pair<size_t, QPointF>> {
        const auto found = endpoints.find(pointKey(endpoint));
        if (found == endpoints.end())
            return std::nullopt;
        for (const size_t index : found->second) {
            if (consumed[index])
                continue;
            if (samePoint(endpoint, segments[index].first))
                return std::pair{index, segments[index].second};
            if (samePoint(endpoint, segments[index].second))
                return std::pair{index, segments[index].first};
        }
        return std::nullopt;
    };

    for (size_t seed = 0; seed < segments.size(); ++seed) {
        if (consumed[seed])
            continue;

        std::deque<QPointF> chain{segments[seed].first, segments[seed].second};
        consumed[seed] = true;
        while (const auto next = connectedSegment(chain.back())) {
            consumed[next->first] = true;
            chain.push_back(next->second);
        }
        while (const auto next = connectedSegment(chain.front())) {
            consumed[next->first] = true;
            chain.push_front(next->second);
        }

        path.moveTo(chain.front());
        for (auto point = std::next(chain.cbegin()); point != chain.cend(); ++point)
            path.lineTo(*point);
        if (chain.size() > 2 && samePoint(chain.front(), chain.back()))
            path.closeSubpath();
    }
    return path;
}

QRectF projectedRect (const DynamicLCC &projection, const Rect2D &bound) {
    const auto &[topLeft, bottomRight] = bound;
    const double top = topLeft.first;
    const double left = topLeft.second;
    const double bottom = bottomRight.first;
    const double right = bottomRight.second;
    const auto corners = projection.trans({
        {top, left}, {top, right}, {bottom, left}, {bottom, right}
    });
    QRectF result;
    bool initialized{};
    for (const auto &corner : corners) {
        if (!finitePoint(corner))
            continue;
        if (!initialized) {
            result = QRectF(toQPoint(corner), QSizeF{});
            initialized = true;
        } else {
            result |= QRectF(toQPoint(corner), QSizeF{});
        }
    }
    return result.normalized();
}

// MORA records are one-degree geographic cells.  Keep the projected cell
// corners instead of rectifying each cell independently: adjacent cells then
// share the exact same projected edge and can never overlap at high latitude.
std::optional<QPainterPath> moraFrame (const DynamicLCC &projection, const Rect2D &bound) {
    const auto &[topLeft, bottomRight] = bound;
    const auto corners = projection.trans({
        topLeft,
        {topLeft.first, bottomRight.second},
        bottomRight,
        {bottomRight.first, topLeft.second}
    });
    if (corners.size() != 4 || !std::ranges::all_of(corners, finitePoint))
        return std::nullopt;

    QPainterPath path(toQPoint(corners.front()));
    for (size_t index = 1; index < corners.size(); ++index)
        path.lineTo(toQPoint(corners[index]));
    path.closeSubpath();
    return path;
}
} // namespace


MapPathItem::MapPathItem (std::vector<MapItemData> data, const QPainterPath &path,
                          std::vector<QPointF> labelAnchors, QGraphicsItem *parent) :
    QGraphicsPathItem(path, parent), dataItems(std::move(data)) {
    if (dataItems.empty() || !std::ranges::all_of(dataItems, [this](const MapItemData &item) {
        return isPathData(item) && dataTypeMatchesVariant(item)
                && dataType(item) == dataType(dataItems.front());
    }))
        throw std::invalid_argument("MapPathItem requires airway or FIR data");

    setFlag(ItemIsSelectable, true);
    setPen(defaultPen(itemType()));
    setBrush(defaultBrush(itemType()));
    setZValue(defaultZValue(itemType()));

    if (itemType() == MapItemType::awy)
        airwaySegments = lineSegments(path);

    if (itemType() == MapItemType::awy) {
        labelItems.reserve(dataItems.size());
        for (size_t index = 0; index < dataItems.size(); ++index) {
            auto *labelItem = new QGraphicsSimpleTextItem(dataLabel(dataItems[index]), this);
            labelItem->setData(mapItemTypeDataKey, static_cast<int>(itemType()));
            labelItem->setData(mapItemIdDataKey, dataId(dataItems[index]));
            const QPointF anchor = index < labelAnchors.size()
                                       ? labelAnchors[index]
                                       : (path.isEmpty() ? QPointF{} : path.pointAtPercent(0.5));
            configureLabel(*labelItem, anchor);
            labelItems.emplace_back(labelItem);
        }
        const auto segments = lineSegments(path);
        if (labelItems.size() == 1 && !segments.empty())
            configureAirwayLabel(*labelItems.front(), segments.front());
    }
}

const MapItemData& MapPathItem::mapData () const noexcept {
    return dataItems.front();
}

const MapItemData* MapPathItem::findData (const MapItemType type, const int id) const noexcept {
    const auto found = std::ranges::find_if(dataItems, [type, id](const MapItemData &item) {
        return dataType(item) == type && dataId(item) == id;
    });
    return found == dataItems.end() ? nullptr : &*found;
}

QRectF MapPathItem::boundingRect () const {
    QRectF result = QGraphicsPathItem::boundingRect().united(shape().boundingRect());
    if (itemType() == MapItemType::awy && !airwaySegments.empty())
        result.adjust(-airwayBoundingMargin, -airwayBoundingMargin,
                      airwayBoundingMargin, airwayBoundingMargin);
    return result;
}

QPainterPath MapPathItem::shape () const {
    if (path().isEmpty())
        return {};
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(pen().widthF(), 6.0));
    QPainterPath result = path().united(stroker.createStroke(path()));
    return result;
}

void MapPathItem::paint (QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QGraphicsPathItem::paint(painter, option, widget);
    if (itemType() != MapItemType::awy)
        return;

    const qreal lineWidth = std::max(pen().widthF(), airwayLineWidth);
    const QTransform itemToDevice = painter->worldTransform();
    painter->save();
    // The route geometry is in projected metres.  Convert each segment to
    // device coordinates first so the arrow keeps its LNM pixel size at every
    // zoom level, then draw it without the map transform.
    painter->setWorldTransform(QTransform{});
    painter->setPen(pen());
    painter->setBrush(Qt::NoBrush);
    const size_t count = std::min(dataItems.size(), airwaySegments.size());
    for (size_t index = 0; index < count; ++index) {
        const auto *airway = std::get_if<MapAwyData>(&dataItems[index]);
        if (!airway)
            continue;
        const QLineF deviceSegment(itemToDevice.map(airwaySegments[index].p1()),
                                   itemToDevice.map(airwaySegments[index].p2()));
        const QPolygonF arrow = airwayArrow(deviceSegment, airway->direct, lineWidth);
        if (!arrow.isEmpty())
            painter->drawPolygon(arrow);
    }
    painter->restore();
}

MapItemType MapPathItem::itemType () const noexcept {
    return dataType(dataItems.front());
}

const std::vector<QGraphicsSimpleTextItem*>& MapPathItem::labels () const noexcept {
    return labelItems;
}

std::vector<QRectF> MapPathItem::airwayArrowBounds (const QTransform &sceneToDevice) const {
    std::vector<QRectF> result;
    if (itemType() != MapItemType::awy)
        return result;

    const qreal lineWidth = std::max(pen().widthF(), airwayLineWidth);
    const QTransform itemToDevice = deviceTransform(sceneToDevice);
    const size_t count = std::min(dataItems.size(), airwaySegments.size());
    result.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        const auto *airway = std::get_if<MapAwyData>(&dataItems[index]);
        if (!airway)
            continue;
        const QLineF deviceSegment(itemToDevice.map(airwaySegments[index].p1()),
                                   itemToDevice.map(airwaySegments[index].p2()));
        const QPolygonF arrow = airwayArrow(deviceSegment, airway->direct, lineWidth);
        if (!arrow.isEmpty())
            result.emplace_back(arrow.boundingRect());
    }
    return result;
}

void MapPathItem::setLabelsVisible (const bool visible) {
    for (auto *labelItem : labelItems)
        labelItem->setVisible(visible);
}

void MapPathItem::setLabelColor (const QColor &color) {
    for (auto *labelItem : labelItems)
        labelItem->setBrush(color);
}

void MapPathItem::setAirwayLabelSegments (const std::vector<QLineF> &segments) {
    if (itemType() != MapItemType::awy)
        throw std::logic_error("label segments are only valid for airway items");
    prepareGeometryChange();
    airwaySegments = segments;
    const size_t count = std::min(labelItems.size(), segments.size());
    for (size_t index = 0; index < count; ++index) {
        if (segments[index].length() > 1.0e-9)
            configureAirwayLabel(*labelItems[index], segments[index]);
    }
}

void MapPathItem::setAirwayLabelPosition (const size_t index, const qreal position) {
    if (itemType() != MapItemType::awy || index >= labelItems.size() || index >= airwaySegments.size())
        throw std::out_of_range("invalid airway label index");
    configureAirwayLabel(*labelItems[index], airwaySegments[index], position);
}

/**
 * @brief 构造航点、导航台、机场或 MORA 地图图元。
 * @param data 图元对应的地图数据，仅接受点类数据。
 * @param symbol 使用图元局部坐标描述的符号路径。
 * @param screenFixed 是否忽略视图变换，使符号尺寸保持不变。
 * @param parent 父图元。
 */
MapPointItem::MapPointItem (MapItemData data, QPainterPath symbol, const bool screenFixed, QGraphicsItem *parent) :
    QGraphicsItem(parent), data(std::move(data)), symbolPath(std::move(symbol)),
    itemPen(defaultPen(dataType(this->data))), itemBrush(defaultBrush(dataType(this->data))) {
    if (!isPointData(this->data) || !dataTypeMatchesVariant(this->data))
        throw std::invalid_argument("MapPointItem requires airport, fix, navaid or MORA data");

    if (isNdb(this->data))
        configureNdbPen(itemPen);

    setFlag(ItemIsSelectable, true);
    setFlag(ItemIgnoresTransformations, screenFixed);
    setZValue(defaultZValue(itemType()));
    labelItem = new QGraphicsSimpleTextItem(label(), this);
    if (itemType() == MapItemType::mora) {
        configureMoraLabel(*labelItem, symbolPath.boundingRect().center(), symbolPath.boundingRect());
        labelItem->setOpacity(0.35);
    } else {
        configureLabel(*labelItem, symbolPath.boundingRect().center());
    }
}

QRectF MapPointItem::boundingRect () const {
    return shape().boundingRect();
}

QPainterPath MapPointItem::shape () const {
    if (symbolPath.isEmpty())
        return {};
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(itemPen.widthF(), 6.0));
    return symbolPath.united(stroker.createStroke(symbolPath));
}

void MapPointItem::paint (QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) {
    if (itemType() == MapItemType::airport) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(itemBrush);
        painter->drawEllipse(symbolPath.boundingRect());

        if (symbolPath.elementCount() >= 2) {
            const auto first = symbolPath.elementAt(symbolPath.elementCount() - 2);
            const auto second = symbolPath.elementAt(symbolPath.elementCount() - 1);
            const qreal diameter = std::min(symbolPath.boundingRect().width(),
                                            symbolPath.boundingRect().height());
            QPen runwayPen(Qt::white, diameter / 4.0, Qt::SolidLine, Qt::RoundCap);
            runwayPen.setCosmetic(true);
            painter->setPen(runwayPen);
            painter->drawLine(QPointF{first.x, first.y}, QPointF{second.x, second.y});
        }
        return;
    }

    painter->setPen(itemPen);
    painter->setBrush(itemBrush);
    painter->drawPath(symbolPath);

    const auto *navaid = std::get_if<MapNavData>(&data);
    if (!navaid)
        return;
    const QRectF bounds = symbolPath.boundingRect();
    const qreal size = std::max(bounds.width(), bounds.height());
    const qreal centerWidth = navaid->navType == NavaidType::ndb
                                  ? ndbCenterPointWidth(size)
                                  : vorCenterPointWidth(size);
    QPen centerPen(itemPen.color(), centerWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    centerPen.setCosmetic(true);
    painter->setPen(centerPen);
    painter->drawPoint(QPointF{});
}

const MapItemData& MapPointItem::mapData () const noexcept {
    return data;
}

MapItemType MapPointItem::itemType () const noexcept {
    return dataType(data);
}

QString MapPointItem::label () const {
    return dataLabel(data);
}

QGraphicsSimpleTextItem* MapPointItem::labelGraphicsItem () const noexcept {
    return labelItem;
}

void MapPointItem::setPen (const QPen &pen) {
    prepareGeometryChange();
    itemPen = pen;
    if (isNdb(data))
        configureNdbPen(itemPen);
    update();
}

void MapPointItem::setBrush (const QBrush &brush) {
    itemBrush = brush;
    update();
}

void MapPointItem::setLabelsVisible (const bool visible) {
    labelItem->setVisible(visible);
}

void MapPointItem::setLabelColor (const QColor &color) {
    labelItem->setBrush(color);
}

void MapPointItem::setLabelAnchor (const QPointF &anchor, const bool centered) const {
    if (centered && itemType() == MapItemType::mora)
        configureMoraLabel(*labelItem, anchor, symbolPath.boundingRect());
    else if (centered)
        configureCenteredLabel(*labelItem, anchor);
    else
        configureLabel(*labelItem, anchor);
}

MapItemManage::MapItemManage (const QString &databaseFilePath) : query(databaseFilePath) {
    airportSymbolPath = airportSymbol();
    fixSymbolPath = fixSymbol();
    navaidSymbols = {vorSymbol(), dmeSymbol(), vordmeSymbol(), ndbSymbol()};
}

/**
 * @brief 根据当前视口更新静态地图图元缓存及其投影。
 * @param viewportBound 当前视口的经纬度边界。
 * @return 实际重建缓存时返回 true；边界无效或现有缓存已覆盖视口时返回 false。
 */
bool MapItemManage::updateViewport (const Rect2D &viewportBound) {
    // 先规范化视口；若现有缓存完整覆盖该范围，就无需重新查询和创建图元。
    const NormalizedBound viewport = normalizeBound(viewportBound);
    if (!viewport.valid)
        return false;
    if (cacheValid && contains(normalizeBound(cachedItemBound), viewport))
        return false;

    // 扩大可视边界作为图元缓存缓冲区；数据库缓存换代时才同步重设投影。
    const Rect2D newItemBound = expandForItems(viewport);
    auto [mapData, queriedDatabase] = query.queryMapItemData(newItemBound);
    projectionResetPending = projectionResetPending || queriedDatabase;

    DynamicLCC newProjection;
    const bool replaceProjection = projectionResetPending;
    const DynamicLCC *itemProjection = &projection;
    if (replaceProjection) {
        newProjection.reset(newItemBound.first.second, newItemBound.second.second,
                            newItemBound.second.first, newItemBound.first.first);
        itemProjection = &newProjection;
    }

    // 在临时容器中构造下一代缓存，全部成功后再替换当前有效状态。
    std::vector<std::unique_ptr<QGraphicsItem>> newItems;
    newItems.reserve(mapData.size());
    std::vector<ProjectedPathSegment> airwaySegments;
    std::vector<ProjectedPathSegment> firSegments;
    for (const auto &data : mapData) {
        // 航路与 FIR 先保存投影线段，循环结束后分别合并为较少的路径图元。
        if (const auto *airway = std::get_if<MapAwyData>(&data)) {
            const auto points = itemProjection->trans({airway->p1, airway->p2});
            if (points.size() != 2 || !finitePoint(points[0]) || !finitePoint(points[1]))
                continue;
            airwaySegments.push_back({data, toQPoint(points[0]), toQPoint(points[1])});
        } else if (const auto *fir = std::get_if<MapFirData>(&data)) {
            const auto points = itemProjection->trans({fir->p1, fir->p2});
            if (points.size() != 2 || !finitePoint(points[0]) || !finitePoint(points[1]))
                continue;
            firSegments.push_back({data, toQPoint(points[0]), toQPoint(points[1])});
        } else if (const auto *mora = std::get_if<MapMoraData>(&data)) {
            // MORA 使用随地图缩放的投影边框；其余点元素只投影锚点，符号保持屏幕尺寸。
            const auto path = moraFrame(*itemProjection, mora->bounds);
            if (!path)
                continue;
            auto item = std::make_unique<MapPointItem>(data, *path, false);
            item->setLabelAnchor(path->boundingRect().center(), true);
            newItems.emplace_back(std::move(item));
        } else {
            const Point2D realPosition = std::visit([](const auto &item) -> Point2D {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, MapApData> || std::is_same_v<T, MapNavData>)
                    return item.realPos;
                return {};
            }, data);
            const auto positions = itemProjection->trans({realPosition});
            if (positions.size() != 1 || !finitePoint(positions.front()))
                continue;
            auto item = std::make_unique<MapPointItem>(data, symbolForData(data), true);
            item->setPos(toQPoint(positions.front()));
            newItems.emplace_back(std::move(item));
        }
    }

    const auto appendPathItem = [&newItems, itemProjection](std::vector<ProjectedPathSegment> segments) {
        if (segments.empty())
            return;
        const bool isFir = dataType(segments.front().data) == MapItemType::fir;
        if (!isFir) {
            for (auto &segment : segments) {
                QPainterPath path(segment.first);
                path.lineTo(segment.second);
                auto item = std::make_unique<MapPathItem>(
                    std::vector<MapItemData>{std::move(segment.data)}, path,
                    std::vector<QPointF>{(segment.first + segment.second) / 2.0});
                newItems.emplace_back(std::move(item));
            }
            return;
        }

        // FIR 仅对绘制几何去重，原始线段仍用于保留数据关联和各段标签位置。
        const std::vector<ProjectedPathSegment> geometrySegments = uniqueFirSegments(segments, *itemProjection);
        std::vector<MapItemData> sourceData;
        std::vector<QPointF> labelAnchors;
        sourceData.reserve(segments.size());
        labelAnchors.reserve(segments.size());
        for (const auto &segment : segments) {
            sourceData.emplace_back(segment.data);
            labelAnchors.emplace_back((segment.first.x() + segment.second.x()) / 2.0,
                                      (segment.first.y() + segment.second.y()) / 2.0);
        }
        auto item = std::make_unique<MapPathItem>(
            std::move(sourceData), combineSegments(geometrySegments), std::move(labelAnchors));
        newItems.emplace_back(std::move(item));
    };
    appendPathItem(std::move(firSegments));
    appendPathItem(std::move(airwaySegments));

    // 所有数据转换完成后一次性提交边界和图元；新数据库缓存同时提交对应的新投影。
    if (replaceProjection)
        projection = std::move(newProjection);
    cachedItemBound = newItemBound;
    cachedProjectedBound = projectedRect(projection, cachedItemBound);
    cachedItems = std::move(newItems);
    cacheValid = true;
    applyZoomPolicy();
    projectionResetPending = false;
    return true;
}

bool MapItemManage::refresh (const Rect2D &viewportBound) {
    if (!normalizeBound(viewportBound).valid)
        return false;
    const bool previousCacheState = cacheValid;
    cacheValid = false;
    try {
        return updateViewport(viewportBound);
    } catch (...) {
        cacheValid = previousCacheState;
        throw;
    }
}

const QRectF& MapItemManage::projectedBound () const noexcept {
    return cachedProjectedBound;
}

const std::vector<std::unique_ptr<QGraphicsItem>>& MapItemManage::items () const noexcept {
    return cachedItems;
}

const MapItemData* MapItemManage::dataForItem (const QGraphicsItem *item) const noexcept {
    const QGraphicsItem *selectedItem = item;
    while (item) {
        if (const auto *pathItem = dynamic_cast<const MapPathItem*>(item)) {
            if (selectedItem != item && selectedItem->data(mapItemTypeDataKey).isValid()
                && selectedItem->data(mapItemIdDataKey).isValid()) {
                const auto type = static_cast<MapItemType>(selectedItem->data(mapItemTypeDataKey).toInt());
                if (const auto *data = pathItem->findData(type, selectedItem->data(mapItemIdDataKey).toInt()))
                    return data;
            }
            return &pathItem->mapData();
        }
        if (const auto *pointItem = dynamic_cast<const MapPointItem*>(item))
            return &pointItem->mapData();
        item = item->parentItem();
    }
    return nullptr;
}

MapItemDetails MapItemManage::itemDetails (const MapItemType type, const int id) {
    return query.queryItemDetails(type, id);
}

std::vector<Point2D> MapItemManage::project (std::vector<Point2D> positions) const {
    return projection.trans(std::move(positions));
}

std::vector<Point2D> MapItemManage::unproject (std::vector<Point2D> positions) const {
    return projection.revertTrans(std::move(positions));
}

void MapItemManage::setZoomLevel (const int level) {
    const int boundedLevel = std::clamp(level, 0, 3);
    if (currentZoomLevel == boundedLevel)
        return;
    currentZoomLevel = boundedLevel;

    applyZoomPolicy();
}

void MapItemManage::updateDisplayPriority (const QTransform &sceneToDevice,
                                           const QRectF &deviceViewport,
                                           const QPolygonF &labelSuppressionArea) {
    constexpr qreal spacing{2.0};
    struct Candidate {
        int priority{};
        QRectF bounds;
        QRectF persistentBounds;
        QGraphicsSimpleTextItem *label{};
        MapPointItem *pointItem{};
        MapPathItem *pathItem{};
        size_t pathLabelIndex{};
    };

    // applyZoomPolicy() also clears the result of the previous view position.
    applyZoomPolicy();
    std::vector<Candidate> candidates;
    QRegion airwayArrowRegion;
    for (const auto &cachedItem : cachedItems) {
        if (auto *pointItem = dynamic_cast<MapPointItem*>(cachedItem.get())) {
            if (!pointItem->isVisible())
                continue;

            auto *labelItem = pointItem->labelGraphicsItem();
            const MapItemType type = pointItem->itemType();
            // MORA 数字不参与普通文字避让，但附加航图内只保留网格框。
            if (type == MapItemType::mora) {
                const QPointF labelCenter = labelItem->mapToScene(labelItem->boundingRect().center());
                if (!labelSuppressionArea.isEmpty()
                    && labelSuppressionArea.containsPoint(labelCenter, Qt::OddEvenFill))
                    labelItem->setVisible(false);
                continue;
            }

            const QRectF symbolBounds = deviceBounds(*pointItem, sceneToDevice);
            if (!labelSuppressionArea.isEmpty()
                && labelSuppressionArea.containsPoint(pointItem->scenePos(), Qt::OddEvenFill))
                labelItem->setVisible(false);
            candidates.push_back({displayPriority(type), symbolBounds, symbolBounds,
                                  labelItem->isVisible() ? labelItem : nullptr, pointItem});
            continue;
        }

        auto *pathItem = dynamic_cast<MapPathItem*>(cachedItem.get());
        if (!pathItem)
            continue;
        for (const QRectF &arrowBounds : pathItem->airwayArrowBounds(sceneToDevice))
            airwayArrowRegion += QRegion(arrowBounds.adjusted(
                -spacing, -spacing, spacing, spacing).toAlignedRect());
        const auto &labels = pathItem->labels();
        for (size_t index = 0; index < labels.size(); ++index) {
            auto *labelItem = labels[index];
            if (labelItem->isVisible())
                candidates.push_back({displayPriority(pathItem->itemType()),
                                      deviceBounds(*labelItem, sceneToDevice), {}, labelItem, nullptr,
                                      pathItem, index});
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate &first,
                                                               const Candidate &second) {
        return first.priority < second.priority;
    });

    QRegion occupied;
    for (const Candidate &candidate : candidates) {
        if (candidate.pointItem && candidate.label) {
            bool placed{};
            const QRectF symbolLocalBounds = candidate.pointItem->boundingRect();
            const auto offsets = pointLabelOffsets(
                symbolLocalBounds, candidate.label->boundingRect(), candidate.label->pos());
            const size_t offsetCount = candidate.pointItem->itemType() == MapItemType::airport
                                           ? 2
                                           : offsets.size();
            const QPointF currentOffset = candidate.label->transform().map(QPointF{});
            const auto tryOffset = [&](const QPointF &offset) {
                candidate.label->setTransform(QTransform::fromTranslate(offset.x(), offset.y()));
                const QRectF labelBounds = deviceBounds(*candidate.label, sceneToDevice)
                                               .adjusted(-spacing, -spacing, spacing, spacing);
                if (!deviceViewport.contains(labelBounds)
                    || occupied.intersects(QRegion(labelBounds.toAlignedRect())))
                    return false;

                occupied += QRegion(labelBounds.toAlignedRect());
                return true;
            };

            // 视口变化后优先保留上一次的合法位置，避免仅因边界空间变宽就跳回
            // 排名更靠前的候选方向；当前位置失效时才重新执行避让。
            // 机场名称只在符号左右候选，避免对角偏移破坏名称与机场的视觉关联。
            if (std::find(offsets.begin(), offsets.begin() + offsetCount, currentOffset)
                != offsets.begin() + offsetCount)
                placed = tryOffset(currentOffset);
            for (size_t index = 0; index < offsetCount; ++index) {
                if (placed)
                    break;
                const QPointF &offset = offsets[index];
                if (offset == currentOffset)
                    continue;
                placed = tryOffset(offset);
            }
            if (!placed)
                candidate.label->setVisible(false);
            occupied += QRegion(candidate.persistentBounds.adjusted(
                -spacing, -spacing, spacing, spacing).toAlignedRect());
            continue;
        }

        if (candidate.pathItem && candidate.label) {
            constexpr std::array<qreal, 5> positions{0.5, 0.35, 0.65, 0.2, 0.8};
            bool placed{};
            for (const qreal position : positions) {
                candidate.pathItem->setAirwayLabelPosition(candidate.pathLabelIndex, position);
                if (!labelSuppressionArea.isEmpty()
                    && labelSuppressionArea.containsPoint(candidate.label->scenePos(), Qt::OddEvenFill))
                    continue;
                const QRectF labelBounds = deviceBounds(*candidate.label, sceneToDevice)
                                               .adjusted(-spacing, -spacing, spacing, spacing);
                const QRegion labelRegion(labelBounds.toAlignedRect());
                if (!deviceViewport.contains(labelBounds)
                    || occupied.intersects(labelRegion)
                    || airwayArrowRegion.intersects(labelRegion))
                    continue;
                occupied += labelRegion;
                placed = true;
                break;
            }
            if (!placed)
                candidate.label->setVisible(false);
            continue;
        }

        const QRectF paddedBounds = candidate.bounds.adjusted(-spacing, -spacing, spacing, spacing);
        if (!paddedBounds.isValid() || !paddedBounds.intersects(deviceViewport))
            continue;

        const QRegion candidateRegion(paddedBounds.toAlignedRect());
        if (occupied.intersects(candidateRegion)) {
            if (candidate.label)
                candidate.label->setVisible(false);
            if (candidate.persistentBounds.isValid())
                occupied += QRegion(candidate.persistentBounds.adjusted(
                    -spacing, -spacing, spacing, spacing).toAlignedRect());
            continue;
        }
        occupied += candidateRegion;
    }
}

void MapItemManage::applyZoomPolicy () {
    for (const auto &item : cachedItems) {
        if (auto *pathItem = dynamic_cast<MapPathItem*>(item.get())) {
            pathItem->setLabelsVisible(currentZoomLevel < 2);
        } else if (auto *pointItem = dynamic_cast<MapPointItem*>(item.get())) {
            const MapItemType type = pointItem->itemType();
            if (type == MapItemType::mora) {
                const bool visible = currentZoomLevel < 3;
                pointItem->setVisible(visible);
                pointItem->setLabelsVisible(visible);
            } else {
                pointItem->setVisible(true);
                pointItem->setLabelsVisible(currentZoomLevel < 2 || type == MapItemType::airport);
            }
        }
    }
}

QPainterPath MapItemManage::symbolForData (const MapItemData &data) const {
    if (const auto *navaid = std::get_if<MapNavData>(&data)) {
        const auto symbolIndex = static_cast<size_t>(navaid->navType);
        if (symbolIndex < navaidSymbols.size())
            return navaidSymbols[symbolIndex];
        return {};
    }

    if (dataType(data) == MapItemType::fix)
        return fixSymbolPath;

    QPainterPath symbol = airportSymbolPath;
    if (const auto *airport = std::get_if<MapApData>(&data); airport && std::isfinite(airport->geo)) {
        QTransform rotation;
        rotation.rotate(airport->geo);
        symbol = rotation.map(symbol);
    }
    return symbol;
}
