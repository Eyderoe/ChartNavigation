#include "mapGeometry.hpp"
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#include <numeric>

// Ported from genmap, extend_options, the cumulative frequency table and the
// diagonal smoothing in new_game in Simon Tatham's map.c, 20260923.616da16.
// Copyright (c) 2004-2024 Simon Tatham. MIT; see SIMON-TATHAM-LICENSE.txt.
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/puzzles.tar.gz
namespace MapGeometry {
static void cf_init(int *table, int n)
{
    int i;

    for (i = 0; i < n; i++)
        table[i] = 0;
}

/*
 * Increment the count of symbol `sym' by `count'.
 */
static void cf_add(int *table, int n, int sym, int count)
{
    int bit;

    bit = 1;
    while (sym != 0) {
        if (sym & bit) {
            table[sym] += count;
            sym &= ~bit;
        }
        bit <<= 1;
    }

    table[0] += count;
}

/*
 * Cumulative frequency lookup: return the total count of symbols
 * with value less than `sym'.
 */
static int cf_clookup(int *table, int n, int sym)
{
    int bit, index, limit, count;

    if (sym == 0)
        return 0;

    Q_ASSERT(0 < sym && sym <= n);

    count = table[0];                  /* start with the whole table size */

    bit = 1;
    while (bit < n)
        bit <<= 1;

    limit = n;

    while (bit > 0) {
        /*
         * Find the least number with its lowest set bit in this
         * position which is greater than or equal to sym.
         */
        index = ((sym + bit - 1) &~ (bit * 2 - 1)) + bit;

        if (index < limit) {
            count -= table[index];
            limit = index;
        }

        bit >>= 1;
    }

    return count;
}

/*
 * Single frequency lookup: return the count of symbol `sym'.
 */
static int cf_slookup(int *table, int n, int sym)
{
    int count, bit;

    Q_ASSERT(0 <= sym && sym < n);

    count = table[sym];

    for (bit = 1; sym+bit < n && !(sym & bit); bit <<= 1)
        count -= table[sym+bit];

    return count;
}

/*
 * Return the largest symbol index such that the cumulative
 * frequency up to that symbol is less than _or equal to_ count.
 */
static int cf_whichsym(int *table, int n, int count) {
    int bit, sym, top;

    Q_ASSERT(count >= 0 && count < table[0]);

    bit = 1;
    while (bit < n)
        bit <<= 1;

    sym = 0;
    top = table[0];

    while (bit > 0) {
        if (sym+bit < n) {
            if (count >= top - table[sym+bit])
                sym += bit;
            else
                top -= table[sym+bit];
        }

        bit >>= 1;
    }

    return sym;
}

/* ----------------------------------------------------------------------
 * Map generation.
 *
 * FIXME: this isn't entirely optimal at present, because it
 * inherently prioritises growing the largest region since there
 * are more squares adjacent to it. This acts as a destabilising
 * influence leading to a few large regions and mostly small ones.
 * It might be better to do it some other way.
 */

constexpr int WEIGHT_INCREASED = 2;
constexpr int WEIGHT_DECREASED = 4;
constexpr int WEIGHT_UNCHANGED = 3;

/*
 * Look at a square and decide which colours can be extended into
 * it.
 *
 * If called with index < 0, it adds together one of
 * WEIGHT_INCREASED, WEIGHT_DECREASED or WEIGHT_UNCHANGED for each
 * colour that has a valid extension (according to the effect that
 * it would have on the perimeter of the region being extended) and
 * returns the overall total.
 *
 * If called with index >= 0, it returns one of the possible
 * colours depending on the value of index, in such a way that the
 * number of possible inputs which would give rise to a given
 * return value correspond to the weight of that value.
 */
static int extend_options(int w, int h, int n, int *map,
                          int x, int y, int index)
{
    int c, i, dx, dy;
    int col[8];
    int total = 0;

    if (map[y*w+x] >= 0) {
        Q_ASSERT(index < 0);
        return 0;                      /* can't do this square at all */
    }

    /*
     * Fetch the eight neighbours of this square, in order around
     * the square.
     */
    for (dy = -1; dy <= +1; dy++)
        for (dx = -1; dx <= +1; dx++) {
            int index = (dy < 0 ? 6-dx : dy > 0 ? 2+dx : 2*(1+dx));
            if (x+dx >= 0 && x+dx < w && y+dy >= 0 && y+dy < h)
                col[index] = map[(y+dy)*w+(x+dx)];
            else
                col[index] = -1;
        }

    /*
     * Iterate over each colour that might be feasible.
     *
     * FIXME: this routine currently has O(n) running time. We
     * could turn it into O(FOUR) by only bothering to iterate over
     * the colours mentioned in the four neighbouring squares.
     */

    for (c = 0; c < n; c++) {
        int count, neighbours, runs;

        /*
         * One of the even indices of col (representing the
         * orthogonal neighbours of this square) must be equal to
         * c, or else this square is not adjacent to region c and
         * obviously cannot become an extension of it at this time.
         */
        neighbours = 0;
        for (i = 0; i < 8; i += 2)
            if (col[i] == c)
                neighbours++;
        if (!neighbours)
            continue;

        /*
         * Now we know this square is adjacent to region c. The
         * next question is, would extending it cause the region to
         * become non-simply-connected? If so, we mustn't do it.
         *
         * We determine this by looking around col to see if we can
         * find more than one separate run of colour c.
         */
        runs = 0;
        for (i = 0; i < 8; i++)
            if (col[i] == c && col[(i+1) & 7] != c)
                runs++;
        if (runs > 1)
            continue;

        Q_ASSERT(runs == 1);

        /*
         * This square is a possibility. Determine its effect on
         * the region's perimeter (computed from the number of
         * orthogonal neighbours - 1 means a perimeter increase, 3
         * a decrease, 2 no change; 4 is impossible because the
         * region would already not be simply connected) and we're
         * done.
         */
        Q_ASSERT(neighbours > 0 && neighbours < 4);
        count = (neighbours == 1 ? WEIGHT_INCREASED :
                 neighbours == 2 ? WEIGHT_UNCHANGED : WEIGHT_DECREASED);

        total += count;
        if (index >= 0 && index < count)
            return c;
        else
            index -= count;
    }

    Q_ASSERT(index < 0);

    return total;
}

std::vector<int> generate (int w, int h, int n, QRandomGenerator &random)
{
    int wh = w*h;
    int x, y, i, k;
    Q_ASSERT(n > 0 && n <= wh);
    std::vector<int> cells(wh), table(wh);
    int *map = cells.data(), *tmp = table.data();

    /*
     * Clear the map, and set up `tmp' as a list of grid indices.
     */
    for (i = 0; i < wh; i++) {
        map[i] = -1;
        tmp[i] = i;
    }

    /*
     * Place the region seeds by selecting n members from `tmp'.
     */
    k = wh;
    for (i = 0; i < n; i++) {
        int j = random.bounded(k);
        map[tmp[j]] = i;
        tmp[j] = tmp[--k];
    }

    /*
     * Re-initialise `tmp' as a cumulative frequency table. This
     * will store the number of possible region colours we can
     * extend into each square.
     */
    cf_init(tmp, wh);

    /*
     * Go through the grid and set up the initial cumulative
     * frequencies.
     */
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
            cf_add(tmp, wh, y*w+x,
                   extend_options(w, h, n, map, x, y, -1));

    /*
     * Now repeatedly choose a square we can extend a region into,
     * and do so.
     */
    while (tmp[0] > 0) {
        int k = random.bounded(tmp[0]);
        int sq;
        int colour;
        int xx, yy;

        sq = cf_whichsym(tmp, wh, k);
        k -= cf_clookup(tmp, wh, sq);
        x = sq % w;
        y = sq / w;
        colour = extend_options(w, h, n, map, x, y, k);

        map[sq] = colour;

        /*
         * Re-scan the nine cells around the one we've just
         * modified.
         */
        for (yy = std::max(y-1, 0); yy < std::min(y+2, h); yy++)
            for (xx = std::max(x-1, 0); xx < std::min(x+2, w); xx++) {
                cf_add(tmp, wh, yy*w+xx,
                       -cf_slookup(tmp, wh, yy*w+xx) +
                       extend_options(w, h, n, map, xx, yy, -1));
            }
    }

    /*
     * Finally, go through and normalise the region labels into
     * order, meaning that indistinguishable maps are actually
     * identical.
     */
    for (i = 0; i < n; i++)
        tmp[i] = -1;
    k = 0;
    for (i = 0; i < wh; i++) {
        Q_ASSERT(map[i] >= 0);
        if (tmp[map[i]] < 0)
            tmp[map[i]] = k++;
        map[i] = tmp[map[i]];
    }

    return cells;
}


std::vector<Tile> smooth (const std::vector<int> &cells, int width, int height, QRandomGenerator &random) {
    std::vector<Tile> tiles(cells.size());
    std::vector<int> order(cells.size());
    for (size_t i = 0; i < cells.size(); ++i) tiles[i].fill(cells[i]);
    std::iota(order.begin(), order.end(), 0);
    // Same Fisher-Yates order as upstream shuffle().
    for (int i = static_cast<int>(order.size()) - 1; i > 0; --i)
        std::swap(order[i], order[random.bounded(i + 1)]);
    bool changed;
    do {
        changed = false;
        for (int cell : order) {
            const int x = cell % width, y = cell / width;
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) continue;
            auto &tile = tiles[cell];
            if (tile[Top] != tile[Bottom]) continue;
            const int c = tile[Top];
            const int top = tiles[cell - width][Bottom], bottom = tiles[cell + width][Top];
            const int left = tiles[cell - 1][Right], right = tiles[cell + 1][Left];
            if (top != bottom && (top == c || bottom == c) &&
                ((left == top && right == bottom) || (left == bottom && right == top))) {
                tile = {top, bottom, left, right};
                changed = true;
            }
        }
    } while (changed);
    return tiles;
}

