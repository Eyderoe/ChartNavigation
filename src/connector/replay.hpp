#ifndef CHARTNAVIGATION_REPLAY_HPP
#define CHARTNAVIGATION_REPLAY_HPP

#include "interface.hpp"


// 用于保证回放时, 不被适配器影响
class replayAdapter : public InterfaceSimu {
    public:
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
        std::string name () const override;
};

#endif //CHARTNAVIGATION_REPLAY_HPP
