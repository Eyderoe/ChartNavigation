#include "mapCanvasManage.hpp"

#include <QFont>
#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPolygonF>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <numbers>
#include <ranges>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "utils/constValue.hpp"


namespace reserveCode {
namespace {

constexpr int moraLastZoomLevel{2};
constexpr qreal minimumHitWidth{8.0};

struct NormalizedBound {
    double top{};
    double bottom{};
    double left{};
    double right{};
    bool valid{false};
};

struct ItemStyle {
    QColor line;
    QColor label;
    QColor fill{Qt::transparent};
    qreal width{1.0};
};

struct SegmentKey {
    std::int64_t x1{};
    std::int64_t y1{};
    std::int64_t x2{};
    std::int64_t y2{};

    bool operator== (const SegmentKey&) const = default;
};

struct SegmentKeyHash {
    std::size_t operator() (const SegmentKey &key) const noexcept {
        std::size_t result{};
        for (const auto value : {key.x1, key.y1, key.x2, key.y2}) {
            const std::size_t next = std::hash<std::int64_t>{}(value);
            result ^= next + 0x9e3779b9U + (result << 6U) + (result >> 2U);
        }
        return result;
    }
};

NormalizedBound normalizeBound (const Rect2D &rect) {
    const auto &[topLeft, bottomRight] = rect;
    if (!allFinite(topLeft, bottomRight))
        return {};

    double left = normalizeLongitude(topLeft.second);
    double right = normalizeLongitude(bottomRight.second);
    if (getLongiSpan(topLeft.second, bottomRight.second) >= 360.0) {
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
        if (!std::ranges::any_of(outerRanges, [innerLeft, innerRight](const LongiRange &range) {
                return range.first <= innerLeft && range.second >= innerRight;
            }))
            return false;
    }
    return true;
}

Rect2D expandForCache (const NormalizedBound &viewport) {
    const double latitudeSpan = std::min(maxSupportLat * 2.0,
                                         (viewport.top - viewport.bottom) * 2.0);
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

    const double longitudeSpan = std::min(360.0,
                                          getLongiSpan(viewport.left, viewport.right) * 2.0);
    if (longitudeSpan >= 360.0)
        return {{top, -180.0}, {bottom, 180.0}};
    const double center = getLongiRangeCenter(viewport.left, viewport.right);
    return {
        {top, normalizeLongitude(center - longitudeSpan / 2.0)},
        {bottom, normalizeLongitude(center + longitudeSpan / 2.0)}
    };
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

bool finitePoint (const QPointF &point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QPointF toPoint (const Point2D &point) {
    return {point.first, point.second};
}

QRectF pointBounds (const std::vector<Point2D> &points) {
    QRectF result;
    bool initialized{};
    for (const Point2D &point : points) {
        if (!allFinite(point))
            continue;
        const QRectF value(toPoint(point), QSizeF{});
        if (!initialized) {
            result = value;
            initialized = true;
        } else {
            result |= value;
        }
    }
    return result.normalized();
}

ItemStyle styleFor (const MapItemType type, const bool dark) {
    if (dark) {
        switch (type) {
            case MapItemType::airport:
                return {QColor(80, 200, 255), QColor(225, 225, 225), Qt::transparent, 2.4};
            case MapItemType::awy:
                return {QColor(90, 150, 255), QColor(225, 225, 225)};
            case MapItemType::fir:
                return {QColor(QStringLiteral("#606080")), QColor(225, 225, 225), Qt::transparent, 5.0};
            case MapItemType::fix:
                return {QColor(230, 230, 230), QColor(225, 225, 225), Qt::transparent, 1.7};
            case MapItemType::mora:
                return {QColor(115, 115, 115), QColor(225, 225, 225), QColor(90, 90, 90, 20)};
            case MapItemType::navaid:
                return {QColor(210, 110, 230), QColor(225, 225, 225)};
        }
    }

    switch (type) {
        case MapItemType::airport:
            return {QColor(QStringLiteral("#006B8F")), QColor(QStringLiteral("#17212B")), Qt::transparent, 2.4};
        case MapItemType::awy:
            return {QColor(QStringLiteral("#245AA5")), QColor(QStringLiteral("#1D3F73")), Qt::transparent, 1.25};
        case MapItemType::fir:
            return {QColor(QStringLiteral("#55556F")), QColor(QStringLiteral("#303044")), Qt::transparent, 5.0};
        case MapItemType::fix:
            return {QColor(QStringLiteral("#263238")), QColor(QStringLiteral("#17212B")), Qt::transparent, 1.7};
        case MapItemType::mora:
            return {QColor(QStringLiteral("#747B83")), QColor(QStringLiteral("#454B52")), QColor(65, 85, 105, 12)};
        case MapItemType::navaid:
            return {QColor(QStringLiteral("#8A278F")), QColor(QStringLiteral("#671B6B")), Qt::transparent, 1.25};
    }
    return {};
}

QPainterPath airportSymbol () {
    QPainterPath path;
    path.addEllipse(QPointF{}, 6.0, 6.0);
    path.moveTo(0.0, -4.0);
    path.lineTo(0.0, 4.0);
    return path;
}

QPainterPath fixSymbol () {
    QPainterPath path;
    path.addPolygon(QPolygonF{{0.0, -5.0}, {5.0, 5.0}, {-5.0, 5.0}, {0.0, -5.0}});
    return path;
}

void addVorHexagon (QPainterPath &path) {
    path.addPolygon(QPolygonF{
        {-3.0, -6.0}, {3.0, -6.0}, {6.0, 0.0}, {3.0, 6.0},
        {-3.0, 6.0}, {-6.0, 0.0}, {-3.0, -6.0}
    });
}

QPainterPath navaidSymbol (const NavaidType type) {
    QPainterPath path;
    if (type == NavaidType::dme || type == NavaidType::vordme)
        path.addRect(-6.0, -6.0, 12.0, 12.0);
    if (type == NavaidType::vor || type == NavaidType::vordme)
        addVorHexagon(path);
    if (type == NavaidType::ndb) {
        path.addEllipse(QPointF{}, 6.0, 6.0);
        path.addEllipse(QPointF{}, 4.0, 4.0);
    }
    return path;
}

QPainterPath translatedSymbol (const QPainterPath &symbol, const QPointF &position,
                               const qreal rotation = 0.0) {
    QTransform transform;
    transform.translate(position.x(), position.y());
    transform.rotate(rotation);
    return transform.map(symbol);
}

QPainterPath lineHitPath (const QLineF &line, const qreal width = minimumHitWidth) {
    QPainterPath path(line.p1());
    path.lineTo(line.p2());
    QPainterPathStroker stroker;
    stroker.setWidth(width);
    return stroker.createStroke(path);
}

SegmentKey segmentKey (const QLineF &line) {
    constexpr qreal precision{10.0};
    std::array<std::pair<std::int64_t, std::int64_t>, 2> points{
        std::pair{std::llround(line.x1() * precision), std::llround(line.y1() * precision)},
        std::pair{std::llround(line.x2() * precision), std::llround(line.y2() * precision)}
    };
    if (points[1] < points[0])
        std::swap(points[0], points[1]);
    return {points[0].first, points[0].second, points[1].first, points[1].second};
}

QPolygonF airwayArrow (const QLineF &segment, const char direct, const qreal lineWidth) {
    if ((direct != 'F' && direct != 'R') || segment.length() <= 1.0e-9)
        return {};
    QPointF direction = segment.p2() - segment.p1();
    if (direct == 'R')
        direction = -direction;
    direction /= segment.length();
    const QPointF right{-direction.y(), direction.x()};
    const QPointF center = segment.center();
    const qreal size = 2.5 * std::max(lineWidth, 1.0);
    return {
        center + direction * size,
        center + right * size - direction * size,
        center - direction * size / 2.0,
        center - right * size - direction * size
    };
}

} // namespace


MapCanvasManage::MapCanvasManage (const QString &databaseFilePath) : query(databaseFilePath) {}

bool MapCanvasManage::updateViewport (const Rect2D &viewportBound) {
    const NormalizedBound viewport = normalizeBound(viewportBound);
    if (!viewport.valid)
        return false;
    if (cacheValid && contains(normalizeBound(cachedItemBound), viewport))
        return false;

    const Rect2D newBound = expandForCache(viewport);
    DynamicLCC newProjection;
    newProjection.reset(newBound.first.second, newBound.second.second,
                        newBound.second.first, newBound.first.first);
    auto sourceData = query.queryMapItemData(newBound).first;
    std::vector<ProjectedItem> newItems;
    newItems.reserve(sourceData.size());

    for (auto &data : sourceData) {
        std::vector<Point2D> geographicGeometry = std::visit([](const auto &item) {
            using T = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<T, MapApData> || std::is_same_v<T, MapNavData>) {
                return std::vector<Point2D>{item.realPos};
            } else if constexpr (std::is_same_v<T, MapAwyData> || std::is_same_v<T, MapFirData>) {
                return std::vector<Point2D>{item.p1, item.p2};
            } else {
                const auto &[topLeft, bottomRight] = item.bounds;
                return std::vector<Point2D>{
                    topLeft,
                    {topLeft.first, bottomRight.second},
                    bottomRight,
                    {bottomRight.first, topLeft.second}
                };
            }
        }, data);
        const auto projected = newProjection.trans(std::move(geographicGeometry));
        if (projected.empty() || !std::ranges::all_of(projected, [](const Point2D &point) {
                return allFinite(point);
            }))
            continue;

        std::vector<QPointF> geometry;
        geometry.reserve(projected.size());
        std::ranges::transform(projected, std::back_inserter(geometry), toPoint);
        newItems.push_back({std::move(data), std::move(geometry)});
    }

    projection = std::move(newProjection);
    cachedItemBound = newBound;
    projectedItems = std::move(newItems);
    cacheValid = true;
    rebuildIndex();
    lastHitRegions.clear();
    return true;
}

bool MapCanvasManage::refresh (const Rect2D &viewportBound) {
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

void MapCanvasManage::clear () noexcept {
    projection = DynamicLCC{};
    cachedItemBound = {};
    cacheValid = false;
    projectedItems.clear();
    itemIndex.clear();
    lastHitRegions.clear();
}

bool MapCanvasManage::render (QPainter &painter, const QRectF &canvasRect,
                              const Rect2D &viewportBound) {
    const bool rebuilt = updateViewport(viewportBound);
    paint(painter, canvasRect, viewportBound);
    return rebuilt;
}

void MapCanvasManage::paint (QPainter &painter, const QRectF &canvasRect,
                             const Rect2D &viewportBound) {
    const NormalizedBound viewport = normalizeBound(viewportBound);
    if (!cacheValid || !viewport.valid || !contains(normalizeBound(cachedItemBound), viewport)
        || !canvasRect.isValid() || canvasRect.isEmpty()) {
        lastHitRegions.clear();
        return;
    }

    const QTransform toCanvas = canvasTransform(canvasRect, viewportBound);
    if (!toCanvas.isInvertible()) {
        lastHitRegions.clear();
        return;
    }

    lastHitRegions.clear();
    std::vector<QRectF> occupiedLabels;
    std::unordered_set<SegmentKey, SegmentKeyHash> paintedFirSegments;

    painter.save();
    painter.setClipRect(canvasRect);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const auto appendHit = [this](const std::size_t itemIndex, QPainterPath path) {
        lastHitRegions.push_back({itemIndex, std::move(path)});
    };
    const auto labelFits = [&occupiedLabels, &canvasRect](const QRectF &rect) {
        if (!canvasRect.intersects(rect))
            return false;
        if (std::ranges::any_of(occupiedLabels, [&rect](const QRectF &occupied) {
                return occupied.adjusted(-2.0, -1.0, 2.0, 1.0).intersects(rect);
            }))
            return false;
        occupiedLabels.emplace_back(rect);
        return true;
    };
    const auto drawLabel = [&](const std::size_t itemIndex, const QString &text,
                               const QPointF &anchor, const QColor &color,
                               const bool centered = false, const bool avoidCollision = true) {
        if (!showLabels || text.isEmpty())
            return;
        const QFontMetricsF metrics(painter.font());
        QRectF rect = metrics.boundingRect(text);
        rect.moveTopLeft(centered ? anchor - rect.center()
                                  : anchor + QPointF{5.0, -rect.height() / 2.0});
        if (avoidCollision ? !labelFits(rect) : !canvasRect.intersects(rect))
            return;
        painter.setPen(color);
        painter.drawText(rect.topLeft() + QPointF{0.0, metrics.ascent()}, text);
        QPainterPath hit;
        hit.addRect(rect);
        appendHit(itemIndex, std::move(hit));
    };
    const auto drawAirwayLabel = [&](const std::size_t itemIndex, const QString &text,
                                     const QLineF &line, const QColor &color) {
        if (!showLabels || currentZoomLevel >= 2 || text.isEmpty() || line.length() <= 1.0e-9)
            return;
        const QFontMetricsF metrics(painter.font());
        QRectF localRect = metrics.boundingRect(text);
        localRect.moveCenter(QPointF{});
        qreal angle = -std::atan2(line.dy(), line.dx()) * 180.0 / std::numbers::pi;
        if (angle > 90.0)
            angle -= 180.0;
        else if (angle <= -90.0)
            angle += 180.0;
        QTransform labelTransform;
        labelTransform.translate(line.center().x(), line.center().y());
        labelTransform.rotate(angle);
        const QRectF canvasBounds = labelTransform.mapRect(localRect);
        if (!labelFits(canvasBounds))
            return;
        painter.save();
        painter.setTransform(labelTransform, true);
        painter.setPen(color);
        painter.drawText(localRect.topLeft() + QPointF{0.0, metrics.ascent()}, text);
        painter.restore();
        QPainterPath hit;
        hit.addRect(canvasBounds);
        appendHit(itemIndex, std::move(hit));
    };

    // 面图层：MORA 网格始终留在缓存中，只由缩放策略决定本帧是否绘制。
    if (currentZoomLevel <= moraLastZoomLevel) {
        for (std::size_t index = 0; index < projectedItems.size(); ++index) {
            const ProjectedItem &item = projectedItems[index];
            if (dataType(item.data) != MapItemType::mora || item.geometry.size() != 4)
                continue;
            QPolygonF polygon;
            for (const QPointF &point : item.geometry)
                polygon.emplace_back(toCanvas.map(point));
            const ItemStyle style = styleFor(MapItemType::mora, useDarkTheme);
            painter.setPen(QPen(style.line, style.width));
            painter.setBrush(style.fill);
            painter.drawPolygon(polygon);
            QPainterPath hit;
            hit.addPolygon(polygon);
            hit.closeSubpath();
            appendHit(index, hit);

            if (showLabels) {
                QFont font = painter.font();
                const QRectF bounds = polygon.boundingRect();
                font.setPixelSize(std::max(8, static_cast<int>(std::lround(bounds.height() * 0.20))));
                font.setWeight(QFont::DemiBold);
                painter.save();
                painter.setFont(font);
                painter.setOpacity(0.35);
                drawLabel(index, dataLabel(item.data), bounds.center(), style.label, true, false);
                painter.restore();
            }
        }
    }

    // 线图层：不创建组合图元，每个数据库段都在当前帧直接绘制。
    for (const MapItemType layer : {MapItemType::fir, MapItemType::awy}) {
        for (std::size_t index = 0; index < projectedItems.size(); ++index) {
            const ProjectedItem &item = projectedItems[index];
            if (dataType(item.data) != layer || item.geometry.size() != 2)
                continue;
            const QLineF line(toCanvas.map(item.geometry[0]), toCanvas.map(item.geometry[1]));
            if (!finitePoint(line.p1()) || !finitePoint(line.p2()))
                continue;
            if (layer == MapItemType::fir && !paintedFirSegments.emplace(segmentKey(line)).second)
                continue;

            const ItemStyle style = styleFor(layer, useDarkTheme);
            QPen pen(style.line, style.width);
            if (layer == MapItemType::fir) {
                pen.setCapStyle(Qt::FlatCap);
                pen.setDashPattern({2.0, 2.0});
            }
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(line);
            appendHit(index, lineHitPath(line));

            if (layer == MapItemType::awy) {
                const auto &airway = std::get<MapAwyData>(item.data);
                const QPolygonF arrow = airwayArrow(line, airway.direct, style.width);
                if (!arrow.isEmpty())
                    painter.drawPolygon(arrow);
                drawAirwayLabel(index, airway.ident, line, style.label);
            }
        }
    }

    // 点图层最后绘制，命中测试按相反顺序检查，因此点会优先于底层线和网格。
    // 点标签优先级高于航路标签，仅在点标签之间执行避让。
    occupiedLabels.clear();
    for (std::size_t index = 0; index < projectedItems.size(); ++index) {
        const ProjectedItem &item = projectedItems[index];
        const MapItemType type = dataType(item.data);
        if ((type != MapItemType::airport && type != MapItemType::fix && type != MapItemType::navaid)
            || item.geometry.size() != 1)
            continue;
        const QPointF position = toCanvas.map(item.geometry.front());
        const ItemStyle style = styleFor(type, useDarkTheme);
        QPainterPath symbol;
        QColor lineColor = style.line;
        qreal rotation{};
        if (const auto *airport = std::get_if<MapApData>(&item.data)) {
            symbol = airportSymbol();
            rotation = std::isfinite(airport->geo) ? airport->geo : 0.0;
        } else if (type == MapItemType::fix) {
            symbol = fixSymbol();
        } else if (const auto *navaid = std::get_if<MapNavData>(&item.data)) {
            symbol = navaidSymbol(navaid->navType);
            if (navaid->navType == NavaidType::ndb)
                lineColor = useDarkTheme ? QColor(QStringLiteral("#F08080"))
                                         : QColor(QStringLiteral("#800000"));
        }
        const QPainterPath canvasSymbol = translatedSymbol(symbol, position, rotation);
        QPen pen(lineColor, style.width);
        if (const auto *navaid = std::get_if<MapNavData>(&item.data);
            navaid && navaid->navType == NavaidType::ndb) {
            pen.setStyle(Qt::CustomDashLine);
            pen.setDashPattern({2.0, 2.0});
        }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(canvasSymbol);
        if (type == MapItemType::navaid) {
            painter.save();
            QPen centerPen(lineColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(centerPen);
            painter.drawPoint(position);
            painter.restore();
        }
        QPainterPathStroker stroker;
        stroker.setWidth(minimumHitWidth);
        appendHit(index, canvasSymbol.united(stroker.createStroke(canvasSymbol)));

        const bool labelAllowed = type == MapItemType::airport || currentZoomLevel < 2;
        if (labelAllowed)
            drawLabel(index, dataLabel(item.data), position,
                      type == MapItemType::navaid ? lineColor : style.label);
    }

    painter.restore();
}

void MapCanvasManage::setZoomLevel (const int level) noexcept {
    currentZoomLevel = std::clamp(level, 0, 3);
}

int MapCanvasManage::zoomLevel () const noexcept {
    return currentZoomLevel;
}

void MapCanvasManage::setLabelsVisible (const bool visible) noexcept {
    showLabels = visible;
}

bool MapCanvasManage::labelsVisible () const noexcept {
    return showLabels;
}

void MapCanvasManage::setDarkTheme (const bool dark) noexcept {
    useDarkTheme = dark;
}

bool MapCanvasManage::darkTheme () const noexcept {
    return useDarkTheme;
}

bool MapCanvasManage::hasCache () const noexcept {
    return cacheValid;
}

const Rect2D& MapCanvasManage::itemBound () const noexcept {
    return cachedItemBound;
}

std::size_t MapCanvasManage::itemCount () const noexcept {
    return projectedItems.size();
}

const MapItemData* MapCanvasManage::findData (const MapItemType type, const int id) const noexcept {
    const auto found = itemIndex.find(indexKey(type, id));
    if (found == itemIndex.end() || found->second >= projectedItems.size())
        return nullptr;
    return &projectedItems[found->second].data;
}

const MapItemData* MapCanvasManage::dataAt (const QPointF &canvasPosition,
                                            const qreal tolerance) const {
    const qreal boundedTolerance = std::isfinite(tolerance) ? std::max(0.0, tolerance) : 0.0;
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(0.01, boundedTolerance * 2.0));
    for (auto region = lastHitRegions.crbegin(); region != lastHitRegions.crend(); ++region) {
        if (region->itemIndex >= projectedItems.size())
            continue;
        if (region->path.contains(canvasPosition)
            || (boundedTolerance > 0.0 && stroker.createStroke(region->path).contains(canvasPosition)))
            return &projectedItems[region->itemIndex].data;
    }
    return nullptr;
}

std::vector<Point2D> MapCanvasManage::project (std::vector<Point2D> positions) const {
    return projection.trans(std::move(positions));
}

std::vector<Point2D> MapCanvasManage::unproject (std::vector<Point2D> positions) const {
    return projection.revertTrans(std::move(positions));
}

MapCanvasManage::ItemIndexKey MapCanvasManage::indexKey (const MapItemType type,
                                                         const int id) noexcept {
    return (static_cast<ItemIndexKey>(static_cast<unsigned int>(type)) << 32U)
           | static_cast<std::uint32_t>(id);
}

QTransform MapCanvasManage::canvasTransform (const QRectF &canvasRect,
                                             const Rect2D &viewportBound) const {
    const auto &[topLeft, bottomRight] = viewportBound;
    const QRectF projectedViewport = pointBounds(projection.trans({
        topLeft,
        {topLeft.first, bottomRight.second},
        bottomRight,
        {bottomRight.first, topLeft.second}
    }));
    if (projectedViewport.isEmpty() || projectedViewport.width() <= 0.0
        || projectedViewport.height() <= 0.0)
        return QTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    const qreal scale = std::min(canvasRect.width() / projectedViewport.width(),
                                 canvasRect.height() / projectedViewport.height());
    if (!std::isfinite(scale) || scale <= 0.0)
        return QTransform{0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    QTransform transform;
    transform.translate(canvasRect.center().x(), canvasRect.center().y());
    transform.scale(scale, scale);
    transform.translate(-projectedViewport.center().x(), -projectedViewport.center().y());
    return transform;
}

void MapCanvasManage::rebuildIndex () {
    itemIndex.clear();
    itemIndex.reserve(projectedItems.size());
    for (std::size_t index = 0; index < projectedItems.size(); ++index) {
        const MapItemData &data = projectedItems[index].data;
        itemIndex.insert_or_assign(indexKey(dataType(data), dataId(data)), index);
    }
}

} // namespace reserveCode
