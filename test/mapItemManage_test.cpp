#include <doctest.h>

#include <QFile>
#include <QGraphicsSimpleTextItem>
#include <QTemporaryDir>

#include <cmath>
#include <stdexcept>
#include <variant>

#include "services/mapItemManage.hpp"


namespace {

class MapItemTestDatabase {
    public:
        MapItemTestDatabase () {
            databasePath = directory.filePath("map_item_manage.db");
            if (!QFile::copy(QStringLiteral(CHARTNAVI_TEST_DATABASE_PATH), databasePath))
                throw std::runtime_error("failed to copy the test database");

            Database database(databasePath.toStdString());
            database.addRecords("airport", {
                {int64_t{101}, std::string{"TEST"}, std::string{"Test Airport"}, int64_t{10}, int64_t{1000},
                 0.5, 0.5},
                {int64_t{102}, std::string{"WEST"}, std::string{"West Date Line"}, int64_t{10}, int64_t{1000},
                 0.5, 179.8},
                {int64_t{103}, std::string{"EAST"}, std::string{"East Date Line"}, int64_t{10}, int64_t{1000},
                 0.5, -179.8}
            });
            database.addRecords("fix", {
                {int64_t{201}, std::string{"FIX01"}, 0.75, 0.75}
            });
            database.addRecords("navaid", {
                {int64_t{301}, std::string{"VOR01"}, int64_t{1}, 113.0, int64_t{20}, std::string{"Test VOR"},
                 0.25, 0.25}
            });
            database.addRecords("awy", {
                {int64_t{401}, 0.0, 0.0, 1.0, 1.0},
                {int64_t{402}, 1.0, 1.0, 1.0, 0.0}
            });
            database.addRecords("awy_idx", {
                {int64_t{401}, int64_t{401}, int64_t{0}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"A401"}},
                {int64_t{402}, int64_t{402}, int64_t{0}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"A402"}}
            });
            database.addRecords("fir", {
                {int64_t{501}, int64_t{0}, std::string{"ZAAA"}, 0.0, 0.0, 0.0, 1.0,
                 std::string{"Test FIR"}, std::string{}},
                {int64_t{502}, int64_t{1}, std::string{"ZAAA"}, 0.0, 1.0, 1.0, 1.0,
                 std::string{"Test FIR"}, std::string{}}
            });
            database.addRecords("fir_rtree", {
                {int64_t{501}, 0.0, 1.0, 0.0, 0.0},
                {int64_t{502}, 1.0, 1.0, 0.0, 1.0}
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

bool isFinite (const Point2D &point) {
    return std::isfinite(point.first) && std::isfinite(point.second);
}

} // namespace


TEST_CASE("map graphics items preserve data and enforce their geometry category") {
    const MapAwyData airway{
        .ident = QStringLiteral("A1"),
        .p1 = {0.0, 0.0},
        .p2 = {1.0, 1.0},
        .id = 1,
        .type = MapItemType::awy
    };
    QPainterPath line;
    line.lineTo(10.0, 10.0);
    MapPathItem pathItem(airway, line);
    CHECK(pathItem.itemType() == MapItemType::awy);
    CHECK(pathItem.itemId() == 1);
    CHECK(pathItem.label() == QStringLiteral("A1"));
    CHECK(std::holds_alternative<MapAwyData>(pathItem.mapData()));
    CHECK(pathItem.shape().boundingRect().width() > pathItem.path().boundingRect().width());

    const MapApData airport{
        .icao = QStringLiteral("TEST"),
        .realPos = {0.5, 0.5},
        .id = 2,
        .type = MapItemType::airport
    };
    QPainterPath symbol;
    symbol.addEllipse(QPointF{}, 4.0, 4.0);
    MapPointItem pointItem(airport, symbol);
    CHECK(pointItem.itemType() == MapItemType::airport);
    CHECK(pointItem.itemId() == 2);
    CHECK(pointItem.label() == QStringLiteral("TEST"));
    CHECK(!pointItem.boundingRect().isEmpty());
    CHECK(pointItem.boundingRect().contains(pointItem.shape().boundingRect()));

    pointItem.setDetail(MapItemDetail::symbolOnly);
    pathItem.setDetail(MapItemDetail::symbolOnly);
    CHECK(pointItem.detail() == MapItemDetail::symbolOnly);
    CHECK(pathItem.detail() == MapItemDetail::symbolOnly);

    CHECK_THROWS_AS(MapPathItem(airport, line), std::invalid_argument);
    CHECK_THROWS_AS(MapPointItem(airway, symbol), std::invalid_argument);
}

TEST_CASE("map symbols follow the vector shapes used by the reference map") {
    CHECK_FALSE(airportSymbol().isEmpty());
    CHECK_FALSE(fixSymbol().isEmpty());
    CHECK(vorSymbol().elementCount() > fixSymbol().elementCount());
    CHECK(dmeSymbol().boundingRect() == QRectF(-5.0, -5.0, 10.0, 10.0));
    CHECK(vordmeSymbol().elementCount() > vorSymbol().elementCount());
    CHECK(ndbSymbol().elementCount() > dmeSymbol().elementCount());
    CHECK_FALSE(moraSymbol().isEmpty());
    CHECK_FALSE(firSymbol().isEmpty());
    CHECK_FALSE(awySymbol().isEmpty());
}

TEST_CASE("merged path items retain airway segment labels and suppress FIR labels") {
    const MapAwyData first{
        .ident = QStringLiteral("A1"), .p1 = {0.0, 0.0}, .p2 = {1.0, 1.0},
        .id = 1, .type = MapItemType::awy
    };
    const MapAwyData second{
        .ident = QStringLiteral("B2"), .p1 = {1.0, 1.0}, .p2 = {2.0, 1.0},
        .id = 2, .type = MapItemType::awy
    };
    QPainterPath airwayPath(QPointF{0.0, 0.0});
    airwayPath.lineTo(10.0, 10.0);
    airwayPath.lineTo(10.0, 20.0);
    MapPathItem airwayItem(
        std::vector<MapItemData>{first, second}, airwayPath,
        std::vector<QPointF>{{5.0, 5.0}, {10.0, 15.0}});
    CHECK(airwayItem.mapDataItems().size() == 2);
    CHECK(airwayItem.findData(MapItemType::awy, 2) != nullptr);
    REQUIRE(airwayItem.childItems().size() == 2);
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[0])->text() == QStringLiteral("A1"));
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[1])->text() == QStringLiteral("B2"));

    const MapFirData fir{
        .ident = QStringLiteral("TEST"), .p1 = {0.0, 0.0}, .p2 = {1.0, 1.0},
        .id = 3, .type = MapItemType::fir
    };
    MapPathItem firItem(fir, airwayPath);
    CHECK(firItem.childItems().empty());
}

