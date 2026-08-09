#ifndef CHARTNAVIGATION_REAL_HPP
#define CHARTNAVIGATION_REAL_HPP

#include "interface.hpp"
#include <QtPositioning>

class realPos : public QObject {
        Q_OBJECT
    public:
        realPos ();
        void close () const;
        void setCallback (const std::function<void  (bool)> &callbackFunc);
        [[nodiscard]] bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) const;
        void setFrequency (int32_t freq) const;
    private:
        QGeoPositionInfoSource *source{nullptr};
        double lat, lon, alt, trk;
        bool state{false};
        std::function<void  (bool)> callback{nullptr};

        void setState (bool newState);
};

class realAdapter : public InterfaceSimu {
    public:
        realAdapter ();
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
        std::string name () const override;
    private:
        realPos realPosition;
        std::map<std::string, int> datarefMap;
};

#endif //CHARTNAVIGATION_REAL_HPP
