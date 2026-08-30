#include "mapItemManage.hpp"

#include <QColor>
#include <QGraphicsSimpleTextItem>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>

#include <algorithm>
#include <cmath>
#include <deque>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>


QPainterPath airportSymbol () {
    QPainterPath path;
    path.addEllipse(QPointF{}, 6.0, 6.0);
    path.moveTo(0.0, -4.0);
    path.lineTo(0.0, 4.0);
    return path;
}

QPainterPath fixSymbol () {
    QPainterPath path;
    path.addPolygon(QPolygonF{
        QPointF{0.0, -5.0}, QPointF{5.0, 4.0}, QPointF{-5.0, 4.0}, QPointF{0.0, -5.0}
    });
    return path;
}

QPainterPath vorSymbol () {
    QPainterPath path;
    path.addPolygon(QPolygonF{
        QPointF{-3.0, -6.0}, QPointF{3.0, -6.0}, QPointF{6.0, 0.0},
        QPointF{3.0, 6.0}, QPointF{-3.0, 6.0}, QPointF{-6.0, 0.0}, QPointF{-3.0, -6.0}
    });
    path.addEllipse(QPointF{}, 1.0, 1.0);
    return path;
}

QPainterPath dmeSymbol () {
    QPainterPath path;
    path.addRect(-5.0, -5.0, 10.0, 10.0);
    path.addEllipse(QPointF{}, 1.0, 1.0);
    return path;
}

QPainterPath vordmeSymbol () {
    QPainterPath path = dmeSymbol();
    path.addPath(vorSymbol());
    return path;
}

QPainterPath ndbSymbol () {
    QPainterPath path;
    path.addEllipse(QPointF{}, 6.0, 6.0);
    path.addEllipse(QPointF{}, 4.0, 4.0);
    path.addEllipse(QPointF{}, 1.0, 1.0);
    return path;
}

QPainterPath moraSymbol () {
    QPainterPath path;
    path.addRect(-8.0, -6.0, 16.0, 12.0);
    return path;
}

QPainterPath firSymbol () {
    QPainterPath path;
    path.moveTo(-8.0, 3.0);
    path.lineTo(-3.0, -2.0);
    path.lineTo(2.0, 2.0);
    path.lineTo(8.0, -3.0);
    return path;
}

QPainterPath awySymbol () {
    QPainterPath path;
    path.moveTo(-8.0, 0.0);
    path.lineTo(8.0, 0.0);
    return path;
}

