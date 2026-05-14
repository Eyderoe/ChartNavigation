#ifndef CHARTNAVIGATION_NOAA_GLOBE_HPP
#define CHARTNAVIGATION_NOAA_GLOBE_HPP

#include <mio.hpp>
#include <vector>
#include <filesystem>
#include <map>
#include <span>

// Global Land One-km Base Elevation
class NoaaGlobe {
    static constexpr float res = 0.008333334; // 分辨率30弧秒
    struct Tile {
        int columns, rows, maxLat, minLon;
        std::vector<short> alt;
    };
    public:
        explicit NoaaGlobe (std::filesystem::path const &folder, int maxTile = 16);
        short getAlt (float latitude, float longitude);
    private:
        static char getTileName (float latitude, float longitude);

        std::map<char, std::filesystem::path> filesPath;
        std::map<char, Tile> tiles;
        std::map<char, bool> available;
        bool loadTile (char tileName);
        int max;
};

// 使用内存映射
class NoaaGlobeView {
    static constexpr float res = 0.008333334; // 分辨率30弧秒
    struct Tile {
        int columns, rows, maxLat, minLon;
        mio::mmap_source file;
        std::span<const short> alt;
        bool available;
    };
    public:
        explicit NoaaGlobeView (const std::filesystem::path &folder);
        short getAlt (float latitude, float longitude);
        short getAlt (const std::pair<float,float> &location);
    private:
        static char getTileName (float latitude, float longitude);

        std::map<char, Tile> tiles;
        void loadTile (const std::filesystem::path &tilePath);
};

#endif //CHARTNAVIGATION_NOAA_GLOBE_HPP
