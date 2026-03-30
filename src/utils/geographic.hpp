#ifndef CHARTNAVIGATION_GEOGRAPHIC_HPP
#define CHARTNAVIGATION_GEOGRAPHIC_HPP

double distanceSimple(double lat1, double lon1, double lat2, double lon2);
double distanceSimple(const std::pair<double,double> &loc1, const std::pair<double,double> &loc2);

#endif //CHARTNAVIGATION_GEOGRAPHIC_HPP