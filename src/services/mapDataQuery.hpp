#ifndef CHARTNAVIGATION_MAPDATAQUERY_HPP
#define CHARTNAVIGATION_MAPDATAQUERY_HPP


#include <QString>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "utils/geographic.hpp"
#include "utils/sqliteHelper.hpp"


enum class MapItemType { airport, awy, fir, fix, mora, navaid };
enum class NavaidType { vor, dme, vordme, ndb };

constexpr int moraGridId (const int latitudeCell, const int longitudeCell) noexcept {
    return (latitudeCell + 90) * 360 + (longitudeCell + 180) + 1;
}

std::vector<int> moraGridIds (const Rect2D &requestedBound);

struct MapApData {
    QString icao;
    Point2D realPos;
    int id{};
    double geo{}; // 最长跑道航向
    MapItemType type;
};
struct MapAwyData {
    QString ident;
    Point2D p1, p2;
    int id{}; // 航路自身 id
    int id1{}, id2{}; // 两个端点关联的航点 id
    char direct{'B'}; // 航路方向: 'B' 双向, 'F' 正向, 'R' 反向
    MapItemType type;
};
struct MapFirData {
    QString ident;
    Point2D p1, p2;
    int id;
    MapItemType type;
};
struct MapMoraData {
    Rect2D bounds;
    int id;
    int alt; // 数据库为 null 时使用 1000
    MapItemType type;
};
struct MapNavData {
    QString ident;
    Point2D realPos;
    int id;
    MapItemType type;
    NavaidType navType;
};

using MapItemData = std::variant<MapApData, MapAwyData, MapFirData, MapMoraData, MapNavData>;

struct MapFixDetails {
    QString ident;
    Point2D position;
};

struct MapAirportDetails {
    QString icao;
    QString name;
    int altitudeFeet{};
    int longestRunwayMetres{};
    Point2D position;
};

struct MapNavaidDetails {
    QString ident;
    QString name;
    NavaidType type{NavaidType::vor};
    double frequency{};
    int altitudeFeet{};
    Point2D position;
};

using MapItemDetails = std::variant<MapFixDetails, MapAirportDetails, MapNavaidDetails>;

class MapDataQuery {
    public:
        explicit MapDataQuery (const QString &databaseFilePath);
        std::pair<std::vector<MapItemData>, bool> queryMapItemData (const Rect2D &requestedBound);
        std::optional<MapItemDetails> queryItemDetails (MapItemType type, int id);
    private:
        using DetailCacheKey = std::uint64_t;

        std::unique_ptr<Database> db;
        std::vector<MapItemData> cache;
        std::unordered_map<DetailCacheKey, std::optional<MapItemDetails>> detailCache;
        Rect2D bound; // 实际缓存区域
        bool cacheValid{false};
};

#endif //CHARTNAVIGATION_MAPDATAQUERY_HPP
