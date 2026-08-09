#include "xpStd.hpp"

xpAdapter::xpAdapter () : xp(true) {
    datarefMap["id"] = {"sim/cockpit2/tcas/targets/modeS_id", 64};
    datarefMap["lat"] = {"sim/cockpit2/tcas/targets/position/lat", 64};
    datarefMap["lon"] = {"sim/cockpit2/tcas/targets/position/lon", 64};
    datarefMap["alt"] = {"sim/cockpit2/tcas/targets/position/ele", 64};
    datarefMap["trk"] = {"sim/cockpit2/tcas/targets/position/psi", 64};
    datarefMap["vs"] = {"sim/cockpit2/tcas/targets/position/vertical_speed", 64};
    datarefMap["flightId"] = {"sim/cockpit2/tcas/targets/flight_id", 512};
    datarefMap["icao"] = {"sim/cockpit2/tcas/targets/icao_type", 512};
}

void xpAdapter::setCallback (const std::function<void  (bool)> &callbackFunc) {
    xp.setCallback(callbackFunc);
}

void xpAdapter::close () {
    xp.close();
}

DatarefIdx xpAdapter::addDatarefArray (const std::string &dataref, const int32_t freq) {
    const auto it = datarefMap.find(dataref);
    if (it == datarefMap.end())
        throw std::runtime_error("dataref not found");
    return {xp.addDatarefArray(it->second.first, it->second.second, freq).getIdx()};
}

bool xpAdapter::getDataref (const DatarefIdx &dataref, std::span<float> container, const float defaultValue) {
    const eyderoe::XPlaneUdp::DatarefIndex idx(dataref.idx);
    return xp.getDataref(idx, container, defaultValue);
}

std::string xpAdapter::name () const {
    return "xp";
}
