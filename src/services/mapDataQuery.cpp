#include "mapDataQuery.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

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

/**
 * @brief 规范化地图查询边界。
 * @param rect 原始经纬度矩形。
 * @return 纬度限制在允许范围、经度归一化后的边界；输入无效时返回无效边界。
 */
NormalizedBound normalizeBound (const Rect2D &rect) {
    const auto [topLeft, bottomRight] = rect;
    if (!std::isfinite(topLeft.first) || !std::isfinite(topLeft.second)
        || !std::isfinite(bottomRight.first) || !std::isfinite(bottomRight.second))
        return {};

    double left = normalizeLongitude(topLeft.second);
    double right = normalizeLongitude(bottomRight.second);
    if (getLongiRange(topLeft.second, bottomRight.second) >= 360.0) {
        left = -180.0;
        right = 180.0;
    }

    const NormalizedBound result{
        .top = std::clamp(std::max(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
        .bottom = std::clamp(std::min(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
        .left = left,
        .right = right,
        .valid = true
    };
    return result;
}

/**
 * @brief 按缓存策略扩大查询边界。
 * @param requested 当前请求的规范化边界。
 * @return 扩大后的边界。
 */
Rect2D enlargedBound (const NormalizedBound &requested) {
    constexpr double fullLongitudeSpan = 360.0;

    const double latitudeSpan = requested.top - requested.bottom;
    const double latitudeMargin = latitudeSpan * 0.75;
    const double centerLatitude = (requested.top + requested.bottom) / 2.0;
    const double top = std::clamp(centerLatitude + latitudeMargin, -maxSupportLat, maxSupportLat);
    const double bottom = std::clamp(centerLatitude - latitudeMargin, -maxSupportLat, maxSupportLat);

    const double requestedLongitudeSpan = getLongiRange(requested.left, requested.right);
    const double expandedLongitudeSpan = std::min(fullLongitudeSpan, requestedLongitudeSpan * 1.5);
    if (expandedLongitudeSpan == 0.0)
        return {{top, requested.left}, {bottom, requested.right}};
    if (expandedLongitudeSpan >= fullLongitudeSpan) {
        return {{top, -180.0}, {bottom, 180.0}};
    }

    const double centerLongitude = getLongiRangeCenter(requested.left, requested.right);
    const double halfLongitudeSpan = expandedLongitudeSpan / 2.0;
    const double left = normalizeLongitude(centerLongitude - halfLongitudeSpan);
    const double right = normalizeLongitude(centerLongitude + halfLongitudeSpan);
    return {{top, left}, {bottom, right}};
}

/**
 * @brief 判断一个地图边界是否完全包含另一个地图边界。
 * @param outer 外部边界。
 * @param inner 待判断的内部边界。
 * @return 内部边界完全位于外部边界内时返回 true。
 */
bool contains (const NormalizedBound &outer, const NormalizedBound &inner) {
    if (!outer.valid || !inner.valid || outer.top < inner.top || outer.bottom > inner.bottom)
        return false;

    const auto outerRanges = getLongiRanges(outer.left, outer.right);
    for (const auto &innerRange : getLongiRanges(inner.left, inner.right)) {
        const bool contained = std::ranges::any_of(outerRanges, [&innerRange](const auto &outerRange) {
            return outerRange.first <= innerRange.first && outerRange.second >= innerRange.second;
        });
        if (!contained)
            return false;
    }
    return true;
}

/**
 * @brief 使用 RTree 查询与边界相交的数据行。
 * @param database 地图数据库。
 * @param rtree RTree 表名。
 * @param table 与 RTree 关联的数据表名。
 * @param columns 要返回的列表达式。
 * @param queryBound 查询边界。
 * @param idColumn 数据表中与 RTree 关联的 ID 列名。
 * @return 与查询边界相交的数据行。
 */
SQLiteRows querySpatialRows (const Database &database, const std::string &rtree, const std::string &table,
                             const std::string &columns, const NormalizedBound &queryBound,
                             const std::string &idColumn = "id") {
    const auto rangeSql = [&rtree, &table, &columns, &idColumn] {
        return "select distinct " + columns + " from " + rtree
                + " as r inner join " + table + " as t on t." + idColumn + "=r.id"
                + " where r.max_lon>=? and r.min_lon<=? and r.max_lat>=? and r.min_lat<=?";
    };
    const auto ranges = getLongiRanges(queryBound.left, queryBound.right);
    if (ranges.size() == 1)
        return database.getRecords(rangeSql(),
                                   {
                                       ranges.front().first, ranges.front().second,
                                       queryBound.bottom, queryBound.top
                                   });

    const auto &first = ranges.front();
    const auto &second = ranges.back();
    const std::string sql = rangeSql() + " union " + rangeSql();
    return database.getRecords(sql, {
                                   first.first, first.second, queryBound.bottom, queryBound.top,
                                   second.first, second.second, queryBound.bottom, queryBound.top
                               });
}

/**
 * @brief 查询并整理航路数据。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 * @return 按 awy_uni 整理后的航路记录；跨日期变更线的两条记录合并为一条。
 */
SQLiteRows queryAwyRows (Database &database, const NormalizedBound &queryBound) {
    const auto ranges = getLongiRanges(queryBound.left, queryBound.right);
    std::string matchedRoutesSql;
    SQLiteRow parameters;
    parameters.reserve(ranges.size() * 4);
    for (const auto &[left, right] : ranges) {
        if (!matchedRoutesSql.empty())
            matchedRoutesSql += " union ";
        matchedRoutesSql +=
                "select distinct i.awy_uni from awy_rtree as r "
                "inner join awy_idx as i on i.awy_id=r.id "
                "where r.max_lon>=? and r.min_lon<=? and r.max_lat>=? and r.min_lat<=?";
        parameters.insert(parameters.end(), {left, right, queryBound.bottom, queryBound.top});
    }

    const std::string sql =
            "with matched_routes(awy_uni) as (" + matchedRoutesSql + ") "
            "select v.awy_uni,v.sub_id,v.name,v.p1_lat,v.p1_lon,v.p2_lat,v.p2_lon,v.awy_id "
            "from awy_view as v inner join matched_routes as m on m.awy_uni=v.awy_uni "
            "order by v.awy_uni,v.sub_id";
    auto routeRows = database.getRecords(sql, parameters);

    SQLiteRows rows;
    for (size_t firstIndex = 0; firstIndex < routeRows.size();) {
        size_t endIndex = firstIndex + 1;
        const auto routeId = std::get<int64_t>(routeRows[firstIndex][0]);
        while (endIndex < routeRows.size() && std::get<int64_t>(routeRows[endIndex][0]) == routeId)
            ++endIndex;

        if (endIndex - firstIndex == 2) {
            const auto &first = routeRows[firstIndex];
            const auto &last = routeRows[firstIndex + 1];
            rows.push_back({first[0], first[2], first[3], first[4], last[5], last[6], first[7]});
        } else {
            for (size_t index = firstIndex; index < endIndex; ++index) {
                const auto &segment = routeRows[index];
                rows.push_back({segment[0], segment[2], segment[3], segment[4],
                                segment[5], segment[6], segment[7]});
            }
        }
        firstIndex = endIndex;
    }
    return rows;
}

/**
 * @brief 计算地图元素的经纬度包围范围。
 * @param item 地图元素。
 * @return 地图元素的规范化包围范围。
 */
ItemBound itemBound (const MapItemData &item) {
    return std::visit([](const auto &data) -> ItemBound {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, MapApData> || std::is_same_v<T, MapNavData>) {
            const double longitude = normalizeLongitude(data.realPos.second);
            return {
                .top = data.realPos.first,
                .bottom = data.realPos.first,
                .left = longitude,
                .right = longitude,
                .valid = true
            };
        } else if constexpr (std::is_same_v<T, MapAwyData> || std::is_same_v<T, MapFirData>) {
            const double longitude1 = normalizeLongitude(data.p1.second);
            const double longitude2 = normalizeLongitude(data.p2.second);
            const bool wrapsLongitude = std::abs(longitude1 - longitude2) > 180.0;
            return {
                .top = std::max(data.p1.first, data.p2.first),
                .bottom = std::min(data.p1.first, data.p2.first),
                .left = wrapsLongitude ? std::max(longitude1, longitude2) : std::min(longitude1, longitude2),
                .right = wrapsLongitude ? std::min(longitude1, longitude2) : std::max(longitude1, longitude2),
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

/**
 * @brief 判断地图元素是否与查询边界相交。
 * @param item 地图元素包围范围。
 * @param queryBound 查询边界。
 * @return 两个范围相交时返回 true。
 */
bool intersects (const ItemBound &item, const NormalizedBound &queryBound) {
    if (!item.valid || !queryBound.valid || !std::isfinite(item.top) || !std::isfinite(item.bottom)
        || !std::isfinite(item.left) || !std::isfinite(item.right))
        return false;
    if (item.bottom > queryBound.top || item.top < queryBound.bottom)
        return false;

    const auto itemRanges = getLongiRanges(item.left, item.right);
    return std::ranges::any_of(getLongiRanges(queryBound.left, queryBound.right), [&itemRanges](const auto &range) {
        return std::ranges::any_of(itemRanges, [&range](const auto &itemRange) {
            return itemRange.second >= range.first && itemRange.first <= range.second;
        });
    });
}

/**
 * @brief 计算网格坐标的半开区间。
 * @param lower 区间下界。
 * @param upper 区间上界。
 * @param minimum 允许的最小网格坐标。
 * @param maximumExclusive 允许的最大网格坐标（不包含）。
 * @return 覆盖输入范围的网格下标区间。
 */
std::pair<int, int> cellRange (const double lower, const double upper, const int minimum, const int maximumExclusive) {
    if (upper <= lower) {
        const int cell = std::clamp(static_cast<int>(std::floor(lower)), minimum, maximumExclusive - 1);
        return {cell, cell + 1};
    }

    const int first = std::clamp(static_cast<int>(std::floor(lower)), minimum, maximumExclusive - 1);
    const int end = std::clamp(static_cast<int>(std::ceil(upper)), first + 1, maximumExclusive);
    return {first, end};
}

std::vector<int> moraGridIds (const Rect2D &requestedBound) {
    const NormalizedBound queryBound = normalizeBound(requestedBound);
    if (!queryBound.valid)
        return {};

    std::vector<int> ids;
    const auto latitudeCells = cellRange(queryBound.bottom, queryBound.top,
                                         static_cast<int>(-maxSupportLat), static_cast<int>(maxSupportLat));
    for (const auto [left, right] : getLongiRanges(queryBound.left, queryBound.right)) {
        const auto longitudeCells = cellRange(left, right, -180, 180);
        for (int latitude = latitudeCells.first; latitude < latitudeCells.second; ++latitude) {
            for (int longitude = longitudeCells.first; longitude < longitudeCells.second; ++longitude)
                ids.emplace_back(moraGridId(latitude, longitude));
        }
    }
    return ids;
}

constexpr int defaultMoraAltitude{1000};

int moraAltitude (const SQLiteVal &value) {
    if (const auto *altitude = std::get_if<int64_t>(&value)) {
        if (*altitude >= std::numeric_limits<int>::min() && *altitude <= std::numeric_limits<int>::max())
            return static_cast<int>(*altitude);
    } else if (const auto *alt = std::get_if<double>(&value)) {
        if (std::isfinite(*alt) && *alt >= std::numeric_limits<int>::min()
            && *alt <= std::numeric_limits<int>::max())
            return static_cast<int>(*alt);
    }
    return defaultMoraAltitude;
}

std::unordered_map<int, int> queryMoraAltitudes (const Database &database, const NormalizedBound &queryBound) {
    const auto ranges = getLongiRanges(queryBound.left, queryBound.right);
    std::string sql = "select id,alt from mora where max_lat>=? and min_lat<=? and (";
    SQLiteRow parameters{queryBound.bottom, queryBound.top};
    parameters.reserve(2 + ranges.size() * 2);
    for (size_t index = 0; index < ranges.size(); ++index) {
        if (index != 0)
            sql += " or ";
        sql += "(max_lon>=? and min_lon<=?)";
        parameters.insert(parameters.end(), {ranges[index].first, ranges[index].second});
    }
    sql += ')';

    const auto rows = database.getRecords(sql, parameters);
    std::unordered_map<int, int> altitudes;
    altitudes.reserve(rows.size());
    for (const auto &row : rows) {
        const auto id = std::get<int64_t>(row[0]);
        if (id >= std::numeric_limits<int>::min() && id <= std::numeric_limits<int>::max())
            altitudes.emplace(static_cast<int>(id), moraAltitude(row[1]));
    }
    return altitudes;
}

/**
 * @brief 将查询边界覆盖的 MORA 网格加入地图元素列表。
 * @param items 要追加的地图元素列表。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 */
void appendMora (std::vector<MapItemData> &items, Database &database, const NormalizedBound &queryBound) {
    const Rect2D bound{{queryBound.top, queryBound.left}, {queryBound.bottom, queryBound.right}};
    const auto altitudes = queryMoraAltitudes(database, queryBound);
    for (const int id : moraGridIds(bound)) {
        const int zeroBasedId = id - 1;
        const int latitude = zeroBasedId / 360 - 90;
        const int longitude = zeroBasedId % 360 - 180;
        const auto altitude = altitudes.find(id);
        items.emplace_back(MapMoraData{
            .bounds = {
                {latitude + 1.0, static_cast<double>(longitude)},
                {static_cast<double>(latitude), longitude + 1.0}
            },
            .id = id,
            .alt = altitude == altitudes.end() ? defaultMoraAltitude : altitude->second,
            .type = MapItemType::mora
        });
    }
}

/**
 * @brief 查询并转换所有类型的地图元素。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 * @return 查询到的地图元素列表。
 */
std::vector<MapItemData> queryAllItems (Database &database, const NormalizedBound &queryBound) {
    std::vector<MapItemData> items;

    for (const auto &row : querySpatialRows(database, "airport_rtree", "airport",
                                            "t.icao,t.latitude,t.longitude,t.id", queryBound)) {
        items.emplace_back(MapApData{
            .icao = QString::fromStdString(std::get<std::string>(row[0])),
            .realPos = {std::get<double>(row[1]), std::get<double>(row[2])},
            .id = static_cast<int>(std::get<int64_t>(row[3])),
            .type = MapItemType::airport
        });
    }

    for (const auto &row : queryAwyRows(database, queryBound)) {
        items.emplace_back(MapAwyData{
            .ident = QString::fromStdString(std::get<std::string>(row[1])),
            .p1 = {std::get<double>(row[2]), std::get<double>(row[3])},
            .p2 = {std::get<double>(row[4]), std::get<double>(row[5])},
            .id = static_cast<int>(std::get<int64_t>(row[6])),
            .type = MapItemType::awy
        });
    }

    for (const auto &row : querySpatialRows(database, "fir_rtree", "fir",
                                            "substr(cast(t.ident as text),1,4),t.p1_lat,t.p1_lon,t.p2_lat,t.p2_lon,t.id",
                                            queryBound)) {
        items.emplace_back(MapFirData{
            .ident = QString::fromStdString(std::get<std::string>(row[0])),
            .p1 = {std::get<double>(row[1]), std::get<double>(row[2])},
            .p2 = {std::get<double>(row[3]), std::get<double>(row[4])},
            .id = static_cast<int>(std::get<int64_t>(row[5])),
            .type = MapItemType::fir
        });
    }

    for (const auto &row : querySpatialRows(database, "fix_rtree", "fix",
                                            "t.ident,t.latitude,t.longitude,t.id", queryBound)) {
        items.emplace_back(MapApData{
            .icao = QString::fromStdString(std::get<std::string>(row[0])),
            .realPos = {std::get<double>(row[1]), std::get<double>(row[2])},
            .id = static_cast<int>(std::get<int64_t>(row[3])),
            .type = MapItemType::fix
        });
    }

    appendMora(items, database, queryBound);

    for (const auto &row : querySpatialRows(database, "navaid_rtree", "navaid",
                                            "t.ident,t.latitude,t.longitude,t.type,t.id", queryBound)) {
        items.emplace_back(MapNavData{
            .ident = QString::fromStdString(std::get<std::string>(row[0])),
            .realPos = {std::get<double>(row[1]), std::get<double>(row[2])},
            .id = static_cast<int>(std::get<int64_t>(row[4])),
            .type = MapItemType::navaid,
            .navType = static_cast<NavaidType>(std::get<int64_t>(row[3]) - 1)
        });
    }

    return items;
}

MapDataQuery::MapDataQuery (const QString &databaseFilePath) :
    db(std::make_unique<Database>(std::filesystem::path(databaseFilePath.toStdString()))) {}

/**
 * @brief 查询区域内的地图元素
 * @param requestedBound 区域(经纬度表示)
 * @return 区域内元素
 * @note 返回区域内元素, 假如传入范围m*n大于缓存范围, 则重新查询数据库1.5m*1.5n并更新缓存
 */
std::vector<MapItemData> MapDataQuery::queryMapItemData (const Rect2D &requestedBound) {
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
