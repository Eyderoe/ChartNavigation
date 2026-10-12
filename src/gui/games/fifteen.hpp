#ifndef CHARTNAVIGATION_GAMES_FIFTEEN_HPP
#define CHARTNAVIGATION_GAMES_FIFTEEN_HPP

#include "game.hpp"
#include "fifteenPuzzle.hpp"

class FifteenView;
class FifteenGame : public Game {
    public:
        explicit FifteenGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        FifteenView *view{};
        FifteenPuzzle::State state, initial;
        std::vector<FifteenPuzzle::State> history, future;
        void reset ();
        void move (int cell);
        void hint ();
        void undo ();
        void redo ();
        void refresh ();
};

#endif // CHARTNAVIGATION_GAMES_FIFTEEN_HPP
