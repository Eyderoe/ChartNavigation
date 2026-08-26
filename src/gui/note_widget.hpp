#ifndef CHARTNAVIGATION_NOTE_WIDGET_HPP
#define CHARTNAVIGATION_NOTE_WIDGET_HPP

#include <QWidget>
#include "ui/theme.hpp"


QT_BEGIN_NAMESPACE

class QGraphicsScene;
class QEvent;
class QShowEvent;

namespace Ui
{
class note_widget;
}

QT_END_NAMESPACE

class note_widget : public QWidget {
        Q_OBJECT
    public:
        explicit note_widget (QWidget *parent = nullptr);
        ~note_widget () override;
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        Ui::note_widget *ui;
        void initConnect ();
        // 右侧预设
        QGraphicsScene *flightLevelScene{};
        void fitChinaFlightLevelToWidth () const;
        void initGraphic ();

        void initConversation () const;

        [[nodiscard]] std::tuple<QComboBox*, QLineEdit*, QComboBox*, QLineEdit*> getUnit (const QString &name) const;
        void saveUnit (bool save = true) const;
        void initUnit () const;

        void initAlfaTable () const;
    private slots:
        void on_comboBox_activated (int index) const;
        void unitConvertChange () const;
};


#endif //CHARTNAVIGATION_NOTE_WIDGET_HPP
