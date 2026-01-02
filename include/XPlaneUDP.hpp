#ifndef XPLANEUDP_HPP
#define XPLANEUDP_HPP

#include <boost/system.hpp>
#include <boost/asio.hpp>
#include <boost/dynamic_bitset.hpp>
#include <format>
#include <iostream>
#include <ranges>
#include <memory>
#include <array>
#include <cstring>
#include <boost/pool/pool_alloc.hpp>

namespace eyderoe
{
#ifdef _WIN32
constexpr bool IS_WIN = true;
#else
constexpr bool IS_WIN = false;
#endif

static constexpr std::string MULTI_CAST_GROUP{"239.255.1.1"};
static constexpr unsigned short MULTI_CAST_PORT{49707};

template <typename T>
concept Container = std::ranges::random_access_range<T> &&
        std::ranges::sized_range<T> &&
        std::is_assignable_v<std::ranges::range_reference_t<T>, float> && requires(T contain) {
            contain.size();
        } ;
template <typename T>
concept CharContainer = requires(T contain) {
    requires std::same_as<typename T::value_type, char>;
    contain.data();
};

constexpr int HEADER_LENGTH{5}; // 指令头部长度 4字母+1空
constexpr static std::string DATAREF_GET_HEAD{'R', 'R', 'E', 'F', '\x00'};
constexpr static std::string DATAREF_SET_HEAD{'D', 'R', 'E', 'F', '\x00'};
constexpr static std::string BASIC_INFO_HEAD{'R', 'P', 'O', 'S', '\x00'};
constexpr static std::string BECON_HEAD{'B', 'E', 'C', 'N', '\x00'};

namespace sys = boost::system;
namespace asio = boost::asio;
namespace ip = asio::ip;

class XPlaneUdp;
class BufferPool;

template <typename T, typename... Rests>
    requires (std::same_as<std::string, T> || std::is_fundamental_v<T>)
size_t packSize (size_t offset, const T &first, const Rests &... rest);
template <typename T1, typename T2, typename... Rests>
    requires (std::same_as<std::string, T2> || std::is_fundamental_v<T2>)
size_t pack (T1 &container, size_t offset, const T2 &first, const Rests &... rest);


class BufferPool {
    struct BufferPro {
        std::array<char, 1472> data{};
        size_t length;
        BufferPro () : length(0) { std::memset(data.data(), 0x00, data.size()); }
    };
    public:
        BufferPool () = default;
        [[nodiscard]] std::shared_ptr<std::array<char, 1472>> getBuffer (size_t length) const;
    private:
        boost::pool_allocator<BufferPro> allocator;
        void recycleBuffer (BufferPro *buffer) const;
};

class XPlaneUdp {
    public:
        struct DatarefIndex {
            const size_t idx;
        };
        struct PlaneInfo {
            double lon, lat, alt; // 经纬度 高度
            float agl, pitch, track, roll; // 离地高 / 俯仰 真航向 滚转
            float vX, vY, vZ, rollRate, pitchRate, yawRate; // 三轴速度 / 横滚 俯仰 偏航
        };

        explicit XPlaneUdp (bool autoReConnect = true);
        ~XPlaneUdp ();
        XPlaneUdp (const XPlaneUdp &) = delete;
        XPlaneUdp& operator= (const XPlaneUdp &) = delete;
        XPlaneUdp (XPlaneUdp &&) = delete;
        XPlaneUdp& operator= (XPlaneUdp &&) = delete;

        void setCallback (const std::function<void  (bool)> &callbackFunc);
        void reconnect (bool del = false);
        void close ();

        DatarefIndex addDataref (const std::string &dataref, int32_t freq = 1, int index = -1);
        DatarefIndex addDatarefArray (const std::string &dataref, int length, int32_t freq = 1);
        bool getDataref (const DatarefIndex &dataref, float &value, float defaultValue = 0) const;
        template <Container T>
        bool getDataref (const DatarefIndex &dataref, T &container, float defaultValue = 0);
        void changeDatarefFreq (const DatarefIndex &dataref, float freq);
        void setDataref (const std::string &dataref, float value, int index = -1);
        template <Container T>
        void setDatarefArray (const std::string &dataref, const T &value);

        void addPlaneInfo (int freq = 1);
        void getPlaneInfo (PlaneInfo &infoDst) const;
    private:
        struct DatarefInfo {
            std::string name; // dataref 长度
            int start, end; // [start,end]
            int32_t freq; // 频率
            bool available; // 是否可用
            bool isArray; // 是否是数组
        };

