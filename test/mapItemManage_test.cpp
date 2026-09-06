#include <doctest.h>

#include <QFile>
#include <QFont>
#include <QGraphicsSimpleTextItem>
#include <QImage>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
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
                 45.0, 0.5, 0.5},
                {int64_t{102}, std::string{"WEST"}, std::string{"West Date Line"}, int64_t{10}, int64_t{1000},
                 90.0, 0.5, 179.8},
                {int64_t{103}, std::string{"EAST"}, std::string{"East Date Line"}, int64_t{10}, int64_t{1000},
                 135.0, 0.5, -179.8}
            });
            database.addRecords("fix", {
                {int64_t{201}, std::string{"FIX01"}, 0.75, 0.75},
                {int64_t{202}, std::string{"ORPHAN"}, 0.50, 0.75}
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
                {int64_t{401}, int64_t{401}, int64_t{0}, int64_t{1}, int64_t{201}, int64_t{1}, int64_t{0},
                 std::string{"A401"}, std::string{"both"}},
                {int64_t{402}, int64_t{402}, int64_t{0}, int64_t{1}, int64_t{0}, int64_t{1}, int64_t{0},
                 std::string{"A402"}, std::string{"both"}}
            });
            database.addRecords("fir", {
                {int64_t{501}, int64_t{0}, std::string{"ZAAA"}, 0.0, 0.0, 0.0, 1.0,
                 std::string{"Test FIR"}, std::string{}},
                {int64_t{502}, int64_t{1}, std::string{"ZAAA"}, 0.0, 1.0, 1.0, 1.0,
                 std::string{"Test FIR"}, std::string{}},
                // 相邻 FIR 以相反端点顺序重复保存 502 的边界。
                {int64_t{503}, int64_t{0}, std::string{"ZHAA"}, 1.0, 1.0, 0.0, 1.0,
                 std::string{"Adjacent FIR"}, std::string{}}
            });
            database.addRecords("fir_rtree", {
                {int64_t{501}, 0.0, 1.0, 0.0, 0.0},
                {int64_t{502}, 1.0, 1.0, 0.0, 1.0},
                {int64_t{503}, 1.0, 1.0, 0.0, 1.0}
            });
            database.addRecords("mora", {
                {int64_t{32581}, 0.0, 1.0, 0.0, 1.0, int64_t{5000}},
                {int64_t{57781}, 70.0, 71.0, 0.0, 1.0, int64_t{7000}}
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
    CHECK(airportSymbol().boundingRect() == QRectF(-6.0, -6.0, 12.0, 12.0));
    CHECK(fixSymbol().boundingRect() == QRectF(-5.0, -5.0, 10.0, 10.0));
    CHECK(fixSymbol().elementAt(0).x == doctest::Approx(0.0));
    CHECK(fixSymbol().elementAt(0).y == doctest::Approx(-5.0));
    CHECK(fixSymbol().elementAt(1).x == doctest::Approx(5.0));
    CHECK(fixSymbol().elementAt(1).y == doctest::Approx(5.0));
    CHECK(vorSymbol().elementCount() > fixSymbol().elementCount());
    CHECK(vorSymbol().elementCount() == 7);
    CHECK(vorSymbol().boundingRect() == QRectF(-6.0, -6.0, 12.0, 12.0));
    CHECK(dmeSymbol().boundingRect() == QRectF(-6.0, -6.0, 12.0, 12.0));
    CHECK(dmeSymbol().elementCount() == 5);
    CHECK(vordmeSymbol().elementCount() > vorSymbol().elementCount());
    CHECK(vordmeSymbol().elementCount() == 12);
    CHECK(ndbSymbol().elementCount() > dmeSymbol().elementCount());
    CHECK(ndbSymbol().boundingRect() == QRectF(-6.0, -6.0, 12.0, 12.0));
    CHECK(fixSymbol(20.0).boundingRect() == QRectF(-10.0, -10.0, 20.0, 20.0));
    CHECK_THROWS_AS(vorSymbol(0.0), std::invalid_argument);
    CHECK_FALSE(moraSymbol().isEmpty());
    CHECK_FALSE(firSymbol().isEmpty());
    CHECK(awySymbol().elementCount() == 2);
    CHECK(awySymbol().boundingRect() == QRectF(-8.0, 0.0, 16.0, 0.0));
}

TEST_CASE("NDB uses broken rings with a solid center point") {
    const MapNavData ndb{
        .ident = QStringLiteral("NDB1"),
        .realPos = {0.0, 0.0},
        .id = 302,
        .type = MapItemType::navaid,
        .navType = NavaidType::ndb
    };
    MapPointItem item(ndb, ndbSymbol());

    CHECK(item.symbol().elementCount() == 26);
    CHECK(item.pen().style() == Qt::CustomDashLine);
    REQUIRE(item.pen().dashPattern().size() == 2);
    CHECK(item.pen().dashPattern().at(0) == doctest::Approx(2.0));
    CHECK(item.pen().dashPattern().at(1) == doctest::Approx(2.0));
    CHECK(item.pen().color() == QColor(QStringLiteral("#800000")));
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
    airwayItem.setAirwayLabelSegments({QLineF{{0.0, 0.0}, {10.0, 10.0}},
                                        QLineF{{10.0, 10.0}, {10.0, 20.0}}});
    CHECK(airwayItem.mapDataItems().size() == 2);
    CHECK(airwayItem.findData(MapItemType::awy, 2) != nullptr);
    REQUIRE(airwayItem.childItems().size() == 2);
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[0])->text() == QStringLiteral("A1"));
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[1])->text() == QStringLiteral("B2"));
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[0])->rotation()
          == doctest::Approx(45.0));
    CHECK(dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[1])->rotation()
          == doctest::Approx(90.0));
    const auto *firstLabel = dynamic_cast<QGraphicsSimpleTextItem*>(airwayItem.childItems()[0]);
    CHECK(firstLabel->pos() == QPointF(5.0, 5.0));
    CHECK(firstLabel->transform().map(firstLabel->boundingRect().center()).x() == doctest::Approx(0.0));
    CHECK(firstLabel->transform().map(firstLabel->boundingRect().center()).y() == doctest::Approx(0.0));

    const MapFirData fir{
        .ident = QStringLiteral("TEST"), .p1 = {0.0, 0.0}, .p2 = {1.0, 1.0},
        .id = 3, .type = MapItemType::fir
    };
    MapPathItem firItem(fir, airwayPath);
    CHECK(firItem.childItems().empty());
    CHECK(firItem.pen().style() == Qt::CustomDashLine);
    CHECK(firItem.pen().widthF() == doctest::Approx(5.0));
    CHECK(firItem.pen().dashPattern() == QList<qreal>{2.0, 2.0});
    CHECK(firItem.brush().style() == Qt::NoBrush);
}

