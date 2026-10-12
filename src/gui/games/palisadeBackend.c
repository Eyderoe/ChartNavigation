#include "palisadeBackend.h"
#include "simon/palisadeCore.inc"
#include "simon/inputBridge.inc"
struct PalisadeBackend {
    game_state *state;
    char *aux;
};
struct PalisadeInput {
    game_ui *ui;
};
static game_params to_params(PalisadeConfig c) {
    game_params p = {c.width, c.height, c.region};
    return p;
}
static PalisadeBackend *wrap(game_state *state, const char *aux) {
    if (!state)
        return NULL;
    PalisadeBackend *b = snew(PalisadeBackend);
    b->state = state;
    b->aux = aux ? dupstr(aux) : NULL;
    return b;
}
int palisade_preset_count(void) { return lenof(presets); }
PalisadeConfig palisade_preset(int index) {
    if (index < 0 || index >= palisade_preset_count())
        index = 0;
    game_params p = presets[index];
    PalisadeConfig c = {p.w, p.h, p.k};
    return c;
}
const char *palisade_validate(PalisadeConfig c) {
    game_params p = to_params(c);
    return validate_params(&p, true);
}
PalisadeBackend *palisade_new(PalisadeConfig c, const char *seed) {
    if (palisade_validate(c))
        return NULL;
    game_params p = to_params(c);
    random_state *rs = random_new(seed, strlen(seed));
    char *aux = NULL, *desc = new_game_desc(&p, rs, &aux, true);
    PalisadeBackend *b = wrap(new_game(NULL, &p, desc), aux);
    sfree(desc);
    sfree(aux);
    random_free(rs);
    return b;
}
PalisadeBackend *palisade_copy(const PalisadeBackend *b) {
    return b ? wrap(dup_game(b->state), b->aux) : NULL;
}
void palisade_free(PalisadeBackend *b) {
    if (b) {
        free_game(b->state);
        sfree(b->aux);
        sfree(b);
    }
}
PalisadeBackend *palisade_solve(const PalisadeBackend *initial, const PalisadeBackend *b) {
    if (!initial || !b)
        return NULL;
    const char *error = NULL;
    char *move = solve_game(initial->state, b->state, initial->aux, &error);
    if (!move)
        return NULL;
    PalisadeBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
PalisadeInput *palisade_input_new(const PalisadeBackend *b) {
    PalisadeInput *i = snew(PalisadeInput);
    i->ui = new_ui(b->state);
    return i;
}
void palisade_input_free(PalisadeInput *i) {
    if (i) {
        free_ui(i->ui);
        sfree(i);
    }
}
PalisadeBackend *palisade_input(PalisadeInput *i, const PalisadeBackend *b, int x, int y, int button,
                                bool control, bool shift) {
    if (!i || !b)
        return NULL;
    game_drawstate ds = {0};
    ds.tilesize = 100;
    char *move = interpret_move(b->state, i->ui, &ds, x, y, upstream_button(button, control, shift));
    if (!real_move(move))
        return NULL;
    PalisadeBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
void palisade_input_mode(PalisadeInput *i, bool full, bool clear) {
    if (i) {
        i->ui->legacy_cursor = full;
        i->ui->clear_complete_regions = clear;
    }
}
bool palisade_input_visible(const PalisadeInput *i) { return i && i->ui->show; }
void palisade_cursor(const PalisadeInput *i, int *x, int *y) {
    *x = i ? i->ui->x : 1;
    *y = i ? i->ui->y : 1;
}
int palisade_clue(const PalisadeBackend *b, int cell) { return b->state->shared->clues[cell]; }
int palisade_border(const PalisadeBackend *b, int cell) { return b->state->borders[cell]; }
bool palisade_won(const PalisadeBackend *b) { return b && b->state->completed; }
bool palisade_cheated(const PalisadeBackend *b) { return b && b->state->cheated; }
#define BORDER_ERROR(x) ((x) << 8)
#define F_ERROR_U BORDER_ERROR(BORDER_U) /* BIT( 8) */
#define F_ERROR_R BORDER_ERROR(BORDER_R) /* BIT( 9) */
#define F_ERROR_D BORDER_ERROR(BORDER_D) /* BIT(10) */
#define F_ERROR_L BORDER_ERROR(BORDER_L) /* BIT(11) */
#define F_ERROR_CLUE BIT(12)
#define F_FLASH BIT(13)
#define CONTAINS_CURSOR(x) ((x) << 14)

void palisade_display(const PalisadeBackend *b, const PalisadeInput *input, int *output) {
    const game_state *state = b->state;
    const game_ui *ui = input->ui;
    int w = state->shared->params.w, h = state->shared->params.h, wh = w * h, k = state->shared->params.k;
    int r, c, flash = 0;
    DSF *black_border_dsf = dsf_new(wh), *yellow_border_dsf = dsf_new(wh);
    build_dsf(w, h, state->borders, black_border_dsf, true);
    build_dsf(w, h, state->borders, yellow_border_dsf, false);

    for (r = 0; r < h; ++r)
        for (c = 0; c < w; ++c) {
            int i = r * w + c, clue = state->shared->clues[i], flags, dir;
            bool force_uncertain_edges_off =
                (ui->clear_complete_regions && dsf_size(black_border_dsf, i) == k);
            int on = bitcount[state->borders[i] & BORDER_MASK];
            int off = force_uncertain_edges_off ? 4 - on : bitcount[(state->borders[i] >> 4) & BORDER_MASK];

            flags = state->borders[i];

            if (flash)
                flags |= F_FLASH;

            if (clue != EMPTY && (on > clue || clue > 4 - off))
                flags |= F_ERROR_CLUE;

            if (ui->show) {
                int u, v;
                for (u = 0; u < 3; u++)
                    for (v = 0; v < 3; v++)
                        if (ui->x == 2 * c + u && ui->y == 2 * r + v)
                            flags |= CONTAINS_CURSOR(BIT(3 * u + v));
            }

            /* border errors */
            for (dir = 0; dir < 4; ++dir) {
                int rr = r + dy[dir], cc = c + dx[dir], ii = rr * w + cc;

                if (OUT_OF_BOUNDS(cc, rr, w, h))
                    continue;

                /* we draw each border twice, except the outermost
                 * big border, so we have to check for errors on
                 * both sides of each border.*/
                if (/* region too large */
                    ((dsf_size(yellow_border_dsf, i) > k || dsf_size(yellow_border_dsf, ii) > k) &&
                     (dsf_canonify(yellow_border_dsf, i) != dsf_canonify(yellow_border_dsf, ii)))

                    ||
                    /* region too small */
                    ((dsf_size(black_border_dsf, i) < k || dsf_size(black_border_dsf, ii) < k) &&
                     dsf_canonify(black_border_dsf, i) != dsf_canonify(black_border_dsf, ii))

                    ||
                    /* dangling borders within a single region */
                    ((state->borders[i] & BORDER(dir)) &&
                     /* we know it's a single region because there's a
                      * path crossing no border from i to ii... */
                     (dsf_canonify(yellow_border_dsf, i) == dsf_canonify(yellow_border_dsf, ii) ||
                      /* or because any such border would be an error */
                      (dsf_size(black_border_dsf, i) <= k &&
                       dsf_canonify(black_border_dsf, i) == dsf_canonify(black_border_dsf, ii)))))

                    flags |= BORDER_ERROR(BORDER(dir));
            }

            if (force_uncertain_edges_off)
                flags |= BORDER_MASK << 4;

            output[i] = flags;
        }

    dsf_free(black_border_dsf);
    dsf_free(yellow_border_dsf);
}
