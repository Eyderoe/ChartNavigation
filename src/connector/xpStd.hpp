#ifndef CHARTNAVIGATION_XPSTD_HPP
#define CHARTNAVIGATION_XPSTD_HPP

#include "interface.hpp"


#ifndef  __ANDROID__

#include "XPlaneUDP.hpp"
class xpAdapter : public InterfaceSimu {
    public:
        xpAdapter ();
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
    private:
        eyderoe::XPlaneUdp xp;
        std::map<std::string, std::pair<std::string, int>> datarefMap{};
};

#else

#include "real.hpp"
using xpAdapter = realAdapter;

#endif


#endif //CHARTNAVIGATION_XPSTD_HPP
