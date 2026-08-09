#include "dataProvider.hpp"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QDataStream>
#include <QFile>
#include <cassert>
#include <json.hpp>
#include <set>
#include <stdexcept>

#include "settingManage.hpp"
#include "services/settingManage.hpp"
#include "utils/constValue.hpp"


DataProvider::DataProvider (QObject *parent) : QObject(parent) {
    SettingsManager &ins = SettingsManager::instance();
    readTurbulenceCategory();
    initConnect();
    // Debug 用途
    debugStoreData = ins.get(SettingsManager::debugStoreData, false).toBool();
    debugReplayData = ins.get(SettingsManager::debugReplayData, false).toBool();
    debugStoreData = debugStoreData && !debugReplayData;
    if (debugStoreData) {
        const QString filePath = QDir::tempPath() + "/ChartNavigation-" + QDateTime::currentDateTime().
                toString("yyyyMMdd-HHmmss") + ".txt";
        replayData = std::make_unique<QFile>(filePath);
        replayData->open(QIODevice::WriteOnly);
        const QByteArray freqBlock = QByteArray::number(infoFreq) + "\n";
        replayData->write("offsetTimeMs,0\n");
        replayData->write(freqBlock);
        startTime = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "ReplayData path: " + filePath;
    }
    // 高程数据
    const auto globeFolder = ins.get(SettingsManager::globeFolder).toString().toStdString();
    globeView = std::make_unique<NoaaGlobeView>(globeFolder);
    // 接收器切换
    if (debugReplayData)
        ins.set(SettingsManager::dataSource, 3);
    SimulatorSource source = static_cast<SimulatorSource>(ins.get(SettingsManager::dataSource, 0).toInt());
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
        case SimulatorSource::replay:
            connector = std::make_unique<replayAdapter>();
            break;
        default:
            throw std::invalid_argument("inop adapter");
    }
    // 模拟器
    initSimulateDataConnect();
    // 定时器
    if (!debugReplayData) { // 正常情况
        simuUpdateTimer.setInterval(static_cast<int>(1000.0 / infoFreq));
        connect(&simuUpdateTimer, &QTimer::timeout, this, &DataProvider::simulateDataUpdate);
        simuUpdateTimer.start();
    } else { // 回放数据
        fs::path replayFile = ins.get(SettingsManager::debugReplayFile, "inop").toString().toStdString();
        eventManager = std::make_unique<EventManage>(replayFile);
        connect(eventManager.get(), &EventManage::progressChanged, this, &DataProvider::replayProgressChanged);
        connect(eventManager.get(), &EventManage::eventReady, this, [this](const Event &event) {
            switch (event.type) {
                case EventType::connectState:
                    setConnectState(std::get<bool>(event.payload));
                    break;
                case EventType::simulateData:
                    replayDataUpdate(event);
                    break;
            }
        });
        connect(eventManager.get(), &EventManage::finished, this, [this] {
            qDebug() << "Replay finished";
            setConnectState(false);
        });
        // 回放控制窗口, 只有回放模式会创建并显示; 挂在宿主窗口下, 随主窗口一起关闭
        replayControl = new replay_control(qobject_cast<QWidget *>(parent));
        replayControl->setWindowFlag(Qt::Window);
        replayControl->setDuration(eventManager->durationMs());
        connect(replayControl, &replay_control::stepEventsRequested, this, &DataProvider::replayStepEvents);
        connect(replayControl, &replay_control::stepPercentRequested, this, &DataProvider::replayStepPercent);
        connect(replayControl, &replay_control::seekPercentRequested, this, &DataProvider::replaySeekPercent);
        connect(this, &DataProvider::replayProgressChanged, replayControl, &replay_control::setPosition);
        replayControl->show();
        eventManager->start();
    }
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
        case SimulatorSource::replay:
            connector = std::make_unique<replayAdapter>();
            break;
        default:
            assert(false && "need to update switch case. [DataProvider::setConnector]");
    }
    initSimulateDataConnect();
}

