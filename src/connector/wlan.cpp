#include "wlan.hpp"
#include <ranges>
#include <QtProtobuf/qprotobufserializer.h>
#include "utils/stringProcess.hpp"


wlanUdp::wlanUdp () : workGuard(asio::make_work_guard(io_context))
                      , worker([this] () { io_context.run(); }) {
    // 绑定组播
    multicastSocket.open(ip::udp::v4());
    multicastSocket.set_option(asio::socket_base::reuse_address(true));
#ifndef _WIN32 // if constexpr 还是会检查未经过的分支宏定义
    multicastSocket.set_option(asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT>(true));
#endif
    multicastSocket.bind(ip::udp::endpoint(ip::address_v4::any(), 57316));
    multicastSocket.set_option(ip::multicast::join_group(ip::make_address("239.255.73.16")));
    // 载波监听
    detectBeacon();
}

wlanUdp::~wlanUdp () {
    multicastSocket.cancel();
    multicastSocket.close();
    workGuard.reset();
    io_context.stop();
    if (worker.joinable())
        worker.join();
}

void wlanUdp::setCallback (const std::function<void  (bool)> &callbackFunc) {
    callback = callbackFunc;
}

bool wlanUdp::getDataref (const DatarefIdx &dataref, std::span<float> container, const float defaultValue) {
    if (!available) {
        std::ranges::fill(container, defaultValue);
        return false;
    }

    const size_t count = std::min(container.size(), static_cast<size_t>(available));
    auto copy2array = [&](auto &&view) {
        auto taken = view | std::views::take(count);
        auto [in, out] = std::ranges::copy(taken, container.begin());
        std::ranges::fill(out, container.end(), defaultValue);
    };
    switch (dataref.idx) {
        case 1:
            // 因为pdfView判断是通过id!=0,要不就后面设计为GetN
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.id_proto() + 1); }));
            break;
        case 2:
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.lat()); }));
            break;
        case 3:
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.lon()); }));
            break;
        case 4:
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.alt()); }));
            break;
        case 5:
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.trk()); }));
            break;
        case 6:
            copy2array(planes | std::views::transform([](auto &p) { return static_cast<float>(p.vs()); }));
            break;
        case 7:
            std::ranges::fill(container, 0);
            for (int i = 0; i < available; ++i)
                std::ranges::copy(planes[i].flight().toStdString(), container.begin() + i * 8);
            break;
        case 8:
            std::ranges::fill(container, 0);
            for (int i = 0; i < available; ++i)
                std::ranges::copy(planes[i].icao().toStdString(), container.begin() + i * 8);
            break;
        default:
            // 永远达不到的真实
            std::ranges::fill(container, defaultValue);
    }
    return true;
}

void wlanUdp::setState (const bool newState) {
    if (newState == state)
        return;
    if (!newState)
        available = 0;
    state = newState;
    if (callback)
        callback(newState);
}

void wlanUdp::detectBeacon () {
    asio::co_spawn(io_context, detect(), asio::detached);
}

asio::awaitable<void> wlanUdp::detect () {
    ip::udp::endpoint senderEndpoint;
    asio::steady_timer timer(co_await asio::this_coro::executor);
    while (true) {
        auto buffer = std::make_shared<std::array<char, 1472>>();
        timer.expires_after(std::chrono::seconds(3));
        timer.async_wait([this](const auto &ec) { if (!ec)setState(false); });
        const size_t receiveBytes = co_await multicastSocket.async_receive_from(
            asio::buffer(*buffer), senderEndpoint, asio::use_awaitable);
        receiveDataProcess(buffer, receiveBytes, senderEndpoint);
        timer.cancel();
    }
}

void wlanUdp::receiveDataProcess (const std::shared_ptr<std::array<char, 1472>> &data, const size_t size,
                                  const ip::udp::endpoint &sender) {
    static constexpr std::string magicHead{0x40, 0x79, 0x54, 0x20};
    if (size <= 5)
        return;

    if (std::ranges::equal(magicHead, *data | std::views::take(4))) {
        // qDebug() << toHex(data->data(), size);
        setState(true);
        available = static_cast<int>(static_cast<unsigned int>((*data)[4])); // 难绷 之前取的 [5]
        Planes planes_;
        QProtobufSerializer serializer;
        if (!planes_.deserialize(&serializer, QByteArrayView(data->data() + 5, static_cast<qsizetype>(size - 5))))
            return;
        for (const auto &plane : planes_.planes())
            planes[static_cast<size_t>(plane.id_proto())] = plane; // id确实是从0开始
    }
}

wlanAdapter::wlanAdapter () {
    datarefMap["id"] = 1;
    datarefMap["lat"] = 2;
    datarefMap["lon"] = 3;
    datarefMap["alt"] = 4;
    datarefMap["trk"] = 5;
    datarefMap["vs"] = 6;
    datarefMap["flightId"] = 7;
    datarefMap["icao"] = 8;
}

void wlanAdapter::setCallback (const std::function<void  (bool)> &callbackFunc) {
    wlan.setCallback(callbackFunc);
}

void wlanAdapter::close () {}

DatarefIdx wlanAdapter::addDatarefArray (const std::string &dataref, int32_t freq) {
    const auto it = datarefMap.find(dataref);
    if (it == datarefMap.end())
        throw std::invalid_argument("dataref not found");
    return {static_cast<size_t>(it->second)};
}

bool wlanAdapter::getDataref (const DatarefIdx &dataref, const std::span<float> container, float defaultValue) {
    return wlan.getDataref(dataref, container, defaultValue);
}
