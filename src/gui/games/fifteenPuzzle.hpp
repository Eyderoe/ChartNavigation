#ifndef CHARTNAVIGATION_GAMES_FIFTEENPUZZLE_HPP
#define CHARTNAVIGATION_GAMES_FIFTEENPUZZLE_HPP

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <vector>

// Adapted from fifteen.c, Simon Tatham's Portable Puzzle Collection (MIT),
// snapshot 20260923.616da16, matching the web build.
// https://www.chiark.greenend.org.uk/~sgtatham/puzzles/puzzles.tar.gz
// See SIMON-TATHAM-LICENSE.txt. Random draws use the host's generator;
// generation rules match upstream, but upstream random seeds are not exposed.
namespace FifteenPuzzle {
struct State {
    int w{4}, h{4};
    std::vector<int> tiles;
    int gap_pos{}, movecount{}, completed{};
    bool used_solve{};
    bool operator== (const State &) const = default;
};

inline bool isCompleted (const std::vector<int> &tiles) {
    for (int i = 0; i < static_cast<int>(tiles.size()); ++i)
        if (tiles[i] != (i + 1) % static_cast<int>(tiles.size())) return false;
    return true;
}

inline int permutationParity (const std::vector<int> &tiles) {
    int parity = 0;
    for (int i = 0; i < static_cast<int>(tiles.size()) - 1; ++i)
        for (int j = i + 1; j < static_cast<int>(tiles.size()); ++j)
            if (tiles[i] > tiles[j]) parity ^= 1;
    return parity;
}

template <typename RandomBelow>
State generate (int width, int height, RandomBelow randomBelow) {
    State state;
    state.w = width;
    state.h = height;
    const int n = width * height;
    std::vector<bool> used(n);
    do {
        state.tiles.assign(n, -1);
        std::fill(used.begin(), used.end(), false);
        state.gap_pos = randomBelow(n);
        state.tiles[state.gap_pos] = 0;
        used[0] = true;
        int x = 0;
        for (int i = n - 1; i > 2; --i) {
            int k = randomBelow(i), piece = 0;
            for (; piece < n; ++piece)
                if (!used[piece] && k-- == 0) break;
            used[piece] = true;
            while (state.tiles[x] >= 0) ++x;
            state.tiles[x] = piece;
        }
        while (state.tiles[x] >= 0) ++x;
        const int x1 = x++;
        while (state.tiles[x] >= 0) ++x;
        const int x2 = x;
        int p1 = 0;
        while (used[p1]) ++p1;
        int p2 = p1 + 1;
        while (used[p2]) ++p2;
        state.tiles[x1] = p1;
        state.tiles[x2] = p2;
        const int parity = ((state.gap_pos % width - (width - 1)) ^
                            (state.gap_pos / width - (height - 1)) ^ (n + 1)) & 1;
        if (permutationParity(state.tiles) != parity)
            std::swap(state.tiles[x1], state.tiles[x2]);
    } while (isCompleted(state.tiles));
    return state;
}

inline bool slide (State &state, int cell) {
    if (cell < 0 || cell >= static_cast<int>(state.tiles.size()) || cell == state.gap_pos)
        return false;
    const int gx = state.gap_pos % state.w, gy = state.gap_pos / state.w;
    const int x = cell % state.w, y = cell / state.w;
    if (x != gx && y != gy) return false;
    const int step = y == gy ? (x > gx ? 1 : -1) : (y > gy ? state.w : -state.w);
    for (int p = state.gap_pos; p != cell; p += step) {
        state.tiles[p] = state.tiles[p + step];
        ++state.movecount;
    }
    state.tiles[cell] = 0;
    state.gap_pos = cell;
    if (!state.completed && isCompleted(state.tiles)) state.completed = state.movecount;
    return true;
}

inline void solve (State &state) {
    for (int i = 0; i < static_cast<int>(state.tiles.size()); ++i)
        state.tiles[i] = (i + 1) % static_cast<int>(state.tiles.size());
    state.gap_pos = static_cast<int>(state.tiles.size()) - 1;
    state.used_solve = true;
    state.completed = state.movecount = 1;
}

inline int tileX (const State *state, int cell) { return cell % state->w; }
inline int tileY (const State *state, int cell) { return cell / state->w; }

inline void next_move_3x2(int ax, int ay, int bx, int by,
                          int gx, int gy, int *dx, int *dy)
{
    /* When w = 3 and h = 2 and the tile going in the top left corner
     * is at (ax, ay) and the tile going in the bottom left corner is
     * at (bx, by) and the blank tile is at (gx, gy), how do you move? */

    /* Hard-coded shortest solutions.  Sorry. */
    static const unsigned char move[120] = {
        1,2,0,1,2,2,
        2,0,0,2,0,0,
        0,0,2,0,2,0,
        0,0,0,2,0,2,
        2,0,0,0,2,0,

        0,3,0,1,1,1,
        3,0,3,2,1,2,
        2,1,1,0,1,0,
        2,1,2,1,0,1,
        1,2,0,2,1,2,

        0,1,3,1,3,0,
        1,3,1,3,0,3,
        0,0,3,3,0,0,
        0,0,0,1,2,1,
        3,0,0,1,1,1,

        3,1,1,1,3,0,
        1,1,1,1,1,1,
        1,3,1,1,3,0,
        1,1,3,3,1,3,
        1,3,0,0,0,0
    };
    static const struct { int dx, dy; } d[4] = {{+1,0},{-1,0},{0,+1},{0,-1}};

    int ea = 3*ay + ax, eb = 3*by + bx, eg = 3*gy + gx, v;
    if (eb > ea) --eb;
    if (eg > ea) --eg;
    if (eg > eb) --eg;
    v = move[ea + eb*6 + eg*5*6];
    *dx = d[v].dx;
    *dy = d[v].dy;
}

inline void next_move(int nx, int ny, int ox, int oy, int gx, int gy,
                      int tx, int ty, int w, int *dx, int *dy)
{
    const int to_tile_x = (gx < nx ? +1 : -1);
    const int to_goal_x = (gx < tx ? +1 : -1);
    const bool gap_x_on_goal_side = ((nx-tx) * (nx-gx) > 0);

    assert (nx != tx || ny != ty); /* not already in place */
    assert (nx != gx || ny != gy); /* not placing the gap */
    assert (ty <= ny); /* because we're greedy (and flipping) */
    assert (ty <= gy); /* because we're greedy (and flipping) */

    /* TODO: define a termination function.  Idea: 0 if solved, or
     * the number of moves to solve the next piece plus the number of
     * further unsolved pieces times an upper bound on the number of
     * moves required to solve any piece.  If such a function can be
     * found, we have (termination && (termination => correctness)).
     * The catch is our temporary disturbance of 2x3 corners. */

    /* handles end-of-row, when 3 and 4 are in the top right 2x3 box */
    if (tx == w - 2 &&
        ny <= ty + 2 && (nx == tx || nx == tx + 1) &&
        oy <= ty + 2 && (ox == tx || ox == tx + 1) &&
        gy <= ty + 2 && (gx == tx || gx == tx + 1))
    {
        next_move_3x2(oy - ty, tx + 1 - ox,
                      ny - ty, tx + 1 - nx,
                      gy - ty, tx + 1 - gx, dy, dx);
        *dx *= -1;
        return;
    }

    if (tx == w - 1) {
        if (ny <= ty + 2 && (nx == tx || nx == tx - 1) &&
            gy <= ty + 2 && (gx == tx || gx == tx - 1)) {
            next_move_3x2(ny - ty, tx - nx, 0, 1, gy - ty, tx - gx, dy, dx);
            *dx *= -1;
        } else if (gy == ty)
            *dy = +1;
        else if (nx != tx || ny != ty + 1) {
            next_move((w - 1) - nx, ny, -1, -1, (w - 1) - gx, gy,
                      0, ty + 1, -1, dx, dy);
            *dx *= -1;
        } else if (gx == nx)
            *dy = -1;
        else
            *dx = +1;
        return;
    }

    /* note that *dy = -1 is unsafe when gy = ty + 1 and gx < tx */
    if (gy < ny)
        if (nx == gx || (gy == ty && gx == tx))
            *dy = +1;
        else if (!gap_x_on_goal_side)
            *dx = to_tile_x;
        else if (ny - ty > abs(nx - tx))
            *dx = to_tile_x;
        else *dy = +1;

    else if (gy == ny)
        if (nx == tx) /* then we know ny > ty */
            if (gx > nx || ny > ty + 1)
                *dy = -1; /* ... so this is safe */
            else
                *dy = +1;
        else if (gap_x_on_goal_side)
            *dx = to_tile_x;
        else if (gy == ty || (gy == ty + 1 && gx < tx))
            *dy = +1;
        else
            *dy = -1;

    else if (nx == tx) /* gy > ny */
        if (gx > nx)
            *dy = -1;
        else
            *dx = +1;
    else if (gx == nx)
        *dx = to_goal_x;
    else if (gap_x_on_goal_side)
        if (gy == ty + 1 && gx < tx)
            *dx = to_tile_x;
        else
            *dy = -1;

    else if (ny - ty > abs(nx - tx))
        *dy = -1;
    else
        *dx = to_tile_x;
}

inline bool compute_hint(const State *state, int *out_x, int *out_y)
{
    /* The overall solving process is this:
     * 1. Find the next piece to be put in its place
     * 2. Move it diagonally towards its place
     * 3. Move it horizontally or vertically towards its place
     * (Modulo the last two tiles at the end of each row/column)
     */

    int gx = tileX(state, state->gap_pos);
    int gy = tileY(state, state->gap_pos);

    int tx, ty, nx, ny, ox, oy, /* {target,next,next2}_{x,y} */ i;
    int dx = 0, dy = 0;

    /* 1. Find the next piece
     * if (there are no more unfinished columns than rows) {
     *     fill the top-most row, left to right
     * } else { fill the left-most column, top to bottom }
     */
    const int w = state->w, h = state->h, n = w*h;
    int next_piece = 0, next_piece_2 = 0, solr = 0, solc = 0;
    int unsolved_rows = h, unsolved_cols = w;

    assert(out_x);
    assert(out_y);

    while (solr < h && solc < w) {
        int start, step, stop;
        if (unsolved_cols <= unsolved_rows)
            start = solr*w + solc, step = 1, stop = unsolved_cols;
        else
            start = solr*w + solc, step = w, stop = unsolved_rows;
        for (i = 0; i < stop; ++i) {
            const int j = start + i*step;
            if (state->tiles[j] != j + 1) {
                next_piece = j + 1;
                next_piece_2 = next_piece + step;
                break;
            }
        }
        if (i < stop) break;

        (unsolved_cols <= unsolved_rows)
            ? (++solr, --unsolved_rows)
            : (++solc, --unsolved_cols);
    }

    if (next_piece == n)
        return false;

    /* 2, 3. Move the next piece towards its place */

    /* gx, gy already set */
    tx = tileX(state, next_piece - 1); /* where we're going */
    ty = tileY(state, next_piece - 1);
    for (i = 0; i < n && state->tiles[i] != next_piece; ++i);
    nx = tileX(state, i); /* where we're at */
    ny = tileY(state, i);
    for (i = 0; i < n && state->tiles[i] != next_piece_2; ++i);
    ox = tileX(state, i);
    oy = tileY(state, i);

    if (unsolved_cols <= unsolved_rows)
        next_move(nx, ny, ox, oy, gx, gy, tx, ty, w, &dx, &dy);
    else
        next_move(ny, nx, oy, ox, gy, gx, ty, tx, h, &dy, &dx);

    assert (dx || dy);

    *out_x = gx + dx;
    *out_y = gy + dy;
    return true;
}

inline int hintCell (const State &state) {
    int x{}, y{};
    if (!compute_hint(&state, &x, &y)) return -1;
    return y * state.w + x;
}

} // namespace FifteenPuzzle

#endif // CHARTNAVIGATION_GAMES_FIFTEENPUZZLE_HPP