bool DataProvider::isConnected () const {
    return connected;
}

bool DataProvider::isReplayMode () const {
    return debugReplayData;
}

size_t DataProvider::replayEventCount () const {
    return eventManager ? eventManager->eventCount() : 0;
}

void DataProvider::replayStepEvents (const int delta) {
    if (eventManager)
        eventManager->stepEvents(delta);
}

void DataProvider::replayStepPercent (const int deltaPercent) {
    if (eventManager)
        eventManager->stepPercent(deltaPercent);
}

void DataProvider::replaySeekPercent (const int percent) {
    if (eventManager)
        eventManager->seekPercent(percent);
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

void DataProvider::simulateDataUpdate () {
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
    // Debug用途
    if (debugStoreData) {
        QByteArray payload;
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds << (QDateTime::currentMSecsSinceEpoch() - startTime)
                << QByteArray("data");
        auto writeArray = [&ds](const auto &arr) {
            for (const float v : arr)
                ds << v;
        };
        writeArray(multiIdVal);
        writeArray(multiLatVal);
        writeArray(multiLonVal);
        writeArray(multiAltVal);
        writeArray(multiTrkVal);
        writeArray(multiVsVal);
        writeArray(multiFlightIdVal);
        writeArray(multiIcaoVal);
        const QByteArray compressed = qCompress(payload, 1); // 大量的连续0, 有点压缩等级就行
        replayData->write(QByteArray::number(QDateTime::currentMSecsSinceEpoch() - startTime));
        replayData->write(",data,");
        replayData->write(QByteArray::number(compressed.size()));
        replayData->write("\n");
        replayData->write(compressed);
    }
    processDataFrame();
}

void DataProvider::processDataFrame () {
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
    const int planeAlt = static_cast<int>(multiAltVal[0]);
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

void DataProvider::initSimulateDataConnect () {
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
        if (debugStoreData) {
            const QByteArray stateBlock = state ? QByteArray("true\n") : QByteArray("false\n");
            replayData->write(QByteArray::number(QDateTime::currentMSecsSinceEpoch() - startTime));
            replayData->write(",connectState,");
            replayData->write(QByteArray::number(stateBlock.size()));
            replayData->write("\n");
            replayData->write(stateBlock);
        }
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

const std::deque<Point2D>& DataProvider::getPoints (const std::string &flightId) {
    const auto it = trails.find(flightId);
    return (it == trails.end()) ? emptyDeque : it->second.getPoints();
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

void DataProvider::readTurbulenceCategory () {
    QFile mappingFile(":/doc/resources/documents/wtc.json");
    mappingFile.open(QIODevice::ReadOnly);
    QTextStream stream(&mappingFile);
    auto database = nlohmann::json{};
    database = nlohmann::json::parse(stream.readAll().toUtf8().constData());
    for (auto &[aftType, turbType] : database.items())
        turbuCate[aftType] = turbType.get<std::string>()[0]; // json没有字符类型 只有取字符串再拿
}

void DataProvider::replayDataUpdate (const Event &event) {
    // simulateData: payload 是压缩后的 QDataStream 字节, 使用时先解压
    const auto &bytes = std::get<std::vector<uint8_t>>(event.payload);
    QByteArray uncompressed = qUncompress(reinterpret_cast<const uchar*>(bytes.data()),
                                          static_cast<qsizetype>(bytes.size()));
    QDataStream ds(&uncompressed, QIODevice::ReadOnly);
    qint64 time;
    QByteArray tag;
    ds >> time >> tag;
    auto readArray = [&ds](auto &arr) {
        for (auto &v : arr)
            ds >> v;
    };
    readArray(multiIdVal);
    readArray(multiLatVal);
    readArray(multiLonVal);
    readArray(multiAltVal);
    readArray(multiTrkVal);
    readArray(multiVsVal);
    readArray(multiFlightIdVal);
    readArray(multiIcaoVal);
    processDataFrame();
}
