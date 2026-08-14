#include <QCoreApplication>
#include <QTimer>
#include <cstdio>
#include <thread>

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);

    std::thread worker([] {
        // No Qt calls at all.
    });

    QTimer::singleShot(150, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
