#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <cmath>
#include <numbers>

#include "../XPlane.hpp"
#include "../cmake-build-debug/plane.pb.h"

Plane makePlane (const int32_t id, const float lat, const float lon, const int32_t alt, const int32_t trk,
                 const int32_t vs, const std::string &flight, const std::string &icao) {
    Plane plane;
    plane.set_id(id);
    plane.set_lat(lat);
    plane.set_lon(lon);
    plane.set_alt(alt);
    plane.set_trk(trk);
    plane.set_vs(vs);
    plane.set_flight(flight);
    plane.set_icao(icao);
    return plane;
}

inline bool randomBool () {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 3);
    return dis(gen) % 2 == 0;
}

constexpr double ft2m = 0.3048;
constexpr std::string magicHead = {0x40, 0x79, 0x54, 0x20};

int main () {
    XPMPIUBS xp{};
    std::vector<Plane> aircrafts;
    aircrafts.push_back(makePlane(0, 29.9486, 106.7211, 10000 * ft2m, 0, 0, "ID0", "A20N"));
    aircrafts.push_back(makePlane(1, 29.8683, 106.7527, 15000 * ft2m, 90, 1500, "ID1", "B738"));
    aircrafts.push_back(makePlane(2, 30.0391, 106.8773, 5000 * ft2m, 180, -1500, "ID2", "C172"));
    aircrafts.push_back(makePlane(3, 30.2008, 106.8126, 10000 * ft2m, 270, 1500, "ID3", "CONC"));
    aircrafts.push_back(makePlane(4, 29.7234, 106.6561, 13500 * ft2m, 30, 1500, "ID4", "H20"));
    aircrafts.push_back(makePlane(5, 29.985, 106.76, 12000 * ft2m, 90, 0, "ID5", "B788"));
    while (true) {
        Planes planes_msg;

        // 动态
        for (auto &p : aircrafts | std::views::take(4)) {
            p.set_lat(p.lat() + (randomBool() ? 0.0015f : -0.0015f));
            p.set_lon(p.lon() + (randomBool() ? 0.0015f : -0.0015f));
            p.set_trk(p.trk() + (randomBool() ? 10 : -5));
            planes_msg.add_planes()->CopyFrom(p);
        }

        Plane &vertical = aircrafts[4];
        vertical.set_lat((vertical.lat() <= 30.5) ? vertical.lat() + 0.002 : 29.7);
        planes_msg.add_planes()->CopyFrom(vertical);

        Plane &circular = aircrafts[5];
        static constexpr double cLat = (29.72 + 30.03) / 2.0;   // 29.875
        static constexpr double cLon = (106.65 + 106.87) / 2.0; // 106.76
        static constexpr double cRad = std::min(30.03 - cLat, 106.87 - cLon); // 0.11
        static double theta = 0.0;
        constexpr double dTheta = 0.05;
        theta = std::fmod(theta + dTheta, 2.0 * std::numbers::pi);
        circular.set_lat(static_cast<float>(cLat + cRad * std::cos(theta)));
        circular.set_lon(static_cast<float>(cLon + cRad * std::sin(theta)));
        double trkDeg = std::fmod(
            std::atan2(std::cos(theta), -std::sin(theta)) * 180.0 / std::numbers::pi + 360.0,
            360.0);
        circular.set_trk(static_cast<int32_t>(std::round(trkDeg)));
        planes_msg.add_planes()->CopyFrom(circular);

        std::string data = magicHead;
        data += static_cast<char>(0xff & aircrafts.size()); // 动态获取飞机数量
        planes_msg.AppendToString(&data);
        xp.sendData(std::make_shared<std::string>(std::move(data)));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        xp.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
    }
    return 0;
}
