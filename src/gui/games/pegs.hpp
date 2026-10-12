#ifndef CHARTNAVIGATION_GAMES_PEGS_HPP
#define CHARTNAVIGATION_GAMES_PEGS_HPP

#include "game.hpp"
#include <QPointF>
#include <vector>

class PegsView;
class PegsGame : public Game {
    public:
        explicit PegsGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        PegsView *view{};
        int width{}, height{}, cursor{}, selected{-1}, dragSource{-1};
        QPointF dragStart;
        std::vector<int> board, initial;
        std::vector<std::vector<int>> history;
        void reset ();
        bool jump (int from, int to);
        void click (int cell);
        void refresh ();
};

#endif // CHARTNAVIGATION_GAMES_PEGS_HPP
