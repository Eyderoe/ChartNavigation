#ifndef CHARTNAVIGATION_GEOGRAPHIC_HPP
#define CHARTNAVIGATION_GEOGRAPHIC_HPP


#include <deque>
#include <GeographicLib/LambertConformalConic.hpp>
#include <memory>
#include <utility>
#include <vector>


constexpr double maxSupportLat{80}; // 纬度最大绝对值

// 十进制
using Point2D = std::pair<double, double>; // (纬度,经度) 或者 (x,y)
using Rect2D = std::pair<Point2D, Point2D>; // (左上,右下)
using LongiRange = std::pair<double, double>; // [左,右]
// 弧度制
using doubleR = double;
using Point2DR = std::pair<doubleR, doubleR>;
using Rect2DR = std::pair<Point2DR, Point2DR>;
using LongiRangeR = std::pair<doubleR, doubleR>;

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
        Point2DR center{}; // 弧度制中心点
        doubleR centralMeridian{}; // 弧度制中央经线
        double centerNorthing{}, falseEasting{}, falseNorthing{}; // 单位米
        std::unique_ptr<GeographicLib::LambertConformalConic> projection;
        bool configured{false};
};

double distanceSimple (double lat1, double lon1, double lat2, double lon2);
double distanceSimple (const Point2D &loc1, const Point2D &loc2);
double bearingSimple (double lat1, double lon1, double lat2, double lon2);
double bearingSimple (const Point2D &loc1, const Point2D &loc2);
Point2D pointBearingDistance (const Point2D &fix, double bear, double distance);
double distanceGeometry (const Point2D &loc1, const Point2D &loc2);

double normalizeLongitude (double longitude);
double getLongiRange (double left, double right);
double getLongiRangeCenter (double left, double right);
std::vector<LongiRange> getLongiRanges (double left, double right);

#endif //CHARTNAVIGATION_GEOGRAPHIC_HPP
