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
    constexpr int latitudeCell{0};
    constexpr int westDateLineId{32401};
    constexpr int eastDateLineId{32760};

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

class MapDataTestDatabase {
    public:
        MapDataTestDatabase () {
            databasePath = directory.filePath("map_data_query.db");
            if (!QFile::copy(QStringLiteral(CHARTNAVI_TEST_DATABASE_PATH), databasePath))
                throw std::runtime_error("failed to copy the test database");

            Database database(databasePath.toStdString());
            database.addRecords("awy", {
                {int64_t{1}, 3.5, 176.19333333, 6.51979159, 179.999},
                {int64_t{2}, 6.51979159, -179.999, 8.99111111, -176.88472222},
                {int64_t{3}, 30.0, 10.0, 31.0, 11.0}
            });
            database.addRecords("awy_idx", {
                {int64_t{1}, int64_t{42}, int64_t{1}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"TEST"}},
                {int64_t{2}, int64_t{42}, int64_t{2}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"TEST"}},
                {int64_t{3}, int64_t{43}, int64_t{0}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"SINGLE"}}
            });
            database.addRecords("fir", {
                {int64_t{1}, int64_t{0}, std::string{"TESTFIR"}, 0.0, 179.0, 0.0, -179.0,
                 std::string{"Test FIR"}, std::string{"TEST"}}
            });
            database.addRecords("fir_rtree", {
                {int64_t{1}, -179.0, 179.0, 0.0, 0.0}
            });
            database.addRecords("mora", {
                {int64_t{32581}, 0.0, 1.0, 0.0, 1.0, null},
                {int64_t{32582}, 0.0, 1.0, 1.0, 2.0, int64_t{12000}},
                {int64_t{32401}, 0.0, 1.0, -180.0, -179.0, int64_t{8000}},
                {int64_t{32760}, 0.0, 1.0, 179.0, 180.0, int64_t{9000}}
            });
            database.commit();
        }

        [[nodiscard]] QString path () const {
            return databasePath;
        }

    private:
        QTemporaryDir directory;
        QString databasePath;
};

int countAirways (const std::vector<MapItemData> &items) {
    int count{};
    for (const auto &item : items)
        count += std::holds_alternative<MapAwyData>(item);
    return count;
}

int countFirs (const std::vector<MapItemData> &items) {
    int count{};
    for (const auto &item : items)
        count += std::holds_alternative<MapFirData>(item);
    return count;
}

TEST_CASE("map data query") {
    MapDataTestDatabase fixture;
    MapDataQuery query(fixture.path());

    SUBCASE("handles dateline ranges and cache reuse") {
        const Rect2D wideBound{{10.0, -20.0}, {-10.0, 180.0}};
        const auto wide = query.queryMapItemData(wideBound);
        CHECK(countAirways(wide) == 1);
        CHECK(countFirs(wide) == 1);

        const Rect2D equatorialBound{{10.0, 0.0}, {-10.0, 10.0}};
        const auto equatorial = query.queryMapItemData(equatorialBound);
        CHECK(countAirways(equatorial) == 0);
        CHECK(countFirs(equatorial) == 0);
    }

    SUBCASE("accepts longitudes outside the canonical range") {
        const Rect2D bound{{10.0, 170.0}, {-10.0, 190.0}};
        const auto items = query.queryMapItemData(bound);
        CHECK(countAirways(items) == 1);
        CHECK(countFirs(items) == 1);
    }

    SUBCASE("accepts an explicit full longitude range") {
        const Rect2D worldBound{{10.0, -180.0}, {-10.0, 180.0}};
        const auto items = query.queryMapItemData(worldBound);
        CHECK(countAirways(items) == 1);
        CHECK(countFirs(items) == 1);
    }

    SUBCASE("returns a single-segment airway from the batch query") {
        const auto items = query.queryMapItemData({{32.0, 9.0}, {29.0, 12.0}});
        bool found{};
        for (const auto &item : items) {
            if (!std::holds_alternative<MapAwyData>(item))
                continue;
            const auto &airway = std::get<MapAwyData>(item);
            if (airway.id == 3) {
                CHECK(airway.ident == QStringLiteral("SINGLE"));
                CHECK(airway.p1 == Point2D(30.0, 10.0));
                CHECK(airway.p2 == Point2D(31.0, 11.0));
                found = true;
            }
        }
        CHECK(found);
    }

    SUBCASE("uses the MORA altitude and defaults NULL to 1000") {
        const auto items = query.queryMapItemData({{2.0, 0.0}, {0.0, 2.0}});
        bool foundNullAltitude{};
        bool foundStoredAltitude{};
        for (const auto &item : items) {
            if (!std::holds_alternative<MapMoraData>(item))
                continue;
            const auto &mora = std::get<MapMoraData>(item);
            if (mora.id == 32581) {
                CHECK(mora.alt == 1000);
                foundNullAltitude = true;
            } else if (mora.id == 32582) {
                CHECK(mora.alt == 12000);
                foundStoredAltitude = true;
            }
        }
        CHECK(foundNullAltitude);
        CHECK(foundStoredAltitude);
    }

    SUBCASE("batch-loads MORA altitudes across the date line") {
        const auto items = query.queryMapItemData({{1.0, 179.5}, {0.0, -179.5}});
        bool foundWest{};
        bool foundEast{};
        for (const auto &item : items) {
            if (!std::holds_alternative<MapMoraData>(item))
                continue;
            const auto &mora = std::get<MapMoraData>(item);
            if (mora.id == 32401) {
                CHECK(mora.alt == 8000);
                foundWest = true;
            } else if (mora.id == 32760) {
                CHECK(mora.alt == 9000);
                foundEast = true;
            }
        }
        CHECK(foundWest);
        CHECK(foundEast);
    }
}
