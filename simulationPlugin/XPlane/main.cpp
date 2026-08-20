#include <array>
#include <cmath>
#include <XPLMUtilities.h>
#include <XPLMProcessing.h>
#include <XPLMDataAccess.h>

#include "XPlane.hpp"
#include "plane.pb.h"
#include "func.hpp"


constexpr std::string magicHead = {0x40, 0x79, 0x54, 0x20};

XPLMDataRef multiId = nullptr; // int[64]: 唯一飞机标识
XPLMDataRef multiLat = nullptr; // float[64]: 纬度
XPLMDataRef multiLon = nullptr; // float[64]: 经度
XPLMDataRef multiAlt = nullptr; // float[64]: 高度
XPLMDataRef multiTrk = nullptr; // float[64]: 真航向
XPLMDataRef multiVs = nullptr; // float[64]: 垂直速度
XPLMDataRef multiFlightId = nullptr; // byte[512]: 航班号
XPLMDataRef multiIcao = nullptr; // byte[512]: 机型ICAO号

std::array<int, 64> idValues{};
std::array<float, 64> latValues{}, lonValues{}, altValues{}, trkValues{}, vsValues{};
std::array<char, 512> flightValues{}, icaoValues{};

XPMPIUBS xp{};

bool needPool{false};

float callback (float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon) {
    if (needPool) {
        needPool = false;
        xp.poll();
        return 0.9;
    }
    // 先确定哪些可用
    XPLMGetDatavi(multiId, idValues.data(), 0, 64);
    const size_t available = std::ranges::count_if(idValues, [](const int id) { return id != 0; });
    // 获取
    XPLMGetDatavf(multiLat, latValues.data(), 0, 64);
    XPLMGetDatavf(multiLon, lonValues.data(), 0, 64);
    XPLMGetDatavf(multiAlt, altValues.data(), 0, 64);
    XPLMGetDatavf(multiTrk, trkValues.data(), 0, 64);
    XPLMGetDatavf(multiVs, vsValues.data(), 0, 64);
    XPLMGetDatab(multiFlightId, flightValues.data(), 0, 512);
    XPLMGetDatab(multiIcao, icaoValues.data(), 0, 512);
    // 循环
    std::vector<Planes> planes;
    planes.emplace_back();
    size_t nowSize{};
    for (int i = 0; i < available; ++i) {
        Plane plane;
        plane.set_id(i);
        plane.set_lat(latValues[i]);
        plane.set_lon(lonValues[i]);
        plane.set_alt(static_cast<int>(altValues[i]));
        const float trk = std::fmod(trkValues[i] + 360, 360.0f);
        plane.set_trk(static_cast<int>(trk));
        plane.set_vs(static_cast<int>(vsValues[i]));
        plane.set_flight(getString(flightValues, i));
        plane.set_icao(getString(icaoValues, i));
        // 序列化
        std::string data;
        plane.SerializeToString(&data);
        if (data.size() + nowSize < 1300) { // 没超UDP包 1472
            nowSize += data.size();
        } else { //超过了
            planes.emplace_back();
            nowSize = data.size();
        }
        *planes[planes.size() - 1].add_planes() = std::move(plane);
    }
    // 发送包
    for (const auto &plane : planes) {
        std::string data{magicHead + static_cast<char>(0xff & available)};
        plane.AppendToString(&data);
        xp.sendData(std::make_shared<std::string>(std::move(data)));
    }
    needPool = true;
    return 0.1;
}

PLUGIN_API int XPluginStart (char *outName, char *outSig, char *outDesc) {
    // 插件信息
    strcpy(outName, "ChartNavi");
    strcpy(outSig, "Eyderoe.CharNavi");
    strcpy(outDesc, "brocast multi-player info");
    // 网络调试输出
    const auto realNetwork = xp.getIp();
    auto info = std::format("Chart Navigation : Send udp via {}, ", realNetwork.to_string());
    info += realNetwork.to_string().starts_with("192.168") ? "may be a good choice.\n" : "may be a bad choice.\n";
    XPLMDebugString(info.c_str());
    // 查找dataref
    multiId = XPLMFindDataRef("sim/cockpit2/tcas/targets/modeS_id");
    multiLat = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/lat");
    multiLon = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/lon");
    multiAlt = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/ele");
    multiTrk = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/psi");
    multiVs = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vertical_speed");
    multiFlightId = XPLMFindDataRef("sim/cockpit2/tcas/targets/flight_id");
    multiIcao = XPLMFindDataRef("sim/cockpit2/tcas/targets/icao_type");
    // 注册回调
    XPLMRegisterFlightLoopCallback(callback, 1, nullptr);
    return 1;
}

PLUGIN_API void XPluginStop (void) {
    XPLMUnregisterFlightLoopCallback(callback, nullptr);
}

PLUGIN_API void XPluginDisable (void) {}

PLUGIN_API int XPluginEnable (void) { return 1; }

PLUGIN_API void XPluginReceiveMessage (XPLMPluginID inFrom, int inMsg, void *inParam) {}
