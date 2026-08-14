#include <QCoreApplication>
#include <QTimer>
#include <QByteArrayView>
#include <QtProtobuf/qprotobufserializer.h>
#include <cstdio>
#include <thread>

#include "plane.qpb.h"

/**
 * 复刻 wlanUdp worker 线程里的 QtProtobuf 调用：
 *  - QProtobufSerializer + Planes::deserialize（脏数据也足以走完整解析路径）
 *  - 不含 qDebug（wlan 里那行本来就是注释掉的；回调里的 qDebug 已被
 *    QT_NO_DEBUG_OUTPUT 实验覆盖）
 *
 * 用法：编译时同时编 plane.qpb.cpp / plane_qtprotoreg.cpp，
 * 并把 ChartNavigation_autogen/include 加进 include 路径（内含 moc_plane.qpb.cpp）。
 */
int main (int argc, char **argv) {
    QCoreApplication app(argc, argv);

    std::thread worker([] {
        QProtobufSerializer serializer;
        Planes planes;
        const QByteArrayView garbage("\x40\x79\x54\x20\x01\x02\x03", 7);
        planes.deserialize(&serializer, garbage);
    });

    QTimer::singleShot(200, &app, &QCoreApplication::quit);
    worker.join();
    std::printf("worker joined\n");
    const int rc = app.exec();
    std::printf("clean exit rc=%d\n", rc);
    return rc;
}
