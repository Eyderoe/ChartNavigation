/*
 * signpost.c: implementation of the janko game 'arrow path'
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#ifdef NO_TGMATH_H
#  include <math.h>
#else
#  include <tgmath.h>
#endif

#include "puzzles.h"

#define PREFERRED_TILE_SIZE 48
#define TILE_SIZE (ds->tilesize)
#define BLITTER_SIZE TILE_SIZE
#define BORDER    (TILE_SIZE / 2)

#define COORD(x)  ( (x) * TILE_SIZE + BORDER )
#define FROMCOORD(x)  ( ((x) - BORDER + TILE_SIZE) / TILE_SIZE - 1 )

#define INGRID(s,x,y) ((x) >= 0 && (x) < (s)->w && (y) >= 0 && (y) < (s)->h)

#define FLASH_SPIN 0.7F

#define NBACKGROUNDS 16

enum {
    COL_BACKGROUND, COL_HIGHLIGHT, COL_LOWLIGHT,
    COL_GRID, COL_CURSOR, COL_ERROR, COL_DRAG_ORIGIN,
    COL_ARROW, COL_ARROW_BG_DIM,
    COL_NUMBER, COL_NUMBER_SET, COL_NUMBER_SET_MID,
    COL_B0,                             /* background colours */
    COL_M0 =   COL_B0 + 1*NBACKGROUNDS, /* mid arrow colours */
    COL_D0 =   COL_B0 + 2*NBACKGROUNDS, /* dim arrow colours */
    COL_X0 =   COL_B0 + 3*NBACKGROUNDS, /* dim arrow colours */
    NCOLOURS = COL_B0 + 4*NBACKGROUNDS
};

enum {
    PREF_FLASH_TYPE,
    N_PREF_ITEMS
};

struct game_params {
    int w, h;
    bool force_corner_start;
};

enum { DIR_N = 0, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_MAX };
static const char *dirstrings[8] = { "N ", "NE", "E ", "SE", "S ", "SW", "W ", "NW" };

static const int dxs[DIR_MAX] = {  0,  1, 1, 1, 0, -1, -1, -1 };
static const int dys[DIR_MAX] = { -1, -1, 0, 1, 1,  1,  0, -1 };

#define DIR_OPPOSITE(d) ((d+4)%8)

struct game_state {
    int w, h, n;
    bool completed, used_solve, impossible;
    int *dirs;                  /* direction enums, size n */
    int *nums;                  /* numbers, size n */
    unsigned int *flags;        /* flags, size n */
    int *next, *prev;           /* links to other cell indexes, size n (-1 absent) */
    DSF *dsf;                   /* connects regions with a dsf. */
    int *numsi;                 /* for each number, which index is it in? (-1 absent) */
};

#define FLAG_IMMUTABLE  1
#define FLAG_ERROR      2

/* --- Generally useful functions --- */

#define ISREALNUM(state, num) ((num) > 0 && (num) <= (state)->n)

static int whichdir(int fromx, int fromy, int tox, int toy)
{
    int i, dx, dy;

    dx = tox - fromx;
    dy = toy - fromy;

    if (dx && dy && abs(dx) != abs(dy)) return -1;

    if (dx) dx = dx / abs(dx); /* limit to (-1, 0, 1) */
    if (dy) dy = dy / abs(dy); /* ditto */

    for (i = 0; i < DIR_MAX; i++) {
        if (dx == dxs[i] && dy == dys[i]) return i;
    }
    return -1;
}

static int whichdiri(game_state *state, int fromi, int toi)
{
    int w = state->w;
    return whichdir(fromi%w, fromi/w, toi%w, toi/w);
}

static bool ispointing(const game_state *state, int fromx, int fromy,
                       int tox, int toy)
{
    int w = state->w, dir = state->dirs[fromy*w+fromx];

    /* (by convention) squares do not point to themselves. */
    if (fromx == tox && fromy == toy) return false;

    /* the final number points to nothing. */
    if (state->nums[fromy*w + fromx] == state->n) return false;

    while (1) {
        if (!INGRID(state, fromx, fromy)) return false;
        if (fromx == tox && fromy == toy) return true;
        fromx += dxs[dir]; fromy += dys[dir];
    }
    return false; /* not reached */
}

static bool ispointingi(game_state *state, int fromi, int toi)
{
    int w = state->w;
    return ispointing(state, fromi%w, fromi/w, toi%w, toi/w);
}

/* Taking the number 'num', work out the gap between it and the next
 * available number up or down (depending on d). Return true if the
 * region at (x,y) will fit in that gap. */
static bool move_couldfit(
    const game_state *state, int num, int d, int x, int y)
{
    int n, gap, i = y*state->w+x, sz;

    assert(d != 0);
    /* The 'gap' is the number of missing numbers in the grid between
     * our number and the next one in the sequence (up or down), or
     * the end of the sequence (if we happen not to have 1/n present) */
    for (n = num + d, gap = 0;
         ISREALNUM(state, n) && state->numsi[n] == -1;
         n += d, gap++) ; /* empty loop */

    if (gap == 0) {
        /* no gap, so the only allowable move is that that directly
         * links the two numbers. */
        n = state->nums[i];
        return n != num+d;
    }
    if (state->prev[i] == -1 && state->next[i] == -1)
        return true; /* single unconnected square, always OK */

    sz = dsf_size(state->dsf, i);
    return sz <= gap;
}

