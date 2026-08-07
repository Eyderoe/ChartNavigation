#include "real.hpp"
#include <ranges>

realPos::realPos () {
    source = QGeoPositionInfoSource::createDefaultSource(this);
    if (source) {
        connect(source, &QGeoPositionInfoSource::positionUpdated, this, [this](const QGeoPositionInfo &info) {
            if (info.isValid()) {
                const QGeoCoordinate coord = info.coordinate();
                lat = coord.latitude();
                lon = coord.longitude();
                trk = info.attribute(QGeoPositionInfo::Direction);
                trk = qIsNaN(trk) ? 0 : trk; // nan可能导致绘制失效
                alt = (coord.type() == QGeoCoordinate::Coordinate3D) ? coord.altitude() : 0;
                setState(true);
            } else {
                setState(false);
            }
        });
        source->setUpdateInterval(1000);
        source->startUpdates();
    } else {
        qDebug() << "程序不支持定位";
    }
}

void realPos::close () const {
    if (callback)
        callback(false);
}

void realPos::setCallback (const std::function<void  (bool)> &callbackFunc) {
    callback = callbackFunc;
}

bool realPos::getDataref (const DatarefIdx &dataref, std::span<float> container, const float defaultValue) const {
    if (!state || container.empty()) {
        std::ranges::fill(container, defaultValue);
        return false;
    }

    // 数据可用
    auto copy2array = [&](const double value) {
        container[0] = static_cast<float>(value);
        std::ranges::fill(container.begin() + 1, container.end(), defaultValue);
    };
    switch (dataref.idx) {
        case 1: // id
            copy2array(0);
            break;
        case 2:
            copy2array(lat);
            break;
        case 3:
            copy2array(lon);
            break;
        case 4:
            copy2array(alt);
            break;
        case 5:
            copy2array(trk);
            break;
        default:
            std::ranges::fill(container, defaultValue);
    }
    return true;
}

void realPos::setFrequency (const int32_t freq) const {
    if (source)
        source->setUpdateInterval(1000 / freq);
}

void realPos::setState (const bool newState) {
    if (newState == state)
        return;
    state = newState;
    if (callback)
        callback(newState);
}

realAdapter::realAdapter () {
    datarefMap["id"] = 1;
    datarefMap["lat"] = 2;
    datarefMap["lon"] = 3;
    datarefMap["alt"] = 4;
    datarefMap["trk"] = 5;
    datarefMap["vs"] = 6;
    datarefMap["flightId"] = 7;
    datarefMap["icao"] = 8;
}

void realAdapter::setCallback (const std::function<void  (bool)> &callbackFunc) {
    realPosition.setCallback(callbackFunc);
}

void realAdapter::close () {
    realPosition.close();
}

DatarefIdx realAdapter::addDatarefArray (const std::string &dataref, int32_t freq) {
    const auto it = datarefMap.find(dataref);
    if (it == datarefMap.end())
        throw std::invalid_argument("dataref not found");
    return {static_cast<size_t>(it->second)};
}

bool realAdapter::getDataref (const DatarefIdx &dataref, const std::span<float> container, const float defaultValue) {
    return realPosition.getDataref(dataref, container, defaultValue);
}
