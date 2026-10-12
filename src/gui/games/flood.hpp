#ifndef CHARTNAVIGATION_GAMES_FLOOD_HPP
#define CHARTNAVIGATION_GAMES_FLOOD_HPP

#include "game.hpp"
#include <array>
#include <vector>

class FloodView;

class FloodGame : public Game {
    public:
        explicit FloodGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        FloodView *floodView{};
        std::vector<int> floodBoard;
        std::vector<int> floodInitialBoard;
        std::vector<std::vector<int>> floodHistory;
        std::array<QToolButton*, 6> floodColors{};
        int floodSide{};
        int floodColorCount{};
        int floodMoveLimit{};
        int floodCursor{};
        void initFlood ();
        void resetFlood ();
        void moveFlood (int color);
        void updateFlood ();
        bool handleFloodEvent (QEvent *event);
};

#endif // CHARTNAVIGATION_GAMES_FLOOD_HPP
