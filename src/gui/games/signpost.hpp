#ifndef CHARTNAVIGATION_GAMES_SIGNPOST_HPP
#define CHARTNAVIGATION_GAMES_SIGNPOST_HPP

#include "game.hpp"
#include "signpostBackend.h"
#include <QPointF>
#include <memory>
#include <vector>

class SignpostView;
class SignpostGame : public Game {
    public:
        explicit SignpostGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        using State = std::shared_ptr<SignpostBackend>;
        SignpostView *view{};
        SignpostConfig config{};
        State state, initial;
        std::vector<State> history, future;
        int cursor{}, dragStart{-1};
        bool busy{}, backwards{}, keyboardDrag{}, cursorVisible{};
        unsigned generation{};
        static State take (SignpostBackend *value);
        void selectPreset ();
        void applyCustom ();
        void reset ();
        void retry ();
        void adopt (State next);
        void undo ();
        void redo ();
        void refresh ();
        void cancelDrag ();
        void beginDrag (int cell, bool reverse, bool keyboard);
        void finishDrag (int cell);
};
#endif
