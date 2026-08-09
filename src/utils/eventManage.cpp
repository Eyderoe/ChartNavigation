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

size_t EventManage::eventCount () const {
    return events.size();
}

size_t EventManage::position () const {
    return currentIndex;
}

qint64 EventManage::durationMs () const {
    return events.empty() ? 0 : events.back().time + timeOffset;
}

void EventManage::seekToEvent (const size_t eventIndex) {
    if (events.empty())
        return;
    currentIndex = std::min(eventIndex, events.size() - 1);
    // 以目标事件为锚点: 下一个事件按 (其时间 - 当前事件时间) 的间隔继续
    baseMs = QDateTime::currentMSecsSinceEpoch() - (events[currentIndex].time + timeOffset);
    emit eventReady(events[currentIndex]);
    emit progressChanged(events[currentIndex].time + timeOffset);
    ++currentIndex;
    scheduleNext();
}

void EventManage::seekTimeMs (const qint64 timeMs) {
    if (events.empty())
        return;
    // 定位到第一个有效时间 >= timeMs 的事件
    const auto it = std::lower_bound(events.begin(), events.end(), timeMs - timeOffset,
                                     [](const Event &event, const qint64 t) { return event.time < t; });
    const size_t target = (it == events.end()) ? events.size() - 1 : static_cast<size_t>(it - events.begin());
    seekToEvent(target);
}

void EventManage::seekPercent (const int percent) {
    if (events.empty())
        return;
    seekTimeMs(durationMs() * std::clamp(percent, 0, 100) / 100);
}

void EventManage::stepEvents (const int delta) {
    if (events.empty())
        return;
    const long long target = static_cast<long long>(currentIndex) + delta;
    seekToEvent(static_cast<size_t>(std::clamp(target, 0LL, static_cast<long long>(events.size()) - 1)));
}

void EventManage::stepPercent (const int deltaPercent) {
    if (events.empty())
        return;
    int delta = static_cast<int>(events.size()) * deltaPercent / 100;
    if (delta == 0)
        delta = deltaPercent > 0 ? 1 : -1; // 事件太少时至少走一步
    stepEvents(delta);
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
    emit progressChanged(events[currentIndex].time + timeOffset);
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
            // 保存压缩后的数据, 使用时再解压
            events.push_back({EventType::simulateData, time,
                              std::vector<uint8_t>(block.begin(), block.end())});
        } else {
            qWarning() << "EventManage: unknown record type:" << fields[1];
        }
    }
    qDebug() << "EventManage: loaded" << events.size() << "events";
}
