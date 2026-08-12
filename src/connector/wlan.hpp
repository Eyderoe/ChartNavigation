#ifndef CHARTNAVIGATION_WLAN_HPP
#define CHARTNAVIGATION_WLAN_HPP

#include <boost/system.hpp>
#include <boost/asio.hpp>
#include <boost/dynamic_bitset.hpp>
#include "interface.hpp"
#include "plane.qpb.h"


namespace sys = boost::system;
namespace asio = boost::asio;
namespace ip = asio::ip;


class wlanUdp {
    public:
        wlanUdp ();
        ~wlanUdp ();
        void close() const;
        void setCallback (const std::function<void  (bool)> &callbackFunc);
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue);
    private:
        // 数据
        int available{};
        std::array<Plane, 64> planes;
        // 网络
        asio::io_context io_context{}; // 上下文
        asio::executor_work_guard<asio::io_context::executor_type> workGuard;
        ip::udp::socket multicastSocket{io_context}; // 监听多播
        std::thread worker; // io_content驱动
        // 回调
        void setState (bool newState);
        bool state{false};
        std::function<void  (bool)> callback{nullptr}; // 回调
        // 信息处理
        void detectBeacon ();
        asio::awaitable<void> detect ();
        void receiveDataProcess (const std::shared_ptr<std::array<char, 1472>> &data, size_t size,
                                 const ip::udp::endpoint &sender);
};

class wlanAdapter : public InterfaceSimu {
    public:
        wlanAdapter ();
        void setCallback (const std::function<void  (bool)> &callbackFunc) override;
        void close () override;
        DatarefIdx addDatarefArray (const std::string &dataref, int32_t freq) override;
        bool getDataref (const DatarefIdx &dataref, std::span<float> container, float defaultValue) override;
        std::string getName () const override;
        SimulatorSource getType () const override;
    private:
        wlanUdp wlan;
        std::map<std::string, int> datarefMap;
};

#endif //CHARTNAVIGATION_WLAN_HPP
