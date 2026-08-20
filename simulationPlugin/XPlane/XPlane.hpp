#ifndef XPBROADCAST_XPLANE_HPP
#define XPBROADCAST_XPLANE_HPP

#include <boost/system.hpp>
#include <boost/asio.hpp>

namespace sys = boost::system;
namespace asio = boost::asio;
namespace ip = asio::ip;

// XPlane Multi-Player Information Udp Broadcast System
class XPMPIUBS {
    public:
        XPMPIUBS ();
        void sendData (const std::shared_ptr<std::string> &data);
        void poll();
        ip::address_v4 getIp();
    private:
        asio::io_context io_context{};
        ip::udp::socket infoSocket{io_context};
        ip::udp::endpoint infoEndpoint;
        asio::executor_work_guard<asio::io_context::executor_type> workGuard;

        asio::awaitable<void> send (std::shared_ptr<std::string> data);
};

#endif //XPBROADCAST_XPLANE_HPP
