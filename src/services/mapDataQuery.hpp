#ifndef CHARTNAVIGATION_MAPDATAQUERY_HPP
#define CHARTNAVIGATION_MAPDATAQUERY_HPP


#include <QString>
#include <memory>
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
    int id;
    MapItemType type;
};
struct MapAwyData {
    QString ident;
    Point2D p1, p2;
    int id;
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
    int alt; // 单位：英尺；数据库为 NULL 时使用 1000
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

class MapDataQuery {
    public:
        explicit MapDataQuery (const QString& databaseFilePath);
        std::vector<MapItemData> queryMapItemData (const Rect2D &requestedBound);
    private:
        std::unique_ptr<Database> db;
        std::vector<MapItemData> cache;
        Rect2D bound;
        bool cacheValid{false};
};

#endif //CHARTNAVIGATION_MAPDATAQUERY_HPP
