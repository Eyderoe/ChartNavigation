#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <cstdio>
#include <thread>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);

    std::thread worker([] {
        qDebug() << "worker: qDebug from foreign thread";
    });

    QTimer::singleShot(150, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
