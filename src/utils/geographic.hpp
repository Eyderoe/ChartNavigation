#ifndef CHARTNAVIGATION_GEOGRAPHIC_HPP
#define CHARTNAVIGATION_GEOGRAPHIC_HPP


#include <deque>
#include <GeographicLib/LambertConformalConic.hpp>
#include <memory>
#include <utility>
#include <vector>


constexpr double maxLat{80}; // 纬度最大绝对值, 单位度

using Point2D = std::pair<double, double>; // (纬度,经度)
using Rect2D = std::pair<Point2D, Point2D>; // (左上,右下)

class AircraftTrail {
    public:
        explicit AircraftTrail (int interval);
        [[nodiscard]] int calculateGroundSpeed () const;
        [[nodiscard]] int calculateGeoHeading () const;
        std::deque<Point2D>& getPoints ();
        void addPoint (Point2D point);
    private:
        int interval; // 数据间隔 ms
        int maxSize; // 队列大小, 至多存储一分钟
        std::deque<Point2D> points; // 轨迹点, 尾部为最新
};

class DynamicLCC { // WGS84,兰伯特等角圆锥投影
    public:
        void reset (const Point2D &newCenter, int verticalMargin, int horizontalMargin);
        void reset (double left, double right, double bottom, double top);
        [[nodiscard]] std::vector<Point2D> trans (std::vector<Point2D> positions) const;
    private:
        void configure (const Point2D &newCenter, double verticalMargin, double horizontalMargin);
        Point2D center{};
        double centralMeridian{}, centerNorthing{}, falseEasting{}, falseNorthing{};
        std::unique_ptr<GeographicLib::LambertConformalConic> projection;
        bool configured{false};
};

double distanceSimple (double lat1, double lon1, double lat2, double lon2);
double distanceSimple (const Point2D &loc1, const Point2D &loc2);
double bearingSimple (double lat1, double lon1, double lat2, double lon2);
double bearingSimple (const Point2D &loc1, const Point2D &loc2);
Point2D pointBearingDistance (const Point2D &fix, double bear, double distance);
double distanceGeometry (const Point2D &loc1, const Point2D &loc2);

#endif //CHARTNAVIGATION_GEOGRAPHIC_HPP
