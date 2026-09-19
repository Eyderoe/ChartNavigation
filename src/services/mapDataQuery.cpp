#include "mapDataQuery.hpp"

#include "utils/constValue.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <limits>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
    if (!allFinite(topLeft, bottomRight))
        return {};
    double left = normalizeLongitude(topLeft.second);
    double right = normalizeLongitude(bottomRight.second);
    if (getLongiSpan(topLeft.second, bottomRight.second) >= 360.0) {
        left = -180.0;
        right = 180.0;
    }
    const NormalizedBound result(std::clamp(std::max(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
                                 std::clamp(std::min(topLeft.first, bottomRight.first), -maxSupportLat, maxSupportLat),
                                 left, right, true);
    return result;
}

/**
 * @brief 按缓存策略扩大查询边界。
 * @param requested 当前请求的规范化边界。
 * @return 扩大后的边界。
 */
Rect2D enlargedBound (const NormalizedBound &requested) {
    // MapItemManage 请求 2m×2n 的图元范围；再扩大 2 倍得到 4m×4n 的数据库缓存。
    constexpr double databaseToItemSpanRatio{2.0};
    constexpr double maximumLatitudeSpan{maxSupportLat * 2.0};
    const double requestedLatitudeSpan = requested.top - requested.bottom;
    const double latitudeSpan = std::min(maximumLatitudeSpan,
                                         requestedLatitudeSpan * databaseToItemSpanRatio);
    const double latitudeCenter = (requested.top + requested.bottom) / 2.0;
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

    const double longitudeSpan = std::min(360.0, getLongiSpan(requested.left, requested.right)
                                                 * databaseToItemSpanRatio);
    if (longitudeSpan >= 360.0)
        return {{top, -180.0}, {bottom, 180.0}};
    const double centerLongitude = getLongiRangeCenter(requested.left, requested.right);
    return {
        {top, normalizeLongitude(centerLongitude - longitudeSpan / 2.0)},
        {bottom, normalizeLongitude(centerLongitude + longitudeSpan / 2.0)}
    };
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
        return "select " + columns + " from " + rtree
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

double realValue (const SQLiteVal &value) {
    if (const auto *number = std::get_if<double>(&value))
        return finite(*number) ? *number : 0.0;
    if (const auto *number = std::get_if<int64_t>(&value))
        return static_cast<double>(*number);
    return 0.0;
}

QString textValue (const SQLiteVal &value) {
    if (const auto *text = std::get_if<std::string>(&value))
        return QString::fromStdString(*text);
    return {};
}

int integerValue (const SQLiteVal &value) {
    if (const auto *number = std::get_if<int64_t>(&value)) {
        if (*number >= std::numeric_limits<int>::min() && *number <= std::numeric_limits<int>::max())
            return static_cast<int>(*number);
    }
    if (const auto *number = std::get_if<double>(&value)) {
        if (finite(*number) && *number >= std::numeric_limits<int>::min()
            && *number <= std::numeric_limits<int>::max())
            return static_cast<int>(*number);
    }
    return 0;
}

char airwayDirection (const SQLiteVal &value) {
    const auto *text = std::get_if<std::string>(&value);
    if (!text || text->empty())
        return 'B';

    std::string direction;
    direction.reserve(text->size());
    for (const unsigned char character : *text)
        direction.push_back(static_cast<char>(std::tolower(character)));
    if (direction == "forw" || direction == "forward" || direction == "f")
        return 'F';
    if (direction == "back" || direction == "backward" || direction == "reverse" || direction == "r")
        return 'R';
    return 'B';
}

/**
 * @brief 查询并整理航路数据。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 * @return 按 awy_uni 整理后的航路记录；跨日期变更线的两条记录合并为一条。
 */
SQLiteRows queryAwyRows (const Database &database, const NormalizedBound &queryBound) {
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
            "select v.awy_uni,v.sub_id,v.name,v.p1_lat,v.p1_lon,v.p2_lat,v.p2_lon,v.awy_id,"
            "v.p1_id,v.p2_id,v.direct,v.p1_type,v.p2_type "
            "from awy_view as v inner join matched_routes as m on m.awy_uni=v.awy_uni "
            "order by v.awy_uni,v.sub_id";
    const auto routeRows = database.getRecords(sql, parameters);

    SQLiteRows rows;
    for (size_t firstIndex = 0; firstIndex < routeRows.size();) {
        size_t endIndex = firstIndex + 1;
        const auto routeId = std::get<int64_t>(routeRows[firstIndex][0]);
        while (endIndex < routeRows.size() && std::get<int64_t>(routeRows[endIndex][0]) == routeId)
            ++endIndex;

        if (endIndex - firstIndex == 2) {
            const auto &first = routeRows[firstIndex];
            const auto &last = routeRows[firstIndex + 1];
            rows.push_back({
                first[0], first[2], first[3], first[4], last[5], last[6], first[7],
                first[8], last[9], first[10], first[11], last[12]
            });
        } else {
            for (size_t index = firstIndex; index < endIndex; ++index) {
                const auto &segment = routeRows[index];
                rows.push_back({
                    segment[0], segment[2], segment[3], segment[4],
                    segment[5], segment[6], segment[7], segment[8], segment[9], segment[10],
                    segment[11], segment[12]
                });
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
    return std::visit([]<typename T0>(const T0 &data) -> ItemBound {
        using T = std::decay_t<T0>;
        if constexpr (std::is_same_v<T, MapApData> || std::is_same_v<T, MapNavData>) {
            const double longitude = normalizeLongitude(data.realPos.second);
            return ItemBound(data.realPos.first, data.realPos.first, longitude, longitude, true);
        } else if constexpr (std::is_same_v<T, MapAwyData> || std::is_same_v<T, MapFirData>) {
            const double longitude1 = normalizeLongitude(data.p1.second);
            const double longitude2 = normalizeLongitude(data.p2.second);
            const bool wrapsLongitude = std::abs(longitude1 - longitude2) > 180.0;
            return ItemBound(std::max(data.p1.first, data.p2.first), std::min(data.p1.first, data.p2.first),
                             wrapsLongitude ? std::max(longitude1, longitude2) : std::min(longitude1, longitude2),
                             wrapsLongitude ? std::min(longitude1, longitude2) : std::max(longitude1, longitude2),
                             true);
        } else {
            const auto bounds = normalizeBound(data.bounds);
            return ItemBound(bounds.top, bounds.bottom, bounds.left, bounds.right, bounds.valid);
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
    if (!item.valid || !queryBound.valid || !allFinite(item.top, item.bottom, item.left, item.right))
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

int moraGridId (const int latitudeCell, const int longitudeCell) {
    return (latitudeCell + 90) * 360 + (longitudeCell + 180) + 1;
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
        if (finite(*alt) && *alt >= std::numeric_limits<int>::min()
            && *alt <= std::numeric_limits<int>::max())
            return static_cast<int>(*alt);
    }
    return defaultMoraAltitude;
}

std::unordered_map<int, int> queryMoraAltitudes (const Database &database, const std::vector<int> &ids) {
    constexpr size_t maxParametersPerQuery{900};
    std::unordered_map<int, int> altitudes;
    altitudes.reserve(ids.size());
    for (size_t offset = 0; offset < ids.size(); offset += maxParametersPerQuery) {
        const size_t count = std::min(maxParametersPerQuery, ids.size() - offset);
        std::string sql = "select id,alt from mora where id in (?";
        SQLiteRow parameters;
        parameters.reserve(count);
        parameters.emplace_back(static_cast<int64_t>(ids[offset]));
        for (size_t index = 1; index < count; ++index) {
            sql += ",?";
            parameters.emplace_back(static_cast<int64_t>(ids[offset + index]));
        }
        sql += ')';

        for (const auto &row : database.getRecords(sql, parameters)) {
            const auto id = std::get<int64_t>(row[0]);
            if (id >= std::numeric_limits<int>::min() && id <= std::numeric_limits<int>::max())
                altitudes.emplace(static_cast<int>(id), moraAltitude(row[1]));
        }
    }
    return altitudes;
}

/**
 * @brief 将查询边界覆盖的 MORA 网格加入地图元素列表。
 * @param items 要追加的地图元素列表。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 */
void appendMora (std::vector<MapItemData> &items, const Database &database, const NormalizedBound &queryBound) {
    const Rect2D bound{{queryBound.top, queryBound.left}, {queryBound.bottom, queryBound.right}};
    const auto ids = moraGridIds(bound);
    const auto altitudes = queryMoraAltitudes(database, ids);
    for (const int id : ids) {
        const int zeroBasedId = id - 1;
        const int latitude = zeroBasedId / 360 - 90;
        const int longitude = zeroBasedId % 360 - 180;
        const auto altitude = altitudes.find(id);
        items.emplace_back(MapMoraData(
            Rect2D(Point2D(latitude + 1.0, static_cast<double>(longitude)),
                   Point2D(static_cast<double>(latitude), longitude + 1.0)), id,
            altitude == altitudes.end() ? defaultMoraAltitude : altitude->second, MapItemType::mora));
    }
}

/**
 * @brief 查询并转换所有类型的地图元素。
 * @param database 地图数据库。
 * @param queryBound 查询边界。
 * @return 查询到的地图元素列表。
 */
std::vector<MapItemData> queryAllItems (const Database &database, const NormalizedBound &queryBound) {
    std::vector<MapItemData> items;
    // 机场
    for (const auto &row : querySpatialRows(database, "airport_rtree", "airport",
                                            "t.icao,t.latitude,t.longitude,t.id,t.longest_geo", queryBound)) {
        items.emplace_back(MapApData(QString::fromStdString(std::get<std::string>(row[0])),
                                     Point2D(std::get<double>(row[1]), std::get<double>(row[2])),
                                     static_cast<int>(std::get<int64_t>(row[3])), realValue(row[4]),
                                     MapItemType::airport));
    }
    // 航路, 还需负责筛选处于航路上的点
    const auto airwayRows = queryAwyRows(database, queryBound);
    constexpr int64_t fixPointType{1};
    std::unordered_set<int> airwayFixIds;
    airwayFixIds.reserve(airwayRows.size() * 2);
    for (const auto &row : airwayRows) {
        if (std::get<int64_t>(row[10]) == fixPointType)
            airwayFixIds.emplace(static_cast<int>(std::get<int64_t>(row[7])));
        if (std::get<int64_t>(row[11]) == fixPointType)
            airwayFixIds.emplace(static_cast<int>(std::get<int64_t>(row[8])));
    }
    for (const auto &row : airwayRows) {
        items.emplace_back(MapAwyData(QString::fromStdString(std::get<std::string>(row[1])),
                                      Point2D(std::get<double>(row[2]), std::get<double>(row[3])),
                                      Point2D(std::get<double>(row[4]), std::get<double>(row[5])),
                                      static_cast<int>(std::get<int64_t>(row[6])),
                                      static_cast<int>(std::get<int64_t>(row[7])),
                                      static_cast<int>(std::get<int64_t>(row[8])), airwayDirection(row[9]),
                                      MapItemType::awy));
    }
    // FIR
    for (const auto &row : querySpatialRows(database, "fir_rtree", "fir",
                                            "substr(cast(t.ident as text),1,4),t.p1_lat,t.p1_lon,t.p2_lat,t.p2_lon,t.id",
                                            queryBound)) {
        items.emplace_back(MapFirData(QString::fromStdString(std::get<std::string>(row[0])),
                                      Point2D(std::get<double>(row[1]), std::get<double>(row[2])),
                                      Point2D(std::get<double>(row[3]), std::get<double>(row[4])),
                                      static_cast<int>(std::get<int64_t>(row[5])), MapItemType::fir));
    }
    // 航点
    for (const auto &row : querySpatialRows(database, "fix_rtree", "fix",
                                            "t.ident,t.latitude,t.longitude,t.id", queryBound)) {
        const auto id = static_cast<int>(std::get<int64_t>(row[3]));
        if (!airwayFixIds.contains(id))
            continue;
        items.emplace_back(MapApData(QString::fromStdString(std::get<std::string>(row[0])),
                                     Point2D(std::get<double>(row[1]), std::get<double>(row[2])),
                                     static_cast<int>(std::get<int64_t>(row[3])), 0, MapItemType::fix));
    }
    // MORA
    appendMora(items, database, queryBound);
    // 导航台
    for (const auto &row : querySpatialRows(database, "navaid_rtree", "navaid",
                                            "t.ident,t.latitude,t.longitude,t.type,t.id", queryBound)) {
        items.emplace_back(MapNavData(QString::fromStdString(std::get<std::string>(row[0])),
                                      Point2D(std::get<double>(row[1]), std::get<double>(row[2])),
                                      static_cast<int>(std::get<int64_t>(row[4])), MapItemType::navaid,
                                      static_cast<NavaidType>(std::get<int64_t>(row[3]) - 1)));
    }
    return items;
}

MapDataQuery::MapDataQuery (const QString &databaseFilePath) :
    db(std::make_unique<Database>(std::filesystem::path(databaseFilePath.toStdString()))) {}

/**
 * @brief 查询区域内的地图元素
 * @param requestedBound 区域(经纬度表示)
 * @return 区域内元素，以及本次查询是否重新访问了数据库
 * @note MapItemManage 传入 2m*2n 的图元范围；缓存未覆盖时查询 4m*4n 并更新缓存。
 */
std::pair<std::vector<MapItemData>, bool> MapDataQuery::queryMapItemData (const Rect2D &requestedBound) {
    const NormalizedBound requested = normalizeBound(requestedBound);
    if (!requested.valid)
        return {{}, false};
    // 尝试更新
    const NormalizedBound cached = normalizeBound(bound);
    const bool needRequire = !cacheValid || !contains(cached, requested);
    if (needRequire) {
        bound = enlargedBound(requested);
        const NormalizedBound expanded = normalizeBound(bound);
        cache = queryAllItems(*db, expanded);
        cacheValid = true;
    }
    // 返回查询范围内的
    std::vector<MapItemData> result;
    result.reserve(cache.size());
    for (const auto &item : cache) {
        if (intersects(itemBound(item), requested))
            result.emplace_back(item);
    }
    return {std::move(result), needRequire};
}

/**
 * @brief 从数据库中请求一个元素的具体信息
 * @param type 元素类型
 * @param id 元素id
 * @return 具体信息
 */
MapItemDetails MapDataQuery::queryItemDetails (const MapItemType type, const int id) {
    const cacheKey key{type, id};
    if (const auto found = detailCache.find(key); found != detailCache.end())
        return found->second;

    SQLiteRows rows;
    MapItemDetails details = [&] () -> MapItemDetails {
        switch (type) {
            case MapItemType::fix:
                rows = db->getRecords("select ident,latitude,longitude from fix where id=?",
                                      {static_cast<int64_t>(id)});
                return MapFixDetails(textValue(rows.front()[0]),
                                     Point2D(realValue(rows.front()[1]), realValue(rows.front()[2])));
            case MapItemType::airport:
                rows = db->getRecords("select icao,name,alt_ft,longest_m,latitude,longitude from airport where id=?",
                                      {static_cast<int64_t>(id)});
                return MapAirportDetails(textValue(rows.front()[0]), textValue(rows.front()[1]),
                                         integerValue(rows.front()[2]), integerValue(rows.front()[3]),
                                         Point2D(realValue(rows.front()[4]), realValue(rows.front()[5])));
            case MapItemType::navaid:
                rows = db->getRecords("select ident,name,type,frequency,alt,latitude,longitude from navaid where id=?",
                                      {static_cast<int64_t>(id)});
                if (const int storedType = integerValue(rows.front()[2]); storedType >= 1 && storedType <= 4) {
                    return MapNavaidDetails(textValue(rows.front()[0]), textValue(rows.front()[1]),
                                            static_cast<NavaidType>(storedType - 1), realValue(rows.front()[3]),
                                            integerValue(rows.front()[4]),
                                            Point2D(realValue(rows.front()[5]), realValue(rows.front()[6])));
                }
            default:
                assert(false && "map item type has no detail record");
        }
    }();

    detailCache.emplace(key, details);
    return details;
}
