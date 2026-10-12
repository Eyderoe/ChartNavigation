#ifndef CHARTNAVIGATION_GAMES_GAME_HPP
#define CHARTNAVIGATION_GAMES_GAME_HPP

#include <QCoreApplication>
#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QToolButton;
namespace Ui {
class note_widget;
}
QT_END_NAMESPACE

// Each controller is owned by Games; Designer widgets remain owned by the UI.
class Game : public QObject {
    protected:
        explicit Game (Ui::note_widget *ui, QObject *parent) : QObject(parent), ui(ui) {}
        Ui::note_widget *ui;

        // Preserve the translation context used before the games were split.
        static QString tr (const char *text, const char *comment = nullptr, int count = -1) {
            return QCoreApplication::translate("Games", text, comment, count);
        }
};

#endif // CHARTNAVIGATION_GAMES_GAME_HPP
