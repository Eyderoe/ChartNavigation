#ifndef CHARTNAVIGATION_GAMES_MAPGEOMETRY_HPP
#define CHARTNAVIGATION_GAMES_MAPGEOMETRY_HPP

#include <QPointF>
#include <array>
#include <vector>

class QRandomGenerator;

namespace MapGeometry {
enum Edge { Top, Bottom, Left, Right };
// Each cell consists of four triangles joining its edges to its centre.
// Equal labels form a whole square, or two halves separated by a diagonal.
using Tile = std::array<int, 4>;

std::vector<int> generate (int width, int height, int regions, QRandomGenerator &random);
std::vector<Tile> smooth (const std::vector<int> &cells, int width, int height, QRandomGenerator &random);
std::vector<std::vector<int>> adjacency (const std::vector<int> &cells, int width, int height, int regions);
int regionAt (const std::vector<Tile> &tiles, int width, int height, QPointF point);
std::array<QPointF, 3> triangle (int x, int y, Edge edge);
}

#endif // CHARTNAVIGATION_GAMES_MAPGEOMETRY_HPP
