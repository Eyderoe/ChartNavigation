#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTimer>
#include <cstdio>
#include <thread>

/**
 * 复刻 DataProvider 回调里 debugStoreData 分支的 Qt 调用：
 * QDateTime::currentMSecsSinceEpoch + QFile::write，不含 qDebug。
 */
int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);

    std::thread worker([] {
        const qint64 t = QDateTime::currentMSecsSinceEpoch();
        QFile f("repro_filedate.bin");
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QByteArray::number(t));
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
