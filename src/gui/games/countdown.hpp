#ifndef CHARTNAVIGATION_GAMES_COUNTDOWN_HPP
#define CHARTNAVIGATION_GAMES_COUNTDOWN_HPP

#include "game.hpp"
#include <array>

class CountdownGame : public Game {
    public:
        explicit CountdownGame (Ui::note_widget *ui, QObject *parent);
    private:
        std::array<int, 6> countdownNumbers{};
        int countdownTarget{};
        QString countdownAnswer;
        qint64 countdownBestValue{};
        void initCountdown ();
        void resetCountdown ();
        void checkCountdown () const;
};

#endif // CHARTNAVIGATION_GAMES_COUNTDOWN_HPP
