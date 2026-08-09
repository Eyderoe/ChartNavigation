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
    int64_t time{};
    std::variant<bool, std::vector<uint8_t>> payload;
};

Q_DECLARE_METATYPE(Event)

class EventManage : public QObject {
        Q_OBJECT
    public:
        explicit EventManage (fs::path replayDataPath, QObject *parent = nullptr);
        void start ();

    Q_SIGNALS:
        void eventReady (const Event &event);
        void finished ();

    private:
        std::vector<Event> events;
        QTimer timer;
        size_t currentIndex{0};
        int64_t timeOffset{};
        int64_t baseMs{0};
        fs::path replayDataPath;

        void readData ();
        void scheduleNext ();
        void fireCurrent ();
};

#endif //CHARTNAVIGATION_EVENTMANAGE_HPP
