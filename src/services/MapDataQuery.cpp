#include "MapDataQuery.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using LongitudeRange = std::pair<double, double>;

struct NormalizedBound {
    double top{};
    double bottom{};
    double left{};
    double right{};
    bool valid{false};
};

struct ItemBound {
    double top{};
    double bottom{};
    double left{};
    double right{};
    bool valid{false};
};

NormalizedBound normalizeBound (const Rect2D &rect) {
    const auto [topLeft, bottomRight] = rect;
    if (!std::isfinite(topLeft.first) || !std::isfinite(topLeft.second)
        || !std::isfinite(bottomRight.first) || !std::isfinite(bottomRight.second))
        return {};

    NormalizedBound result{
        .top = std::clamp(std::max(topLeft.first, bottomRight.first), -90.0, 90.0),
        .bottom = std::clamp(std::min(topLeft.first, bottomRight.first), -90.0, 90.0),
        .left = std::clamp(topLeft.second, -180.0, 180.0),
        .right = std::clamp(bottomRight.second, -180.0, 180.0),
        .valid = true
    };
    return result;
}

double longitudeSpan (const NormalizedBound &bound) {
    if (bound.left <= bound.right)
        return bound.right - bound.left;
    return 360.0 - bound.left + bound.right;
}

double longitudeCenter (const NormalizedBound &bound) {
    const double center = bound.left + longitudeSpan(bound) / 2.0;
    if (center > 180.0)
        return center - 360.0;
    return center;
}

double normalizeLongitude (double longitude) {
    while (longitude < -180.0)
        longitude += 360.0;
    while (longitude > 180.0)
        longitude -= 360.0;
    return longitude;
}

Rect2D enlargedBound (const NormalizedBound &requested) {
    constexpr double fullLatitudeSpan = 180.0;
    constexpr double fullLongitudeSpan = 360.0;

    const double latitudeSpan = requested.top - requested.bottom;
    const double latitudeMargin = latitudeSpan * 0.75;
    const double centerLatitude = (requested.top + requested.bottom) / 2.0;
    const double top = std::clamp(centerLatitude + latitudeMargin, -90.0, 90.0);
    const double bottom = std::clamp(centerLatitude - latitudeMargin, -90.0, 90.0);

    const double requestedLongitudeSpan = longitudeSpan(requested);
    const double expandedLongitudeSpan = std::min(fullLongitudeSpan, requestedLongitudeSpan * 1.5);
    if (expandedLongitudeSpan >= fullLongitudeSpan) {
        return {{top, -180.0}, {bottom, 180.0}};
    }

    const double centerLongitude = longitudeCenter(requested);
    const double halfLongitudeSpan = expandedLongitudeSpan / 2.0;
    const double left = normalizeLongitude(centerLongitude - halfLongitudeSpan);
    const double right = normalizeLongitude(centerLongitude + halfLongitudeSpan);
    return {{top, left}, {bottom, right}};
}

std::vector<LongitudeRange> longitudeRanges (const NormalizedBound &bound) {
    if (bound.left <= bound.right)
        return {{bound.left, bound.right}};
    return {{bound.left, 180.0}, {-180.0, bound.right}};
}

bool contains (const NormalizedBound &outer, const NormalizedBound &inner) {
    if (!outer.valid || !inner.valid || outer.top < inner.top || outer.bottom > inner.bottom)
        return false;

    const auto outerRanges = longitudeRanges(outer);
    for (const auto &innerRange : longitudeRanges(inner)) {
        const bool contained = std::ranges::any_of(outerRanges, [&innerRange](const auto &outerRange) {
            return outerRange.first <= innerRange.first && outerRange.second >= innerRange.second;
        });
        if (!contained)
            return false;
    }
    return true;
}

std::optional<double> asDouble (const SQLiteVal &value) {
    if (const auto *number = std::get_if<double>(&value))
        return *number;
    if (const auto *number = std::get_if<int64_t>(&value))
        return static_cast<double>(*number);
    return std::nullopt;
}

std::optional<int> asInt (const SQLiteVal &value) {
    if (const auto *number = std::get_if<int64_t>(&value)) {
        if (*number < std::numeric_limits<int>::min() || *number > std::numeric_limits<int>::max())
            return std::nullopt;
        return static_cast<int>(*number);
    }
    if (const auto *number = std::get_if<double>(&value)) {
        if (!std::isfinite(*number) || *number < std::numeric_limits<int>::min()
            || *number > std::numeric_limits<int>::max())
            return std::nullopt;
        return static_cast<int>(*number);
    }
    return std::nullopt;
}

std::optional<QString> asQString (const SQLiteVal &value) {
    if (const auto *text = std::get_if<std::string>(&value))
        return QString::fromStdString(*text);
    return std::nullopt;
}

