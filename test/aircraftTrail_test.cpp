#include <doctest.h>
#include "utils/geographic.hpp"
#include <cmath>

TEST_CASE("aircraft trail handles startup and unavailable positions") {
    AircraftTrail trail(1000);
    CHECK(trail.calculateGroundSpeed() == 0);
    CHECK(trail.calculateGeoHeading() == -1);
    CHECK(trail.calculateVerticalSpeed() == 0);
    trail.addPoint({0, 0});
    CHECK(trail.getPoints().empty());
    trail.addPoint({30, 110}, 1000);
    CHECK(trail.calculateGeoHeading() == -1);
    trail.addPoint({0, 0}, 1000);
    REQUIRE(trail.getPoints().size() == 2);
    CHECK(trail.getPoints().back() == Point2D{30, 110});
    CHECK(trail.calculateGroundSpeed() == 0);
    CHECK(trail.calculateVerticalSpeed() == 0);
}

TEST_CASE("aircraft trail reports speed in knots and climb in feet per minute") {
    AircraftTrail trail(1000);
    // One nautical mile east in ten seconds: 360 kt; 30.48 m climb: 600 ft/min.
    for (int i = 0; i <= 10; ++i)
        trail.addPoint({0, 10 + i * (1.0 / 60.0) / 10}, 1000 + i * 3.048);
    CHECK(std::abs(trail.calculateGroundSpeed() - 360) <= 2);
    CHECK(trail.calculateGeoHeading() == 90);
    CHECK(trail.calculateVerticalSpeed() == 600);
}

TEST_CASE("aircraft trail averages headings across north") {
    AircraftTrail trail(1000);
    trail.addPoint({30.00, 110.000});
    trail.addPoint({30.01, 110.001});
    trail.addPoint({30.02, 110.000});
    CHECK(trail.calculateGeoHeading() == 0);
}

TEST_CASE("aircraft trail expires old movement and keeps altitude samples aligned") {
    AircraftTrail trail(1000);
    for (int i = 0; i < 70; ++i)
        trail.addPoint({30, 110 + i * 0.001}, 2000 + i * 3.048);
    REQUIRE(trail.getPoints().size() == 60);
    CHECK(trail.getPoints().front() == Point2D{30, 110.01});
    CHECK(trail.calculateVerticalSpeed() == 600);
    for (int i = 1; i <= 30; ++i)
        trail.addPoint({30, 110.069}, 2207.312 - i * 3.048);
    CHECK(trail.calculateGroundSpeed() == 0);
    CHECK(trail.calculateVerticalSpeed() == -600);
    trail.addPoint({30, 110.069}); // An absent altitude must not create a huge descent.
    CHECK(trail.calculateVerticalSpeed() == 0);
}
