#include "eventManage.hpp"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <algorithm>
#include <limits>
#include <utility>


EventManage::EventManage (fs::path replayDataPath, QObject *parent)
    : QObject(parent), replayDataPath(std::move(replayDataPath)) {
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &EventManage::fireCurrent);
    readData();
}

void EventManage::start () {
    if (events.empty()) {
        emit finished();
        return;
    }
    currentIndex = 0;
    baseMs = QDateTime::currentMSecsSinceEpoch();
    scheduleNext();
}

void EventManage::scheduleNext () {
    if (currentIndex >= events.size()) {
        emit finished();
        return;
    }
    // 目标时刻 = 回放开始时刻 + 事件相对时间 + 偏移, 迟到了就立即补发
    const int64_t target = baseMs + events[currentIndex].time + timeOffset;
    const int64_t delay = std::clamp(target - QDateTime::currentMSecsSinceEpoch(),
                                     int64_t{0}, static_cast<int64_t>(std::numeric_limits<int>::max()));
    timer.start(static_cast<int>(delay));
}

void EventManage::fireCurrent () {
    emit eventReady(events[currentIndex]);
    ++currentIndex;
    scheduleNext();
}

void EventManage::readData () {
    QFile file(QString::fromStdString(replayDataPath.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "EventManage: cannot open replay file:" << QString::fromStdString(replayDataPath.string());
        return;
    }

    // 文件头: offsetTimeMs,{偏移}, 下一行是 freq
    const auto offsetFields = file.readLine().trimmed().split(',');
    if (offsetFields.size() < 2 || offsetFields[0] != "offsetTimeMs") {
        qWarning() << "EventManage: bad file header";
        return;
    }
    timeOffset = offsetFields[1].toLongLong();
    file.readLine(); // freq 数据块, 回放时用不到

    while (!file.atEnd()) {
        const auto fields = file.readLine().trimmed().split(',');
        if (fields.size() != 3) {
            qWarning() << "EventManage: bad record line";
            break;
        }
        const int64_t time = fields[0].toLongLong();
        const QByteArray block = file.read(fields[2].toInt());

        if (fields[1] == "connectState") {
            events.push_back({EventType::connectState, time, block.startsWith('t')});
        } else if (fields[1] == "data") {
            const QByteArray uncompressed = qUncompress(block);
            if (uncompressed.isEmpty()) {
                qWarning() << "EventManage: qUncompress failed at time:" << time;
                continue;
            }
            events.push_back({EventType::simulateData, time,
                              std::vector<uint8_t>(uncompressed.begin(), uncompressed.end())});
        } else {
            qWarning() << "EventManage: unknown record type:" << fields[1];
        }
    }
    qDebug() << "EventManage: loaded" << events.size() << "events";
}
