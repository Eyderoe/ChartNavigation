#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <cstdio>
#include <thread>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::thread worker([] {
        volatile qint64 t = QDateTime::currentMSecsSinceEpoch();
        (void)t;
    });
    QTimer::singleShot(200, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
