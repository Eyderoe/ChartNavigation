#include "SimConnectClient.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace msfs {
namespace {
QString findLibrary () {
    const auto explicitPath = qEnvironmentVariable("CHARTNAVIGATION_SIMCONNECT_DLL");
    if (!explicitPath.isEmpty())
        return explicitPath;
    QStringList candidates{QCoreApplication::applicationDirPath() + "/SimConnect.dll"};
    // CMake 使用完整源文件路径编译，开发时也能直接找到仓库内的 DLL。
    const QFileInfo sourceFile(QString::fromUtf8(__FILE__));
    if (sourceFile.isAbsolute())
        candidates << sourceFile.dir().filePath("SimConnect.dll");
    for (const auto *variable : {"MSFS2024_SDK", "MSFS_SDK", "MSFS2020_SDK"}) {
        const auto root = qEnvironmentVariable(variable);
        if (!root.isEmpty())
            candidates << QDir(root).filePath("SimConnect SDK/lib/SimConnect.dll");
    }
    candidates << "C:/MSFS 2024 SDK/SimConnect SDK/lib/SimConnect.dll"
               << "C:/MSFS SDK/SimConnect SDK/lib/SimConnect.dll";
    for (const auto &path : candidates)
        if (QFileInfo::exists(path))
            return QFileInfo(path).absoluteFilePath();
    // 只查明确的应用目录，不从当前工作目录加载未知 DLL。
    return candidates.first();
}

std::string fixedString (const char *data, size_t size) {
    size_t length{};
    while (length < size && data[length] != '\0')
        ++length;
    while (length && data[length - 1] == ' ')
        --length;
    return {data, length};
}

std::optional<Aircraft> decode (uint32_t object, const api::AircraftData &data) {
    if (!std::isfinite(data.latitude) || std::abs(data.latitude) > 90
        || !std::isfinite(data.longitude) || std::abs(data.longitude) > 180
        || !std::isfinite(data.altitude) || !std::isfinite(data.heading)
        || !std::isfinite(data.verticalSpeed) || !std::isfinite(data.isUser))
        return {};
    Aircraft aircraft;
    aircraft.object = object;
    aircraft.latitude = data.latitude;
    aircraft.longitude = data.longitude;
    aircraft.altitude = data.altitude; // 定义中已请求米。
    aircraft.heading = std::fmod(std::fmod(data.heading, 360.0) + 360.0, 360.0);
    aircraft.verticalSpeed = data.verticalSpeed; // 定义中已请求 ft/min。
    aircraft.flight = fixedString(data.flight, sizeof(data.flight));
    aircraft.atcModel = fixedString(data.model, sizeof(data.model));
    // ATC MODEL 不是保证准确的 ICAO designator。仅传递短字面代码；
    // TT:ATCCOM... 等本地化键和完整机型名称保留给诊断，不能截成伪 ICAO。
    if (aircraft.atcModel.size() >= 2 && aircraft.atcModel.size() <= 4
        && std::ranges::all_of(aircraft.atcModel, [](unsigned char ch) {
            return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        }))
        aircraft.icao = aircraft.atcModel;
    return aircraft;
}

double distanceKey (const Aircraft &a, const Aircraft &b) {
    // Haversine 中间值；排序无需转换成距离，跨日期变更线也能正确排序。
    constexpr double radians = 3.14159265358979323846 / 180.0;
    const double lat = std::sin((a.latitude - b.latitude) * radians / 2);
    const double lon = std::sin((a.longitude - b.longitude) * radians / 2);
    return lat * lat + std::cos(a.latitude * radians) * std::cos(b.latitude * radians) * lon * lon;
}
} // namespace

SimConnectClient::SimConnectClient (QString libraryPath, uint32_t radiusMeters) : radius(radiusMeters) {
    if (!radius || radius > 200000)
        throw std::invalid_argument("SimConnect radius must be 1..200000 meters");
    library.setFileName(libraryPath.isEmpty() ? findLibrary() : QFileInfo(libraryPath).absoluteFilePath());
    timer.setInterval(25);
    connect(&timer, &QTimer::timeout, this, &SimConnectClient::poll);
}

SimConnectClient::~SimConnectClient () {
    callback = {};
    close();
}

void SimConnectClient::start () {
    if (running)
        return;
    running = true;
    clock.start();
    retryAt = 0;
    timer.start();
    poll();
}

