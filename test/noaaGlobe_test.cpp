#include <doctest.h>
#include "utils/noaaGlobe.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <cstdint>

TEST_CASE("GLOBE returns unavailable for missing or truncated tiles") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    SUBCASE("missing folder") {
        NoaaGlobeView view(directory.filePath("missing").toStdString());
        CHECK(view.getAlt(60, -135) == -500);
    }
    SUBCASE("empty folder") {
        NoaaGlobeView view(directory.path().toStdString());
        CHECK(view.getAlt(60, -135) == -500);
    }
    SUBCASE("truncated tile") {
        QFile tile(directory.filePath("a10g"));
        REQUIRE(tile.open(QIODevice::WriteOnly));
        REQUIRE(tile.write("short") == 5);
        tile.close();
        NoaaGlobeView view(directory.path().toStdString());
        CHECK(view.getAlt(60, -135) == -500);
    }
}

TEST_CASE("GLOBE reads row and column offsets and wraps longitude") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    QFile tile(directory.filePath("a10g"));
    REQUIRE(tile.open(QIODevice::ReadWrite));
    // Sparse temporary tile: correct format size without a large checked-in data file.
    constexpr qint64 columns = 10800;
    REQUIRE(tile.resize(columns * 4800 * sizeof(std::int16_t)));
    const auto writeHeight = [&](qint64 row, qint64 column, std::int16_t height) {
        REQUIRE(tile.seek((row * columns + column) * sizeof(height)));
        REQUIRE(tile.write(reinterpret_cast<const char *>(&height), sizeof(height)) == sizeof(height));
    };
    // Coordinates lie inside cells, avoiding float rounding at cell boundaries.
    writeHeight(1200, 1200, 1234); // 79.999 N, 169.999 W
    writeHeight(1320, 1440, -500); // 78.999 N, 167.999 W: ocean sentinel
    tile.close();
    NoaaGlobeView view(directory.path().toStdString());
    CHECK(view.getAlt(79.999f, -169.999f) == 1234);
    CHECK(view.getAlt(std::pair{79.999f, -169.999f}) == 1234);
    CHECK(view.getAlt(79.999f, 190.001f) == 1234);
    CHECK(view.getAlt(78.999f, -167.999f) == -500);
    CHECK(view.getAlt(60, 10) == -500); // Another tile was not supplied.
    CHECK(view.getAlt(100, -169.999f) == -500);
}
