#ifndef CHARTNAVIGATION_EVENTMANAGE_HPP
#define CHARTNAVIGATION_EVENTMANAGE_HPP

#include <variant>
#include <vector>
#include <QMetaType>
#include <QObject>
#include <QTimer>
#include <filesystem>

// 参考ns3这类离散事件模拟器

namespace fs = std::filesystem;

enum class EventType { connectState, simulateData, };

struct Event {
    EventType type;
    qint64 time{};
    std::variant<bool, std::vector<uint8_t>> payload; // simulateData: 压缩后的字节, 使用时再解压
};

Q_DECLARE_METATYPE(Event)

class EventManage : public QObject {
        Q_OBJECT
    public:
        explicit EventManage (fs::path replayDataPath, QObject *parent = nullptr);
        void start ();
        void pause ();
        void resume ();
        void seekToEvent (size_t eventIndex);
        void seekTimeMs (qint64 timeMs);
        void seekPercent (int percent);
        void stepEvents (int delta);
        void stepPercent (int deltaPercent);
        [[nodiscard]] size_t eventCount () const;
        [[nodiscard]] size_t position () const;
        [[nodiscard]] qint64 durationMs () const;

    Q_SIGNALS:
        void eventReady (const Event &event);
        void progressChanged (qint64 timeMs);
        void finished ();
    private:
        std::vector<Event> events;
        QTimer timer;
        size_t currentIndex{0};
        qint64 timeOffset{};
        qint64 baseMs{0};
        qint64 remainingMs{0};
        bool paused{false};
        fs::path replayDataPath;

        void readData ();
        void scheduleNext ();
        void fireCurrent ();
};

#endif //CHARTNAVIGATION_EVENTMANAGE_HPP