void SimConnectClient::close () {
    running = false;
    timer.stop();
    disconnect({});
}

void SimConnectClient::setCallback (const std::function<void(bool)> &callbackFunc) {
    callback = callbackFunc;
    if (callback)
        callback(connected);
}

bool SimConnectClient::isConnected () const { return connected; }
QString SimConnectClient::errorString () const { return lastError; }

void SimConnectClient::setState (bool state) {
    if (connected == state)
        return;
    connected = state;
    if (callback)
        callback(state);
}

void SimConnectClient::disconnect (const QString &reason) {
    #ifdef _WIN32
        if (handle && closeApi)
            closeApi(handle);
    #endif
    handle = nullptr;
    own.reset();
    traffic.clear();
    pendingTraffic.clear();
    entries.clear();
    trafficRequest = expectedEntries = 0;
    ownAt = trafficAt = -1;
    retryAt = clock.isValid() ? clock.elapsed() + 3000 : 0;
    if (!reason.isEmpty() && lastError != reason) {
        lastError = reason;
        qWarning().noquote() << "MSFS:" << reason;
    }
    setState(false);
}

bool SimConnectClient::open () {
    #ifdef _WIN32
        if (!library.isLoaded() && !library.load()) {
            disconnect("Cannot load " + library.fileName() + ": " + library.errorString());
            return false;
        }
        openApi = reinterpret_cast<api::Open>(library.resolve("SimConnect_Open"));
        closeApi = reinterpret_cast<api::Close>(library.resolve("SimConnect_Close"));
        addApi = reinterpret_cast<api::AddDefinition>(library.resolve("SimConnect_AddToDataDefinition"));
        requestApi = reinterpret_cast<api::Request>(library.resolve("SimConnect_RequestDataOnSimObject"));
        requestByTypeApi = reinterpret_cast<api::RequestByType>(library.resolve("SimConnect_RequestDataOnSimObjectType"));
        dispatchApi = reinterpret_cast<api::Dispatch>(library.resolve("SimConnect_GetNextDispatch"));
        if (!openApi || !closeApi || !addApi || !requestApi || !requestByTypeApi || !dispatchApi) {
            disconnect("SimConnect.dll is missing required native exports");
            return false;
        }
        const auto hr = openApi(&handle, "ChartNavigation", nullptr, 0, nullptr, 0);
        if (hr < 0 || !handle) {
            disconnect(QString("SimConnect_Open failed (0x%1); start MSFS and load a flight")
                           .arg(static_cast<uint32_t>(hr), 8, 16, QChar('0')));
            return false;
        }
        struct Definition { const char *name, *units; uint32_t type; };
        static constexpr Definition definitions[]{
            {"PLANE LATITUDE", "degrees", api::float64},
            {"PLANE LONGITUDE", "degrees", api::float64},
            {"PLANE ALTITUDE", "meters", api::float64},
            {"PLANE HEADING DEGREES TRUE", "degrees", api::float64},
            {"VERTICAL SPEED", "feet per minute", api::float64},
            {"IS USER SIM", "Bool", api::float64},
            {"ATC ID", nullptr, api::string32},
            {"ATC MODEL", nullptr, api::string128},
        };
        for (const auto &item : definitions) {
            if (addApi(handle, api::definition, item.name, item.units, item.type, 0, api::unused) < 0) {
                disconnect("Cannot define SimVar: " + QString::fromLatin1(item.name));
                return false;
            }
        }
        // 用户机使用固定 OBJECT_ID_USER，不能拿查询周边机时的对象编号替代。
        if (requestApi(handle, api::ownRequest, api::definition, api::objectUser,
                       api::periodSecond, 0, 0, 0, 0) < 0) {
            disconnect("Cannot subscribe to user aircraft");
            return false;
        }
        nextTrafficAt = clock.elapsed();
        openedAt = clock.elapsed();
        lastError.clear();
        return true;
    #else
        disconnect("Native SimConnect requires Windows x64");
        return false;
    #endif
}

