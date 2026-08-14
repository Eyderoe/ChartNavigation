#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <cstdio>
#include <thread>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);
    std::thread worker([] {
        QFile f("repro_qfile.bin");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QByteArray("x"));
            f.close();
        }
    });
    QTimer::singleShot(200, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
