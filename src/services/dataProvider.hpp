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
#include "utils/noaaGlobe.hpp"


enum class TcasMode:int {
    nm30, nm6, none, all // 30NM9900,6NM1200ft,none,all
};
enum class InfoMode:int {
    base, extend, full // 基本符号，拓展符号，完整符号
};

template <typename Str>
Str slice (const std::array<float, 512> &array, int idx);

// 因为数据不止 pdfView 需要了, 又抽象出来
class DataProvider : public QObject {
        Q_OBJECT
    public:
        explicit DataProvider (QObject *parent = nullptr);
        void closeSimu () const;
        void setConnector (int value);
        [[nodiscard]] bool isConnected () const;

        size_t getAvailableNum ();
        [[nodiscard]] const std::array<float, 64>& getIdValues () const;
        [[nodiscard]] const std::array<float, 64>& getLatValues () const;
        [[nodiscard]] const std::array<float, 64>& getLonValues () const;
        [[nodiscard]] const std::array<float, 64>& getAltValues () const;
        [[nodiscard]] const std::array<float, 64>& getTrkValues () const;
        [[nodiscard]] const std::array<float, 64>& getVsValues () const;
        [[nodiscard]] const std::array<float, 512>& getFlightIdValues () const;
        [[nodiscard]] const std::array<float, 512>& getFlightIcao () const;
        [[nodiscard]] char getWakeCategory (const std::string &icao) const; // 尾流等级
        [[nodiscard]] int getGroundSpeed (const std::string &flightId) const; // 地速, 不可用时为 0
        [[nodiscard]] int getGeoHeading (const std::string &flightId) const; // 计算航向, 不可用时为 -1
        std::deque<Point2D> getPoints (const std::string &flightId);
        short getAlt (float latitude, float longitude) const;

        [[nodiscard]] TcasMode getTcasMode () const; // TCAS显示范围
        [[nodiscard]] InfoMode getInfoMode () const; // 飞行器信息模式
        [[nodiscard]] bool getShowTrail () const;
        [[nodiscard]] bool getUseCalGeo () const;
    private:
        std::unique_ptr<InterfaceSimu> connector;
        DatarefIdx multiId{}, multiLat{}, multiLon{}, multiAlt{}, multiTrk{}, multiVs{};
        std::array<float, 64> multiIdVal{}, multiLatVal{}, multiLonVal{}, multiAltVal{}, multiTrkVal{}, multiVsVal{};
        DatarefIdx multiFlightId{}, multiIcao{};
        std::array<float, 512> multiFlightIdVal{}, multiIcaoVal{};
        QTimer simuUpdateTimer;
        bool connected{false};
        int infoFreq{1}; // 信息更新频率 Hz

        std::map<std::string, char> turbuCate; // 尾流等级
        std::map<std::string, AircraftTrail> trails; // 各航班轨迹, 航班号非空时可用
        TcasMode tcasMode{TcasMode::nm30};
        InfoMode infoMode{InfoMode::base};
        bool showTrail{false}, useCalGeo{false};

        std::unique_ptr<NoaaGlobeView> globeView;

        void initConnect ();
        void simuInit ();
        void simuInfoUpdate ();
        void setConnectState (bool state);
        void readTurbuCate ();
    Q_SIGNALS:
        void dataUpdated ();
};


/**
 * @brief 从数组中按索引切出对应内容
 * @param array 数组, 目前适用于航班号和机型ICAO码
 * @param idx 航空器索引, 64->8*64
 * @return 有效字符串
 */
template <typename Str>
Str slice (const std::array<float, 512> &array, const int idx) {
    Str str;
    str.reserve(8);
    for (int i = 8 * idx; i < 8 * (idx + 1) - 1; ++i) {
        if (array[i] == 0)
            continue;
        if constexpr (std::is_same_v<Str, QString>)
            str.append(QChar(static_cast<char>(array[i])));
        else
            str.push_back(static_cast<char>(array[i]));
    }
    return str;
}

#endif //CHARTNAVIGATION_DATAPROVIDER_HPP
