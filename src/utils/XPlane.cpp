#include "XPlane.hpp"

eydTest::XPlaneUdp::XPlaneUdp () {}

void eydTest::XPlaneUdp::setCallback (const std::function<void(bool)> &callbackFunc) {
    callbackFunc(true);
}

eydTest::XPlaneUdp::DatarefIndex eydTest::XPlaneUdp::addDatarefArray (const std::string &dataref, int length,
                                                                      int32_t freq) {}
