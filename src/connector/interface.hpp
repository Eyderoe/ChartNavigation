#ifndef CHARTNAVIGATION_INTERFACE_HPP
#define CHARTNAVIGATION_INTERFACE_HPP

#include <vector>
#include <span>


struct DatarefIdx {
    size_t idx;
};

class InterfaceSimu {
    public:
        virtual ~InterfaceSimu () = default;

        virtual void setCallback (const std::function<void  (bool)> &callbackFunc) =0;
        virtual void close () =0;

        virtual DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq = 1) =0;
        virtual bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue = 0) =0;
};

#endif //CHARTNAVIGATION_INTERFACE_HPP
