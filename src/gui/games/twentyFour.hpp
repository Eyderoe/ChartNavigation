#ifndef CHARTNAVIGATION_GAMES_TWENTYFOUR_HPP
#define CHARTNAVIGATION_GAMES_TWENTYFOUR_HPP

#include "game.hpp"
#include <array>

class TwentyFourGame : public Game {
    public:
        explicit TwentyFourGame (Ui::note_widget *ui, QObject *parent);
    private:
        std::array<int, 4> twentyFourCards{};
        QString twentyFourAnswer;
        void initTwentyFour ();
        void dealTwentyFour ();
        void checkTwentyFour () const;
};

#endif // CHARTNAVIGATION_GAMES_TWENTYFOUR_HPP