TEST_CASE("one-way airway arrows keep a visible screen size") {
    const MapAwyData forward{
        .ident = QStringLiteral("FWD"), .p1 = {0.0, 0.0}, .p2 = {1.0, 1.0},
        .id = 1, .direct = 'F', .type = MapItemType::awy
    };
    QPainterPath line(QPointF{0.0, 0.0});
    line.lineTo(1000.0, 0.0);
    MapPathItem item(forward, line);
    CHECK(item.boundingRect().width() < 10000.0);

    QImage image(120, 40, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.translate(10.0, 20.0);
    painter.scale(0.1, 0.1);
    QStyleOptionGraphicsItem option;
    item.paint(&painter, &option, nullptr);
    painter.end();

    QRect painted;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) > 0)
                painted |= QRect(x, y, 1, 1);
    // The arrow is centered at screen x=60 and points right. Its upper edge
    // must be outside the one-pixel route line, even after map scaling.
    CHECK(painted.x() == 10);
    CHECK(painted.y() == 18);
    CHECK(painted.width() == 101);
    CHECK(painted.height() == 5);
    CHECK(qAlpha(image.pixel(59, 20)) > 0);
}

TEST_CASE("MORA labels use hundreds of feet with large translucent text") {
    const MapMoraData mora{
        .bounds = {{1.0, 0.0}, {0.0, 1.0}},
        .id = 1,
        .alt = 5000,
        .type = MapItemType::mora
    };
    QPainterPath grid;
    grid.addRect(0.0, 0.0, 100.0, 100.0);
    MapPointItem item(mora, grid, false);
    CHECK(item.label() == QStringLiteral("50"));
    REQUIRE(item.childItems().size() == 1);
    const auto *label = dynamic_cast<QGraphicsSimpleTextItem*>(item.childItems().front());
    REQUIRE(label != nullptr);
    CHECK_FALSE(label->flags().testFlag(QGraphicsItem::ItemIgnoresTransformations));
    CHECK(label->boundingRect().height() == doctest::Approx(grid.boundingRect().height() * 0.20).epsilon(0.08));
    CHECK(label->opacity() == doctest::Approx(0.35));
}