static bool isvalidmove(const game_state *state, bool clever,
                        int fromx, int fromy, int tox, int toy)
{
    int w = state->w, from = fromy*w+fromx, to = toy*w+tox;
    int nfrom, nto;

    if (!INGRID(state, fromx, fromy) || !INGRID(state, tox, toy))
        return false;

    /* can only move where we point */
    if (!ispointing(state, fromx, fromy, tox, toy))
        return false;

    nfrom = state->nums[from]; nto = state->nums[to];

    /* can't move _from_ the preset final number, or _to_ the preset 1. */
    if (((nfrom == state->n) && (state->flags[from] & FLAG_IMMUTABLE)) ||
        ((nto   == 1)        && (state->flags[to]   & FLAG_IMMUTABLE)))
        return false;

    /* can't create a new connection between cells in the same region
     * as that would create a loop. */
    if (dsf_equivalent(state->dsf, from, to))
        return false;

    /* if both cells are actual numbers, can't drag if we're not
     * one digit apart. */
    if (ISREALNUM(state, nfrom) && ISREALNUM(state, nto)) {
        if (nfrom != nto-1)
            return false;
    } else if (clever && ISREALNUM(state, nfrom)) {
        if (!move_couldfit(state, nfrom, +1, tox, toy))
            return false;
    } else if (clever && ISREALNUM(state, nto)) {
        if (!move_couldfit(state, nto, -1, fromx, fromy))
            return false;
    }

    return true;
}

static void makelink(game_state *state, int from, int to)
{
    if (state->next[from] != -1)
        state->prev[state->next[from]] = -1;
    state->next[from] = to;

    if (state->prev[to] != -1)
        state->next[state->prev[to]] = -1;
    state->prev[to] = from;
}

static bool game_can_format_as_text_now(const game_params *params)
{
    if (params->w * params->h >= 100) return false;
    return true;
}

static char *game_text_format(const game_state *state)
{
    int len = state->h * 2 * (4*state->w + 1) + state->h + 2;
    int x, y, i, num, n, set;
    char *ret, *p;

    p = ret = snewn(len, char);

    for (y = 0; y < state->h; y++) {
        for (x = 0; x < state->h; x++) {
            i = y*state->w+x;
            *p++ = dirstrings[state->dirs[i]][0];
            *p++ = dirstrings[state->dirs[i]][1];
            *p++ = (state->flags[i] & FLAG_IMMUTABLE) ? 'I' : ' ';
            *p++ = ' ';
        }
        *p++ = '\n';
        for (x = 0; x < state->h; x++) {
            i = y*state->w+x;
            num = state->nums[i];
            if (num == 0) {
                *p++ = ' ';
                *p++ = ' ';
                *p++ = ' ';
            } else {
                n = num % (state->n+1);
                set = num / (state->n+1);

                assert(n <= 99); /* two digits only! */

                if (set != 0)
                    *p++ = set+'a'-1;

                *p++ = (n >= 10) ? ('0' + (n/10)) : ' ';
                *p++ = '0' + (n%10);

                if (set == 0)
                    *p++ = ' ';
            }
            *p++ = ' ';
        }
        *p++ = '\n';
        *p++ = '\n';
    }
    *p++ = '\0';

    return ret;
}

static void debug_state(const char *desc, game_state *state)
{
#ifdef DEBUGGING
    char *dbg;
    if (state->n >= 100) {
        debug(("[ no game_text_format for this size ]"));
        return;
    }
    dbg = game_text_format(state);
    debug(("%s\n%s", desc, dbg));
    sfree(dbg);
#endif
}


static void strip_nums(game_state *state) {
    int i;
    for (i = 0; i < state->n; i++) {
        if (!(state->flags[i] & FLAG_IMMUTABLE))
            state->nums[i] = 0;
    }
    memset(state->next, -1, state->n*sizeof(int));
    memset(state->prev, -1, state->n*sizeof(int));
    memset(state->numsi, -1, (state->n+1)*sizeof(int));
    dsf_reinit(state->dsf);
}

static bool check_nums(game_state *orig, game_state *copy, bool only_immutable)
{
    int i;
    bool ret = true;
    assert(copy->n == orig->n);
    for (i = 0; i < copy->n; i++) {
        if (only_immutable && !(copy->flags[i] & FLAG_IMMUTABLE)) continue;
        assert(copy->nums[i] >= 0);
        assert(copy->nums[i] <= copy->n);
        if (copy->nums[i] != orig->nums[i]) {
            debug(("check_nums: (%d,%d) copy=%d, orig=%d.",
                   i%orig->w, i/orig->w, copy->nums[i], orig->nums[i]));
            ret = false;
        }
    }
    return ret;
}

/* --- Game parameter/presets functions --- */

static game_params *default_params(void)
{
    game_params *ret = snew(game_params);
    ret->w = ret->h = 4;
    ret->force_corner_start = true;

    return ret;
}

static const struct game_params signpost_presets[] = {
  { 4, 4, 1 },
  { 4, 4, 0 },
  { 5, 5, 1 },
  { 5, 5, 0 },
  { 6, 6, 1 },
  { 7, 7, 1 }
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
    game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(signpost_presets))
        return false;

    ret = default_params();
    *ret = signpost_presets[i];
    *params = ret;

    sprintf(buf, "%dx%d%s", ret->w, ret->h,
            ret->force_corner_start ? "" : ", free ends");
    *name = dupstr(buf);

    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *ret, char const *string)
{
    ret->w = ret->h = atoi(string);
    while (*string && isdigit((unsigned char)*string)) string++;
    if (*string == 'x') {
        string++;
        ret->h = atoi(string);
        while (*string && isdigit((unsigned char)*string)) string++;
    }
    ret->force_corner_start = false;
    if (*string == 'c') {
        string++;
        ret->force_corner_start = true;
    }

}

