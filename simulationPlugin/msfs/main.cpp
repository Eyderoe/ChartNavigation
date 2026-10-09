#include "SimConnectClient.hpp"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QTextStream>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("msfs_monitor");
    QCommandLineParser parser;
    parser.setApplicationDescription("ChartNavigation native SimConnect diagnostic client (no network bridge)");
    parser.addHelpOption();
    parser.addOption({"dll", "Absolute path to the native x64 SimConnect.dll", "path"});
    parser.addOption({"seconds", "Exit after this many seconds (0 = keep running)", "seconds", "0"});
    parser.process(app);
    bool valid{};
    const int seconds = parser.value("seconds").toInt(&valid);
    if (!valid || seconds < 0 || seconds > 86400) {
        QTextStream(stderr) << "--seconds must be 0..86400\n";
        return 2;
    }
    msfs::SimConnectClient client(parser.value("dll"));
    client.setCallback([](bool state) { QTextStream(stdout) << (state ? "ONLINE\n" : "OFFLINE\n"); });
    QTimer output;
    output.setInterval(1000);
    bool sawFrame{};
    QString printedError;
    QObject::connect(&output, &QTimer::timeout, &app, [&] {
        const auto planes = client.snapshot();
        if (planes.empty()) {
            if (!client.errorString().isEmpty() && printedError != client.errorString()) {
                printedError = client.errorString();
                QTextStream(stdout) << "ERROR: " << printedError << '\n';
            }
            return;
        }
        sawFrame = true;
        QTextStream stream(stdout);
        stream << "planes=" << planes.size() << '\n';
        for (const auto &plane : planes)
            stream << "  object=" << plane.object << " " << QString::fromStdString(plane.flight)
                   << " lat=" << plane.latitude << " lon=" << plane.longitude
                   << " alt_m=" << plane.altitude << " heading_true=" << plane.heading
                   << " vs_fpm=" << plane.verticalSpeed
                   << " atc_model=" << QString::fromStdString(plane.atcModel) << '\n';
    });
    client.start();
    output.start();
    if (seconds)
        QTimer::singleShot(seconds * 1000, &app, [&] { app.exit(sawFrame ? 0 : 1); });
    return app.exec();
}
