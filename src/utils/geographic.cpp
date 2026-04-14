#include "geographic.hpp"
#include <numbers>
#include "constValue.hpp"
#include <GeographicLib/Geodesic.hpp>

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