static char *encode_params(const game_params *params, bool full)
{
    char data[256];

    if (full)
        sprintf(data, "%dx%d%s", params->w, params->h,
                params->force_corner_start ? "c" : "");
    else
        sprintf(data, "%dx%d", params->w, params->h);

    return dupstr(data);
}

static config_item *game_configure(const game_params *params)
{
    config_item *ret;
    char buf[80];

    ret = snewn(4, config_item);

    ret[0].name = "Width";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].u.string.sval = dupstr(buf);

    ret[1].name = "Height";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->h);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Start and end in corners";
    ret[2].type = C_BOOLEAN;
    ret[2].u.boolean.bval = params->force_corner_start;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
    game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->h = atoi(cfg[1].u.string.sval);
    ret->force_corner_start = cfg[2].u.boolean.bval;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
    if (params->w < 1) return "Width must be at least one";
    if (params->h < 1) return "Height must be at least one";
    if (params->w > INT_MAX / params->h)
        return "Width times height must not be unreasonably large";
    if (full && params->w == 1 && params->h == 1)
	/* The UI doesn't let us move these from unsolved to solved,
	 * so we disallow generating (but not playing) them. */
	return "Width and height cannot both be one";
    return NULL;
}

/* --- Game description string generation and unpicking --- */

static void blank_game_into(game_state *state)
{
    memset(state->dirs, 0, state->n*sizeof(int));
    memset(state->nums, 0, state->n*sizeof(int));
    memset(state->flags, 0, state->n*sizeof(unsigned int));
    memset(state->next, -1, state->n*sizeof(int));
    memset(state->prev, -1, state->n*sizeof(int));
    memset(state->numsi, -1, (state->n+1)*sizeof(int));
}

static game_state *blank_game(int w, int h)
{
    game_state *state = snew(game_state);

    memset(state, 0, sizeof(game_state));
    state->w = w;
    state->h = h;
    state->n = w*h;

    state->dirs  = snewn(state->n, int);
    state->nums  = snewn(state->n, int);
    state->flags = snewn(state->n, unsigned int);
    state->next  = snewn(state->n, int);
    state->prev  = snewn(state->n, int);
    state->dsf = dsf_new(state->n);
    state->numsi  = snewn(state->n+1, int);

    blank_game_into(state);

    return state;
}

static void dup_game_to(game_state *to, const game_state *from)
{
    to->completed = from->completed;
    to->used_solve = from->used_solve;
    to->impossible = from->impossible;

    memcpy(to->dirs, from->dirs, to->n*sizeof(int));
    memcpy(to->flags, from->flags, to->n*sizeof(unsigned int));
    memcpy(to->nums, from->nums, to->n*sizeof(int));

    memcpy(to->next, from->next, to->n*sizeof(int));
    memcpy(to->prev, from->prev, to->n*sizeof(int));

    dsf_copy(to->dsf, from->dsf);
    memcpy(to->numsi, from->numsi, (to->n+1)*sizeof(int));
}

static game_state *dup_game(const game_state *state)
{
    game_state *ret = blank_game(state->w, state->h);
    dup_game_to(ret, state);
    return ret;
}

static void free_game(game_state *state)
{
    sfree(state->dirs);
    sfree(state->nums);
    sfree(state->flags);
    sfree(state->next);
    sfree(state->prev);
    dsf_free(state->dsf);
    sfree(state->numsi);
    sfree(state);
}

static void unpick_desc(const game_params *params, const char *desc,
                        game_state **sout, const char **mout)
{
    game_state *state = blank_game(params->w, params->h);
    const char *msg = NULL;
    char c;
    int num = 0, i = 0;

    while (*desc) {
        if (i >= state->n) {
            msg = "Game description longer than expected";
            goto done;
        }

        c = *desc;
        if (isdigit((unsigned char)c)) {
            num = (num*10) + (int)(c-'0');
            if (num > state->n) {
                msg = "Number too large";
                goto done;
            }
        } else if ((c-'a') >= 0 && (c-'a') < DIR_MAX) {
            state->nums[i] = num;
            state->flags[i] = num ? FLAG_IMMUTABLE : 0;
            num = 0;

            state->dirs[i] = c - 'a';
            i++;
        } else if (!*desc) {
            msg = "Game description shorter than expected";
            goto done;
        } else {
            msg = "Game description contains unexpected characters";
            goto done;
        }
        desc++;
    }
    if (i < state->n) {
        msg = "Game description shorter than expected";
        goto done;
    }

done:
    if (msg) { /* sth went wrong. */
        if (mout) *mout = msg;
        free_game(state);
    } else {
        if (mout) *mout = NULL;
        if (sout) *sout = state;
        else free_game(state);
    }
}

static char *generate_desc(game_state *state, bool issolve)
{
    char *ret, buf[80];
    int retlen, i, k;

    ret = NULL; retlen = 0;
    if (issolve) {
        ret = sresize(ret, 2, char);
        ret[0] = 'S'; ret[1] = '\0';
        retlen += 1;
    }
    for (i = 0; i < state->n; i++) {
        if (state->nums[i])
            k = sprintf(buf, "%d%c", state->nums[i], (int)(state->dirs[i]+'a'));
        else
            k = sprintf(buf, "%c", (int)(state->dirs[i]+'a'));
        ret = sresize(ret, retlen + k + 1, char);
        strcpy(ret + retlen, buf);
        retlen += k;
    }
    return ret;
}

/* --- Game generation --- */

/* Fills in preallocated arrays ai (indices) and ad (directions)
 * showing all non-numbered cells adjacent to index i, returns length */