SQLiteRows querySpatialRows (Database &database, const std::string &rtree, const std::string &table,
                             const std::string &columns, const NormalizedBound &queryBound) {
    const std::string sql = "select distinct " + columns + " from " + rtree
        + " as r inner join " + table + " as t on t.id=r.id"
        + " where r.max_lon>=? and r.min_lon<=? and r.max_lat>=? and r.min_lat<=?";

    SQLiteRows rows;
    for (const auto [left, right] : longitudeRanges(queryBound)) {
        auto part = database.getRecords(sql, {left, right, queryBound.bottom, queryBound.top});
        rows.insert(rows.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
    }
    return rows;
}

template <typename T>
bool assignPoint (const SQLiteRow &row, const size_t latitudeIndex, const size_t longitudeIndex,
                  const size_t idIndex, T &item) {
    if (row.size() <= std::max({latitudeIndex, longitudeIndex, idIndex}))
        return false;
    const auto latitude = asDouble(row[latitudeIndex]);
    const auto longitude = asDouble(row[longitudeIndex]);
    const auto id = asInt(row[idIndex]);
    if (!latitude || !longitude || !id || !std::isfinite(*latitude) || !std::isfinite(*longitude))
        return false;
    item.realPos = {*latitude, *longitude};
    item.id = *id;
    return true;
}

bool assignSegment (const SQLiteRow &row, MapAwyData &item) {
    if (row.size() < 6)
        return false;
    const auto p1Lat = asDouble(row[1]);
    const auto p1Lon = asDouble(row[2]);
    const auto p2Lat = asDouble(row[3]);
    const auto p2Lon = asDouble(row[4]);
    const auto id = asInt(row[5]);
    if (!p1Lat || !p1Lon || !p2Lat || !p2Lon || !id
        || !std::isfinite(*p1Lat) || !std::isfinite(*p1Lon)
        || !std::isfinite(*p2Lat) || !std::isfinite(*p2Lon))
        return false;
    item.p1 = {*p1Lat, *p1Lon};
    item.p2 = {*p2Lat, *p2Lon};
    item.id = *id;
    return true;
}

bool assignSegment (const SQLiteRow &row, MapFirData &item) {
    if (row.size() < 6)
        return false;
    const auto p1Lat = asDouble(row[1]);
    const auto p1Lon = asDouble(row[2]);
    const auto p2Lat = asDouble(row[3]);
    const auto p2Lon = asDouble(row[4]);
    const auto id = asInt(row[5]);
    if (!p1Lat || !p1Lon || !p2Lat || !p2Lon || !id
        || !std::isfinite(*p1Lat) || !std::isfinite(*p1Lon)
        || !std::isfinite(*p2Lat) || !std::isfinite(*p2Lon))
        return false;
    item.p1 = {*p1Lat, *p1Lon};
    item.p2 = {*p2Lat, *p2Lon};
    item.id = *id;
    return true;
}

ItemBound itemBound (const MapItemData &item) {
    return std::visit([](const auto &data) -> ItemBound {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, MapApData> || std::is_same_v<T, MapNavData>) {
            return {
                .top = data.realPos.first,
                .bottom = data.realPos.first,
                .left = data.realPos.second,
                .right = data.realPos.second,
                .valid = true
            };
        } else if constexpr (std::is_same_v<T, MapAwyData> || std::is_same_v<T, MapFirData>) {
            return {
                .top = std::max(data.p1.first, data.p2.first),
                .bottom = std::min(data.p1.first, data.p2.first),
                .left = std::min(data.p1.second, data.p2.second),
                .right = std::max(data.p1.second, data.p2.second),
                .valid = true
            };
        } else {
            const auto bounds = normalizeBound(data.bounds);
            return {
                .top = bounds.top,
                .bottom = bounds.bottom,
                .left = bounds.left,
                .right = bounds.right,
                .valid = bounds.valid
            };
        }
    }, item);
}

bool intersects (const ItemBound &item, const NormalizedBound &queryBound) {
    if (!item.valid || !queryBound.valid || !std::isfinite(item.top) || !std::isfinite(item.bottom)
        || !std::isfinite(item.left) || !std::isfinite(item.right))
        return false;
    if (item.bottom > queryBound.top || item.top < queryBound.bottom)
        return false;

    return std::ranges::any_of(longitudeRanges(queryBound), [&item](const auto &range) {
        return item.right >= range.first && item.left <= range.second;
    });
}

std::pair<int, int> cellRange (const double lower, const double upper, const int minimum,
                               const int maximumExclusive) {
    if (upper <= lower) {
        const int cell = std::clamp(static_cast<int>(std::floor(lower)), minimum, maximumExclusive - 1);
        return {cell, cell + 1};
    }

    const int first = std::clamp(static_cast<int>(std::floor(lower)), minimum, maximumExclusive - 1);
    const int end = std::clamp(static_cast<int>(std::ceil(upper)), first + 1, maximumExclusive);
    return {first, end};
}

