#include "noaaGlobe.hpp"

#include <fstream>
#include <iostream>
#include <numeric>
#include <ranges>

namespace fs = std::filesystem;

/**
 * @param folder 存储 a10g-p10g 文件的文件夹
 * @param maxTile 内存加载最大的瓦片数量
 */
NoaaGlobe::NoaaGlobe (fs::path const &folder, const int maxTile) : max(maxTile) {
    // 仅记录文件位置
    if (!fs::exists(folder) || !fs::is_directory(folder)) {
        std::cerr << std::format("Folder: {} does not exist.", folder.string()) << std::endl;
        return;
    }
    for (const auto &entry : fs::directory_iterator(folder)) {
        if (!entry.is_regular_file())
            continue;
        const std::string filename = entry.path().filename().string();
        if (filename.size() == 4) {
            const char firstChar = static_cast<char>(tolower(filename[0]));
            const char fourthChar = static_cast<char>(tolower(filename[3]));
            if (!(firstChar >= 'a' && firstChar <= 'p'))
                continue;
            if (fourthChar != 'g')
                continue;
            if (!isdigit(filename[1]) || !isdigit(filename[2]))
                continue;
            filesPath[firstChar] = entry.path();
        }
    }
}

/**
 * @brief 获取某点高度
 * @param latitude 纬度
 * @param longitude 经度
 * @return 高度(米), 数据不可用为-500
 */
short NoaaGlobe::getAlt (const float latitude, float longitude) {
    longitude = static_cast<float>(fmod(longitude + 180, 360) - 180);
    const char tileName = getTileName(latitude, longitude);
    // 首次使用该瓦片时加载瓦片
    if (!available.contains(tileName))
        available[tileName] = loadTile(tileName);
    if (!available[tileName])
        return -500;
    // 解算
    const auto &[columns, rows, maxLat, minLon, alt] = tiles[tileName];
    const int row = static_cast<int>((static_cast<float>(maxLat) - latitude) / res);
    const int column = static_cast<int>((longitude - static_cast<float>(minLon)) / res);
    if ((row < 0) || (row >= rows) || (column < 0) || (column >= columns))
        return -500;
    return alt[row * columns + column];
}

// 获取经纬度对应瓦片编号
char NoaaGlobe::getTileName (const float latitude, const float longitude) {
    const int row = (latitude > 50) ? 0 : (latitude > 0) ? 1 : (latitude > -50) ? 2 : 3;
    const int column = (longitude > 90) ? 3 : (longitude > 0) ? 2 : (longitude > -90) ? 1 : 0;
    return static_cast<char>('a' + row * 4 + column);
}

// 尝试加载某个瓦片
bool NoaaGlobe::loadTile (const char tileName) {
    if (!filesPath.contains(tileName)) // 文件不存在
        return false;
    constexpr int columnsExpect = 10800;
    const int rowsExpect = ((tileName <= 'd') || (tileName >= 'm')) ? 4800 : 6000;
    // 计算参数
    Tile tile;
    tile.alt.resize(columnsExpect * rowsExpect);
    tile.columns = columnsExpect;
    tile.rows = rowsExpect;
    static std::vector<short> latStep{40, 50, 50, 40};
    const int row = 3 - (tileName - 'a') / 4;
    const int column = (tileName - 'a') % 4;
    tile.minLon = -180 + column * 90;
    tile.maxLat = reduce(latStep.begin(), latStep.begin() + row + 1, -90);
    // 读取文件
    std::ifstream file(filesPath[tileName], std::ios::binary);
    if (!file) // 文件打不开
        return false;
    const int64_t expectSize = (columnsExpect * rowsExpect) * static_cast<int>(sizeof(short));
    file.read(reinterpret_cast<char*>(tile.alt.data()), expectSize);
    if (file.gcount() != expectSize) { // 文件大小错误
        file.close();
        return false;
    }
    file.close();
    if (tiles.size() + 1 > max)
        tiles.clear();
    tiles[tileName] = tile;
    return true;
}

