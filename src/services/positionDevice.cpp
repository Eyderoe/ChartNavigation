#include "positionDevice.hpp"

#include <cmath>
#include <cstdio>

PositionDevice::PositionDevice () :
    positionSource(QGeoPositionInfoSource::createDefaultSource(nullptr)),
    satelliteSource(QGeoSatelliteInfoSource::createDefaultSource(nullptr)) {
    if (positionSource) {
        QObject::connect(positionSource, &QGeoPositionInfoSource::positionUpdated,
                         [this](const QGeoPositionInfo &info) { lastPosition = info; });
        positionSource->startUpdates();
        positionSource->setUpdateInterval(5 * 1000);
    }
    if (satelliteSource) {
        QObject::connect(satelliteSource, &QGeoSatelliteInfoSource::satellitesInUseUpdated,
                         [this](const QList<QGeoSatelliteInfo> &satellites) { satellitesInUse = satellites; });
        QObject::connect(satelliteSource, &QGeoSatelliteInfoSource::satellitesInViewUpdated,
                         [this](const QList<QGeoSatelliteInfo> &satellites) { satellitesInView = satellites; });
        satelliteSource->startUpdates();
        satelliteSource->setUpdateInterval(10 * 1000);
    }
}

PositionDevice::~PositionDevice () {
    delete satelliteSource;
    delete positionSource;
}

/**
 * @brief 获取经纬度信息
 * @return <纬度,经度>
 */
std::optional<Point2D> PositionDevice::getPosition () const {
    if (!positionSource || !lastPosition.isValid())
        return std::nullopt;
    const QGeoCoordinate coord = lastPosition.coordinate();
    return Point2D{coord.latitude(), coord.longitude()};
}

/**
 * @brief 获取速度
 * @return m/s
 */
std::optional<double> PositionDevice::getSpeed () const {
    if (!lastPosition.isValid() || !lastPosition.hasAttribute(QGeoPositionInfo::GroundSpeed))
        return std::nullopt;
    return lastPosition.attribute(QGeoPositionInfo::GroundSpeed);
}

/**
 * @brief 获取水平精度
 * @return 水平精度 (米)
 */
std::optional<double> PositionDevice::getHorizontalAccuracy () const {
    if (!positionSource || !lastPosition.isValid() || !lastPosition.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        return std::nullopt;
    return lastPosition.attribute(QGeoPositionInfo::HorizontalAccuracy);
}

/**
 * @brief 获取垂直精度
 * @return 垂直精度 (米)
 */
std::optional<double> PositionDevice::getVerticalAccuracy () const {
    if (!positionSource || !lastPosition.isValid() || !lastPosition.hasAttribute(QGeoPositionInfo::VerticalAccuracy))
        return std::nullopt;
    return lastPosition.attribute(QGeoPositionInfo::VerticalAccuracy);
}

/**
 * @brief 获取锁定卫星数量
 * @return 当前锁定卫星数
 */
std::optional<int> PositionDevice::getSatelliteNum () const {
    if (!satelliteSource || (satellitesInUse.size() == 0))
        return std::nullopt;
    return static_cast<int>(satellitesInUse.size());
}

/**
 * @brief 获取锁定卫星的平均信号强度
 * @return 平均信号强度 (0~99)
 */
std::optional<int> PositionDevice::getSatelliteStrength () {
    if (!satelliteSource || satellitesInUse.isEmpty())
        return std::nullopt;
    double sum = 0;
    for (const QGeoSatelliteInfo &satellite : satellitesInUse)
        sum += satellite.signalStrength();
    return static_cast<int>(std::lround(sum / static_cast<int>(satellitesInUse.size())));
}

QString getPosDeviceInfo (PositionDevice *device) {
    if (!device)
        return {};
    QString text;
    // 速度
    const auto speed = device->getSpeed();
    if (speed != std::nullopt)
        text += QString::asprintf("速度:(%.1f节)", (*speed) * 1.94);
    // 定位精度
    const auto horiz = device->getHorizontalAccuracy();
    const auto vert = device->getVerticalAccuracy();
    if ((horiz != std::nullopt) || (vert != std::nullopt)) {
        text += "定位精度(";
        if (horiz != std::nullopt) {
            text += QString::asprintf("水平:%.1fm", *horiz);
            if (vert == std::nullopt)
                text += ")";
            else
                text += QString::asprintf("垂直:%.1fm)", *vert);
        } else {
            text += QString::asprintf("垂直:%.1fm)", *vert);
        }
    }
    // 卫星
    const auto num = device->getSatelliteNum();
    const auto strength = device->getSatelliteStrength();
    if ((num != std::nullopt) || (strength != std::nullopt)) {
        text += "卫星(";
        if (num != std::nullopt) {
            text += QString::asprintf("数量:%d", *num);
            if (strength == std::nullopt)
                text += ")";
            else
                text += QString::asprintf("强度:%d)", *strength);
        } else {
            text += QString::asprintf("强度:%d)", *strength);
        }
    }
    return text;
}