void appendMora (std::vector<MapItemData> &items, const NormalizedBound &queryBound) {
    const auto latitudeCells = cellRange(queryBound.bottom, queryBound.top, -90, 90);
    for (const auto [left, right] : longitudeRanges(queryBound)) {
        const auto longitudeCells = cellRange(left, right, -180, 180);
        for (int latitude = latitudeCells.first; latitude < latitudeCells.second; ++latitude) {
            for (int longitude = longitudeCells.first; longitude < longitudeCells.second; ++longitude) {
                const int id = (latitude + 90) * 360 + (longitude + 180) + 1;
                items.emplace_back(MapMoraData{
                    .bounds = {{latitude + 1.0, static_cast<double>(longitude)},
                               {static_cast<double>(latitude), longitude + 1.0}},
                    .id = id,
                    .type = MapItemType::mora
                });
            }
        }
    }
}

std::vector<MapItemData> queryAllItems (Database &database, const NormalizedBound &queryBound) {
    std::vector<MapItemData> items;

    for (const auto &row : querySpatialRows(database, "airport_rtree", "airport",
                                            "t.icao,t.latitude,t.longitude,t.id", queryBound)) {
        if (row.size() < 4)
            continue;
        const auto icao = asQString(row[0]);
        MapApData airport{.icao = icao.value_or(QString{})};
        airport.type = MapItemType::airport;
        if (icao && assignPoint(row, 1, 2, 3, airport))
            items.emplace_back(std::move(airport));
    }

    for (const auto &row : querySpatialRows(database, "awy_rtree", "awy",
                                            "t.awy,t.p1_lat,t.p1_lon,t.p2_lat,t.p2_lon,t.id", queryBound)) {
        if (row.size() < 6)
            continue;
        const auto ident = asQString(row[0]);
        MapAwyData airway{.ident = ident.value_or(QString{}), .type = MapItemType::awy};
        if (ident && assignSegment(row, airway))
            items.emplace_back(std::move(airway));
    }

    for (const auto &row : querySpatialRows(database, "fir_rtree", "fir",
                                            "substr(cast(t.ident as text),1,4),t.p1_lat,t.p1_lon,t.p2_lat,t.p2_lon,t.id",
                                            queryBound)) {
        if (row.size() < 6)
            continue;
        const auto ident = asQString(row[0]);
        MapFirData fir{.ident = ident.value_or(QString{}), .type = MapItemType::fir};
        if (ident && assignSegment(row, fir))
            items.emplace_back(std::move(fir));
    }

    for (const auto &row : querySpatialRows(database, "fix_rtree", "fix",
                                            "t.ident,t.latitude,t.longitude,t.id", queryBound)) {
        if (row.size() < 4)
            continue;
        const auto ident = asQString(row[0]);
        MapApData fixPoint{.icao = ident.value_or(QString{})};
        fixPoint.type = MapItemType::fix;
        if (ident && assignPoint(row, 1, 2, 3, fixPoint))
            items.emplace_back(std::move(fixPoint));
    }

    appendMora(items, queryBound);

    for (const auto &row : querySpatialRows(database, "navaid_rtree", "navaid",
                                            "t.ident,t.latitude,t.longitude,t.type,t.id", queryBound)) {
        if (row.size() < 5)
            continue;
        const auto ident = asQString(row[0]);
        const auto navType = asInt(row[3]);
        MapNavData navaid{.ident = ident.value_or(QString{})};
        navaid.type = MapItemType::navaid;
        if (!ident || !navType || *navType < 1 || *navType > 4
            || !assignPoint(row, 1, 2, 4, navaid))
            continue;
        navaid.navType = static_cast<NavaidType>(*navType - 1);
        items.emplace_back(std::move(navaid));
    }

    return items;
}

} // namespace

MapDataQuery::MapDataQuery (QString databaseFilePath) :
    db(std::make_unique<Database>(std::filesystem::path(databaseFilePath.toStdString()))) {
}

/**
 * @brief 查询区域内的地图元素
 * @param bound 区域(经纬度表示)
 * @return 区域内元素
 * @note 返回区域内元素, 假如传入范围m*n大于缓存范围, 则重新查询数据库1.5m*1.5n并更新缓存
 */
std::vector<MapItemData> MapDataQuery::queryMapItemData (Rect2D requestedBound) {
    const NormalizedBound requested = normalizeBound(requestedBound);
    if (!requested.valid)
        return {};

    const NormalizedBound cached = normalizeBound(bound);
    if (!cacheValid || !contains(cached, requested)) {
        bound = enlargedBound(requested);
        const NormalizedBound expanded = normalizeBound(bound);
        cache = queryAllItems(*db, expanded);
        cacheValid = true;
    }

    std::vector<MapItemData> result;
    result.reserve(cache.size());
    for (const auto &item : cache) {
        if (intersects(itemBound(item), requested))
            result.emplace_back(item);
    }
    return result;
}
