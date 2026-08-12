#include "replay.hpp"

void replayAdapter::setCallback (const std::function<void  (bool)> &callbackFunc) {}
void replayAdapter::close () {}
DatarefIdx replayAdapter::addDatarefArray (const std::string &dataref, int32_t freq) {
    return {};
}
bool replayAdapter::getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) {
    return false;
}
std::string replayAdapter::getName () const {
    return "replay";
}

SimulatorSource replayAdapter::getType () const {
    return SimulatorSource::replay;
}
