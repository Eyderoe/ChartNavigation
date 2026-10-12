#ifndef CHARTNAVIGATION_GAMES_NET_HPP
#define CHARTNAVIGATION_GAMES_NET_HPP

#include "game.hpp"
#include <vector>

struct NetState {
    std::vector<int> ports;
    std::vector<bool> locked;
    int moves{};
    bool operator== (const NetState &) const = default;
};

class NetView;
class NetGame : public Game {
    public:
        explicit NetGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        NetView *view{};
        int width{}, height{}, cursor{};
        bool wrapping{};
        NetState state, initial;
        std::vector<NetState> history;
        void reset ();
        void rotate (int cell, int turns);
        void toggleLock (int cell);
        void refresh ();
};

#endif // CHARTNAVIGATION_GAMES_NET_HPP
