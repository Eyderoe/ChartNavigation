#include "real.hpp"

real::real () {
    source = QGeoPositionInfoSource::createDefaultSource(this);
    if (source) {
        connect(source, &QGeoPositionInfoSource::positionUpdated, this, [](const QGeoPositionInfo &info) {
            if (info.isValid()) {
                QGeoCoordinate coord = info.coordinate();
            }
        });
        source->setUpdateInterval(1000);
        source->startUpdates();
    } else {
        qDebug() << "不支持定位";
    }
}

realAdapter::realAdapter () {}
