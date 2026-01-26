#include "geographic.hpp"
#include <numbers>
#include "tools/constValue.hpp"

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