/* This function has been somewhat optimised... */
static int cell_adj(game_state *state, int i, int *ai, int *ad)
{
    int n = 0, a, x, y, sx, sy, dx, dy, newi;
    int w = state->w, h = state->h;

    sx = i % w; sy = i / w;

    for (a = 0; a < DIR_MAX; a++) {
        x = sx; y = sy;
        dx = dxs[a]; dy = dys[a];
        while (1) {
            x += dx; y += dy;
            if (x < 0 || y < 0 || x >= w || y >= h) break;

            newi = y*w + x;
            if (state->nums[newi] == 0) {
                ai[n] = newi;
                ad[n] = a;
                n++;
            }
        }
    }
    return n;
}

static bool new_game_fill(game_state *state, random_state *rs,
                          int headi, int taili)
{
    int nfilled, an, j;
    bool ret = false;
    int *aidx, *adir;

    aidx = snewn(state->n, int);
    adir = snewn(state->n, int);

    debug(("new_game_fill: headi=%d, taili=%d.", headi, taili));

    memset(state->nums, 0, state->n*sizeof(int));

    state->nums[headi] = 1;
    state->nums[taili] = state->n;

    state->dirs[taili] = 0;
    nfilled = 2;
    assert(state->n > 1);

    while (nfilled < state->n) {
        /* Try and expand _from_ headi; keep going if there's only one
         * place to go to. */
        an = cell_adj(state, headi, aidx, adir);
        do {
            if (an == 0) goto done;
            j = random_upto(rs, an);
            state->dirs[headi] = adir[j];
            state->nums[aidx[j]] = state->nums[headi] + 1;
            nfilled++;
            headi = aidx[j];
            an = cell_adj(state, headi, aidx, adir);
        } while (an == 1);

	if (nfilled == state->n) break;

        /* Try and expand _to_ taili; keep going if there's only one
         * place to go to. */
        an = cell_adj(state, taili, aidx, adir);
        do {
            if (an == 0) goto done;
            j = random_upto(rs, an);
            state->dirs[aidx[j]] = DIR_OPPOSITE(adir[j]);
            state->nums[aidx[j]] = state->nums[taili] - 1;
            nfilled++;
            taili = aidx[j];
            an = cell_adj(state, taili, aidx, adir);
        } while (an == 1);
    }
    /* If we get here we have headi and taili set but unconnected
     * by direction: we need to set headi's direction so as to point
     * at taili. */
    state->dirs[headi] = whichdiri(state, headi, taili);

    /* it could happen that our last two weren't in line; if that's the
     * case, we have to start again. */
    if (state->dirs[headi] != -1) ret = true;

done:
    sfree(aidx);
    sfree(adir);
    return ret;
}

/* Better generator: with the 'generate, sprinkle numbers, solve,
 * repeat' algorithm we're _never_ generating anything greater than
 * 6x6, and spending all of our time in new_game_fill (and very little
 * in solve_state).
 *
 * So, new generator steps:
   * generate the grid, at random (same as now). Numbers 1 and N get
      immutable flag immediately.
   * squirrel that away for the solved state.
   *
   * (solve:) Try and solve it.
   * If we solved it, we're done:
     * generate the description from current immutable numbers,
     * free stuff that needs freeing,
     * return description + solved state.
   * If we didn't solve it:
     * count #tiles in state we've made deductions about.
     * while (1):
       * randomise a scratch array.
       * for each index in scratch (in turn):
         * if the cell isn't empty, continue (through scratch array)
         * set number + immutable in state.
         * try and solve state.
         * if we've solved it, we're done.
         * otherwise, count #tiles. If it's more than we had before:
           * good, break from this loop and re-randomise.
         * otherwise (number didn't help):
           * remove number and try next in scratch array.
       * if we've got to the end of the scratch array, no luck:
          free everything we need to, and go back to regenerate the grid.
   */

static int solve_state(game_state *state);

static void debug_desc(const char *what, game_state *state)
{
#if DEBUGGING
    {
        char *desc = generate_desc(state, 0);
        debug(("%s game state: %dx%d:%s", what, state->w, state->h, desc));
        sfree(desc);
    }
#endif
}

/* Expects a fully-numbered game_state on input, and makes sure
 * FLAG_IMMUTABLE is only set on those numbers we need to solve
 * (as for a real new-game); returns true if it managed
 * this (such that it could solve it), or false if not. */
