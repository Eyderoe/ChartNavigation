#include <doctest.h>
#include "utils/noaaGlobe.hpp"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

fs::path globeDataPath () {
    if (const char *configuredPath = std::getenv("CHARTNAVI_GLOBE_PATH"); configuredPath && *configuredPath)
        return configuredPath;
    return "/Users/eyderoe/GLOBE";
}

bool hasGlobeData () {
    return fs::is_directory(globeDataPath());
}

TEST_CASE("folder") {
    SUBCASE("unavailable 1") {
        NoaaGlobeView view("a");
        CHECK(view.getAlt(29,106) == -500);;
    }
    SUBCASE("unavailable 2") {
        NoaaGlobeView view("./");
        CHECK(view.getAlt(29,106) == -500);;
    }
    if (!hasGlobeData()) {
        MESSAGE("GLOBE data is not available; skipping data-backed checks");
        return;
    }
    SUBCASE("available") {
        NoaaGlobeView view(globeDataPath());
        CHECK(view.getAlt(29,106) != -500);;
    }
}

TEST_CASE("getAlt") {
    if (!hasGlobeData()) {
        MESSAGE("GLOBE data is not available; skipping data-backed checks");
        return;
    }
    NoaaGlobeView view(globeDataPath());
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
