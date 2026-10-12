#ifndef CHARTNAVIGATION_GAMES_MINES_HPP
#define CHARTNAVIGATION_GAMES_MINES_HPP

#include "game.hpp"
#include "minesBackend.h"
#include <QByteArray>
#include <memory>
#include <vector>

class MinesView;
class MinesGame : public Game {
    public:
        explicit MinesGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        using State = std::shared_ptr<MinesBackend>;
        MinesView *view{};
        MinesConfig config{};
        QByteArray seed;
        State state, initial;
        std::vector<State> history, future;
        int cursor{}, pressedCell{-1}, deaths{};
        unsigned generation{};
        bool busy{};
        static State take (MinesBackend *value);
        void selectPreset ();
        void applyCustom ();
        void fillCustom ();
        void reset ();
        void retry ();
        void action (int cell, bool flag);
        void adopt (State next);
        void undo ();
        void redo ();
        void refresh ();
};

#endif // CHARTNAVIGATION_GAMES_MINES_HPP
