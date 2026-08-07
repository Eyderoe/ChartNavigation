#ifndef CHARTNAVIGATION_DATAPROVIDER_HPP
#define CHARTNAVIGATION_DATAPROVIDER_HPP


#include <QObject>
#include <QTimer>
#include <array>
#include <map>
#include <memory>
#include <string>

#include "connector/allAdapter.hpp"
#include "utils/geographic.hpp"


enum class TcasMode:int {
    nm30, nm6, none, all // 30NM9900,6NM1200ft,none,all
};
enum class InfoMode:int {
    base, extend, full // 基本符号，拓展符号，完整符号
};

QString slice (const std::array<float, 512> &array, int idx);

// 因为数据不止 pdfView 需要了, 又抽象出来
class DataProvider : public QObject {
        Q_OBJECT
    public:
        explicit DataProvider (QObject *parent = nullptr);
        void closeSimu () const;
        void setConnector (int value);
        bool isConnected () const;

        size_t getAvailableNum ();
        const std::array<float, 64>& getIdValues () const;
        const std::array<float, 64>& getLatValues () const;
        const std::array<float, 64>& getLonValues () const;
        const std::array<float, 64>& getAltValues () const;
        const std::array<float, 64>& getTrkValues () const;
        const std::array<float, 64>& getVsValues () const;
        const std::array<float, 512>& getFlightIdValues () const;
        const std::array<float, 512>& getFlightIcao () const;
        char getWakeCategory (const std::string &icao) const; // 尾流等级
        int getGroundSpeed (const std::string &flightId) const; // 地速, 不可用时为 0
        int getGeoHeading (const std::string &flightId) const; // 计算航向, 不可用时为 -1
        std::deque<Point2D> getPoints (const std::string &flightId) ;

        TcasMode getTcasMode () const; // TCAS显示范围
        InfoMode getInfoMode () const; // 飞行器信息模式
        bool getShowTrail () const;
        bool getUseCalGeo () const;
    private:
        std::unique_ptr<InterfaceSimu> connector;
        DatarefIdx multiId{}, multiLat{}, multiLon{}, multiAlt{}, multiTrk{}, multiVs{};
        std::array<float, 64> multiIdVal{}, multiLatVal{}, multiLonVal{}, multiAltVal{}, multiTrkVal{}, multiVsVal{};
        DatarefIdx multiFlightId{}, multiIcao{};
        std::array<float, 512> multiFlightIdVal{}, multiIcaoVal{};
        std::map<std::string, char> turbuCate; // 尾流等级
        std::map<std::string, AircraftTrail> trails; // 各航班轨迹, 航班号非空时可用
        TcasMode tcasMode{TcasMode::nm30};
        InfoMode infoMode{InfoMode::base};
        QTimer simuUpdateTimer;
        bool connected{false}, showTrail{false}, useCalGeo{false};
        int infoFreq{1}; // 信息更新频率 Hz

        void initConnect ();
        void simuInit ();
        void simuInfoUpdate ();
        void setConnectState (bool state);
        void readTurbuCate ();
    Q_SIGNALS:
        void dataUpdated ();
};


#endif //CHARTNAVIGATION_DATAPROVIDER_HPP
