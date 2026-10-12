#ifndef CHARTNAVIGATION_GAMES_CUBE_HPP
#define CHARTNAVIGATION_GAMES_CUBE_HPP

#include "game.hpp"
#include <vector>

struct CubeState {
    int solid{};
    int d1{};
    int d2{};
    std::vector<int> board;
    std::vector<int> faces;
    int position{};
    int moves{};
    int completedMoves{};
    bool operator== (const CubeState &) const = default;
};

class CubeView;
class CubeGame : public Game {
    public:
        explicit CubeGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        CubeView *view{};
        CubeState state, initial;
        std::vector<CubeState> history;
        void reset ();
        void move (int direction);
        void refresh ();
};

#endif // CHARTNAVIGATION_GAMES_CUBE_HPP