std::vector<std::vector<int>> adjacency (const std::vector<int> &cells, int width, int height, int regions) {
    std::vector<std::vector<int>> graph(regions);
    const auto edge = [&](int a, int b) {
        if (a != b && std::ranges::find(graph[a], b) == graph[a].end()) {
            graph[a].push_back(b);
            graph[b].push_back(a);
        }
    };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const int cell = y * width + x;
            if (x + 1 < width) edge(cells[cell], cells[cell + 1]);
            if (y + 1 < height) edge(cells[cell], cells[cell + width]);
        }
    for (auto &neighbors : graph)
        std::ranges::sort(neighbors);
    return graph;
}

int regionAt (const std::vector<Tile> &tiles, int width, int height, QPointF point) {
    if (point.x() < 0 || point.y() < 0 || point.x() >= width || point.y() >= height) return -1;
    const int x = static_cast<int>(point.x()), y = static_cast<int>(point.y());
    const qreal xOffset = point.x() - x - 0.5, yOffset = point.y() - y - 0.5;
    // Upstream region_from_logical_coords, including its diagonal tie breaks.
    const int quadrant = 2 * (xOffset > yOffset) + (-xOffset > yOffset);
    const auto edge = std::array<Edge, 4>{Bottom, Left, Right, Top}[quadrant];
    return tiles[y * width + x][edge];
}

std::array<QPointF, 3> triangle (int x, int y, Edge edge) {
    const QPointF center(x + 0.5, y + 0.5);
    switch (edge) {
        case Top: return {QPointF(x, y), QPointF(x + 1, y), center};
        case Bottom: return {QPointF(x + 1, y + 1), QPointF(x, y + 1), center};
        case Left: return {QPointF(x, y + 1), QPointF(x, y), center};
        case Right: return {QPointF(x + 1, y), QPointF(x + 1, y + 1), center};
    }
    Q_UNREACHABLE();
}
} // namespace MapGeometry
