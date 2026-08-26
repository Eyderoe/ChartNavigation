#include "geographic.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
                return distanceSimple(points[size - 1 - i * stride], points[size - 1 - (i + 1) * stride]);
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
                const double bearing = bearingSimple(points[size - count + i], points[size - count + i + 1]);
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
    if (point == Point2D(0, 0)) // 初始化时 点可能不可用
        if (!points.empty())
            point = points.back();
    points.push_back(std::move(point));
    while (static_cast<int>(points.size()) > maxSize)
        points.pop_front();
}

double normalizeLongitude (double longitude) {
    constexpr double pi{std::numbers::pi};
    longitude = std::fmod(longitude + pi, 2.0 * pi);
    if (longitude < 0.0)
        longitude += 2.0 * pi;
    return longitude - pi;
}
double clampLatitude (const double latitude) {
    constexpr double degreeToRadian{std::numbers::pi / 180.0};
    constexpr double latitudeLimit{maxLat * degreeToRadian};
    return std::clamp(latitude, -latitudeLimit, latitudeLimit);
}
/**
 * @brief 重新设置投影参数
 * @param newCenter 中心点经纬度
 * @param verticalMargin 上下边界与中心点距离,海里
 * @param horizontalMargin 左右边界与中心点距离,海里
 */
void DynamicLCC::reset (const Point2D &newCenter, const int verticalMargin, const int horizontalMargin) {
    configure(newCenter, std::max(1.0, static_cast<double>(verticalMargin)),
              std::max(1.0, static_cast<double>(horizontalMargin)));
}

void DynamicLCC::configure (const Point2D &newCenter, const double verticalMargin,
                            const double horizontalMargin) {
    constexpr double wgs84SemiMajorAxis{6378137.0};
    constexpr double wgs84Flattening{1.0 / 298.257223563};
    constexpr double degreeToRadian{std::numbers::pi / 180.0};
    constexpr double radianToDegree{180.0 / std::numbers::pi};

    configured = false;
    projection.reset();
    if (!std::isfinite(newCenter.first) || !std::isfinite(newCenter.second)
        || !std::isfinite(verticalMargin) || !std::isfinite(horizontalMargin)
        || std::abs(newCenter.first) > maxLat || verticalMargin <= 0.0 || horizontalMargin <= 0.0)
        return;
    center = {
        std::clamp(newCenter.first, -maxLat, maxLat),
        normalizeLongitude(newCenter.second * degreeToRadian)
    };
    centralMeridian = center.second;
    falseEasting = horizontalMargin * nm2m;
    falseNorthing = verticalMargin * nm2m;
    const Point2D centerDegrees{center.first, center.second * radianToDegree};
    const Point2D northEdge = pointBearingDistance(centerDegrees, 0.0, verticalMargin);
    const Point2D southEdge = pointBearingDistance(centerDegrees, 180.0, verticalMargin);
    double standardSouth = clampLatitude(southEdge.first * degreeToRadian);
    double standardNorth = clampLatitude(northEdge.first * degreeToRadian);
    if (standardSouth > standardNorth)
        std::swap(standardSouth, standardNorth);
    if (std::abs(standardSouth + standardNorth) < 1e-12) {
        const double availableSpan = std::max(standardNorth - standardSouth, 1e-5);
        const double side = std::max(availableSpan / 3.0, 1e-5);
        if (center.first >= 0.0) {
            standardSouth = clampLatitude(center.first * degreeToRadian + side);
            standardNorth = clampLatitude(center.first * degreeToRadian + 2.0 * side);
        } else {
            standardSouth = clampLatitude(center.first * degreeToRadian - 2.0 * side);
            standardNorth = clampLatitude(center.first * degreeToRadian - side);
        }
    }
    try {
        projection = std::make_unique<GeographicLib::LambertConformalConic>(
            wgs84SemiMajorAxis, wgs84Flattening,
            standardSouth * radianToDegree, standardNorth * radianToDegree, 1.0);
    } catch (const std::exception &) {
        projection.reset();
        return;
    }
    double centerEasting{};
    projection->Forward(center.second * radianToDegree, center.first, center.second * radianToDegree, centerEasting,
                        centerNorthing);
    configured = true;
}

/**
 * @brief 重新设置投影参数
 * @param left 左边界经度
 * @param right 右边界经度
 * @param bottom 下边界纬度
 * @param top 上边界纬度
 * @note 经纬度单位为度, left 到 right 按向东方向解释
 */
