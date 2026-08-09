#ifndef CHARTNAVIGATION_REPLAY_CONTROL_HPP
#define CHARTNAVIGATION_REPLAY_CONTROL_HPP

#include <QWidget>


QT_BEGIN_NAMESPACE

namespace Ui
{
class replay_control;
}

QT_END_NAMESPACE

class replay_control : public QWidget {
    Q_OBJECT
    public:
        explicit replay_control (QWidget *parent = nullptr);
        ~replay_control () override;
        void setDuration (qint64 durationMs);
        void setPosition (qint64 timeMs);

    Q_SIGNALS:
        void stepEventsRequested (int delta);
        void stepPercentRequested (int deltaPercent);
        void seekPercentRequested (int percent);

    private:
        Ui::replay_control *ui;
        qint64 duration{0};
        bool updating{false};

        void initConnect ();
};


#endif //CHARTNAVIGATION_REPLAY_CONTROL_HPP
