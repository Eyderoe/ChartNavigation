#ifndef CHARTNAVIGATION_GAMES_GUESSNUMBER_HPP
#define CHARTNAVIGATION_GAMES_GUESSNUMBER_HPP

#include "game.hpp"

class GuessNumberGame : public Game {
    public:
        explicit GuessNumberGame (Ui::note_widget *ui, QObject *parent);
    private:
        QString guessNumberAnswer;
        int guessNumberAttempts{};
        bool guessNumberFinished{};
        void initGuessNumber ();
        void resetGuessNumber ();
        void checkGuessNumber ();
};

#endif // CHARTNAVIGATION_GAMES_GUESSNUMBER_HPP
