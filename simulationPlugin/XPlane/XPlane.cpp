#include "XPlane.hpp"

XPMPIUBS::XPMPIUBS () : workGuard(asio::make_work_guard(io_context)) {
    infoSocket = ip::udp::socket(io_context, ip::udp::v4());
    infoSocket.set_option(ip::multicast::outbound_interface(getIp()));
    infoSocket.set_option(ip::multicast::hops(3));
    infoEndpoint = ip::udp::endpoint(ip::make_address("239.255.73.16"), 57316);
}

void XPMPIUBS::sendData (const std::shared_ptr<std::string> &data) {
    asio::co_spawn(io_context, send(data), asio::detached);
}

void XPMPIUBS::poll () {
    io_context.poll();
}

asio::awaitable<void> XPMPIUBS::send (const std::shared_ptr<std::string> data) {
    co_await infoSocket.async_send_to(asio::buffer(*data), infoEndpoint, asio::use_awaitable);
}

ip::address_v4 XPMPIUBS::getIp () {
    try {
        ip::udp::socket temp(io_context, ip::udp::v4());
        temp.connect(ip::udp::endpoint(ip::make_address("8.8.8.8"), 80));
        return temp.local_endpoint().address().to_v4();
    } catch (...) {
        return {};
    }
}
