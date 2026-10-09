#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include "connector/msfs.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QLibrary>
#include <QFileInfo>
#include <QThread>
#include <array>

namespace {
QLibrary mock;
using Reset = void (__stdcall *)();
using SetMode = void (__stdcall *)(int);
using Count = int (__stdcall *)();
Reset reset;
SetMode setMode;
Count openCount, closeCount;

void pump (int milliseconds) {
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(5);
    }
}
}

TEST_CASE("Native ABI subscription, own slot, nearest 63 aircraft and units") {
    reset();
    msfsAdapter adapter;
    std::vector<bool> states;
    adapter.setCallback([&](bool state) { states.push_back(state); });
    pump(150);
    std::array<float, 64> values;
    CHECK(adapter.getDataref(adapter.addDatarefArray("id", 1), values, -1));
    CHECK(values[0] == 1);
    CHECK(values[63] == 64);
    CHECK(adapter.getType() == SimulatorSource::msfs);
    CHECK(adapter.getName() == "msfs");
    adapter.getDataref(adapter.addDatarefArray("lat", 1), values, -1);
    CHECK(values[0] == doctest::Approx(31.0));
    CHECK(values[1] < values[63]);
    adapter.getDataref(adapter.addDatarefArray("alt", 1), values, -1);
    CHECK(values[0] == doctest::Approx(3048.25));
    adapter.getDataref(adapter.addDatarefArray("trk", 1), values, -1);
    CHECK(values[0] == 350);
    adapter.getDataref(adapter.addDatarefArray("vs", 1), values, -1);
    CHECK(values[0] == doctest::Approx(1234.5));
    CHECK_THROWS_AS(adapter.addDatarefArray("unknown", 1), std::invalid_argument);
    CHECK_FALSE(adapter.getDataref({999}, values, -1));
    CHECK(values[0] == -1);
    CHECK(states == std::vector<bool>{true});
    adapter.close();
    CHECK(closeCount() == 1);
    CHECK(states.back() == false);
}

TEST_CASE("Text arrays are bounded and zero padded; smaller snapshots remove old aircraft") {
    reset();
    msfsAdapter adapter;
    pump(100);
    std::array<float, 10> text{};
    REQUIRE(adapter.getDataref(adapter.addDatarefArray("flightId", 1), text, -1));
    CHECK(text[0] == 'C');
    CHECK(text[7] == '5'); // 固定八字节 CES12345。
    CHECK(text[8] == 'C');
    std::array<float, 3> partial{};
    CHECK(adapter.getDataref(adapter.addDatarefArray("icao", 1), partial, -1));
    CHECK(partial[0] == 'A');
    CHECK(partial[2] == '2');
    std::span<float> empty;
    CHECK(adapter.getDataref(adapter.addDatarefArray("lat", 1), empty, -1));
    setMode(3);
    pump(1200);
    std::array<float, 64> ids{};
    adapter.getDataref(adapter.addDatarefArray("id", 1), ids, -1);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == -1);
    adapter.getDataref(adapter.addDatarefArray("icao", 1), text, -1);
    CHECK(text[4] == 0);
    CHECK(text[8] == 0);
    adapter.close();
}

TEST_CASE("Missing packets expire, valid data restores state, close is terminal until start") {
    reset();
    msfs::SimConnectClient client;
    std::vector<bool> states;
    client.setCallback([&](bool state) { states.push_back(state); });
    client.start();
    pump(100);
    REQUIRE(client.isConnected());
    setMode(1);
    pump(3200);
    CHECK_FALSE(client.isConnected());
    CHECK(client.snapshot().empty());
    setMode(0);
    pump(3200);
    CHECK(client.isConnected());
    client.close();
    const int calls = openCount();
    pump(150);
    CHECK(openCount() == calls);
    CHECK_FALSE(client.isConnected());
    CHECK(states == std::vector<bool>{false, true, false, true, false});
}

TEST_CASE("Simulator quit closes handle then reconnects; invalid payload never connects") {
    reset();
    msfs::SimConnectClient client;
    client.start();
    pump(100);
    setMode(2);
    pump(100);
    CHECK_FALSE(client.isConnected());
    CHECK(closeCount() == 1);
    setMode(0);
    pump(3200);
    CHECK(client.isConnected());
    CHECK(openCount() >= 2);
    client.close();
    reset();
    setMode(4);
    client.start();
    pump(150);
    CHECK_FALSE(client.isConnected());
    CHECK(client.snapshot().empty());
    client.close();
}

TEST_CASE("SimConnect exceptions clear data and identify failed request") {
    reset();
    msfs::SimConnectClient client;
    client.start();
    pump(100);
    setMode(5);
    pump(100);
    CHECK_FALSE(client.isConnected());
    CHECK(client.errorString().contains("exception 19"));
    CHECK(client.snapshot().empty());
    client.close();
}

TEST_CASE("ATC localization keys are never exported as ICAO codes") {
    reset();
    setMode(6);
    msfsAdapter adapter;
    pump(100);
    std::array<float, 8> text;
    REQUIRE(adapter.getDataref(adapter.addDatarefArray("icao", 1), text, -1));
    CHECK(std::ranges::all_of(text, [](float value) { return value == 0; }));
    adapter.close();
}

TEST_CASE("Truncated messages, incorrect definition counts and missing DLL stay offline") {
    for (int mode : {7, 8}) {
        reset();
        setMode(mode);
        msfs::SimConnectClient client;
        client.start();
        pump(100);
        CHECK_FALSE(client.isConnected());
        CHECK(client.snapshot().empty());
        client.close();
    }
    msfs::SimConnectClient missing("Z:/ChartNavigation-not-present/SimConnect.dll");
    missing.start();
    CHECK_FALSE(missing.isConnected());
    CHECK(missing.errorString().contains("Cannot load"));
    missing.close();
}

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2)
        return 2;
    qputenv("CHARTNAVIGATION_SIMCONNECT_DLL", QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath().toLocal8Bit());
    mock.setFileName(QString::fromLocal8Bit(argv[1]));
    if (!mock.load())
        return 3;
    reset = reinterpret_cast<Reset>(mock.resolve("MockReset"));
    setMode = reinterpret_cast<SetMode>(mock.resolve("MockSetMode"));
    openCount = reinterpret_cast<Count>(mock.resolve("MockOpenCount"));
    closeCount = reinterpret_cast<Count>(mock.resolve("MockCloseCount"));
    if (!reset || !setMode || !openCount || !closeCount)
        return 4;
    doctest::Context context;
    return context.run();
}