        // 数据
        std::vector<DatarefInfo> dataRefs;
        std::vector<float> values;
        boost::dynamic_bitset<> space;
        std::unordered_map<std::string, size_t> exist;
        PlaneInfo info{.track = -1};
        BufferPool pool{};
        // 网络
        bool autoReconnect; // 自动重连
        asio::io_context io_context{}; // 上下文
        asio::executor_work_guard<asio::io_context::executor_type> workGuard;
        ip::udp::socket multicastSocket{io_context}; // 监听多播
        ip::udp::socket xpSocket{io_context}; // xp通信
        ip::udp::endpoint xpEndpoint; // xp端口
        std::thread worker; // io_content驱动
        int infoFreq{}; // 基本信息频率
        // 回调
        bool state{false}; // xp状态
        std::function<void  (bool)> callback{}; // 回调

        void setState (bool newState);
        size_t findSpace (size_t length);
        void extendSpace ();
        void detectBeacon ();
        asio::awaitable<void> detect ();
        void sendData (const std::shared_ptr<std::array<char, 1472>> &data, size_t size);
        asio::awaitable<void> send (std::shared_ptr<std::array<char, 1472>> data, size_t size);
        void receiveData ();
        asio::awaitable<void> receive ();
        void receiveDataProcess (const std::shared_ptr<std::array<char, 1472>> &data, size_t size,
                                 const ip::udp::endpoint &sender);
};

/**
 * @brief 解包一串字符数据
 * @param container 容器
 * @param offset 偏移字节
 */
template <typename CharContainer, typename First, typename... Rests>
void unpack (const CharContainer &container, size_t offset, First &first, Rests &... rest) {
    memcpy(&first, container.data() + offset, sizeof(First));
    if constexpr (sizeof...(rest) > 0)
        unpack(container, offset + sizeof(First), rest...);
}


/**
 * @brief 打包字符数量
 * @param offset 偏移量
 * @param first string,基本类型
 * @return 打包数据量
 */
template <typename T, typename... Rests>
    requires (std::same_as<std::string, T> || std::is_fundamental_v<T>)
size_t packSize (const size_t offset, const T &first, const Rests &... rest) {
    if constexpr (std::same_as<std::string, T>) { // string
        if constexpr (sizeof...(rest) > 0)
            return packSize(offset + first.size(), rest...);
        else
            return offset + first.size();
    } else { // 基本类型
        if constexpr (sizeof...(rest) > 0)
            return packSize(offset + sizeof(T), rest...);
        else
            return offset + sizeof(T);
    }
}

/**
 * @brief 打包为字符数组
 * @param container 容器
 * @param offset 偏移量
 * @param first string,基本类型
 * @return 打包数据量
 */
template <typename T1, typename T2, typename... Rests>
    requires (std::same_as<std::string, T2> || std::is_fundamental_v<T2>)
size_t pack (T1 &container, const size_t offset, const T2 &first, const Rests &... rest) {
    if constexpr (std::same_as<std::string, T2>) { // string
        memcpy(container.data() + offset, first.data(), first.size());
        if constexpr (sizeof...(rest) > 0)
            return pack(container, offset + first.size(), rest...);
        else
            return offset + first.size();
    } else { // 基本类型
        memcpy(container.data() + offset, &first, sizeof(T2));
        if constexpr (sizeof...(rest) > 0)
            return pack(container, offset + sizeof(T2), rest...);
        else
            return offset + sizeof(T2);
    }
}

/**
 * @brief 获取 dataref 最新值
 * @param dataref 标识
 * @param container 容器
 * @param defaultValue 默认值
 * @return 值可用
 */
template <Container T>
bool XPlaneUdp::getDataref (const DatarefIndex &dataref, T &container, float defaultValue) {
    const auto &ref = dataRefs[dataref.idx];
    const size_t size = ref.end - ref.start + 1;
    if (!ref.available) {
        std::ranges::fill(container | std::views::take(size), defaultValue);
        return false;
    }
    auto source = values | std::views::drop(ref.start) | std::views::take(std::min(size, container.size()));
    std::ranges::copy(source, container.begin());
    return true;
}

/**
 * @brief 设置某组 dataref 值
 * @param dataref dataref 名称
 * @param value 容器
 */
template <Container T>
void XPlaneUdp::setDatarefArray (const std::string &dataref, const T &value) {
    for (int i = 0; i < value.size(); ++i) {
        const size_t bufferSize = packSize(0, DATAREF_SET_HEAD, value[i], std::format("{}[{}]", dataref, i), '\x00');
        const auto buffer = pool.getBuffer(bufferSize);
        pack(*buffer, 0, DATAREF_SET_HEAD, value[i], std::format("{}[{}]", dataref, i), '\x00');
        sendData(buffer, 509);
    }
}


/**
 * @brief 获取一个 array<char, 1472>
 * @param length 会使用的长度
 * @return 智能指针包含的数组
 */
inline std::shared_ptr<std::array<char, 1472>> BufferPool::getBuffer (const size_t length) const {
    BufferPro *buffer = allocator.allocate(1);
    new(buffer) BufferPro();
    buffer->length = length;
    std::memset(buffer->data.data(), 0x00, buffer->data.size());
    auto deleter = [this](std::array<char, 1472> *ptr) {
        BufferPro *buffer_ = reinterpret_cast<BufferPro*>(ptr);
        this->recycleBuffer(buffer_);
    };
    return {&buffer->data, deleter};
}

inline void BufferPool::recycleBuffer (BufferPro *buffer) const {
    if (buffer) {
        std::memset(buffer->data.data(), 0x00, buffer->length);
        buffer->~BufferPro();
        allocator.deallocate(buffer, 1);
    }
}

inline XPlaneUdp::XPlaneUdp (const bool autoReConnect) : autoReconnect(autoReConnect),
                                                         workGuard(asio::make_work_guard(io_context)),
                                                         worker([this] () { io_context.run(); }) {
    // 监听信标帧
    // * 自身地址
    multicastSocket.open(ip::udp::v4());
    const asio::socket_base::reuse_address option(true);
    multicastSocket.set_option(option);
    // * XPlane广播地址
    ip::udp::endpoint multicastEndpoint;
    if constexpr (IS_WIN)
        multicastEndpoint = ip::udp::endpoint(ip::udp::v4(), MULTI_CAST_PORT);
    else
        multicastEndpoint = ip::udp::endpoint(ip::make_address(MULTI_CAST_GROUP), MULTI_CAST_PORT);
    multicastSocket.bind(multicastEndpoint);
    // * 加入多播组
    const ip::address_v4 multicastAddress = ip::make_address_v4(MULTI_CAST_GROUP);
    multicastSocket.set_option(ip::multicast::join_group(multicastAddress));
    detectBeacon();
}

inline XPlaneUdp::~XPlaneUdp () {
    workGuard.reset();
    xpSocket.close();
    multicastSocket.close();
    io_context.stop();
    if (worker.joinable()) {
        worker.join();
    }
}

/**
 * @brief 设置一个回调函数,xp连接状态改变时会调用
 * @param callbackFunc 回调函数 接受形参bool
 */
inline void XPlaneUdp::setCallback (const std::function<void  (bool)> &callbackFunc) {
    callback = callbackFunc;
}

/**
 * @brief 重连
 */
inline void XPlaneUdp::reconnect (const bool del) {
    // dataref
    for (auto &[name, start, end, freq, available, isArray] : dataRefs) {
        if (!available)
            continue;
        for (int i = start; i <= end; ++i) {
            std::string combine = isArray ? std::format("{}[{}]", name, i - start) : name;
            const size_t size = packSize(0, DATAREF_GET_HEAD, del ? 0 : freq, i, combine);
            auto buffer = pool.getBuffer(size);
            pack(*buffer, 0, DATAREF_GET_HEAD, freq, i, combine);
            sendData(buffer, 413);
        }
    }
    // 信息
    if (infoFreq == 0)
        return;
    const std::string sentence = std::format("{}{}\x00", BASIC_INFO_HEAD, del ? 0 : infoFreq);
    const auto buffer2 = pool.getBuffer(sentence.size());
    pack(*buffer2, 0, sentence);
    sendData(buffer2, sentence.size());
}

/**
 * @brief 关闭所有 UDP 接收
 */
inline void XPlaneUdp::close () {
    reconnect(true);
}

/**
 * @brief 新增监听目标
 * @param dataref dataref 名称
 * @param freq 频率
 * @param index 目标为数组时的索引
 */
inline XPlaneUdp::DatarefIndex XPlaneUdp::addDataref (const std::string &dataref, int32_t freq, int index) {
    std::string name = (index == -1) ? dataref : std::format("{}[{}]", dataref, index);
    if (const auto it = exist.find(name); it != exist.end()) {
        std::cerr << "already exist! nothing change.";
        return {it->second};
    }
    size_t start = findSpace(1);
    dataRefs.emplace_back(name, start, start, freq, true, false);
    const size_t size = packSize(0, DATAREF_GET_HEAD, freq, start, name);
    const auto buffer = pool.getBuffer(size);
    pack(*buffer, 0, DATAREF_GET_HEAD, freq, start, name);
    sendData(buffer, 413);
    exist[name] = dataRefs.size() - 1;
    return {dataRefs.size() - 1};
}

/**
 * @brief 新增监听目标,目标为数组
 * @param dataref dataref 名称
 * @param length 数组长度
 * @param freq 频率
 */
inline XPlaneUdp::DatarefIndex XPlaneUdp::addDatarefArray (const std::string &dataref, const int length, int32_t freq) {
    if (const auto it = exist.find(dataref); it != exist.end()) {
        std::cerr << "already exist! nothing change.";
        return {it->second};
    }
    int start = static_cast<int>(findSpace(length));
    dataRefs.emplace_back(dataref, start, start + length - 1, freq, true, true);
    for (int i = 0; i < length; ++i) {
        std::string name = std::format("{}[{}]", dataref, i);
        const size_t size{packSize(0, DATAREF_GET_HEAD, freq, start + i, name)};
        auto buffer = pool.getBuffer(size);
        pack(*buffer, 0, DATAREF_GET_HEAD, freq, start + i, name);
        sendData(buffer, 413);
    }
    exist[dataref] = dataRefs.size() - 1;
    return {dataRefs.size() - 1};
}

/**
 * @brief 获取 dataref 最新值
 * @param dataref 标识
 * @param value 返回值
 * @param defaultValue 默认值
 * @return 值可用
 */
inline bool XPlaneUdp::getDataref (const DatarefIndex &dataref, float &value, const float defaultValue) const {
    if (!dataRefs[dataref.idx].available) {
        value = defaultValue;
        return false;
    }
    value = values[dataref.idx];
    return true;
}

/**
 * @brief 修改获取 dataref 的频率
 * @param dataref 标识
 * @param freq 频率
 */
inline void XPlaneUdp::changeDatarefFreq (const DatarefIndex &dataref, const float freq) {
    auto &ref = dataRefs[dataref.idx];
    const int size = ref.end - ref.start + 1;
    if (freq == 0) { // 停止接收
        if (!ref.available)
            return;
        ref.available = false;
        space.set(ref.start, size, false);
    } else {
        // 先恢复
        if (!ref.available) {
            ref.available = true;
            const int start = static_cast<int>(findSpace(size));
            ref.start = start;
            ref.end = start + size - 1;
        }
        // 再发送
        if (!ref.isArray) {
            const size_t bufferSize = packSize(0, DATAREF_GET_HEAD, freq, ref.start, ref.name);
            const auto buffer = pool.getBuffer(bufferSize);
            pack(*buffer, 0, DATAREF_GET_HEAD, freq, ref.start, ref.name);
            sendData(buffer, 413);
        } else {
            for (size_t i = 0; i < size; ++i) {
                const size_t bufferSize = packSize(0, DATAREF_GET_HEAD, freq, ref.start,
                                                   std::format("{}[{}]", ref.name, i));
                const auto buffer = pool.getBuffer(bufferSize);
                pack(*buffer, 0, DATAREF_GET_HEAD, freq, ref.start + i, std::format("{}[{}]", ref.name, i));
                sendData(buffer, 413);
            }
        }
    }
}

/**
 * @brief 设置dataref值
 * @param dataref dataref 名称
 * @param value 值
 * @param index 目标为数组时的索引
 */
inline void XPlaneUdp::setDataref (const std::string &dataref, const float value, int index) {
    const std::string name = (index == -1) ? dataref : std::format("{}[{}]", dataref, index);
    const size_t bufferSize = packSize(0, DATAREF_SET_HEAD, value, name, '\x00');
    const auto buffer = pool.getBuffer(bufferSize);
    pack(*buffer, 0, DATAREF_SET_HEAD, value, name, '\x00');
    sendData(buffer, 509);
}

/**
 * @brief 开始接收基本信息
 * @param freq 接收频率
 */
inline void XPlaneUdp::addPlaneInfo (int freq) {
    infoFreq = freq;
    const std::string sentence = std::format("{}{}\x00", BASIC_INFO_HEAD, freq);
    const size_t bufferSize = packSize(0, sentence);
    const auto buffer = pool.getBuffer(bufferSize);
    pack(*buffer, 0, sentence);
    sendData(buffer, bufferSize);
}

/**
 * @brief 获取基本信息最新值
 */
inline void XPlaneUdp::getPlaneInfo (PlaneInfo &infoDst) const {
    infoDst = info;
}

/**
 * @brief 设置xp状态 触发回调
 * @param newState 新状态
 */
inline void XPlaneUdp::setState (const bool newState) {
    if (newState == state)
        return;
    if (newState && autoReconnect)
        reconnect();
    state = newState;
    if (callback)
        callback(newState);
}

/**
 * @brief 找到一段连续可用的空间
 * @param length 长度
 * @return 初始位置
 */
inline size_t XPlaneUdp::findSpace (const size_t length) {
    size_t start{}, count{};
    for (size_t i = 0; i < space.size(); ++i) {
        if (!space[i]) {
            if (count == 0)
                start = i;
            ++count;
            if (count >= length) {
                space.set(start, start + length, true);
                extendSpace();
                return start;
            }
        } else {
            count = 0;
        }
    }
    // 补充
    for (int i = 0; i < length; ++i)
        space.push_back(true);
    extendSpace();
    return space.size() - length;
}

inline void XPlaneUdp::extendSpace () {
    for (int i = 0; i < (space.size() - values.size()); ++i)
        values.emplace_back();
}

/**
 * @brief 监听XPlane是否在线
 */
inline void XPlaneUdp::detectBeacon () {
    asio::co_spawn(io_context, detect(), asio::detached);
}

inline asio::awaitable<void> XPlaneUdp::detect () {
    ip::udp::endpoint senderEndpoint;
    asio::steady_timer timer(co_await asio::this_coro::executor);
    while (true) {
        auto buffer = pool.getBuffer(0);
        timer.expires_after(std::chrono::seconds(2));
        timer.async_wait([this](const auto &ec) { if (!ec)setState(false); });
        size_t receiveBytes = co_await multicastSocket.async_receive_from(
            asio::buffer(*buffer), senderEndpoint, asio::use_awaitable);
        receiveDataProcess(buffer, receiveBytes, senderEndpoint);
        timer.cancel();
    }
}

/**
 * @brief 向xp发送udp数据
 * @param data 数据
 * @param size
 */
inline void XPlaneUdp::sendData (const std::shared_ptr<std::array<char, 1472>> &data, const size_t size) {
    if (!xpSocket.is_open())
        return;
    asio::co_spawn(io_context, send(data, size), asio::detached);
}

inline asio::awaitable<void> XPlaneUdp::send (const std::shared_ptr<std::array<char, 1472>> data, const size_t size) {
    // 协程 多么好的一件美事啊，sendData执行完代码就退出了，留着send慢慢等待调度发送
    co_await xpSocket.async_send_to(asio::buffer(*data, size), xpEndpoint, asio::use_awaitable);
}

/**
 * @brief 接收数据
 */
inline void XPlaneUdp::receiveData () {
    asio::co_spawn(io_context, receive(), asio::detached);
}

inline asio::awaitable<void> XPlaneUdp::receive () {
    ip::udp::endpoint temp;
    while (xpSocket.is_open()) {
        auto buffer = pool.getBuffer(0);
        size_t receiveBytes = co_await xpSocket.async_receive_from(
            asio::buffer(*buffer), temp, asio::use_awaitable);
        receiveDataProcess(buffer, receiveBytes, temp);
    }
}

inline bool compareHead (const std::string &templateHead, const std::array<char, 1472> &data) {
    return std::ranges::equal(templateHead | std::views::take(4), data | std::views::take(4));
}

inline void XPlaneUdp::receiveDataProcess (const std::shared_ptr<std::array<char, 1472>> &data, const size_t size,
                                           const ip::udp::endpoint &sender) {
    if (size <= HEADER_LENGTH) // 头部大小
        return;
    if (compareHead(DATAREF_GET_HEAD, *data)) { // dataref
        if ((size - 5) % 8 != 0)
            return;
        for (int i = HEADER_LENGTH; i < size; i += 8) {
            int index;
            float value;
            unpack(*data, i, index, value);
            values[index] = value;
        }
    } else if (compareHead(BASIC_INFO_HEAD, *data)) { // 基本信息
        if (((size - 5) % 64 != 0) || (size <= 6))
            return;
        unpack(*data, HEADER_LENGTH, info);
    } else if (compareHead(BECON_HEAD, *data)) { // 信标
        if (!xpSocket.is_open()) { // 第一次听见信标
            uint8_t mainVer, minorVer;
            int32_t software, xpVer;
            uint32_t role;
            uint16_t port;
            unpack(*data, HEADER_LENGTH, mainVer, minorVer, software, xpVer, role, port);
            xpEndpoint = ip::udp::endpoint(ip::make_address(sender.address().to_string()), port);
            const ip::udp::endpoint local(ip::udp::v4(), 0);
            xpSocket.open(local.protocol());
            xpSocket.bind(local);
            receiveData();
        }
        setState(true);
    }
    // 手动擦除数据
    std::memset(data->data(), 0x00, size);
}
}

#endif
