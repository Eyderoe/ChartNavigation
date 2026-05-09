#include <doctest.h>
#include "utils/noaaGlobe.hpp"

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("folder") {
    SUBCASE("unavailable 1") {
        NoaaGlobeView view("a");
        CHECK(view.getAlt(29,106) == -500);;
    }
    SUBCASE("unavailable 2") {
        NoaaGlobeView view("./");
        CHECK(view.getAlt(29,106) == -500);;
    }
    SUBCASE("available") {
        NoaaGlobeView view("/Users/eyderoe/GLOBE");
        CHECK(view.getAlt(29,106) != -500);;
    }
}

TEST_CASE("getAlt") {
    NoaaGlobeView view("/Users/eyderoe/GLOBE");
    SUBCASE("normal") {
        CHECK(view.getAlt(29,106) != -500);;
    }
    SUBCASE("ocean") {
        CHECK(view.getAlt(0,0) == -500);;
    }
    SUBCASE("error 1") {
        CHECK(view.getAlt(100,0) == -500);;
    }
    SUBCASE("error 2") {
        CHECK(view.getAlt(0,200) == -500);;
    }
    SUBCASE("error 3") {
        CHECK(view.getAlt(100,200) == -500);;
    }
}
