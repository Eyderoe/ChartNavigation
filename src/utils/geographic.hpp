#ifndef CHARTNAVIGATION_GEOGRAPHIC_HPP
#define CHARTNAVIGATION_GEOGRAPHIC_HPP


#include <deque>


using Point2D = std::pair<double, double>; // (纬度,经度)

class AircraftTrail {
    public:
        explicit AircraftTrail (int interval);
        [[nodiscard]] int calculateGroundSpeed () const;
        int calculateGeoHeading () const;
        std::deque<Point2D>& getPoints ();
        void addPoint (Point2D point);
    private:
        int interval; // 数据间隔 ms
        int maxSize; // 队列大小, 至多存储一分钟
        std::deque<Point2D> points; // 轨迹点, 尾部为最新
};

double distanceSimple (double lat1, double lon1, double lat2, double lon2);
double distanceSimple (const Point2D &loc1, const Point2D &loc2);
double bearingSimple (double lat1, double lon1, double lat2, double lon2);
double bearingSimple (const Point2D &loc1, const Point2D &loc2);
Point2D pointBearingDistance (Point2D fix, double bear, double distance);
double distanceGeometry (const Point2D &loc1, const Point2D &loc2);

#endif //CHARTNAVIGATION_GEOGRAPHIC_HPP
