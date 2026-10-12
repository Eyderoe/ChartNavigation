#include "soloBackend.h"
#include <stdatomic.h>
#include "simon/soloCore.inc"
#include "simon/inputBridge.inc"

struct SoloBackend {
    game_state *state;
    char *aux;
};
struct SoloInput {
    game_ui *ui;
};
static game_params to_params(SoloConfig c) {
    game_params p = {c.columns, c.rows, c.symmetry, c.difficulty, c.killerDifficulty, c.diagonal, c.killer};
    return p;
}
static SoloConfig from_params(game_params p) {
    SoloConfig c = {p.c, p.r, p.symm, p.diff, p.kdiff, p.xtype, p.killer};
    return c;
}
static SoloBackend *wrap(game_state *state, const char *aux) {
    if (!state)
        return NULL;
    SoloBackend *b = snew(SoloBackend);
    b->state = state;
    b->aux = aux ? dupstr(aux) : NULL;
    return b;
}
SoloConfig solo_default(void) {
    game_params *p = default_params();
    SoloConfig c = from_params(*p);
    free_params(p);
    return c;
}
int solo_preset_count(void) {
    int i = 0;
    game_params *p;
    char *name;
    while (game_fetch_preset(i, &name, &p)) {
        sfree(name);
        free_params(p);
        i++;
    }
    return i;
}
SoloConfig solo_preset(int index) {
    game_params *p;
    char *name;
    if (!game_fetch_preset(index, &name, &p))
        return solo_default();
    SoloConfig c = from_params(*p);
    sfree(name);
    free_params(p);
    return c;
}
void solo_preset_name(int index, char *out, size_t size) {
    game_params *p;
    char *name;
    if (!game_fetch_preset(index, &name, &p)) {
        snprintf(out, size, "%s", "");
        return;
    }
    snprintf(out, size, "%s", name);
    sfree(name);
    free_params(p);
}
void solo_choices(bool difficulty, char *out, size_t size) {
    game_params *p = default_params();
    config_item *c = game_configure(p);
    snprintf(out, size, "%s", c[difficulty ? 6 : 5].u.choices.choicenames);
    free_cfg(c);
    free_params(p);
}
const char *solo_validate(SoloConfig c) {
    if (c.rows < 1)
        return "Rows must be at least one";
    if (c.symmetry < 0 || c.symmetry > SYMM_REF8 || c.difficulty < 0 || c.difficulty > DIFF_RECURSIVE ||
        c.killerDifficulty < 0 || c.killerDifficulty > DIFF_KINTERSECT)
        return "Invalid difficulty or symmetry";
    game_params p = to_params(c);
    return validate_params(&p, true);
}
SoloBackend *solo_new(SoloConfig c, const char *seed) {
    if (solo_validate(c))
        return NULL;
    game_params p = to_params(c);
    random_state *rs = random_new(seed, strlen(seed));
    char *aux = NULL, *desc = new_game_desc(&p, rs, &aux, true);
    SoloBackend *b = wrap(new_game(NULL, &p, desc), aux);
    sfree(desc);
    sfree(aux);
    random_free(rs);
    return b;
}
SoloBackend *solo_copy(const SoloBackend *b) { return b ? wrap(dup_game(b->state), b->aux) : NULL; }
void solo_free(SoloBackend *b) {
    if (!b)
        return;
    free_game(b->state);
    sfree(b->aux);
    sfree(b);
}
SoloBackend *solo_solve(const SoloBackend *initial, const SoloBackend *b) {
    if (!initial || !b)
        return NULL;
    const char *error = NULL;
    precompute_sum_bits();
    char *move = solve_game(initial->state, b->state, initial->aux, &error);
    if (!move)
        return NULL;
    SoloBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
SoloBackend *solo_marks(const SoloBackend *b) { return b ? wrap(execute_move(b->state, "M"), b->aux) : NULL; }
SoloInput *solo_input_new(const SoloBackend *b) {
    SoloInput *i = snew(SoloInput);
    i->ui = new_ui(b->state);
    return i;
}
void solo_input_free(SoloInput *i) {
    if (i) {
        free_ui(i->ui);
        sfree(i);
    }
}
void solo_input_changed(SoloInput *i, const SoloBackend *b) {
    if (i && b)
        game_changed_state(i->ui, NULL, b->state);
}
SoloBackend *solo_input(SoloInput *i, const SoloBackend *b, int x, int y, int button) {
    if (!i || !b)
        return NULL;
    game_drawstate ds = {0};
    ds.tilesize = 100;
    char *move = interpret_move(b->state, i->ui, &ds, x, y, upstream_button(button, false, false));
    if (!real_move(move))
        return NULL;
    SoloBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
void solo_input_select(SoloInput *i, int cell, bool pencil, int order) {
    if (!i || cell < 0 || cell >= order * order)
        return;
    i->ui->hx = cell % order;
    i->ui->hy = cell / order;
    i->ui->hpencil = pencil;
    i->ui->hshow = true;
    i->ui->hcursor = true;
}
void solo_input_keep(SoloInput *i, bool keep) {
    if (i)
        i->ui->pencil_keep_highlight = keep;
}
int solo_cursor(const SoloInput *i, int order) { return i ? i->ui->hy * order + i->ui->hx : 0; }
bool solo_cursor_visible(const SoloInput *i) { return i && i->ui->hshow; }
bool solo_cursor_pencil(const SoloInput *i) { return i && i->ui->hpencil; }
int solo_order(const SoloBackend *b) { return b ? b->state->cr : 0; }
int solo_value(const SoloBackend *b, int cell) { return b->state->grid[cell]; }
bool solo_fixed(const SoloBackend *b, int cell) { return b->state->immutable[cell]; }
bool solo_pencil(const SoloBackend *b, int cell, int value) {
    return b->state->pencil[cell * b->state->cr + value - 1];
}
int solo_block(const SoloBackend *b, int cell) { return b->state->blocks->whichblock[cell]; }
int solo_cage(const SoloBackend *b, int cell) {
    return b->state->kblocks ? b->state->kblocks->whichblock[cell] : -1;
}
int solo_cage_sum(const SoloBackend *b, int cell) { return b->state->kgrid ? b->state->kgrid[cell] : 0; }
void solo_errors(const SoloBackend *b, bool *digits, bool *cages) {
    game_state *s = b->state;
    int n = s->cr, cr = n;
    for (int i = 0; i < n * n; i++) {
        digits[i] = cages[i] = false;
        int d = s->grid[i];
        if (!d)
            continue;
        for (int j = 0; j < n * n; j++)
            if (i != j && s->grid[j] == d &&
                (i % n == j % n || i / n == j / n || s->blocks->whichblock[i] == s->blocks->whichblock[j] ||
                 (s->xtype && ((ondiag0(i) && ondiag0(j)) || (ondiag1(i) && ondiag1(j)))) ||
                 (s->kblocks && s->kblocks->whichblock[i] == s->kblocks->whichblock[j])))
                digits[i] = true;
        if (s->kblocks)
            cages[i] = check_killer_cage_sum(s->kblocks, s->kgrid, s->grid, s->kblocks->whichblock[i]) == 0;
    }
}
bool solo_won(const SoloBackend *b) { return b && b->state->completed; }
bool solo_cheated(const SoloBackend *b) { return b && b->state->cheated; }
char solo_symbol(int value) { return value < 10 ? '0' + value : 'a' + value - 10; }
