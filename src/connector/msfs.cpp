#include "msfs.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>

msfsAdapter::msfsAdapter () { client.start(); }

void msfsAdapter::setCallback (const std::function<void(bool)> &callbackFunc) {
    client.setCallback(callbackFunc);
}

void msfsAdapter::close () { client.close(); }

DatarefIdx msfsAdapter::addDatarefArray (const std::string &dataref, int32_t /*freq*/) {
    static constexpr std::array<std::string_view, 8> fields{
        "id", "lat", "lon", "alt", "trk", "vs", "flightId", "icao"};
    const auto it = std::find(fields.begin(), fields.end(), dataref);
    if (it == fields.end())
        throw std::invalid_argument("MSFS dataref not found: " + dataref);
    return {static_cast<size_t>(it - fields.begin()) + 1};
}

bool msfsAdapter::getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) {
    std::ranges::fill(container, defaultValue);
    if (!client.isConnected() || dataref.idx < 1 || dataref.idx > 8)
        return false;
    const auto planes = client.snapshot();
    if (dataref.idx >= 7) {
        std::ranges::fill(container, 0.0f);
        for (size_t i = 0; i < planes.size() && i * 8 < container.size(); ++i) {
            const auto &text = dataref.idx == 7 ? planes[i].flight : planes[i].icao;
            const auto count = std::min({size_t{8}, text.size(), container.size() - i * 8});
            for (size_t j = 0; j < count; ++j)
                container[i * 8 + j] = static_cast<unsigned char>(text[j]);
        }
        return true;
    }
    for (size_t i = 0; i < std::min(planes.size(), container.size()); ++i) {
        switch (dataref.idx) {
            case 1: container[i] = static_cast<float>(i + 1); break;
            case 2: container[i] = static_cast<float>(planes[i].latitude); break;
            case 3: container[i] = static_cast<float>(planes[i].longitude); break;
            case 4: container[i] = static_cast<float>(planes[i].altitude); break;
            case 5: container[i] = static_cast<float>(planes[i].heading); break;
            case 6: container[i] = static_cast<float>(planes[i].verticalSpeed); break;
            default: break;
        }
    }
    return true;
}

std::string msfsAdapter::getName () const { return "msfs"; }
SimulatorSource msfsAdapter::getType () const { return SimulatorSource::msfs; }
