#ifndef CHARTNAVIGATION_ALLADAPTER_HPP
#define CHARTNAVIGATION_ALLADAPTER_HPP

#include "xpStd.hpp"
#include "wlan.hpp"
#include "real.hpp"
#include "replay.hpp"

enum class SimulatorSource {
    xplane,
    wlan,
    real,
    replay,
};

// 绷不住了 现在才发现 XPlane 自身的 dataref 接口在局域网中就有用

#endif //CHARTNAVIGATION_ALLADAPTER_HPP