static bool new_game_strip(game_state *state, random_state *rs)
{
    int *scratch, i, j;
    bool ret = true;
    game_state *copy = dup_game(state);

    debug(("new_game_strip."));

    strip_nums(copy);
    debug_desc("Stripped", copy);

    if (solve_state(copy) > 0) {
        debug(("new_game_strip: soluble immediately after strip."));
        free_game(copy);
        return true;
    }

    scratch = snewn(state->n, int);
    for (i = 0; i < state->n; i++) scratch[i] = i;
    shuffle(scratch, state->n, sizeof(int), rs);

    /* This is scungy. It might just be quick enough.
     * It goes through, adding set numbers in empty squares
     * until either we run out of empty squares (in the one
     * we're half-solving) or else we solve it properly.
     * NB that we run the entire solver each time, which
     * strips the grid beforehand; we will save time if we
     * avoid that. */
    for (i = 0; i < state->n; i++) {
        j = scratch[i];
        if (copy->nums[j] > 0 && copy->nums[j] <= state->n)
            continue; /* already solved to a real number here. */
        assert(state->nums[j] <= state->n);
        debug(("new_game_strip: testing add IMMUTABLE number %d at square (%d,%d).",
               state->nums[j], j%state->w, j/state->w));
        copy->nums[j] = state->nums[j];
        copy->flags[j] |= FLAG_IMMUTABLE;
        state->flags[j] |= FLAG_IMMUTABLE;
        debug_state("Copy of state: ", copy);
        strip_nums(copy);
        if (solve_state(copy) > 0) goto solved;
        assert(check_nums(state, copy, true));
    }
    ret = false;
    goto done;

solved:
    debug(("new_game_strip: now solved."));
    /* Since we added basically at random, try now to remove numbers
     * and see if we can still solve it; if we can (still), really
     * remove the number. Make sure we don't remove the anchor numbers
     * 1 and N. */
    for (i = 0; i < state->n; i++) {
        j = scratch[i];
        if ((state->flags[j] & FLAG_IMMUTABLE) &&
            (state->nums[j] != 1 && state->nums[j] != state->n)) {
            debug(("new_game_strip: testing remove IMMUTABLE number %d at square (%d,%d).",
                  state->nums[j], j%state->w, j/state->w));
            state->flags[j] &= ~FLAG_IMMUTABLE;
            dup_game_to(copy, state);
            strip_nums(copy);
            if (solve_state(copy) > 0) {
                assert(check_nums(state, copy, false));
                debug(("new_game_strip: OK, removing number"));
            } else {
                assert(state->nums[j] <= state->n);
                debug(("new_game_strip: cannot solve, putting IMMUTABLE back."));
                copy->nums[j] = state->nums[j];
                state->flags[j] |= FLAG_IMMUTABLE;
            }
        }
    }

done:
    debug(("new_game_strip: %ssuccessful.", ret ? "" : "not "));
    sfree(scratch);
    free_game(copy);
    return ret;
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
    game_state *state = blank_game(params->w, params->h);
    char *ret;
    int headi, taili;

    /* this shouldn't happen (validate_params), but let's play it safe */
    if (params->w == 1 && params->h == 1) return dupstr("1a");

generate:
    blank_game_into(state);

    /* keep trying until we fill successfully. */
    do {
        if (params->force_corner_start) {
            headi = 0;
            taili = state->n-1;
        } else {
            do {
                headi = random_upto(rs, state->n);
                taili = random_upto(rs, state->n);
            } while (headi == taili);
        }
    } while (!new_game_fill(state, rs, headi, taili));

    debug_state("Filled game:", state);

    assert(state->nums[headi] <= state->n);
    assert(state->nums[taili] <= state->n);

    state->flags[headi] |= FLAG_IMMUTABLE;
    state->flags[taili] |= FLAG_IMMUTABLE;

    /* This will have filled in directions and _all_ numbers.
     * Store the game definition for this, as the solved-state. */
    if (!new_game_strip(state, rs)) {
        goto generate;
    }
    strip_nums(state);
    {
        game_state *tosolve = dup_game(state);
        assert(solve_state(tosolve) > 0);
        free_game(tosolve);
    }
    ret = generate_desc(state, false);
    free_game(state);
    return ret;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
    const char *ret = NULL;

    unpick_desc(params, desc, NULL, &ret);
    return ret;
}

/* --- Linked-list and numbers array --- */

/* Assuming numbers are always up-to-date, there are only four possibilities
 * for regions changing after a single valid move:
 *
 * 1) two differently-coloured regions being combined (the resulting colouring
 *     should be based on the larger of the two regions)
 * 2) a numbered region having a single number added to the start (the
 *     region's colour will remain, and the numbers will shift by 1)
 * 3) a numbered region having a single number added to the end (the
 *     region's colour and numbering remains as-is)
 * 4) two unnumbered squares being joined (will pick the smallest unused set
 *     of colours to use for the new region).
 *
 * There should never be any complications with regions containing 3 colours
 * being combined, since two of those colours should have been merged on a
 * previous move.
 *
 * Most of the complications are in ensuring we don't accidentally set two
 * regions with the same colour (e.g. if a region was split). If this happens
 * we always try and give the largest original portion the original colour.
 */

#define COLOUR(a) ((a) / (state->n+1))
#define START(c) ((c) * (state->n+1))

struct head_meta {
    int i;      /* position */
    int sz;     /* size of region */
    int start;  /* region start number preferred, or 0 if !preference */
    int preference; /* 0 if we have no preference (and should just pick one) */
    const char *why;
};