namespace {

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
    if (getLongiRange(topLeft.second, bottomRight.second) >= 360.0) {
        left = -180.0;
        right = 180.0;
    }
    return {
        .top = std::clamp(std::max(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
        .bottom = std::clamp(std::min(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
        .left = left,
        .right = right,
        .valid = true
    };
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

    const double longitudeSpan = std::min(360.0, getLongiRange(viewport.left, viewport.right) * 2.0);
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
            return QString::number(item.alt);
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
    pen.setWidthF(type == MapItemType::fir ? 1.5 : 1.0);
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
        case MapItemType::mora:
            return -30.0;
        case MapItemType::fir:
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

    bool operator== (const ProjectedPointKey&) const = default;
};

struct ProjectedPointKeyHash {
    size_t operator() (const ProjectedPointKey &key) const noexcept {
        const size_t first = std::hash<std::int64_t>{}(key.x);
        const size_t second = std::hash<std::int64_t>{}(key.y);
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

QPainterPath combineSegments (const std::vector<ProjectedPathSegment> &segments) {
    QPainterPath path;
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

} // namespace


MapPathItem::MapPathItem (MapItemData data, const QPainterPath &path, QGraphicsItem *parent) :
    MapPathItem(std::vector<MapItemData>{std::move(data)}, path,
                std::vector<QPointF>{path.isEmpty() ? QPointF{} : path.pointAtPercent(0.5)}, parent) {}

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
    setBrush(Qt::NoBrush);
    setZValue(defaultZValue(itemType()));

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
    }
}

const MapItemData& MapPathItem::mapData () const noexcept {
    return dataItems.front();
}

const std::vector<MapItemData>& MapPathItem::mapDataItems () const noexcept {
    return dataItems;
}

const MapItemData* MapPathItem::findData (const MapItemType type, const int id) const noexcept {
    const auto found = std::ranges::find_if(dataItems, [type, id](const MapItemData &item) {
        return dataType(item) == type && dataId(item) == id;
    });
    return found == dataItems.end() ? nullptr : &*found;
}

QRectF MapPathItem::boundingRect () const {
    return QGraphicsPathItem::boundingRect().united(shape().boundingRect());
}

QPainterPath MapPathItem::shape () const {
    if (path().isEmpty())
        return {};
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(pen().widthF(), 6.0));
    return path().united(stroker.createStroke(path()));
}

MapItemType MapPathItem::itemType () const noexcept {
    return dataType(dataItems.front());
}

int MapPathItem::itemId () const noexcept {
    return dataId(dataItems.front());
}

QString MapPathItem::label () const {
    return dataLabel(dataItems.front());
}

void MapPathItem::setDetail (const MapItemDetail detail) {
    currentDetail = detail;
    for (auto *labelItem : labelItems)
        labelItem->setVisible(detail == MapItemDetail::full);
}

MapItemDetail MapPathItem::detail () const noexcept {
    return currentDetail;
}

void MapPathItem::setLabelColor (const QColor &color) {
    for (auto *labelItem : labelItems)
        labelItem->setBrush(color);
}

void MapPathItem::setPath (const QPainterPath &path) {
    QGraphicsPathItem::setPath(path);
    if (labelItems.size() == 1)
        configureLabel(*labelItems.front(), path.isEmpty() ? QPointF{} : path.pointAtPercent(0.5));
}


MapPointItem::MapPointItem (MapItemData data, QPainterPath symbol, const bool screenFixed, QGraphicsItem *parent) :
    QGraphicsItem(parent), data(std::move(data)), symbolPath(std::move(symbol)),
    itemPen(defaultPen(dataType(this->data))), itemBrush(defaultBrush(dataType(this->data))) {
    if (!isPointData(this->data) || !dataTypeMatchesVariant(this->data))
        throw std::invalid_argument("MapPointItem requires airport, fix, navaid or MORA data");

    setFlag(ItemIsSelectable, true);
    setFlag(ItemIgnoresTransformations, screenFixed);
    setZValue(defaultZValue(itemType()));
    labelItem = new QGraphicsSimpleTextItem(label(), this);
    configureLabel(*labelItem, symbolPath.boundingRect().center());
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

void MapPointItem::paint (QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*) {
    painter->setPen(itemPen);
    painter->setBrush(itemBrush);
    painter->drawPath(symbolPath);
}

const MapItemData& MapPointItem::mapData () const noexcept {
    return data;
}

MapItemType MapPointItem::itemType () const noexcept {
    return dataType(data);
}

int MapPointItem::itemId () const noexcept {
    return dataId(data);
}

QString MapPointItem::label () const {
    return dataLabel(data);
}

void MapPointItem::setSymbol (const QPainterPath &symbol) {
    prepareGeometryChange();
    symbolPath = symbol;
    configureLabel(*labelItem, symbolPath.boundingRect().center());
    update();
}

const QPainterPath& MapPointItem::symbol () const noexcept {
    return symbolPath;
}

void MapPointItem::setPen (const QPen &pen) {
    prepareGeometryChange();
    itemPen = pen;
    update();
}

QPen MapPointItem::pen () const {
    return itemPen;
}

void MapPointItem::setBrush (const QBrush &brush) {
    itemBrush = brush;
    update();
}

QBrush MapPointItem::brush () const {
    return itemBrush;
}

void MapPointItem::setDetail (const MapItemDetail detail) {
    currentDetail = detail;
    labelItem->setVisible(detail == MapItemDetail::full);
}

MapItemDetail MapPointItem::detail () const noexcept {
    return currentDetail;
}

void MapPointItem::setLabelColor (const QColor &color) {
    labelItem->setBrush(color);
}


MapItemManage::MapItemManage (const QString &databaseFilePath, DataProvider *provider) :
    dataProvider(provider), query(databaseFilePath) {
    itemSymbols = {
        airportSymbol(), awySymbol(), firSymbol(), fixSymbol(), moraSymbol(), vorSymbol()
    };
    navaidSymbols = {vorSymbol(), dmeSymbol(), vordmeSymbol(), ndbSymbol()};
}

MapItemManage::MapItemManage (DataProvider *provider, const QString &databaseFilePath) :
    MapItemManage(databaseFilePath, provider) {}

bool MapItemManage::updateViewport (const Rect2D &viewportBound) {
    const NormalizedBound viewport = normalizeBound(viewportBound);
    if (!viewport.valid)
        return false;
    if (cacheValid && contains(normalizeBound(cachedItemBound), viewport))
        return false;

    const Rect2D newItemBound = expandForItems(viewport);
    DynamicLCC newProjection;
    newProjection.reset(newItemBound.first.second, newItemBound.second.second,
                        newItemBound.second.first, newItemBound.first.first);
    const auto mapData = query.queryMapItemData(newItemBound);

    std::vector<std::unique_ptr<QGraphicsItem>> newItems;
    newItems.reserve(mapData.size());
    std::vector<ProjectedPathSegment> airwaySegments;
    std::vector<ProjectedPathSegment> firSegments;
    for (const auto &data : mapData) {
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
            const auto &[topLeft, bottomRight] = mora->bounds;
            const auto points = newProjection.trans({
                topLeft,
                {topLeft.first, bottomRight.second},
                bottomRight,
                {bottomRight.first, topLeft.second}
            });
            if (points.size() != 4 || !std::ranges::all_of(points, finitePoint))
                continue;
            QPainterPath path(toQPoint(points[0]));
            for (size_t index = 1; index < points.size(); ++index)
                path.lineTo(toQPoint(points[index]));
            path.closeSubpath();
            auto item = std::make_unique<MapPointItem>(data, path, false);
            item->setDetail(currentDetail);
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
            auto item = std::make_unique<MapPointItem>(
                data, itemSymbols[static_cast<size_t>(dataType(data))], true);
            item->setPos(toQPoint(positions.front()));
            applyCurrentSymbol(*item);
            item->setDetail(currentDetail);
            newItems.emplace_back(std::move(item));
        }
    }

    const auto appendPathItem = [this, &newItems](std::vector<ProjectedPathSegment> segments) {
        if (segments.empty())
            return;
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
            std::move(sourceData), combineSegments(segments), std::move(labelAnchors));
        item->setDetail(currentDetail);
        newItems.emplace_back(std::move(item));
    };
    appendPathItem(std::move(firSegments));
    appendPathItem(std::move(airwaySegments));

    projection = std::move(newProjection);
    cachedItemBound = newItemBound;
    cachedProjectedBound = projectedRect(projection, cachedItemBound);
    cachedItems = std::move(newItems);
    cacheValid = true;
    rebuildIndex();
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

void MapItemManage::clear () noexcept {
    itemIndex.clear();
    cachedItems.clear();
    cachedItemBound = {};
    cachedProjectedBound = {};
    projection = DynamicLCC{};
    cacheValid = false;
}

bool MapItemManage::hasCache () const noexcept {
    return cacheValid;
}

const Rect2D& MapItemManage::itemBound () const noexcept {
    return cachedItemBound;
}

const QRectF& MapItemManage::projectedBound () const noexcept {
    return cachedProjectedBound;
}

const std::vector<std::unique_ptr<QGraphicsItem>>& MapItemManage::items () const noexcept {
    return cachedItems;
}

QGraphicsItem* MapItemManage::findItem (const MapItemType type, const int id) const noexcept {
    const auto found = itemIndex.find(indexKey(type, id));
    return found == itemIndex.end() ? nullptr : found->second;
}

const MapItemData* MapItemManage::findData (const MapItemType type, const int id) const noexcept {
    QGraphicsItem *item = findItem(type, id);
    if (const auto *pathItem = dynamic_cast<MapPathItem*>(item))
        return pathItem->findData(type, id);
    if (const auto *pointItem = dynamic_cast<MapPointItem*>(item))
        return &pointItem->mapData();
    return nullptr;
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

std::vector<Point2D> MapItemManage::project (std::vector<Point2D> positions) const {
    return projection.trans(std::move(positions));
}

void MapItemManage::setDetail (const MapItemDetail detail) {
    currentDetail = detail;
    for (const auto &item : cachedItems) {
        if (auto *pathItem = dynamic_cast<MapPathItem*>(item.get()))
            pathItem->setDetail(detail);
        else if (auto *pointItem = dynamic_cast<MapPointItem*>(item.get()))
            pointItem->setDetail(detail);
    }
}

MapItemDetail MapItemManage::detail () const noexcept {
    return currentDetail;
}

void MapItemManage::setSymbol (const MapItemType type, const QPainterPath &symbol) {
    const size_t symbolIndex = static_cast<size_t>(type);
    if (symbolIndex >= itemSymbols.size())
        throw std::invalid_argument("invalid map item type");
    itemSymbols[symbolIndex] = symbol;
    if (type == MapItemType::navaid)
        std::ranges::fill(navaidSymbols, symbol);
    for (const auto &item : cachedItems) {
        auto *pointItem = dynamic_cast<MapPointItem*>(item.get());
        if (pointItem && pointItem->itemType() == type)
            applyCurrentSymbol(*pointItem);
    }
}

void MapItemManage::setNavaidSymbol (const NavaidType type, const QPainterPath &symbol) {
    const size_t symbolIndex = static_cast<size_t>(type);
    if (symbolIndex >= navaidSymbols.size())
        throw std::invalid_argument("invalid navaid type");
    navaidSymbols[symbolIndex] = symbol;
    for (const auto &item : cachedItems) {
        auto *pointItem = dynamic_cast<MapPointItem*>(item.get());
        if (!pointItem || pointItem->itemType() != MapItemType::navaid)
            continue;
        const auto *navaid = std::get_if<MapNavData>(&pointItem->mapData());
        if (navaid && navaid->navType == type)
            pointItem->setSymbol(symbol);
    }
}

void MapItemManage::setDataProvider (DataProvider *provider) noexcept {
    dataProvider = provider;
}

DataProvider* MapItemManage::getDataProvider () const noexcept {
    return dataProvider;
}

MapItemManage::ItemIndexKey MapItemManage::indexKey (const MapItemType type, const int id) noexcept {
    return (static_cast<ItemIndexKey>(static_cast<unsigned int>(type)) << 32)
           | static_cast<std::uint32_t>(id);
}

void MapItemManage::rebuildIndex () {
    itemIndex.clear();
    size_t indexSize{};
    for (const auto &item : cachedItems) {
        if (const auto *pathItem = dynamic_cast<MapPathItem*>(item.get()))
            indexSize += pathItem->mapDataItems().size();
        else
            ++indexSize;
    }
    itemIndex.reserve(indexSize);
    for (const auto &item : cachedItems) {
        if (const auto *pathItem = dynamic_cast<MapPathItem*>(item.get())) {
            for (const auto &data : pathItem->mapDataItems())
                itemIndex.insert_or_assign(indexKey(dataType(data), dataId(data)), item.get());
        } else if (const auto *pointItem = dynamic_cast<MapPointItem*>(item.get()))
            itemIndex.insert_or_assign(indexKey(pointItem->itemType(), pointItem->itemId()), item.get());
    }
}

void MapItemManage::applyCurrentSymbol (MapPointItem &item) const {
    if (const auto *navaid = std::get_if<MapNavData>(&item.mapData())) {
        const size_t symbolIndex = static_cast<size_t>(navaid->navType);
        if (symbolIndex < navaidSymbols.size())
            item.setSymbol(navaidSymbols[symbolIndex]);
        return;
    }
    const size_t symbolIndex = static_cast<size_t>(item.itemType());
    if (item.itemType() != MapItemType::mora && symbolIndex < itemSymbols.size())
        item.setSymbol(itemSymbols[symbolIndex]);
}