/**
 * @param folder 存储 a10g-p10g 文件的文件夹
 */
NoaaGlobeView::NoaaGlobeView (const std::filesystem::path &folder) {
    if (fs::exists(folder)) {
        // 读取文件夹下可用文件
        for (const auto &entry : fs::directory_iterator(folder)) {
            if (!entry.is_regular_file())
                continue;
            const std::string filename = entry.path().filename().string();
            if (filename.size() == 4) {
                const char firstChar = static_cast<char>(tolower(filename[0]));
                const char fourthChar = static_cast<char>(tolower(filename[3]));
                if (!(firstChar >= 'a' && firstChar <= 'p'))
                    continue;
                if (fourthChar != 'g')
                    continue;
                if (!isdigit(filename[1]) || !isdigit(filename[2]))
                    continue;
                loadTile(entry);
            }
        }
    }
    // 补充
    for (char c = 'a'; c <= 'g'; ++c) {
        if (tiles.contains(c))
            continue;
        tiles[c] = {};
        tiles[c].available = false;
    }
}

/**
 * @brief 获取某点高度
 * @param latitude 纬度
 * @param longitude 经度
 * @return 高度(米), 数据不可用为-500
 */
short NoaaGlobeView::getAlt (const float latitude, float longitude) {
    longitude = static_cast<float>(fmod(longitude + 180, 360) - 180);
    const char tileName = getTileName(latitude, longitude);
    if (!tiles[tileName].available)
        return -500;
    // 解算
    const auto &[columns, rows, maxLat, minLon,_1, alt,_2] = tiles[tileName];
    const int row = static_cast<int>((static_cast<float>(maxLat) - latitude) / res);
    const int column = static_cast<int>((longitude - static_cast<float>(minLon)) / res);
    if ((row < 0) || (row >= rows) || (column < 0) || (column >= columns))
        return -500;
    return alt[row * columns + column];
}
/**
 * @brief 获取某点高度
 * @param location {经度,纬度}
 * @return 高度(米), 数据不可用为-500
 */
short NoaaGlobeView::getAlt (const std::pair<float, float> &location) {
    return getAlt(location.first, location.second);
}

char NoaaGlobeView::getTileName (const float latitude, const float longitude) {
    const int row = (latitude > 50) ? 0 : (latitude > 0) ? 1 : (latitude > -50) ? 2 : 3;
    const int column = (longitude > 90) ? 3 : (longitude > 0) ? 2 : (longitude > -90) ? 1 : 0;
    return static_cast<char>('a' + row * 4 + column);
}

void NoaaGlobeView::loadTile (const std::filesystem::path &tilePath) {
    const std::string filename = tilePath.filename().string();
    const char name = static_cast<char>(tolower(filename[0]));
    // 计算参数
    constexpr int columnsExpect = 10800;
    const int rowsExpect = ((name <= 'd') || (name >= 'm')) ? 4800 : 6000;
    tiles[name] = {};
    auto &tile = tiles[name];
    tile.available = false;
    tile.columns = columnsExpect;
    tile.rows = rowsExpect;
    static std::vector<short> latStep{40, 50, 50, 40};
    const int row = 3 - (name - 'a') / 4;
    const int column = (name - 'a') % 4;
    tile.minLon = -180 + column * 90;
    tile.maxLat = reduce(latStep.begin(), latStep.begin() + row + 1, -90);
    // 读取文件
    std::error_code ec;
    tile.file = mio::make_mmap_source(tilePath.string(), ec);
    const auto &mmap = tile.file;
    if (ec)
        return;
    const int64_t expectSize = (columnsExpect * rowsExpect) * static_cast<int>(sizeof(short));
    if (mmap.size() != expectSize)
        return;
    auto *data = reinterpret_cast<const short*>(mmap.data());
    size_t count = mmap.size() / sizeof(short);
    tile.alt = {data, count};
    tile.available = true;
}
