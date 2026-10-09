#ifndef CHARTNAVIGATION_SIMCONNECTAPI_HPP
#define CHARTNAVIGATION_SIMCONNECTAPI_HPP

#include <cstdint>

namespace msfs::api {
    // 原生 SimConnect C ABI 的最小只读子集。动态加载避免 MinGW/MSVC 导入库差异。
    // 官方 ABI 来源及可选 SDK 编译校验见 README.md；不包含微软 SDK 实现。
    inline constexpr uint32_t definition = 1, ownRequest = 1;
    inline constexpr uint32_t recvNull = 0, recvException = 1, recvQuit = 3;
    inline constexpr uint32_t recvData = 8, recvDataByType = 9;
    inline constexpr uint32_t float64 = 4, string32 = 6, string128 = 8;
    inline constexpr uint32_t objectUser = 0, typeAircraft = 2, periodSecond = 4;
    inline constexpr uint32_t unused = 0xffffffffu;

    struct Receive {
        uint32_t size, version, id;
    };
    struct ObjectData {
        Receive header;
        uint32_t request, object, define, flags, entry, outOf, defineCount;
        // 有效载荷紧跟此 40 字节头；通过 memcpy 读取，避免 DWORD 起始地址未对齐。
    };
    struct Exception {
        Receive header;
        uint32_t exception, sendId, index;
    };
    struct AircraftData {
        double latitude, longitude, altitude, heading, verticalSpeed, isUser;
        char flight[32], model[128];
    };
    static_assert(sizeof(Receive) == 12);
    static_assert(sizeof(ObjectData) == 40);
    static_assert(sizeof(Exception) == 24);
    static_assert(sizeof(AircraftData) == 208);

    #ifdef _WIN32
        static_assert(sizeof(void *) == 8, "MSFS native SimConnect requires a Windows x64 build");
        using Result = int32_t;
        using Open = Result (__stdcall *)(void **, const char *, void *, uint32_t, void *, uint32_t);
        using Close = Result (__stdcall *)(void *);
        using AddDefinition = Result (__stdcall *)(void *, uint32_t, const char *, const char *, uint32_t, float, uint32_t);
        using Request = Result (__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
        using RequestByType = Result (__stdcall *)(void *, uint32_t, uint32_t, uint32_t, uint32_t);
        using Dispatch = Result (__stdcall *)(void *, Receive **, uint32_t *);
    #endif
} // namespace msfs::api
#endif