void SimConnectClient::poll () {
    if (!running)
        return;
    const auto now = clock.elapsed();
    if (!handle && (now < retryAt || !open()))
        return;
    #ifdef _WIN32
        // 限制每次处理量，防止大量 AI 数据阻塞 GUI。
        for (int i = 0; handle && i < 256; ++i) {
            api::Receive *message{};
            uint32_t bytes{};
            if (dispatchApi(handle, &message, &bytes) < 0 || !message
                || bytes < sizeof(api::Receive) || message->id == api::recvNull)
                break;
            receive(message, bytes);
        }
        if (!handle)
            return;
        if ((ownAt >= 0 && now - ownAt >= 3000) || (ownAt < 0 && now - openedAt >= 15000)) {
            disconnect("User aircraft data timed out; waiting to reconnect");
            return;
        }
        if (trafficAt >= 0 && now - trafficAt >= 3000)
            traffic.clear();
        if (trafficRequest && now >= nextTrafficAt) {
            // 没有返回周边机时可能没有数据消息；超时后清空旧目标。
            traffic.clear();
            pendingTraffic.clear();
            trafficRequest = 0;
        }
        if (!trafficRequest && now >= nextTrafficAt) {
            trafficRequest = nextRequest++;
            if (nextRequest < 100) // 处理 DWORD 回绕，避开 ownRequest。
                nextRequest = 100;
            pendingTraffic.clear();
            entries.clear();
            expectedEntries = 0;
            nextTrafficAt = now + 1500;
            if (requestByTypeApi(handle, trafficRequest, api::definition, radius, api::typeAircraft) < 0)
                disconnect("Cannot request nearby aircraft");
        }
    #endif
}

void SimConnectClient::receive (const api::Receive *message, uint32_t bytes) {
    if (message->size < sizeof(api::Receive) || message->size > bytes)
        return;
    bytes = message->size;
    if (message->id == api::recvQuit) {
        disconnect("Simulator closed; waiting to reconnect");
        return;
    }
    if (message->id == api::recvException && bytes >= sizeof(api::Exception)) {
        api::Exception exception{};
        std::memcpy(&exception, message, sizeof(exception));
        disconnect(QString("SimConnect exception %1 (send %2, index %3)")
                       .arg(exception.exception).arg(exception.sendId).arg(exception.index));
        return;
    }
    if (message->id != api::recvData && message->id != api::recvDataByType)
        return;
    if (bytes < sizeof(api::ObjectData))
        return;
    api::ObjectData packet{};
    std::memcpy(&packet, message, sizeof(packet));
    if (packet.define != api::definition)
        return;
    if (packet.request != api::ownRequest && (!trafficRequest || packet.request != trafficRequest))
        return;
    if (message->id == api::recvDataByType && packet.request == trafficRequest && packet.outOf == 0) {
        traffic.clear();
        trafficRequest = 0;
        nextTrafficAt = clock.elapsed() + 1000;
        return;
    }
    if (bytes < sizeof(api::ObjectData) + sizeof(api::AircraftData) || packet.defineCount != 8)
        return;
    api::AircraftData raw{};
    std::memcpy(&raw, reinterpret_cast<const char *>(message) + sizeof(api::ObjectData), sizeof(raw));
    auto aircraft = decode(packet.object, raw);
    if (message->id == api::recvData && packet.request == api::ownRequest) {
        if (!aircraft)
            return;
        own = std::move(aircraft);
        ownAt = clock.elapsed();
        setState(true);
    } else if (message->id == api::recvDataByType && packet.request == trafficRequest) {
        if (packet.outOf > 10000 || packet.entry == 0 || packet.entry > packet.outOf)
            return;
        if (expectedEntries && expectedEntries != packet.outOf)
            return;
        expectedEntries = packet.outOf;
        entries.insert(packet.entry);
        if (aircraft && raw.isUser == 0)
            pendingTraffic[packet.object] = std::move(*aircraft);
        if (entries.size() == expectedEntries) {
            traffic = std::move(pendingTraffic);
            trafficAt = clock.elapsed();
            trafficRequest = 0;
            nextTrafficAt = clock.elapsed() + 1000;
        }
    }
}

std::vector<Aircraft> SimConnectClient::snapshot () const {
    if (!connected || !own)
        return {};
    std::vector<Aircraft> result{*own};
    for (const auto &[object, aircraft] : traffic)
        if (object != own->object)
            result.push_back(aircraft);
    std::sort(result.begin() + 1, result.end(), [&](const auto &a, const auto &b) {
        const double da = distanceKey(a, *own), db = distanceKey(b, *own);
        return da == db ? a.object < b.object : da < db;
    });
    if (result.size() > 64)
        result.resize(64);
    return result;
}
} // namespace msfs
