#ifndef CHARTNAVIGATION_GEOGRAPHIC_HPP
#define CHARTNAVIGATION_GEOGRAPHIC_HPP

using Point2D = std::pair<double, double>;

double distanceSimple (double lat1, double lon1, double lat2, double lon2);
double distanceSimple (const Point2D &loc1, const Point2D &loc2);
Point2D pointBearingDistance (Point2D fix,double bear,double distance);

double distanceGeometry(const Point2D &loc1, const Point2D &loc2);

#endif //CHARTNAVIGATION_GEOGRAPHIC_HPP
