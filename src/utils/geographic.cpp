#include "geographic.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <ranges>
#include <utility>
#include "constValue.hpp"
#include <GeographicLib/Geodesic.hpp>

/**
 * @brief 构造飞机轨迹
 * @param interval 数据间隔 ms
 */
AircraftTrail::AircraftTrail (const int interval) :
    interval(interval > 0 ? interval : 1000),
    maxSize(std::max(60000 / (interval > 0 ? interval : 1000), 2)) { // 至多一分钟, 至少保留两个点
}

/**
 * @brief 计算地速
 * @return 地速 (节), 数据不足时为 0
 * @details 只用最新的一半轨迹, 再等间隔取最多 10 个点, 以相邻点距离之和除以总时间得到平均地速
 */
int AircraftTrail::calculateGroundSpeed () const {
    const int size = static_cast<int>(points.size());
    if (size < 2)
        return 0;
    // 参与计算的最新一半轨迹 (至少保留两个点)
    const int half = std::max(size / 2, 2);
    // 从最新一半中等间隔采样, 最多 10 个点, 始终包含最新点
    const int stride = std::max(1, (half + 9) / 10);
    const int count = std::min(10, (half - 1) / stride + 1);
    // 相邻采样点距离之和 (惰性求值, 不拷贝轨迹数据)
    const auto segmentDistances = std::views::iota(0, count - 1)
            | std::views::transform([this, size, stride](const int i) {
                return distanceSimple(points[static_cast<size_t>(size - 1 - i * stride)],
                                      points[static_cast<size_t>(size - 1 - (i + 1) * stride)]);
            });
    const double totalDistance = std::accumulate(segmentDistances.begin(), segmentDistances.end(), 0.0);
    const double speedMs = totalDistance / ((count - 1) * stride * interval) * 1000.0; // m/s
    return static_cast<int>(std::round(speedMs * 3600.0 / nm2m)); // 节
}

/**
 * @brief 计算真航向
 * @return 真航向 (度, 0~359), 数据不足时为 -1
 * @details 取最新最多 5 个点, 相邻两点求航向, 转单位向量后按圆周统计取平均
 */
int AircraftTrail::calculateGeoHeading () const {
    const int size = static_cast<int>(points.size());
    if (size < 2)
        return -1;
    const int count = std::min(5, size);
    // 相邻点航向转单位向量 (惰性求值, 不拷贝轨迹数据), pair = (东分量, 北分量)
    const auto segmentVectors = std::views::iota(0, count - 1)
            | std::views::transform([this, size, count](const int i) {
                const double bearing = bearingSimple(points[static_cast<size_t>(size - count + i)],
                                                     points[static_cast<size_t>(size - count + i + 1)]);
                const double rad = bearing * std::numbers::pi / 180.0;
                return std::pair{std::cos(rad), std::sin(rad)};
            });
    const auto [east, north] = std::accumulate(
        segmentVectors.begin(), segmentVectors.end(), std::pair{0.0, 0.0},
        [](const std::pair<double, double> &acc, const std::pair<double, double> &vec) {
            return std::pair{acc.first + vec.first, acc.second + vec.second};
        });
    const double mean = std::fmod(std::atan2(north, east) * 180.0 / std::numbers::pi + 360.0, 360.0);
    return static_cast<int>(std::round(mean)) % 360;
}

/**
 * @brief 获取轨迹点
 * @return 轨迹点引用 (首部最早, 尾部最新)
 */
std::deque<Point2D>& AircraftTrail::getPoints () {
    return points;
}

/**
 * @brief 添加轨迹点, 超出容量时丢弃最早的点
 * @param point 经纬度点 (纬度,经度)
 */
void AircraftTrail::addPoint (Point2D point) {
    points.push_back(std::move(point));
    while (static_cast<int>(points.size()) > maxSize)
        points.pop_front();
}

/**
 * @brief 简单计算AB两点距离
 * @param lat1 A.纬度
 * @param lon1 A.经度
 * @param lat2 B.纬度
 * @param lon2 B.经度
 * @return AB距离 (米)
 */
double distanceSimple (const double lat1, const double lon1, const double lat2, const double lon2) {
    const double lat1_rad = lat1 * std::numbers::pi / 180.0;
    const double lon1_rad = lon1 * std::numbers::pi / 180.0;
    const double lat2_rad = lat2 * std::numbers::pi / 180.0;
    const double lon2_rad = lon2 * std::numbers::pi / 180.0;
    const double x = (lon2_rad - lon1_rad) * cos((lat1_rad + lat2_rad) / 2.0);
    const double y = lat2_rad - lat1_rad;
    return std::sqrt(x * x + y * y) * avgEarthRadius;
}
/**
 * @brief 简单计算AB两点距离
 * @param loc1 {A.纬度, A.经度}
 * @param loc2 {B.纬度, B.经度}
 * @return AB距离 (米)
 */
double distanceSimple (const Point2D &loc1, const Point2D &loc2) {
    return distanceSimple(loc1.first, loc1.second, loc2.first, loc2.second);
}

/**
 * @brief 简单计算AB两点相对真航向
 * @param lat1 A.纬度
 * @param lon1 A.经度
 * @param lat2 B.纬度
 * @param lon2 B.经度
 * @return B相对A的真航向
 */
double bearingSimple (const double lat1, const double lon1, const double lat2, const double lon2) {
    const double lat1_rad = lat1 * std::numbers::pi / 180.0;
    const double lon1_rad = lon1 * std::numbers::pi / 180.0;
    const double lat2_rad = lat2 * std::numbers::pi / 180.0;
    const double lon2_rad = lon2 * std::numbers::pi / 180.0;
    const double dLon = lon2_rad - lon1_rad;
    const double y = std::sin(dLon) * std::cos(lat2_rad);
    const double x = std::cos(lat1_rad) * std::sin(lat2_rad)
            - std::sin(lat1_rad) * std::cos(lat2_rad) * std::cos(dLon);
    return std::fmod(std::atan2(y, x) * 180.0 / std::numbers::pi + 360.0, 360.0); // 真航向 0~360
}
/**
 * @brief 简单计算AB两点相对真航向
 * @param loc1 {A.纬度, A.经度}
 * @param loc2 {B.纬度, B.经度}
 * @return B相对A的真航向
 */
double bearingSimple (const Point2D &loc1, const Point2D &loc2) {
    return bearingSimple(loc1.first, loc1.second, loc2.first, loc2.second);
}

/**
 * @brief 计算点A<纬,经>,某方向、距离上的B坐标
 * @param fix 起始点
 * @param bear 方向
 * @param distance 海里
 * @return B坐标<纬,经>
 */
Point2D pointBearingDistance (Point2D fix, double bear, double distance) {
    using namespace GeographicLib;
    const Geodesic &geo = Geodesic::WGS84();
    Point2D point;
    geo.Direct(fix.first, fix.second, bear, distance * nm2m, point.first, point.second);
    return point;
}

/**
 * @brief 两点几何意义上的距离
 * @param loc1 点1
 * @param loc2 点2
 * @return 距离
 */
double distanceGeometry (const Point2D &loc1, const Point2D &loc2) {
    return std::sqrt(std::pow(loc1.first - loc2.first, 2) + std::pow(loc1.second - loc2.second, 2));
}