TEST_CASE("map item manager applies the 1x 2x 3x cache strategy") {
    MapItemTestDatabase fixture;
    MapItemManage manager(fixture.path());
    const Rect2D initialViewport{{1.0, 0.0}, {0.0, 1.0}};

    REQUIRE(manager.updateViewport(initialViewport));
    CHECK(manager.hasCache());
    CHECK(manager.itemBound().first.first == doctest::Approx(1.5));
    CHECK(manager.itemBound().second.first == doctest::Approx(-0.5));
    CHECK(getLongiRange(manager.itemBound().first.second, manager.itemBound().second.second)
          == doctest::Approx(2.0));
    CHECK(!manager.projectedBound().isEmpty());

    REQUIRE(manager.findItem(MapItemType::airport, 101) != nullptr);
    REQUIRE(manager.findData(MapItemType::fix, 201) != nullptr);
    CHECK(std::holds_alternative<MapApData>(*manager.findData(MapItemType::airport, 101)));
    CHECK(std::holds_alternative<MapNavData>(*manager.findData(MapItemType::navaid, 301)));
    CHECK(std::holds_alternative<MapAwyData>(*manager.findData(MapItemType::awy, 401)));
    auto *firstAirway = dynamic_cast<MapPathItem*>(manager.findItem(MapItemType::awy, 401));
    auto *secondAirway = dynamic_cast<MapPathItem*>(manager.findItem(MapItemType::awy, 402));
    REQUIRE(firstAirway != nullptr);
    REQUIRE(secondAirway != nullptr);
    CHECK(firstAirway == secondAirway);
    CHECK(firstAirway->mapDataItems().size() == 2);
    CHECK(firstAirway->path().elementCount() == 3);
    REQUIRE(firstAirway->childItems().size() == 2);
    for (const auto *child : firstAirway->childItems()) {
        const auto *segmentData = manager.dataForItem(child);
        REQUIRE(segmentData != nullptr);
        CHECK(std::holds_alternative<MapAwyData>(*segmentData));
        CHECK(std::get<MapAwyData>(*segmentData).ident
              == dynamic_cast<const QGraphicsSimpleTextItem*>(child)->text());
    }
    auto *firstFir = dynamic_cast<MapPathItem*>(manager.findItem(MapItemType::fir, 501));
    auto *secondFir = dynamic_cast<MapPathItem*>(manager.findItem(MapItemType::fir, 502));
    REQUIRE(firstFir != nullptr);
    REQUIRE(secondFir != nullptr);
    CHECK(firstFir == secondFir);
    CHECK(firstFir->mapDataItems().size() == 2);
    CHECK(firstFir->path().elementCount() == 3);
    CHECK(firstFir->childItems().empty());
    const auto airportChildren = manager.findItem(MapItemType::airport, 101)->childItems();
    REQUIRE_FALSE(airportChildren.empty());
    CHECK(manager.dataForItem(airportChildren.front()) == manager.findData(MapItemType::airport, 101));

    const auto oldAirportItem = manager.findItem(MapItemType::airport, 101);
    CHECK_FALSE(manager.updateViewport({{1.2, 0.2}, {0.2, 1.2}}));
    CHECK(manager.findItem(MapItemType::airport, 101) == oldAirportItem);

    CHECK(manager.updateViewport({{4.0, 3.0}, {3.0, 4.0}}));
    CHECK(manager.findItem(MapItemType::airport, 101) == nullptr);

    manager.clear();
    CHECK_FALSE(manager.hasCache());
    CHECK(manager.items().empty());
}

TEST_CASE("map item manager projects a viewport crossing the date line") {
    MapItemTestDatabase fixture;
    MapItemManage manager(fixture.path());

    REQUIRE(manager.updateViewport({{1.0, 179.5}, {0.0, -179.5}}));
    REQUIRE(manager.findItem(MapItemType::airport, 102) != nullptr);
    REQUIRE(manager.findItem(MapItemType::airport, 103) != nullptr);

    const auto positions = manager.project({{0.5, 179.8}, {0.5, -179.8}});
    REQUIRE(positions.size() == 2);
    CHECK(isFinite(positions[0]));
    CHECK(isFinite(positions[1]));
    CHECK(positions[0].first < positions[1].first);
}
