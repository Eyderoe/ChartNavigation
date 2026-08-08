#include "dataProvider.hpp"

#include <QDebug>
#include <cassert>
#include <json.hpp>
#include <set>
#include <stdexcept>

#include "services/settingManage.hpp"
#include "utils/constValue.hpp"


DataProvider::DataProvider (QObject *parent) : QObject(parent) {
    SettingsManager &ins = SettingsManager::instance();
    readTurbuCate();
    initConnect();
    // 高程数据
    const auto globeFolder = ins.get(SettingsManager::globeFolder).toString().toStdString();
    globeView = std::make_unique<NoaaGlobeView>(globeFolder);
    // 接收器切换
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
    simuUpdateTimer.setInterval(static_cast<int>(1000.0 / infoFreq));
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

const std::array<float, 512>& DataProvider::getFlightIcao () const {
    return multiIcaoVal;
}

void DataProvider::initConnect () {
    const auto &setting = SettingsManager::instance();
    // 存储设置
    connect(&setting, qOverload<SettingsManager::ConstKey, const QVariant&>(&SettingsManager::settingChanged), this,
            [this](const SettingsManager::ConstKey key, const QVariant &val) {
                switch (key) {
                    case SettingsManager::dataSource:
                        setConnector(val.toInt());
                        break;
                    case SettingsManager::tcasRange: {
                        switch (const auto mode = static_cast<TcasMode>(val.toInt()); mode) {
                            case TcasMode::nm30:
                            case TcasMode::nm6:
                            case TcasMode::none:
                            case TcasMode::all:
                                tcasMode = mode;
                                break;
                            default:
                                tcasMode = TcasMode::nm30;
                        }
                        break;
                    }
                    case SettingsManager::infoMode: {
                        switch (const auto mode = static_cast<InfoMode>(val.toInt()); mode) {
                            case InfoMode::base:
                            case InfoMode::extend:
                            case InfoMode::full:
                                infoMode = mode;
                                break;
                            default:
                                infoMode = InfoMode::base;
                        }
                        break;
                    }
                    case SettingsManager::useCalGeoHeading:
                        useCalGeo = val.toBool();
                        break;
                    case SettingsManager::showTrail:
                        showTrail = val.toBool();
                        break;
                    default:
                        break;
                }
            });
    // 临时设置
}

/**
 * @brief 获取TCAS显示范围
 * @return 当前TCAS范围模式
 */
TcasMode DataProvider::getTcasMode () const {
    return tcasMode;
}

/**
 * @brief 获取飞行器信息模式
 * @return 当前信息模式
 */
InfoMode DataProvider::getInfoMode () const {
    return infoMode;
}

bool DataProvider::getShowTrail () const {
    return showTrail;
}

bool DataProvider::getUseCalGeo () const {
    return useCalGeo;
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
    connector->getDataref(multiIcao, multiIcaoVal, 0);
    // 更新各航班轨迹 (航班号非空时可用)
    const int intervalMs = static_cast<int>(1000.0 / infoFreq);
    std::set<std::string> seen;
    const size_t available = getAvailableNum();
    for (size_t idx = 1; idx < available; ++idx) {
        const auto flightId = slice<std::string>(multiFlightIdVal, static_cast<int>(idx));
        if (flightId.empty())
            continue;
        seen.insert(flightId);
        trails.try_emplace(flightId, intervalMs).first->second.addPoint({multiLatVal[idx], multiLonVal[idx]});
    }
    if (trails.size() >= 128) { // map 大小达到 128 后, 一次性清空已消失航班的轨迹
        std::erase_if(trails, [&](const auto &item) {
            return !seen.contains(item.first);
        });
    }
    // 状态栏更新
    SettingsManager &ins = SettingsManager::instance();
    ins.set(SettingsManager::latitu, multiLatVal[0]);
    ins.set(SettingsManager::longitu, multiLonVal[0]);
    const int planeAlt = multiAltVal[0];
    if constexpr (platform != MultiPlatform::androidOS) {
        const int groundAlt = globeView->getAlt(multiLatVal[0], multiLonVal[0]);
        int agl = (groundAlt == -500) ? -500 : (planeAlt - groundAlt) * m2ft;
        agl = ((agl != -500) && (agl < 0)) ? 0 : agl;
        ins.set(SettingsManager::altRelat, agl);
    } else {
        ins.set(SettingsManager::altRelat, planeAlt * m2ft);
    }
    emit dataUpdated();
}

void DataProvider::simuInit () {
    // AI或多人
    multiId = connector->addDatarefArray("id", infoFreq);
    multiLat = connector->addDatarefArray("lat", infoFreq);
    multiLon = connector->addDatarefArray("lon", infoFreq);
    multiAlt = connector->addDatarefArray("alt", infoFreq);
    multiTrk = connector->addDatarefArray("trk", infoFreq);
    multiVs = connector->addDatarefArray("vs", infoFreq);
    multiFlightId = connector->addDatarefArray("flightId", infoFreq);
    multiIcao = connector->addDatarefArray("icao", infoFreq);
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

/**
 * @brief 返回机型对应尾流等级
 * @param icao 机型ICAO码
 * @return 不可用时为空格
 */
char DataProvider::getWakeCategory (const std::string &icao) const {
    const auto it = turbuCate.find(icao);
    return (it == turbuCate.end()) ? ' ' : it->second;
}

/**
 * @brief 获取航班地速
 * @param flightId 航班号
 * @return 地速 (节), 无该航班轨迹时为 0
 */
int DataProvider::getGroundSpeed (const std::string &flightId) const {
    const auto it = trails.find(flightId);
    return (it == trails.end()) ? 0 : it->second.calculateGroundSpeed();
}

/**
 * @brief 获取航班计算航向
 * @param flightId 航班号
 * @return 计算航向 (度, 0~359), 无该航班轨迹时为 -1
 */
int DataProvider::getGeoHeading (const std::string &flightId) const {
    const auto it = trails.find(flightId);
    return (it == trails.end()) ? -1 : it->second.calculateGeoHeading();
}

std::deque<Point2D> DataProvider::getPoints (const std::string &flightId) {
    const auto it = trails.find(flightId);
    return (it == trails.end()) ? std::deque<Point2D>{} : it->second.getPoints();
}

short DataProvider::getAlt (const float latitude, const float longitude) const {
    return globeView->getAlt(latitude, longitude);
}

/**
 * @brief 获取可用航空器数量
 * @return 数量
 */
size_t DataProvider::getAvailableNum () {
    return std::ranges::count_if(multiIdVal, [](const float value) { return value != 0.0f; });
}

void DataProvider::readTurbuCate () {
    QFile mappingFile(":/doc/resources/documents/wtc.json");
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    auto database = nlohmann::json{};
    database = nlohmann::json::parse(stream.readAll().toUtf8().constData());
    for (auto &[aftType, turbType] : database.items())
        turbuCate[aftType] = turbType.get<std::string>()[0]; // json没有字符类型 只有取字符串再拿
}
