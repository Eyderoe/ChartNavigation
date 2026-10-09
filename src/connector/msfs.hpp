#ifndef CHARTNAVIGATION_MSFS_HPP
#define CHARTNAVIGATION_MSFS_HPP

#include "interface.hpp"
#include "../../simulationPlugin/msfs/SimConnectClient.hpp"

class msfsAdapter : public InterfaceSimu {
    public:
        msfsAdapter ();
        void setCallback (const std::function<void(bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
        std::string getName () const override;
        SimulatorSource getType () const override;
    private:
        msfs::SimConnectClient client;
};

#endif //CHARTNAVIGATION_MSFS_HPP
