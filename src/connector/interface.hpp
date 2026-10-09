#ifndef CHARTNAVIGATION_INTERFACE_HPP
#define CHARTNAVIGATION_INTERFACE_HPP

#include <vector>
#include <span>
#include <cstdint>
#include <functional>
#include <string>


enum class SimulatorSource {
    xplane,
    wlan,
    real,
    replay,
    msfs, // 追加，保留已存设置中的 0..3 编号。
};

struct DatarefIdx {
    size_t idx;
};

class InterfaceSimu {
    public:
        virtual ~InterfaceSimu () = default;

        virtual void setCallback (const std::function<void  (bool)> &callbackFunc) =0;
        virtual void close () =0;

        virtual DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) =0;
        virtual bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) =0;

        virtual std::string getName () const =0;
        virtual SimulatorSource getType () const =0;
};


#endif //CHARTNAVIGATION_INTERFACE_HPP
