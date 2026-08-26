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
    int id; // id = (floor(lat) + 90) * 360 + (floor(lon) + 180) + 1
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
        explicit MapDataQuery (QString databaseFilePath);
        std::vector<MapItemData> queryMapItemData (Rect2D bound);
    private:
        std::unique_ptr<Database> db;
        std::vector<MapItemData> cache;
        Rect2D bound;
        bool cacheValid{false};
};

#endif //CHARTNAVIGATION_MAPDATAQUERY_HPP