static void head_number(game_state *state, int i, struct head_meta *head)
{
    int off = 0, ss, j = i, c, n, sz;

    /* Insist we really were passed the head of a chain. */
    assert(state->prev[i] == -1 && state->next[i] != -1);

    head->i = i;
    head->sz = dsf_size(state->dsf, i);
    head->why = NULL;

    /* Search through this chain looking for real numbers, checking that
     * they match up (if there are more than one). */
    head->preference = 0;
    while (j != -1) {
        if (state->flags[j] & FLAG_IMMUTABLE) {
            ss = state->nums[j] - off;
            if (!head->preference) {
                head->start = ss;
                head->preference = 1;
                head->why = "contains cell with immutable number";
            } else if (head->start != ss) {
                debug(("head_number: chain with non-sequential numbers!"));
                state->impossible = true;
            }
        }
        off++;
        j = state->next[j];
        assert(j != i); /* we have created a loop, obviously wrong */
    }
    if (head->preference) goto done;

    if (state->nums[i] == 0 && state->nums[state->next[i]] > state->n) {
        /* (probably) empty cell onto the head of a coloured region:
         * make sure we start at a 0 offset. */
        head->start = START(COLOUR(state->nums[state->next[i]]));
        head->preference = 1;
        head->why = "adding blank cell to head of numbered region";
    } else if (state->nums[i] <= state->n) {
        /* if we're 0 we're probably just blank -- but even if we're a
         * (real) numbered region, we don't have an immutable number
         * in it (any more) otherwise it'd have been caught above, so
         * reassign the colour. */
        head->start = 0;
        head->preference = 0;
        head->why = "lowest available colour group";
    } else {
        c = COLOUR(state->nums[i]);
        n = 1;
        sz = dsf_size(state->dsf, i);
        j = i;
        while (state->next[j] != -1) {
            j = state->next[j];
            if (state->nums[j] == 0 && state->next[j] == -1) {
                head->start = START(c);
                head->preference = 1;
                head->why = "adding blank cell to end of numbered region";
                goto done;
            }
            if (COLOUR(state->nums[j]) == c)
                n++;
            else {
                int start_alternate = START(COLOUR(state->nums[j]));
                if (n < (sz - n)) {
                    head->start = start_alternate;
                    head->preference = 1;
                    head->why = "joining two coloured regions, swapping to larger colour";
                } else {
                    head->start = START(c);
                    head->preference = 1;
                    head->why = "joining two coloured regions, taking largest";
                }
                goto done;
            }
        }
        /* If we got here then we may have split a region into
         * two; make sure we don't assign a colour we've already used. */
        if (c == 0) {
            /* not convinced this shouldn't be an assertion failure here. */
            head->start = 0;
            head->preference = 0;
        } else {
            head->start = START(c);
            head->preference = 1;
        }
        head->why = "got to end of coloured region";
    }

done:
    assert(head->why != NULL);
    if (head->preference)
        debug(("Chain at (%d,%d) numbered for preference at %d (colour %d): %s.",
               head->i%state->w, head->i/state->w,
               head->start, COLOUR(head->start), head->why));
    else
        debug(("Chain at (%d,%d) using next available colour: %s.",
               head->i%state->w, head->i/state->w,
               head->why));
}

#if 0
static void debug_numbers(game_state *state)
{
    int i, w=state->w;

    for (i = 0; i < state->n; i++) {
        debug(("(%d,%d) --> (%d,%d) --> (%d,%d)",
               state->prev[i]==-1 ? -1 : state->prev[i]%w,
               state->prev[i]==-1 ? -1 : state->prev[i]/w,
               i%w, i/w,
               state->next[i]==-1 ? -1 : state->next[i]%w,
               state->next[i]==-1 ? -1 : state->next[i]/w));
    }
    w = w+1;
}
#endif

static void connect_numbers(game_state *state)
{
    int i, di, dni;

    dsf_reinit(state->dsf);
    for (i = 0; i < state->n; i++) {
        if (state->next[i] != -1) {
            assert(state->prev[state->next[i]] == i);
            di = dsf_canonify(state->dsf, i);
            dni = dsf_canonify(state->dsf, state->next[i]);
            if (di == dni) {
                debug(("connect_numbers: chain forms a loop."));
                state->impossible = true;
            }
            dsf_merge(state->dsf, di, dni);
        }
    }
}

static int compare_heads(const void *a, const void *b)
{
    const struct head_meta *ha = (const struct head_meta *)a;
    const struct head_meta *hb = (const struct head_meta *)b;

    /* Heads with preferred colours first... */
    if (ha->preference && !hb->preference) return -1;
    if (hb->preference && !ha->preference) return 1;

    /* ...then heads with low colours first... */
    if (ha->start < hb->start) return -1;
    if (ha->start > hb->start) return 1;

    /* ... then large regions first... */
    if (ha->sz > hb->sz) return -1;
    if (ha->sz < hb->sz) return 1;

    /* ... then position. */
    if (ha->i > hb->i) return -1;
    if (ha->i < hb->i) return 1;

    return 0;
}

static int lowest_start(game_state *state, struct head_meta *heads, int nheads)
{
    int n, c;

    /* NB start at 1: colour 0 is real numbers */
    for (c = 1; c < state->n; c++) {
        for (n = 0; n < nheads; n++) {
            if (COLOUR(heads[n].start) == c)
                goto used;
        }
        return c;
used:
        ;
    }
    assert(!"No available colours!");
    return 0;
}

static void update_numbers(game_state *state)
{
    int i, j, n, nnum, nheads;
    struct head_meta *heads = snewn(state->n, struct head_meta);

    for (n = 0; n < state->n; n++)
        state->numsi[n] = -1;

    for (i = 0; i < state->n; i++) {
        if (state->flags[i] & FLAG_IMMUTABLE) {
            assert(state->nums[i] > 0);
            assert(state->nums[i] <= state->n);
            state->numsi[state->nums[i]] = i;
        }
        else if (state->prev[i] == -1 && state->next[i] == -1)
            state->nums[i] = 0;
    }
    connect_numbers(state);

    /* Construct an array of the heads of all current regions, together
     * with their preferred colours. */
    nheads = 0;
    for (i = 0; i < state->n; i++) {
        /* Look for a cell that is the start of a chain
         * (has a next but no prev). */
        if (state->prev[i] != -1 || state->next[i] == -1) continue;

        head_number(state, i, &heads[nheads++]);
    }

    /* Sort that array:
     * - heads with preferred colours first, then
     * - heads with low colours first, then
     * - large regions first
     */
    qsort(heads, nheads, sizeof(struct head_meta), compare_heads);

    /* Remove duplicate-coloured regions. */
    for (n = nheads-1; n >= 0; n--) { /* order is important! */
        if ((n != 0) && (heads[n].start == heads[n-1].start)) {
            /* We have a duplicate-coloured region: since we're
             * sorted in size order and this is not the first
             * of its colour it's not the largest: recolour it. */
            heads[n].start = START(lowest_start(state, heads, nheads));
            heads[n].preference = -1; /* '-1' means 'was duplicate' */
        }
        else if (!heads[n].preference) {
            assert(heads[n].start == 0);
            heads[n].start = START(lowest_start(state, heads, nheads));
        }
    }

    debug(("Region colouring after duplicate removal:"));

    for (n = 0; n < nheads; n++) {
        debug(("  Chain at (%d,%d) sz %d numbered at %d (colour %d): %s%s",
               heads[n].i % state->w, heads[n].i / state->w, heads[n].sz,
               heads[n].start, COLOUR(heads[n].start), heads[n].why,
               heads[n].preference == 0 ? " (next available)" :
               heads[n].preference < 0 ? " (duplicate, next available)" : ""));

        nnum = heads[n].start;
        j = heads[n].i;
        while (j != -1) {
            if (!(state->flags[j] & FLAG_IMMUTABLE)) {
                if (nnum > 0 && nnum <= state->n)
                    state->numsi[nnum] = j;
                state->nums[j] = nnum;
            }
            nnum++;
            j = state->next[j];
            assert(j != heads[n].i); /* loop?! */
        }
    }
    /*debug_numbers(state);*/
    sfree(heads);
}

