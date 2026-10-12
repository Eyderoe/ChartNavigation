#ifndef CHARTNAVIGATION_GAMES_HPP
#define CHARTNAVIGATION_GAMES_HPP

#include <QObject>

QT_BEGIN_NAMESPACE
namespace Ui {
class note_widget;
}
QT_END_NAMESPACE

// Owns game controllers, which are destroyed before the Designer UI.
class Games : public QObject {
        Q_OBJECT
    public:
        explicit Games (Ui::note_widget *ui, QObject *parent);
    private:
        Ui::note_widget *ui;
};

#endif // CHARTNAVIGATION_GAMES_HPP
