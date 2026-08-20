#ifndef CHARTNAVIGATION_XPSTD_HPP
#define CHARTNAVIGATION_XPSTD_HPP

#include "interface.hpp"
#include "XPlaneUDP.hpp"


// 绷不住了 现在才发现 XPlane 自身的 dataref 接口在局域网中就有用
class xpAdapter : public InterfaceSimu {
    public:
        xpAdapter ();
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
        std::string getName () const override;
        SimulatorSource getType () const override;
    private:
        eyderoe::XPlaneUdp xp;
        std::map<std::string, std::pair<std::string, int>> datarefMap{};
};


#endif //CHARTNAVIGATION_XPSTD_HPP