static bool check_completion(game_state *state, bool mark_errors)
{
    int n, j, k;
    bool error = false, complete;

    /* NB This only marks errors that are possible to perpetrate with
     * the current UI in interpret_move. Things like forming loops in
     * linked sections and having numbers not add up should be forbidden
     * by the code elsewhere, so we don't bother marking those (because
     * it would add lots of tricky drawing code for very little gain). */
    if (mark_errors) {
        for (j = 0; j < state->n; j++)
            state->flags[j] &= ~FLAG_ERROR;
    }

    /* Search for repeated numbers. */
    for (j = 0; j < state->n; j++) {
        if (state->nums[j] > 0 && state->nums[j] <= state->n) {
            for (k = j+1; k < state->n; k++) {
                if (state->nums[k] == state->nums[j]) {
                    if (mark_errors) {
                        state->flags[j] |= FLAG_ERROR;
                        state->flags[k] |= FLAG_ERROR;
                    }
                    error = true;
                }
            }
        }
    }

    /* Search and mark numbers n not pointing to n+1; if any numbers
     * are missing we know we've not completed. */
    complete = true;
    for (n = 1; n < state->n; n++) {
        if (state->numsi[n] == -1 || state->numsi[n+1] == -1)
            complete = false;
        else if (!ispointingi(state, state->numsi[n], state->numsi[n+1])) {
            if (mark_errors) {
                state->flags[state->numsi[n]] |= FLAG_ERROR;
                state->flags[state->numsi[n+1]] |= FLAG_ERROR;
            }
            error = true;
        } else {
            /* make sure the link is explicitly made here; for instance, this
             * is nice if the user drags from 2 out (making 3) and a 4 is also
             * visible; this ensures that the link from 3 to 4 is also made. */
            if (mark_errors)
                makelink(state, state->numsi[n], state->numsi[n+1]);
        }
    }

    /* Search and mark numbers less than 0, or 0 with links. */
    for (n = 1; n < state->n; n++) {
        if ((state->nums[n] < 0) ||
            (state->nums[n] == 0 &&
             (state->next[n] != -1 || state->prev[n] != -1))) {
            error = true;
            if (mark_errors)
                state->flags[n] |= FLAG_ERROR;
        }
    }

    if (error) return false;
    return complete;
}
static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
    game_state *state = NULL;

    unpick_desc(params, desc, &state, NULL);
    if (!state) assert(!"new_game failed to unpick");

    update_numbers(state);
    check_completion(state, true); /* update any auto-links */

    return state;
}

/* --- Solver --- */

/* If a tile has a single tile it can link _to_, or there's only a single
 * location that can link to a given tile, fill that link in. */
static int solve_single(game_state *state, game_state *copy, int *from)
{
    int i, j, sx, sy, x, y, d, poss, w=state->w, nlinks = 0;

    /* The from array is a list of 'which square can link _to_ us';
     * we start off with from as '-1' (meaning 'not found'); if we find
     * something that can link to us it is set to that index, and then if
     * we find another we set it to -2. */

    memset(from, -1, state->n*sizeof(int));

    /* poss is 'can I link to anything' with the same meanings. */

    for (i = 0; i < state->n; i++) {
        if (state->next[i] != -1) continue;
        if (state->nums[i] == state->n) continue; /* no next from last no. */

        d = state->dirs[i];
        poss = -1;
        sx = x = i%w; sy = y = i/w;
        while (1) {
            x += dxs[d]; y += dys[d];
            if (!INGRID(state, x, y)) break;
            if (!isvalidmove(state, true, sx, sy, x, y)) continue;

            /* can't link to somewhere with a back-link we would have to
             * break (the solver just doesn't work like this). */
            j = y*w+x;
            if (state->prev[j] != -1) continue;

            if (state->nums[i] > 0 && state->nums[j] > 0 &&
                state->nums[i] <= state->n && state->nums[j] <= state->n &&
                state->nums[j] == state->nums[i]+1) {
                debug(("Solver: forcing link through existing consecutive numbers."));
                poss = j;
                from[j] = i;
                break;
            }

            /* if there's been a valid move already, we have to move on;
             * we can't make any deductions here. */
            poss = (poss == -1) ? j : -2;

            /* Modify the from array as described above (which is enumerating
             * what points to 'j' in a similar way). */
            from[j] = (from[j] == -1) ? i : -2;
        }
        if (poss == -2) {
            /*debug(("Solver: (%d,%d) has multiple possible next squares.", sx, sy));*/
            ;
        } else if (poss == -1) {
            debug(("Solver: nowhere possible for (%d,%d) to link to.", sx, sy));
            copy->impossible = true;
            return -1;
        } else {
            debug(("Solver: linking (%d,%d) to only possible next (%d,%d).",
                   sx, sy, poss%w, poss/w));
            makelink(copy, i, poss);
            nlinks++;
        }
    }

    for (i = 0; i < state->n; i++) {
        if (state->prev[i] != -1) continue;
        if (state->nums[i] == 1) continue; /* no prev from 1st no. */

        x = i%w; y = i/w;
        if (from[i] == -1) {
            debug(("Solver: nowhere possible to link to (%d,%d)", x, y));
            copy->impossible = true;
            return -1;
        } else if (from[i] == -2) {
            /*debug(("Solver: (%d,%d) has multiple possible prev squares.", x, y));*/
            ;
        } else {
            debug(("Solver: linking only possible prev (%d,%d) to (%d,%d).",
                   from[i]%w, from[i]/w, x, y));
            makelink(copy, from[i], i);
            nlinks++;
        }
    }

    return nlinks;
}