TEST_CASE("map item manager applies the 1x 2x 3x cache strategy") {
    MapItemTestDatabase fixture;
    MapItemManage manager(fixture.path());
    const Rect2D initialViewport{{1.0, 0.0}, {0.0, 1.0}};

    REQUIRE(manager.updateViewport(initialViewport));
    CHECK(manager.hasCache());
    CHECK(manager.itemBound().first.first == doctest::Approx(1.5));
    CHECK(manager.itemBound().second.first == doctest::Approx(-0.5));
    CHECK(getLongiSpan(manager.itemBound().first.second, manager.itemBound().second.second)
          == doctest::Approx(2.0));
    CHECK(!manager.projectedBound().isEmpty());

    REQUIRE(manager.findItem(MapItemType::airport, 101) != nullptr);
    REQUIRE(manager.findData(MapItemType::fix, 201) != nullptr);
    CHECK(manager.findData(MapItemType::fix, 202) == nullptr);
    CHECK(std::holds_alternative<MapApData>(*manager.findData(MapItemType::airport, 101)));
    const auto *airportItem = dynamic_cast<const MapPointItem*>(manager.findItem(MapItemType::airport, 101));
    REQUIRE(airportItem != nullptr);
    const auto &airportPath = airportItem->symbol();
    REQUIRE(airportPath.elementCount() >= 2);
    CHECK(airportPath.elementAt(airportPath.elementCount() - 2).x == doctest::Approx(std::sqrt(8.0)));
    CHECK(airportPath.elementAt(airportPath.elementCount() - 2).y == doctest::Approx(-std::sqrt(8.0)));
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
    auto *adjacentFir = dynamic_cast<MapPathItem*>(manager.findItem(MapItemType::fir, 503));
    REQUIRE(firstFir != nullptr);
    REQUIRE(secondFir != nullptr);
    REQUIRE(adjacentFir != nullptr);
    CHECK(firstFir == secondFir);
    CHECK(firstFir == adjacentFir);
    CHECK(firstFir->mapDataItems().size() == 3);
    // 503 的反向重复边仍保留数据索引，但只生成一次绘制几何。
    CHECK(firstFir->path().elementCount() == 3);
    CHECK(firstFir->childItems().empty());
    const auto airportChildren = manager.findItem(MapItemType::airport, 101)->childItems();
    REQUIRE_FALSE(airportChildren.empty());
    CHECK(manager.dataForItem(airportChildren.front()) == manager.findData(MapItemType::airport, 101));

    auto *fixItem = dynamic_cast<MapPointItem*>(manager.findItem(MapItemType::fix, 201));
    REQUIRE(fixItem != nullptr);
    REQUIRE(fixItem->childItems().size() == 1);
    auto *airportLabel = dynamic_cast<QGraphicsSimpleTextItem*>(airportChildren.front());
    auto *fixLabel = dynamic_cast<QGraphicsSimpleTextItem*>(fixItem->childItems().front());
    REQUIRE(airportLabel != nullptr);
    REQUIRE(fixLabel != nullptr);
    auto *moraItem = dynamic_cast<MapPointItem*>(manager.findItem(MapItemType::mora, 32581));
    REQUIRE(moraItem != nullptr);
    CHECK(moraItem->symbol().elementCount() > 4);
    REQUIRE(moraItem->childItems().size() == 1);
    auto *moraLabel = dynamic_cast<QGraphicsSimpleTextItem*>(moraItem->childItems().front());
    REQUIRE(moraLabel != nullptr);

    manager.setZoomLevel(0);
    CHECK(airportLabel->isVisible());
    CHECK(fixLabel->isVisible());
    CHECK(firstAirway->childItems().front()->isVisible());
    CHECK(moraItem->isVisible());
    CHECK(moraLabel->isVisible());

    manager.setZoomLevel(2);
    CHECK(airportLabel->isVisible());
    CHECK_FALSE(fixLabel->isVisible());
    CHECK_FALSE(firstAirway->childItems().front()->isVisible());
    CHECK(moraItem->isVisible());
    CHECK(moraLabel->isVisible());

    manager.setZoomLevel(3);
    CHECK(airportLabel->isVisible());
    CHECK_FALSE(moraItem->isVisible());

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

TEST_CASE("map item manager rebuilds a smaller cache when zooming back in") {
    MapItemTestDatabase fixture;
    MapItemManage manager(fixture.path());

    manager.setZoomLevel(3);
    REQUIRE(manager.refresh({{10.0, 0.0}, {-10.0, 20.0}}));
    const double wideLatitudeSpan = manager.itemBound().first.first - manager.itemBound().second.first;
    const auto wideProjection = manager.project({{0.5, 0.5}});
    REQUIRE(wideProjection.size() == 1);

    manager.setZoomLevel(1);
    REQUIRE(manager.refresh({{1.0, 0.0}, {0.0, 1.0}}));
    CHECK(manager.itemBound().first.first - manager.itemBound().second.first < wideLatitudeSpan);
    const auto narrowProjection = manager.project({{0.5, 0.5}});
    REQUIRE(narrowProjection.size() == 1);
    CHECK(std::abs(wideProjection.front().first - narrowProjection.front().first) > 1.0);
    CHECK(std::abs(wideProjection.front().second - narrowProjection.front().second) > 1.0);
    CHECK(manager.findItem(MapItemType::mora, 32581) != nullptr);

    manager.setZoomLevel(3);
    REQUIRE(manager.refresh({{1.0, 0.0}, {0.0, 1.0}}));
    CHECK(manager.findItem(MapItemType::mora, 32581) == nullptr);
}

TEST_CASE("adjacent high-latitude MORA frames share an edge without overlap") {
    MapItemTestDatabase fixture;
    MapItemManage manager(fixture.path());
    manager.setZoomLevel(1);

    REQUIRE(manager.refresh({{71.0, 0.0}, {70.0, 1.0}}));
    const auto *west = dynamic_cast<const MapPointItem*>(manager.findItem(MapItemType::mora, 57781));
    const auto *east = dynamic_cast<const MapPointItem*>(manager.findItem(MapItemType::mora, 57782));
    REQUIRE(west != nullptr);
    REQUIRE(east != nullptr);

    const QPainterPath &westFrame = west->symbol();
    const QPainterPath &eastFrame = east->symbol();
    REQUIRE(westFrame.elementCount() >= 4);
    REQUIRE(eastFrame.elementCount() >= 4);
    // west top-right/bottom-right == east top-left/bottom-left.
    CHECK(westFrame.elementAt(1).x == doctest::Approx(eastFrame.elementAt(0).x));
    CHECK(westFrame.elementAt(1).y == doctest::Approx(eastFrame.elementAt(0).y));
    CHECK(westFrame.elementAt(2).x == doctest::Approx(eastFrame.elementAt(3).x));
    CHECK(westFrame.elementAt(2).y == doctest::Approx(eastFrame.elementAt(3).y));
}
