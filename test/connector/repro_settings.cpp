#include <QCoreApplication>
#include <QSettings>
#include <QTimer>
#include <cstdio>
#include <thread>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);

    std::thread worker([] {
        QSettings settings;
        settings.setValue("probe", 1);
        settings.sync();
    });

    QTimer::singleShot(150, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
