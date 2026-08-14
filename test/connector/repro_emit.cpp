#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <cstdio>
#include <thread>

/**
 * 复刻 DataProvider 回调在 worker 线程里的行为：
 *  - Debug 里 qDebug() 是活代码；Release 里被 QT_NO_DEBUG_OUTPUT 裁掉
 *  - SettingsManager::set(TempKey) 在 Windows 上 = 内存缓存 + emit（磁盘写在
 *    if constexpr (platform == androidOS) 里，Windows 上整个被裁掉）
 *
 * 构建宏：
 *  STRIP_QDEBUG  编译期去掉 qDebug 行（模拟 Release 的 QT_NO_DEBUG_OUTPUT 效果）
 */
class SignalBox : public QObject {
        Q_OBJECT
    public:
        void fire (const int key, const QVariant &value) { emit sig(key, value); }
    signals:
        void sig (int key, const QVariant &value);
};

class Sink : public QObject {
        Q_OBJECT
    public slots:
        void onSig (int, const QVariant &) {}
};

int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);

    SignalBox box;
    Sink sink;
    QObject::connect(&box, &SignalBox::sig, &sink, &Sink::onSig, Qt::QueuedConnection);

    std::thread worker([&box] {
#ifndef STRIP_QDEBUG
        qDebug() << "Simu-connect change state: true";
#endif
        box.fire(0, true); // 对应 SettingsManager::set(TempKey) 的 emit
    });

    QTimer::singleShot(200, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}

#include "repro_emit.moc"
