#ifndef CHARTNAVIGATION_SIMCONNECTCLIENT_HPP
#define CHARTNAVIGATION_SIMCONNECTCLIENT_HPP

#include <QObject>
#include <QElapsedTimer>
#include <QLibrary>
#include <QTimer>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "SimConnectApi.hpp"

namespace msfs {
struct Aircraft {
    uint32_t object{};
    double latitude{}, longitude{}, altitude{}, heading{}, verticalSpeed{};
    std::string flight, icao, atcModel;
};

// 单线程 Qt 事件循环驱动，SimConnect 调用和上层回调均在所属线程执行。
class SimConnectClient : public QObject {
    public:
        explicit SimConnectClient (QString libraryPath = {}, uint32_t radiusMeters = 200000);
        ~SimConnectClient () override;
        void start ();
        void close ();
        void setCallback (const std::function<void(bool)> &callbackFunc);
        bool isConnected () const;
        std::vector<Aircraft> snapshot () const;
        QString errorString () const;
    private:
        QLibrary library;
        QTimer timer;
        QElapsedTimer clock;
        std::function<void(bool)> callback;
        QString lastError;
        void *handle{};
        uint32_t radius, nextRequest{100}, trafficRequest{}, expectedEntries{};
        qint64 retryAt{}, openedAt{}, ownAt{-1}, trafficAt{-1}, nextTrafficAt{};
        bool connected{}, running{};
        std::optional<Aircraft> own;
        std::map<uint32_t, Aircraft> traffic, pendingTraffic;
        std::set<uint32_t> entries;
        #ifdef _WIN32
            api::Open openApi{};
            api::Close closeApi{};
            api::AddDefinition addApi{};
            api::Request requestApi{};
            api::RequestByType requestByTypeApi{};
            api::Dispatch dispatchApi{};
        #endif
        bool open ();
        void poll ();
        void disconnect (const QString &reason);
        void setState (bool state);
        void receive (const api::Receive *message, uint32_t bytes);
};
} // namespace msfs
#endif
