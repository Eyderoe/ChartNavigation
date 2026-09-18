#include "mapItemManage.hpp"

#include "mapItemTemplate.hpp"

#include <QColor>
#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iterator>
#include <numbers>
#include <optional>
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
constexpr int moraLastZoomLevel{2};
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
            pen.setColor(QColor(80, 200, 255));
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
    if (type == MapItemType::mora)
        return QBrush(QColor(90, 90, 90, 20));
    return Qt::NoBrush;
}

qreal defaultZValue (const MapItemType type) {
    switch (type) {
        case MapItemType::fir:
            return -30.0;
        case MapItemType::mora:
            return -20.0;
        case MapItemType::awy:
            return -10.0;
        default:
            return 0.0;
    }
}

void configureLabel (QGraphicsSimpleTextItem &labelItem, const QPointF &anchor) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    labelItem.setBrush(QColor(225, 225, 225));
    const QRectF labelRect = labelItem.boundingRect();
    labelItem.setPos(anchor + QPointF(5.0, -labelRect.height() / 2.0));
}

void configureCenteredLabel (QGraphicsSimpleTextItem &labelItem, const QPointF &anchor) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    labelItem.setBrush(QColor(225, 225, 225));
    labelItem.setPos(anchor - labelItem.boundingRect().center());
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

void configureAirwayLabel (QGraphicsSimpleTextItem &labelItem, const QLineF &segment) {
    labelItem.setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
    labelItem.setBrush(QColor(225, 225, 225));
    qreal angle = std::atan2(segment.dy(), segment.dx()) * 180.0 / std::numbers::pi;
    if (angle > 90.0)
        angle -= 180.0;
    else if (angle <= -90.0)
        angle += 180.0;

    const QRectF labelRect = labelItem.boundingRect();
    // ItemIgnoresTransformations 下 pos 仍是地图坐标，不能在这里减去像素尺寸。
    // 把地图锚点放在线段中点，再用 item 自身变换按屏幕像素居中文字。
    labelItem.setPos(segment.pointAt(0.5));
    labelItem.setTransformOriginPoint(QPointF{});
    labelItem.setTransform(QTransform::fromTranslate(-labelRect.center().x(), -labelRect.center().y()));
    labelItem.setRotation(angle);
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

std::vector<ProjectedPathSegment> uniqueFirSegments (const std::vector<ProjectedPathSegment> &segments) {
    std::vector<ProjectedPathSegment> unique;
    unique.reserve(segments.size());
    std::unordered_set<ProjectedSegmentKey, ProjectedSegmentKeyHash> seen;
    seen.reserve(segments.size());
    for (const auto &segment : segments) {
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

    if (isNdb(this->data)) {
        itemPen.setColor(QColor(QStringLiteral("#800000")));
        configureNdbPen(itemPen);
    }

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

    // 扩大可视边界作为缓存缓冲区，并让查询与新投影使用同一经纬度范围。
    const Rect2D newItemBound = expandForItems(viewport);
    DynamicLCC newProjection;
    newProjection.reset(newItemBound.first.second, newItemBound.second.second,
                        newItemBound.second.first, newItemBound.first.first);
    const auto mapData = query.queryMapItemData(newItemBound).first;

    // 在临时容器中构造下一代缓存，全部成功后再替换当前有效状态。
    std::vector<std::unique_ptr<QGraphicsItem>> newItems;
    newItems.reserve(mapData.size());
    std::vector<ProjectedPathSegment> airwaySegments;
    std::vector<ProjectedPathSegment> firSegments;
    for (const auto &data : mapData) {
        // 当前缩放层级不显示 MORA 时，重建阶段直接跳过对应图元。
        if (currentZoomLevel > moraLastZoomLevel && std::holds_alternative<MapMoraData>(data))
            continue;
        // 航路与 FIR 先保存投影线段，循环结束后分别合并为较少的路径图元。
        if (const auto *airway = std::get_if<MapAwyData>(&data)) {
            const auto points = newProjection.trans({airway->p1, airway->p2});
            if (points.size() != 2 || !finitePoint(points[0]) || !finitePoint(points[1]))
                continue;
            airwaySegments.push_back({data, toQPoint(points[0]), toQPoint(points[1])});
        } else if (const auto *fir = std::get_if<MapFirData>(&data)) {
            const auto points = newProjection.trans({fir->p1, fir->p2});
            if (points.size() != 2 || !finitePoint(points[0]) || !finitePoint(points[1]))
                continue;
            firSegments.push_back({data, toQPoint(points[0]), toQPoint(points[1])});
        } else if (const auto *mora = std::get_if<MapMoraData>(&data)) {
            // MORA 使用随地图缩放的投影边框；其余点元素只投影锚点，符号保持屏幕尺寸。
            const auto path = moraFrame(newProjection, mora->bounds);
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
            const auto positions = newProjection.trans({realPosition});
            if (positions.size() != 1 || !finitePoint(positions.front()))
                continue;
            auto item = std::make_unique<MapPointItem>(data, symbolForData(data), true);
            item->setPos(toQPoint(positions.front()));
            newItems.emplace_back(std::move(item));
        }
    }

    const auto appendPathItem = [this, &newItems](std::vector<ProjectedPathSegment> segments) {
        if (segments.empty())
            return;
        const bool isFir = dataType(segments.front().data) == MapItemType::fir;
        // FIR 仅对绘制几何去重，原始线段仍用于保留数据关联和各段标签位置。
        const std::vector<ProjectedPathSegment> geometrySegments = isFir ? uniqueFirSegments(segments) : segments;
        std::vector<MapItemData> sourceData;
        std::vector<QPointF> labelAnchors;
        std::vector<QLineF> labelSegments;
        sourceData.reserve(segments.size());
        labelAnchors.reserve(segments.size());
        labelSegments.reserve(segments.size());
        for (const auto &segment : segments) {
            sourceData.emplace_back(segment.data);
            labelAnchors.emplace_back((segment.first.x() + segment.second.x()) / 2.0,
                                      (segment.first.y() + segment.second.y()) / 2.0);
            labelSegments.emplace_back(segment.first, segment.second);
        }
        auto item = std::make_unique<MapPathItem>(
            std::move(sourceData), combineSegments(geometrySegments), std::move(labelAnchors));
        if (item->itemType() == MapItemType::awy)
            item->setAirwayLabelSegments(labelSegments);
        newItems.emplace_back(std::move(item));
    };
    appendPathItem(std::move(firSegments));
    appendPathItem(std::move(airwaySegments));

    // 所有数据转换完成后一次性提交投影、边界和图元，保证缓存属于同一代。
    projection = std::move(newProjection);
    cachedItemBound = newItemBound;
    cachedProjectedBound = projectedRect(projection, cachedItemBound);
    cachedItems = std::move(newItems);
    cacheValid = true;
    applyZoomPolicy();
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

void MapItemManage::applyZoomPolicy () {
    for (const auto &item : cachedItems) {
        if (auto *pathItem = dynamic_cast<MapPathItem*>(item.get())) {
            pathItem->setLabelsVisible(currentZoomLevel < 2);
        } else if (auto *pointItem = dynamic_cast<MapPointItem*>(item.get())) {
            const MapItemType type = pointItem->itemType();
            if (type == MapItemType::mora) {
                const bool visible = currentZoomLevel <= moraLastZoomLevel;
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