void DynamicLCC::reset (const double left, const double right, double bottom, double top) {
    constexpr double degreeToRadian{std::numbers::pi / 180.0};
    constexpr double radianToDegree{180.0 / std::numbers::pi};
    configured = false;
    projection.reset();
    if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(bottom) || !std::isfinite(top) ||
        std::abs(bottom) > maxLat || std::abs(top) > maxLat)
        return;
    if (bottom > top)
        std::swap(bottom, top);
    const double normalizedLeft = normalizeLongitude(left * degreeToRadian) * radianToDegree;
    const double normalizedRight = normalizeLongitude(right * degreeToRadian) * radianToDegree;
    double longitudeSpan = normalizedRight - normalizedLeft;
    if (longitudeSpan < 0.0)
        longitudeSpan += 360.0;
    const double centerLongitude = normalizeLongitude(
        (normalizedLeft + longitudeSpan / 2.0) * degreeToRadian) * radianToDegree;
    const Point2D newCenter{(bottom + top) / 2.0, centerLongitude};
    const GeographicLib::Geodesic &geodesic = GeographicLib::Geodesic::WGS84();
    const auto distanceTo = [&geodesic](const Point2D &from, const Point2D &to) {
        double distance{};
        geodesic.Inverse(from.first, from.second, to.first, to.second, distance);
        return distance;
    };
    const Point2D westEdge{newCenter.first, normalizedLeft};
    const Point2D eastEdge{newCenter.first, normalizedRight};
    const Point2D southEdge{bottom, newCenter.second};
    const Point2D northEdge{top, newCenter.second};
    const double horizontalMargin = std::max(distanceTo(newCenter, westEdge), distanceTo(newCenter, eastEdge)) / nm2m;
    const double verticalMargin = std::max(distanceTo(newCenter, southEdge), distanceTo(newCenter, northEdge)) / nm2m;
    if (!std::isfinite(horizontalMargin) || !std::isfinite(verticalMargin))
        return;
    configure(newCenter, std::max(1.0, verticalMargin), std::max(1.0, horizontalMargin));
    if (!configured)
        return;
    double minimumEasting = Inf;
    double maximumNorthing = -Inf;
    for (const double latitude : {bottom, top}) {
        for (const double longitude : {normalizedLeft, normalizedRight}) {
            double projectedX{}, projectedY{};
            projection->Forward(centerLongitude, latitude, longitude, projectedX, projectedY);
            minimumEasting = std::min(minimumEasting, projectedX);
            maximumNorthing = std::max(maximumNorthing, projectedY);
        }
    }
    if (!std::isfinite(minimumEasting) || !std::isfinite(maximumNorthing)) {
        configured = false;
        projection.reset();
        return;
    }
    falseEasting = -minimumEasting;
    falseNorthing = maximumNorthing - centerNorthing;
}

/**
 * @brief 批量转换坐标
 * @param positions 经纬度
 * @return <x,y>, 单位米, 左上角为原点、向东为 x 正方向、向北为 y 负方向
 */
std::vector<Point2D> DynamicLCC::trans (std::vector<Point2D> positions) const {
    constexpr double degreeToRadian{std::numbers::pi / 180.0};
    constexpr double radianToDegree{180.0 / std::numbers::pi};
    if (!configured)
        return positions;
    for (auto &position : positions) {
        if (!std::isfinite(position.first) || !std::isfinite(position.second) || std::abs(position.first) > maxLat) {
            position = {NaN, NaN};
            continue;
        }
        const double longitude = normalizeLongitude(position.second * degreeToRadian);
        double projectedX{}, projectedY{};
        projection->Forward(centralMeridian * radianToDegree, position.first, longitude * radianToDegree, projectedX,
                            projectedY);
        position = {falseEasting + projectedX, falseNorthing - (projectedY - centerNorthing)};
        if (!std::isfinite(position.first) || !std::isfinite(position.second))
            position = {NaN, NaN};
    }
    return positions;
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
    const double x = std::cos(lat1_rad) * std::sin(lat2_rad) - std::sin(lat1_rad) * std::cos(lat2_rad) * std::cos(dLon);
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
Point2D pointBearingDistance (const Point2D &fix, const double bear, const double distance) {
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
