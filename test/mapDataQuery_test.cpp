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
