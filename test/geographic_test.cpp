#include <doctest.h>
#include "utils/geographic.hpp"
#include <GeographicLib/Geodesic.hpp>
#include <algorithm>
#include <cmath>
#include <vector>


double geographicLibDistance (const Point2D &from, const Point2D &to) {
    double distance{};
    GeographicLib::Geodesic::WGS84().Inverse(from.first, from.second, to.first, to.second, distance);
    return distance;
}

void checkDistance (const Point2D &from, const Point2D &to) {
    const double reference = geographicLibDistance(from, to);
    CHECK(std::abs(distanceSimple(from, to) - reference) / reference <= 0.05);
}

double geographicLibBearing (const Point2D &from, const Point2D &to) {
    double distance{};
    double bearing{};
    GeographicLib::Geodesic::WGS84().Inverse(from.first, from.second, to.first, to.second,
                                              distance, bearing);
    return std::fmod(bearing + 360.0, 360.0);
}

void checkBearing (const Point2D &from, const Point2D &to) {
    const double expected = geographicLibBearing(from, to);
    const double actual = bearingSimple(from, to);
    const double difference = std::abs(actual - expected);
    const double circularDifference = std::min(difference, 360.0 - difference);
    CHECK(circularDifference / std::max(expected, 1.0) <= 0.05);
}


TEST_CASE("point distance") {
    SUBCASE("normal 1") {
        checkDistance({31.23, 121.47}, {39.90, 116.40});
    }
    SUBCASE("normal 2") {
        checkDistance({0.0, 0.0}, {20.0, 30.0});
    }
    SUBCASE("pole") {
        checkDistance({70.0, -20.0}, {70.0, 20.0});
    }
    SUBCASE("data line") {
        checkDistance({10.0, 179.0}, {10.0, -179.0});
    }
}

TEST_CASE("point angle") {
    SUBCASE("normal 1") {
        checkBearing({31.23, 121.47}, {39.90, 116.40});
    }
    SUBCASE("normal 2") {
        checkBearing({0.0, 0.0}, {5.0, 10.0});
    }
    SUBCASE("pole") {
        checkBearing({80.0, -1.0}, {80.0, 1.0});
    }
    SUBCASE("data line") {
        checkBearing({10.0, 179.0}, {10.0, -179.0});
    }
}

TEST_CASE("longitude range helpers") {
    CHECK(normalizeLongitude(190.0) == doctest::Approx(-170.0));
    CHECK(normalizeLongitude(-190.0) == doctest::Approx(170.0));
    CHECK(normalizeLongitude(180.0) == doctest::Approx(180.0));
    CHECK(normalizeLongitude(-180.0) == doctest::Approx(-180.0));
    CHECK(getLongiRange(170.0, 190.0) == doctest::Approx(20.0));
    CHECK(getLongiRangeCenter(170.0, 190.0) == doctest::Approx(180.0));
    CHECK(getLongiRange(-180.0, 180.0) == doctest::Approx(360.0));
    CHECK(getLongiRange(180.0, -180.0) == doctest::Approx(0.0));
    CHECK(getLongiRange(0.0, 360.0) == doctest::Approx(360.0));

    const std::vector<LongiRange> ranges = getLongiRanges(170.0, 190.0);
    REQUIRE(ranges.size() == 2);
    CHECK(ranges[0].first == doctest::Approx(170.0));
    CHECK(ranges[0].second == doctest::Approx(180.0));
    CHECK(ranges[1].first == doctest::Approx(-180.0));
    CHECK(ranges[1].second == doctest::Approx(-170.0));

    const auto fullRanges = getLongiRanges(-180.0, 180.0);
    REQUIRE(fullRanges.size() == 1);
    CHECK(fullRanges.front().first == doctest::Approx(-180.0));
    CHECK(fullRanges.front().second == doctest::Approx(180.0));
}

TEST_CASE("projection") {
    SUBCASE("stays continuous at the date line") {
        DynamicLCC projection;
        projection.reset(Point2D{10.0, 180.0}, 1000, 1000);
        const auto points = projection.trans({{10.0, 179.0}, {10.0, -181.0}, {10.0, -179.0}, {10.0, 181.0},
                                              {10.0, 180.0}, {10.0, -180.0}});

        CHECK(std::abs(points[0].first - points[1].first) < 1e-6);
        CHECK(std::abs(points[2].first - points[3].first) < 1e-6);
        CHECK(std::abs(points[4].first - points[5].first) < 1e-6);
        CHECK(points[0].first < points[4].first);
        CHECK(points[4].first < points[2].first);
        CHECK(std::abs(points[0].second - points[2].second) < 1e-6);
        CHECK(points[2].first - points[0].first > 100000.0);
        CHECK(points[2].first - points[0].first < 500000.0);
    }

    SUBCASE("bounded projection handles a date line crossing") {
        DynamicLCC projection;
        projection.reset(170.0, -170.0, 0.0, 20.0);
        const auto points = projection.trans({{10.0, 179.0}, {10.0, -179.0}, {10.0, 180.0}, {10.0, -180.0}});

        CHECK(std::abs(points[2].first - points[3].first) < 1e-6);
        CHECK(points[0].first < points[2].first);
        CHECK(points[2].first < points[1].first);
        CHECK(std::abs(points[0].second - points[1].second) < 1e-6);
    }

    SUBCASE("equatorial projection remains north-south symmetric") {
        DynamicLCC projection;
        projection.reset(Point2D{0.0, 180.0}, 1000, 1000);
        const auto points = projection.trans({{-5.0, 180.0}, {0.0, 180.0}, {5.0, 180.0}});

        CHECK(points[0].second > points[1].second);
        CHECK(points[1].second > points[2].second);
        CHECK(std::abs((points[0].second - points[1].second) - (points[1].second - points[2].second)) < 1e-3);
    }

    SUBCASE("an explicit full longitude range is not treated as zero width") {
        DynamicLCC projection;
        projection.reset(-180.0, 180.0, -10.0, 10.0);
        const auto points = projection.trans({{0.0, -90.0}, {0.0, 0.0}, {0.0, 90.0}});

        CHECK(std::isfinite(points[0].first));
        CHECK(std::isfinite(points[1].first));
        CHECK(std::isfinite(points[2].first));
        CHECK(points[0].first < points[1].first);
        CHECK(points[1].first < points[2].first);
    }

    SUBCASE("bounded equatorial projection is north-south symmetric") {
        DynamicLCC projection;
        projection.reset(-10.0, 10.0, -10.0, 10.0);
        const auto points = projection.trans({{-5.0, 0.0}, {0.0, 0.0}, {5.0, 0.0}});

        CHECK(points[0].second > points[1].second);
        CHECK(points[1].second > points[2].second);
        CHECK(std::abs((points[0].second - points[1].second) - (points[1].second - points[2].second)) < 1e-3);
    }
}
