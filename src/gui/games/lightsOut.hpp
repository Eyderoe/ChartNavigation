#ifndef CHARTNAVIGATION_GAMES_LIGHTSOUT_HPP
#define CHARTNAVIGATION_GAMES_LIGHTSOUT_HPP

#include "game.hpp"
#include <array>

class LightsOutView;

class LightsOutGame : public Game {
    public:
        explicit LightsOutGame (Ui::note_widget *ui, QObject *parent);
    private:
        std::array<bool, 25> lightsOutBoard{};
        std::array<bool, 25> lightsOutInitialBoard{};
        LightsOutView *lightsOutView{};
        int lightsOutMoves{};
        void initLightsOut ();
        void updateLightsOutTheme () const;
        void resetLightsOut ();
        void updateLightsOut () const;
};

#endif // CHARTNAVIGATION_GAMES_LIGHTSOUT_HPP
