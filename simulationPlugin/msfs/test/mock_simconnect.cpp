// 独立的 Windows ABI 假服务，验证动态符号、请求参数和异步生命周期。
// 不使用客户端的 ABI 头，避免测试与实现共享错误的结构偏移。
#include <Windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace {
struct Header { DWORD size, version, id; };
struct DataHeader { Header header; DWORD request, object, definition, flags, entry, outOf, count; };
struct Data { double lat, lon, alt, heading, vs, user; char flight[32], model[128]; };
static_assert(sizeof(DataHeader) == 40 && sizeof(Data) == 208);
std::deque<std::vector<char>> queue;
std::vector<char> current;
int mode{}, definitions{}, opens{}, closes{};
bool subscribed{};
auto nextOwn = std::chrono::steady_clock::now();

void aircraft (DWORD request, DWORD object, DWORD entry, DWORD total, bool user, DWORD receive = 9) {
    DataHeader header{{248, 0, receive}, request, object, 1, 0, entry, total, 8};
    Data data{31.0 + (user ? 0.0 : object * 0.000001), 121.0, 3048.25, -10.0, 1234.5, user ? 1.0 : 0.0, {}, {}};
    std::strcpy(data.flight, user ? "CES123456789" : "CCA100");
    std::strcpy(data.model, "A320");
    if (mode == 6)
        std::strcpy(data.model, "TT:ATCCOM.AC_MODEL_A320.0.text");
    if (mode == 4)
        data.lat = std::numeric_limits<double>::quiet_NaN();
    if (mode == 7)
        header.count = 7;
    if (mode == 8)
        header.header.size = 40;
    std::vector<char> bytes(sizeof(header) + sizeof(data));
    std::memcpy(bytes.data(), &header, sizeof(header));
    std::memcpy(bytes.data() + sizeof(header), &data, sizeof(data));
    queue.push_back(std::move(bytes));
}
}

#define EXPORT extern "C" __declspec(dllexport)
EXPORT void WINAPI MockReset () {
    queue.clear();
    current.clear();
    mode = definitions = opens = closes = 0;
    subscribed = false;
}
EXPORT void WINAPI MockSetMode (int value) { mode = value; queue.clear(); }
EXPORT int WINAPI MockOpenCount () { return opens; }
EXPORT int WINAPI MockCloseCount () { return closes; }

EXPORT HRESULT WINAPI SimConnect_Open (HANDLE *handle, const char *, HWND, DWORD, HANDLE, DWORD) {
    ++opens;
    definitions = 0;
    subscribed = false;
    nextOwn = std::chrono::steady_clock::now();
    *handle = reinterpret_cast<HANDLE>(1);
    return S_OK;
}
EXPORT HRESULT WINAPI SimConnect_Close (HANDLE) {
    ++closes;
    subscribed = false;
    queue.clear();
    return S_OK;
}
EXPORT HRESULT WINAPI SimConnect_AddToDataDefinition (HANDLE, DWORD definition, const char *name,
                                                      const char *units, DWORD type, float, DWORD) {
    const std::array<const char *, 8> names{"PLANE LATITUDE", "PLANE LONGITUDE", "PLANE ALTITUDE",
        "PLANE HEADING DEGREES TRUE", "VERTICAL SPEED", "IS USER SIM", "ATC ID", "ATC MODEL"};
    const std::array<const char *, 6> expectedUnits{"degrees", "degrees", "meters", "degrees", "feet per minute", "Bool"};
    if (definition != 1 || definitions >= 8 || names[definitions] != std::string(name)
        || type != (definitions < 6 ? 4u : definitions == 6 ? 6u : 8u))
        return E_INVALIDARG;
    if (definitions < 6 ? (!units || std::string(units) != expectedUnits[definitions]) : units != nullptr)
        return E_INVALIDARG;
    ++definitions;
    return S_OK;
}
EXPORT HRESULT WINAPI SimConnect_RequestDataOnSimObject (HANDLE, DWORD request, DWORD definition,
    DWORD object, DWORD period, DWORD, DWORD, DWORD, DWORD) {
    if (definitions != 8 || request != 1 || definition != 1 || object != 0 || period != 4)
        return E_INVALIDARG;
    subscribed = true;
    return S_OK;
}
EXPORT HRESULT WINAPI SimConnect_RequestDataOnSimObjectType (HANDLE, DWORD request, DWORD definition,
                                                            DWORD radius, DWORD type) {
    if (definition != 1 || radius == 0 || radius > 200000 || type != 2)
        return E_INVALIDARG;
    if (mode == 1 || mode == 2 || mode == 4 || mode == 5)
        return S_OK;
    if (mode == 3) { // 空周边查询，只有 40 字节头。
        DataHeader header{{40, 0, 9}, request, 0, 1, 0, 0, 0, 0};
        std::vector<char> bytes(sizeof(header));
        std::memcpy(bytes.data(), &header, sizeof(header));
        queue.push_back(std::move(bytes));
        return S_OK;
    }
    // 逆序返回 70 架周边机和用户机；验证按距离排序及去重。
    for (DWORD i = 1; i <= 71; ++i)
        aircraft(request, i == 35 ? 42 : 1000 + 71 - i, i, 71, i == 35);
    return S_OK;
}
EXPORT HRESULT WINAPI SimConnect_GetNextDispatch (HANDLE, Header **message, DWORD *size) {
    if (mode == 2) {
        mode = 1;
        Header header{12, 0, 3};
        current.resize(sizeof(header));
        std::memcpy(current.data(), &header, sizeof(header));
    } else if (mode == 5) {
        mode = 1;
        const std::array<DWORD, 6> exception{24, 0, 1, 19, 27, 8};
        current.resize(sizeof(exception));
        std::memcpy(current.data(), exception.data(), sizeof(exception));
    } else {
        const auto now = std::chrono::steady_clock::now();
        if (queue.empty() && subscribed && mode != 1 && now >= nextOwn) {
            aircraft(1, 42, 1, 1, true, 8);
            nextOwn = now + std::chrono::seconds(1);
        }
        if (queue.empty()) {
            *message = nullptr;
            *size = 0;
            return E_FAIL; // 官方 API 在空队列时允许 E_FAIL。
        }
        current = std::move(queue.front());
        queue.pop_front();
    }
    *message = reinterpret_cast<Header *>(current.data());
    *size = static_cast<DWORD>(current.size());
    return S_OK;
}
