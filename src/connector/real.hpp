#ifndef CHARTNAVIGATION_REAL_HPP
#define CHARTNAVIGATION_REAL_HPP

#include "interface.hpp"
#include <QtPositioning>

class real : public QObject {
        Q_OBJECT
    public:
        real ();
    private:
        QGeoPositionInfoSource *source{nullptr};
};

class realAdapter : public InterfaceSimu {
    public:
        realAdapter ();
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
};

#endif //CHARTNAVIGATION_REAL_HPP
