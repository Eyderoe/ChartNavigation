#ifndef CHARTNAVIGATION_GAMES_PYRAMID_HPP
#define CHARTNAVIGATION_GAMES_PYRAMID_HPP

#include "game.hpp"
#include <array>

class PyramidGame : public Game {
    public:
        explicit PyramidGame (Ui::note_widget *ui, QObject *parent);
    private:
        std::array<int, 15> pyramidNumbers{};
        std::array<QLineEdit*, 15> pyramidCells{};
        void initPyramid ();
        void resetPyramid ();
        void checkPyramid () const;
};

#endif // CHARTNAVIGATION_GAMES_PYRAMID_HPP
