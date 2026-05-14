#ifndef CHARTNAVIGATION_DATAPROVIDER_HPP
#define CHARTNAVIGATION_DATAPROVIDER_HPP


#include <QObject>
#include <QTimer>
#include <array>
#include <memory>

#include "connector/allAdapter.hpp"


// 因为数据不止 pdfView 需要了, 又抽象出来
class DataProvider : public QObject {
        Q_OBJECT
    public:
        explicit DataProvider (QObject *parent = nullptr);
        void closeSimu () const;
        void setConnector (int value);
        [[nodiscard]] bool isConnected () const;

        [[nodiscard]] const std::array<float, 64>& getIdValues () const;
        [[nodiscard]] const std::array<float, 64>& getLatValues () const;
        [[nodiscard]] const std::array<float, 64>& getLonValues () const;
        [[nodiscard]] const std::array<float, 64>& getAltValues () const;
        [[nodiscard]] const std::array<float, 64>& getTrkValues () const;
        [[nodiscard]] const std::array<float, 64>& getVsValues () const;
        [[nodiscard]] const std::array<float, 512>& getFlightIdValues () const;
    private:
        void initConnect ();
        void simuInit ();
        void simuInfoUpdate ();
        void setConnectState (bool state);

        std::unique_ptr<InterfaceSimu> connector;
        DatarefIdx multiId{}, multiLat{}, multiLon{}, multiAlt{}, multiTrk{}, multiVs{};
        std::array<float, 64> multiIdVal{}, multiLatVal{}, multiLonVal{}, multiAltVal{}, multiTrkVal{}, multiVsVal{};
        DatarefIdx multiFlightId{}, multiIcao{};
        std::array<float, 512> multiFlightIdVal{}, multiIcaoVal{};
        QTimer simuUpdateTimer;
        bool connected{false};
};


#endif //CHARTNAVIGATION_DATAPROVIDER_HPP
