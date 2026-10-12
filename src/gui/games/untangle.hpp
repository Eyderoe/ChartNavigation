#ifndef CHARTNAVIGATION_GAMES_UNTANGLE_HPP
#define CHARTNAVIGATION_GAMES_UNTANGLE_HPP

#include "game.hpp"
#include <QPointF>
#include <array>
#include <vector>

class UntangleView;

class UntangleGame : public Game {
    public:
        explicit UntangleGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        UntangleView *untangleView{};
        std::vector<QPointF> untangleNodes;
        std::vector<QPointF> untangleInitialNodes;
        std::vector<std::array<int, 2>> untangleEdges;
        struct UntangleMove {
            std::vector<QPointF> nodes;
            bool counted{};
        };
        std::vector<UntangleMove> untangleHistory;
        std::vector<QPointF> untangleDragStart;
        QPointF untangleDragOffset;
        int untangleDraggedNode{-1};
        int untangleMoves{};
        int untangleCrossings{};
        void initUntangle ();
        void resetUntangle ();
        void updateUntangle ();
        void finishUntangleDrag ();
        bool handleUntangleEvent (QEvent *event);
};

#endif // CHARTNAVIGATION_GAMES_UNTANGLE_HPP
