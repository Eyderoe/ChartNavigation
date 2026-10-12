#include <doctest.h>
#include <QFile>
#include <QTemporaryDir>

#include "services/mapDataQuery.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

int moraLongitudeCell (const double longitude) {
    return std::clamp(static_cast<int>(std::floor(longitude)), -180, 179);
}

TEST_CASE("MORA ID calculation around the date line") {
    constexpr int latitudeCell = 0;
    constexpr int westDateLineId = 32401;
    constexpr int eastDateLineId = 32760;

    SUBCASE("exact date line coordinates select both edge cells") {
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(-180.0)) == westDateLineId);
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(180.0)) == eastDateLineId);
    }

    SUBCASE("normalized database boundaries keep their original cells") {
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(-179.9999)) == westDateLineId);
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(179.9999)) == eastDateLineId);
    }

    SUBCASE("split segment endpoints keep their original cells") {
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(-179.999)) == westDateLineId);
        CHECK(moraGridId(latitudeCell, moraLongitudeCell(179.999)) == eastDateLineId);
    }

    SUBCASE("a narrow range crossing the date line returns both cells") {
        auto ids = moraGridIds({{0.5, 179.999}, {0.0, -179.999}});
        std::ranges::sort(ids);
        CHECK(ids == std::vector<int>{westDateLineId, eastDateLineId});
    }

    SUBCASE("a wider range crossing the date line returns three cells") {
        auto ids = moraGridIds({{0.5, 178.5}, {0.5, -179.5}});
        std::ranges::sort(ids);
        CHECK(ids == std::vector<int>{
            westDateLineId,
            moraGridId(latitudeCell, 178),
            eastDateLineId
        });
    }

    SUBCASE("exact positive and negative 180 return both cells") {
        auto ids = moraGridIds({{0.5, 180.0}, {0.0, -180.0}});
        std::ranges::sort(ids);
        CHECK(ids == std::vector<int>{westDateLineId, eastDateLineId});
    }
}

namespace {
// Reuse the repository schema, but populate only a disposable copy.
struct MapDatabaseFixture {
    QTemporaryDir directory;
    QString path;

    MapDatabaseFixture() {
        REQUIRE(directory.isValid());
        path = directory.filePath("map.db3");
        REQUIRE(QFile::copy(QString::fromUtf8(CHARTNAVI_TEST_DATABASE_PATH), path));
        SQLite::Database database(path.toStdString(), SQLite::OPEN_READWRITE);
        database.exec(R"SQL(
            INSERT INTO airport VALUES (1,'TEST','Test airport',1234,2400,90,30.0,110.0);
            INSERT INTO airport VALUES (2,'EDGE','Cached airport',0,1800,NULL,30.0,111.5);
            INSERT INTO airport VALUES (3,'WEST','Date line west',0,1800,0,10.0,179.5);
            INSERT INTO airport VALUES (4,'EAST','Date line east',0,1800,0,10.0,-179.5);
            INSERT INTO navaid VALUES (1,'NAV',3,113.5,1200,'Test VOR DME',30.0,110.0);
            INSERT INTO fix VALUES (1,'ROUTE',30.0,110.0);
            INSERT INTO fix VALUES (2,'UNUSED',30.1,110.1);
            INSERT INTO awy VALUES (1,30.0,110.0,30.5,110.5);
            INSERT INTO awy_idx VALUES (1,1,0,1,1,2,1,'A1','forw');
            INSERT INTO mora VALUES (43491,30,31,110,111,85);
            INSERT INTO mora VALUES (43492,30,31,111,112,NULL);
        )SQL");
    }
};

std::vector<int> itemIds(const std::vector<MapItemData> &items, MapItemType type) {
    std::vector<int> ids;
    for (const auto &item : items)
        std::visit([&](const auto &data) { if (data.type == type) ids.push_back(data.id); }, item);
    std::ranges::sort(ids);
    return ids;
}
}

