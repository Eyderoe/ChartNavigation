#include "dataProvider.hpp"

#include <QDebug>
#include <cassert>
#include <stdexcept>

#include "services/settingManage.hpp"


DataProvider::DataProvider (QObject *parent) : QObject(parent) {
    initConnect();
    // 接收器切换
    SettingsManager &ins = SettingsManager::instance();
    const SimulatorSource source = static_cast<SimulatorSource>(ins.get(SettingsManager::dataSource, 0).toInt());
    qDebug() << "data source: " << static_cast<int>(source);
    switch (source) {
        case SimulatorSource::xplane:
            connector = std::make_unique<xpAdapter>();
            break;
        case SimulatorSource::wlan:
            connector = std::make_unique<wlanAdapter>();
            break;
        case SimulatorSource::real:
            connector = std::make_unique<realAdapter>();
            break;
        default:
            throw std::invalid_argument("inop adapter");
    }
    // 模拟器
    simuInit();
    // 定时器
    simuUpdateTimer.setInterval(1000);
    connect(&simuUpdateTimer, &QTimer::timeout, this, &DataProvider::simuInfoUpdate);
    simuUpdateTimer.start();
}

void DataProvider::closeSimu () const {
    if (connector)
        connector->close();
}

void DataProvider::setConnector (const int value) {
    if (connector)
        connector->close();
    setConnectState(false);
    switch (static_cast<SimulatorSource>(value)) {
        case SimulatorSource::xplane:
            connector = std::make_unique<xpAdapter>();
            break;
        case SimulatorSource::wlan:
            connector = std::make_unique<wlanAdapter>();
            break;
        case SimulatorSource::real:
            connector = std::make_unique<realAdapter>();
            break;
        default:
            assert(false && "need to update switch case. [DataProvider::setConnector]");
    }
    simuInit();
}

bool DataProvider::isConnected () const {
    return connected;
}

const std::array<float, 64>& DataProvider::getIdValues () const {
    return multiIdVal;
}

const std::array<float, 64>& DataProvider::getLatValues () const {
    return multiLatVal;
}

const std::array<float, 64>& DataProvider::getLonValues () const {
    return multiLonVal;
}

const std::array<float, 64>& DataProvider::getAltValues () const {
    return multiAltVal;
}

const std::array<float, 64>& DataProvider::getTrkValues () const {
    return multiTrkVal;
}

const std::array<float, 64>& DataProvider::getVsValues () const {
    return multiVsVal;
}

const std::array<float, 512>& DataProvider::getFlightIdValues () const {
    return multiFlightIdVal;
}

void DataProvider::initConnect () {
    const auto &setting = SettingsManager::instance();
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::dataSource:
                        setConnector(val.toInt());
                        break;
                    default:
                        break;
                }
            });
}

void DataProvider::simuInfoUpdate () {
    if (!connected || !connector)
        return;
    connector->getDataref(multiId, multiIdVal, 0);
    connector->getDataref(multiLat, multiLatVal, 0);
    connector->getDataref(multiLon, multiLonVal, 0);
    connector->getDataref(multiAlt, multiAltVal, 0);
    connector->getDataref(multiTrk, multiTrkVal, 0);
    connector->getDataref(multiVs, multiVsVal, 0);
    connector->getDataref(multiFlightId, multiFlightIdVal, 0);
    // 设置更新
    SettingsManager &ins = SettingsManager::instance();
    ins.set(SettingsManager::latitu, static_cast<double>(multiLatVal[0]));
    ins.set(SettingsManager::longitu, static_cast<double>(multiLonVal[0]));
    ins.set(SettingsManager::altitu, static_cast<double>(multiAltVal[0]));
}

void DataProvider::simuInit () {
    // AI或多人
    constexpr int infoFreq = 1;
    multiId = connector->addDatarefArray("id", infoFreq);
    multiLat = connector->addDatarefArray("lat", infoFreq);
    multiLon = connector->addDatarefArray("lon", infoFreq);
    multiAlt = connector->addDatarefArray("alt", infoFreq);
    multiTrk = connector->addDatarefArray("trk", infoFreq);
    multiVs = connector->addDatarefArray("vs", infoFreq);
    multiFlightId = connector->addDatarefArray("flightId", infoFreq);
    // 回调
    connector->setCallback([this](const bool state) {
        setConnectState(state);
        qDebug() << "Simu-connect change state: " << state;
    });
}

void DataProvider::setConnectState (const bool state) {
    if (state == connected)
        return;
    connected = state;
    SettingsManager::instance().set(SettingsManager::simuConnect, state);
}
