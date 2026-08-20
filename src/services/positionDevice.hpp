#ifndef CHARTNAVIGATION_POSITIONDEVICE_HPP
#define CHARTNAVIGATION_POSITIONDEVICE_HPP

#include <QGeoPositionInfoSource>
#include <QGeoSatelliteInfoSource>

#include <optional>

#include "utils/geographic.hpp"


class PositionDevice;

QString getPosDeviceInfo (PositionDevice *device);


// 适用于有现实数据源输入
class PositionDevice {
    public:
        PositionDevice ();
        ~PositionDevice ();
        // 定位
        std::optional<Point2D> getPosition () const;
        std::optional<double> getSpeed() const;
        std::optional<double> getHorizontalAccuracy () const;
        std::optional<double> getVerticalAccuracy () const;
        // 卫星
        std::optional<int> getSatelliteNum () const;
        std::optional<int> getSatelliteStrength ();
    private:
        QGeoPositionInfoSource *positionSource;
        QGeoSatelliteInfoSource *satelliteSource;

        QGeoPositionInfo lastPosition;
        QList<QGeoSatelliteInfo> satellitesInUse;
        QList<QGeoSatelliteInfo> satellitesInView;
};


#endif //CHARTNAVIGATION_POSITIONDEVICE_HPP
