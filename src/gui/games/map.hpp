#ifndef CHARTNAVIGATION_GAMES_MAP_HPP
#define CHARTNAVIGATION_GAMES_MAP_HPP

#include "game.hpp"
#include "mapGeometry.hpp"
#include <QPointF>
#include <array>
#include <vector>

class MapGameView;

class MapGame : public Game {
    public:
        explicit MapGame (Ui::note_widget *ui, QObject *parent);
    protected:
        bool eventFilter (QObject *watched, QEvent *event) override;
    private:
        MapGameView *mapView{};
        int mapWidth{}, mapHeight{};
        std::vector<MapGeometry::Tile> mapTiles;
        std::vector<std::vector<int>> mapAdjacent;
        std::vector<int> mapInitialColors;
        std::vector<int> mapColors;
        std::vector<int> mapNotes;
        struct MapMove {
            std::vector<int> colors;
            std::vector<int> notes;
        };
        std::vector<MapMove> mapHistory;
        std::array<QToolButton*, 4> mapColorButtons{};
        int mapSelectedColor{};
        int mapSelectedRegion{-1};
        int mapDragColor{-1};
        bool mapDragging{};
        QPointF mapDragStart;
        void initMap ();
        void resetMap ();
        void colorMapRegion (int region, int color, bool pencil = false);
        void updateMap ();
        bool handleMapEvent (QEvent *event);
};

#endif // CHARTNAVIGATION_GAMES_MAP_HPP
