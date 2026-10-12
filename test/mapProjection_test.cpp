#include <doctest.h>
#include "utils/geographic.hpp"
#include "utils/constValue.hpp"

#include <array>
#include <cmath>
#include <limits>

namespace {
constexpr std::array projectionTypes{
    MapProjection::Type::dynamicLCC, MapProjection::Type::webMercator,
    MapProjection::Type::gnomonic
};

void checkPoint(Point2D actual, Point2D expected, double tolerance = 1e-7) {
    CHECK(std::abs(actual.first - expected.first) <= tolerance);
    CHECK(std::abs(actual.second - expected.second) <= tolerance);
}

void checkUnavailable(Point2D point) {
    CHECK(std::isnan(point.first));
    CHECK(std::isnan(point.second));
}
}

TEST_CASE("map projections preserve coordinates across the date line") {
    for (const auto type : projectionTypes) {
        CAPTURE(static_cast<int>(type));
        MapProjection projection(type);
        projection.reset(170, -170, 0, 20);
        const std::vector<Point2D> geographic{{5, 179}, {10, -179}, {15, 175}};
        const auto projected = projection.trans(geographic);
        const auto restored = projection.revertTrans(projected);
        REQUIRE(restored.size() == geographic.size());
        for (std::size_t i = 0; i < geographic.size(); ++i)
            checkPoint(restored[i], geographic[i]);

        auto inPlace = geographic;
        projection.transInPlace(inPlace);
        for (std::size_t i = 0; i < geographic.size(); ++i) {
            checkPoint(inPlace[i], projected[i]);
            checkPoint(projection.trans(geographic[i]), projected[i]);
            checkPoint(projection.revertTrans(projected[i]), geographic[i]);
        }
        projection.revertTransInPlace(inPlace);
        for (std::size_t i = 0; i < geographic.size(); ++i)
            checkPoint(inPlace[i], geographic[i]);

        checkPoint(projection.trans(Point2D{10, 180}), projection.trans(Point2D{10, -180}));
        CHECK(projection.trans(Point2D{10, 179}).first < projection.trans(Point2D{10, -179}).first);
        CHECK(projection.trans(Point2D{15, 180}).second < projection.trans(Point2D{5, 180}).second);
    }
}

TEST_CASE("map projection reset invalidates stale state and can recover") {
    for (const auto type : projectionTypes) {
        CAPTURE(static_cast<int>(type));
        MapProjection projection(type);
        checkUnavailable(projection.trans(Point2D{30, 110}));
        checkUnavailable(projection.revertTrans(Point2D{0, 0}));
        projection.reset(100, 120, 20, 40);
        const auto original = projection.trans(Point2D{30, 110});
        REQUIRE(std::isfinite(original.first));
        projection.reset(100, 120, 20, 90); // Outside the supported latitude range.
        checkUnavailable(projection.trans(Point2D{30, 110}));
        checkUnavailable(projection.revertTrans(original));
        projection.reset(100, 120, 20, 40);
        checkPoint(projection.trans(Point2D{30, 110}), original);
        checkUnavailable(projection.trans(Point2D{81, 110}));
        checkUnavailable(projection.revertTrans(Point2D{std::numeric_limits<double>::infinity(), 0}));
    }
}

TEST_CASE("LCC wrapper preserves the existing projection") {
    DynamicLCC reference;
    reference.reset(100, 120, 20, 40);
    MapProjection projection;
    projection.reset(100, 120, 20, 40);
    for (const Point2D point : {Point2D{25, 105}, Point2D{35, 115}})
        checkPoint(projection.trans(point), reference.trans(point));
}

TEST_CASE("Web Mercator uses metre coordinates with a north-west origin") {
    MapProjection projection(MapProjection::Type::webMercator);
    projection.reset(-10, 10, -10, 10);
    checkPoint(projection.trans(Point2D{10, -10}), {0, 0});
    const auto equator = projection.trans(Point2D{0, 0});
    // Fixed spherical Web Mercator references, in metres.
    checkPoint(equator, {1113194.907932736, 1118889.974857960}, 1e-6);
    const auto east = projection.trans(Point2D{0, 1});
    CHECK(std::abs(east.first - equator.first - 111319.490793274) < 1e-6);
}

TEST_CASE("gnomonic projection has the expected scale and visible hemisphere") {
    MapProjection projection(MapProjection::Type::gnomonic);
    projection.reset(-10, 10, -10, 10);
    checkPoint(projection.trans(Point2D{0, 0}), {0, 0});
    // At 45 degrees from the tangent point, distance on the plane is one radius.
    checkPoint(projection.trans(Point2D{0, 45}), {avgEarthRadius, 0});
    checkPoint(projection.trans(Point2D{45, 0}), {0, -avgEarthRadius});
    CHECK(std::isfinite(projection.trans(Point2D{0, 89}).first));
    checkUnavailable(projection.trans(Point2D{0, 90}));
    checkUnavailable(projection.trans(Point2D{0, 120}));
}

TEST_CASE("gnomonic maps a great circle to a straight line away from the origin") {
    MapProjection projection(MapProjection::Type::gnomonic);
    projection.reset(100, 120, 20, 40);
    checkPoint(projection.trans(Point2D{30, 110}), {0, 0});
    checkPoint(projection.revertTrans(Point2D{0, 0}), {30, 110});
    // Three points on the equator, a great circle not through this tangent point.
    const auto a = projection.trans(Point2D{0, 100});
    const auto b = projection.trans(Point2D{0, 110});
    const auto c = projection.trans(Point2D{0, 120});
    CHECK(a.first < b.first);
    CHECK(b.first < c.first);
    const double perpendicularDistance = std::abs((b.first - a.first) * (c.second - a.second)
        - (b.second - a.second) * (c.first - a.first)) / std::hypot(c.first - a.first, c.second - a.second);
    CHECK(perpendicularDistance < 1e-6);
}