TEST_CASE_FIXTURE(MapDatabaseFixture, "map query filters cached results and reloads outside the cache") {
    MapDataQuery query(path);
    const Rect2D area{{31, 109}, {29, 111}};
    const auto [first, loaded] = query.queryMapItemData(area);
    CHECK(loaded);
    CHECK(itemIds(first, MapItemType::airport) == std::vector<int>{1});
    CHECK(itemIds(first, MapItemType::fix) == std::vector<int>{1});
    CHECK(itemIds(first, MapItemType::navaid) == std::vector<int>{1});
    REQUIRE(itemIds(first, MapItemType::awy) == std::vector<int>{1});
    const auto moraIds = itemIds(first, MapItemType::mora);
    REQUIRE(std::ranges::find(moraIds, 43491) != moraIds.end());
    REQUIRE(std::ranges::find(moraIds, 43492) != moraIds.end());
    for (const auto &item : first) {
        if (const auto *airway = std::get_if<MapAwyData>(&item)) {
            CHECK(airway->ident == "A1");
            CHECK(airway->direct == 'F');
            CHECK(airway->p1 == Point2D{30, 110});
            CHECK(airway->p2 == Point2D{30.5, 110.5});
        }
        if (const auto *mora = std::get_if<MapMoraData>(&item)) {
            CAPTURE(mora->id);
            CHECK(mora->alt == (mora->id == 43491 ? 85 : 1000));
        }
    }
    const auto [panned, reloaded] = query.queryMapItemData({{30.2, 111.4}, {29.8, 111.6}});
    CHECK_FALSE(reloaded);
    CHECK(itemIds(panned, MapItemType::airport) == std::vector<int>{2});
    CHECK(itemIds(panned, MapItemType::navaid).empty());
    const auto [distant, distantReload] = query.queryMapItemData({{11, 179}, {9, -179}});
    CHECK(distantReload);
    CHECK(itemIds(distant, MapItemType::airport) == std::vector<int>{3, 4});
}

TEST_CASE_FIXTURE(MapDatabaseFixture, "overview to detail reloads missing layers and detail to overview reuses cache") {
    MapDataQuery query(path);
    const Rect2D area{{31, 109}, {29, 111}};
    const auto [overview, initialLoad] = query.queryMapItemData(area, true);
    CHECK(initialLoad);
    CHECK(itemIds(overview, MapItemType::airport) == std::vector<int>{1});
    CHECK(itemIds(overview, MapItemType::awy) == std::vector<int>{1});
    CHECK(itemIds(overview, MapItemType::fix).empty());
    CHECK(itemIds(overview, MapItemType::mora).empty());
    CHECK(itemIds(overview, MapItemType::navaid).empty());
    const auto [detail, detailReload] = query.queryMapItemData(area);
    CHECK(detailReload);
    CHECK(itemIds(detail, MapItemType::fix) == std::vector<int>{1});
    CHECK_FALSE(itemIds(detail, MapItemType::mora).empty());
    const auto [overviewAgain, overviewReload] = query.queryMapItemData(area, true);
    CHECK_FALSE(overviewReload);
    CHECK(itemIds(overviewAgain, MapItemType::airport) == std::vector<int>{1});
    CHECK(itemIds(overviewAgain, MapItemType::navaid).empty());
    CHECK(itemIds(overviewAgain, MapItemType::fix).empty());
    CHECK(itemIds(overviewAgain, MapItemType::mora).empty());
}

TEST_CASE_FIXTURE(MapDatabaseFixture, "item details keep types with the same ID distinct") {
    MapDataQuery query(path);
    const auto airport = std::get<MapAirportDetails>(query.queryItemDetails(MapItemType::airport, 1));
    CHECK(airport.icao == "TEST");
    CHECK(airport.name == "Test airport");
    CHECK(airport.altitudeFeet == 1234);
    CHECK(airport.longestRunwayMetres == 2400);
    CHECK(airport.position == Point2D{30, 110});
    const auto fix = std::get<MapFixDetails>(query.queryItemDetails(MapItemType::fix, 1));
    CHECK(fix.ident == "ROUTE");
    const auto nav = std::get<MapNavaidDetails>(query.queryItemDetails(MapItemType::navaid, 1));
    CHECK(nav.type == NavaidType::vordme);
    CHECK(nav.frequency == doctest::Approx(113.5));
    CHECK(nav.altitudeFeet == 1200);
    CHECK(std::get<MapAirportDetails>(query.queryItemDetails(MapItemType::airport, 1)).icao == "TEST");
    CHECK_THROWS_AS(query.queryItemDetails(MapItemType::awy, 1), std::invalid_argument);
}
