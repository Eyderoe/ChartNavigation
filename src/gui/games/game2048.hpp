#ifndef CHARTNAVIGATION_GAMES_GAME2048_HPP
#define CHARTNAVIGATION_GAMES_GAME2048_HPP

#include "game.hpp"
#include <array>

class Game2048View;

class Game2048 : public Game {
    public:
        explicit Game2048 (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        std::array<quint64, 16> game2048Board{};
        Game2048View *game2048View{};
        quint64 game2048Score{};
        void init2048 ();
        void reset2048 ();
        void spawn2048Tile ();
        void move2048 (Qt::Key direction);
        void update2048 () const;
};

#endif // CHARTNAVIGATION_GAMES_GAME2048_HPP