/* Returns 1 if we managed to solve it, 0 otherwise. */
static int solve_state(game_state *state)
{
    game_state *copy = dup_game(state);
    int *scratch = snewn(state->n, int), ret;

    debug_state("Before solver: ", state);

    while (1) {
        update_numbers(state);

        if (solve_single(state, copy, scratch)) {
            dup_game_to(state, copy);
            if (state->impossible) break; else continue;
        }
        break;
    }
    free_game(copy);
    sfree(scratch);

    update_numbers(state);
    ret = state->impossible ? -1 : check_completion(state, false);
    debug(("Solver finished: %s",
           ret < 0 ? "impossible" : ret > 0 ? "solved" : "not solved"));
    debug_state("After solver: ", state);
    return ret;
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
    game_state *tosolve;
    char *ret = NULL;
    int result;

    tosolve = dup_game(currstate);
    result = solve_state(tosolve);
    if (result > 0)
        ret = generate_desc(tosolve, true);
    free_game(tosolve);
    if (ret) return ret;

    tosolve = dup_game(state);
    result = solve_state(tosolve);
    if (result < 0)
        *error = "Puzzle is impossible.";
    else if (result == 0)
        *error = "Unable to solve puzzle.";
    else
        ret = generate_desc(tosolve, true);

    free_game(tosolve);

    return ret;
}

/* --- UI and move routines. --- */


static void unlink_cell(game_state *state, int si)
{
    debug(("Unlinking (%d,%d).", si%state->w, si/state->w));
    if (state->prev[si] != -1) {
        debug((" ... removing prev link from (%d,%d).",
               state->prev[si]%state->w, state->prev[si]/state->w));
        state->next[state->prev[si]] = -1;
        state->prev[si] = -1;
    }
    if (state->next[si] != -1) {
        debug((" ... removing next link to (%d,%d).",
               state->next[si]%state->w, state->next[si]/state->w));
        state->prev[state->next[si]] = -1;
        state->next[si] = -1;
    }
}

static game_state *execute_move(const game_state *state, const char *move)
{
    game_state *ret = NULL;
    int sx, sy, ex, ey, si, ei, w = state->w;
    char c;

    debug(("move: %s", move));

    if (move[0] == 'S') {
        game_params p;
	game_state *tmp;
        const char *valid;
	int i;

        p.w = state->w; p.h = state->h;
        valid = validate_desc(&p, move+1);
        if (valid) {
            debug(("execute_move: move not valid: %s", valid));
            return NULL;
        }
	ret = dup_game(state);
        tmp = new_game(NULL, &p, move+1);
	for (i = 0; i < state->n; i++) {
	    ret->prev[i] = tmp->prev[i];
	    ret->next[i] = tmp->next[i];
	}
	free_game(tmp);
        ret->used_solve = true;
    } else if (sscanf(move, "L%d,%d-%d,%d", &sx, &sy, &ex, &ey) == 4) {
        if (!isvalidmove(state, false, sx, sy, ex, ey)) return NULL;

        ret = dup_game(state);

        si = sy*w+sx; ei = ey*w+ex;
        makelink(ret, si, ei);
    } else if (sscanf(move, "%c%d,%d", &c, &sx, &sy) == 3) {
        int sset;

        if (c != 'C' && c != 'X') return NULL;
        if (!INGRID(state, sx, sy)) return NULL;
        si = sy*w+sx;
        if (state->prev[si] == -1 && state->next[si] == -1)
            return NULL;

        ret = dup_game(state);

        sset = state->nums[si] / (state->n+1);
        if (c == 'C' || (c == 'X' && sset == 0)) {
            /* Unlink the single cell we dragged from the board. */
            unlink_cell(ret, si);
        } else {
            int i, set;
            for (i = 0; i < state->n; i++) {
                /* Unlink all cells in the same set as the one we dragged
                 * from the board. */

                if (state->nums[i] == 0) continue;
                set = state->nums[i] / (state->n+1);
                if (set != sset) continue;

                unlink_cell(ret, i);
            }
        }
    } else if (strcmp(move, "H") == 0) {
        ret = dup_game(state);
        solve_state(ret);
    }
    if (ret) {
        update_numbers(ret);
        if (check_completion(ret, true)) ret->completed = true;
    }

    return ret;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

