#include "tentsBackend.h"
#include "simon/tentsCore.inc"
#include "simon/inputBridge.inc"
struct TentsBackend {
    game_state *state;
    char *aux;
};
struct TentsInput {
    game_ui *ui;
};
static game_params to_params(TentsConfig c) {
    game_params p = {c.width, c.height, c.difficulty};
    return p;
}
static TentsBackend *wrap(game_state *state, const char *aux) {
    if (!state)
        return NULL;
    TentsBackend *b = snew(TentsBackend);
    b->state = state;
    b->aux = aux ? dupstr(aux) : NULL;
    return b;
}
int tents_preset_count(void) { return lenof(tents_presets); }
TentsConfig tents_preset(int index) {
    if (index < 0 || index >= tents_preset_count())
        index = 0;
    game_params p = tents_presets[index];
    TentsConfig c = {p.w, p.h, p.diff};
    return c;
}
const char *tents_validate(TentsConfig c) {
    game_params p = to_params(c);
    if (c.difficulty < 0 || c.difficulty >= DIFFCOUNT)
        return "Unknown difficulty rating";
    return validate_params(&p, true);
}
TentsBackend *tents_new(TentsConfig c, const char *seed) {
    if (tents_validate(c))
        return NULL;
    game_params p = to_params(c);
    random_state *rs = random_new(seed, strlen(seed));
    char *aux = NULL, *desc = new_game_desc(&p, rs, &aux, true);
    TentsBackend *b = wrap(new_game(NULL, &p, desc), aux);
    sfree(desc);
    sfree(aux);
    random_free(rs);
    return b;
}
TentsBackend *tents_copy(const TentsBackend *b) { return b ? wrap(dup_game(b->state), b->aux) : NULL; }
void tents_free(TentsBackend *b) {
    if (b) {
        free_game(b->state);
        sfree(b->aux);
        sfree(b);
    }
}
TentsBackend *tents_reveal(const TentsBackend *initial, const TentsBackend *b) {
    if (!initial || !b)
        return NULL;
    const char *error = NULL;
    char *move = solve_game(initial->state, b->state, initial->aux, &error);
    if (!move)
        return NULL;
    TentsBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
TentsInput *tents_input_new(const TentsBackend *b) {
    TentsInput *i = snew(TentsInput);
    i->ui = new_ui(b->state);
    return i;
}
void tents_input_free(TentsInput *i) {
    if (i) {
        free_ui(i->ui);
        sfree(i);
    }
}
TentsBackend *tents_input(TentsInput *i, const TentsBackend *b, int x, int y, int button, bool control,
                          bool shift) {
    if (!i || !b)
        return NULL;
    game_drawstate ds = {0};
    ds.tilesize = 100;
    char *move = interpret_move(b->state, i->ui, &ds, x, y, upstream_button(button, control, shift));
    if (!real_move(move))
        return NULL;
    TentsBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
const char *tents_difficulty_name(int index) {
    return index >= 0 && index < DIFFCOUNT ? tents_diffnames[index] : "";
}
int tents_cell(const TentsBackend *b, int cell) { return b->state->grid[cell]; }
int tents_clue(const TentsBackend *b, int index) { return b->state->numbers->numbers[index]; }
bool tents_won(const TentsBackend *b) { return b && b->state->completed; }
bool tents_cheated(const TentsBackend *b) { return b && b->state->used_solve; }
int tents_cursor(const TentsInput *i, int width) {
    return i && i->ui->cdisp ? i->ui->cx + width * i->ui->cy : -1;
}
void tents_display(const TentsBackend *b, const TentsInput *i, int *values, int *errors) {
    const game_state *s = b->state;
    const game_ui *ui = i->ui;
    int area = s->p.w * s->p.h;
    char *grid = snewn(area, char);
    memcpy(grid, s->grid, area);
    if (ui->drag_button >= 0)
        grid[ui->dsy * s->p.w + ui->dsx] = drag_xform(ui, ui->dsx, ui->dsy, grid[ui->dsy * s->p.w + ui->dsx]);
    int *e = find_errors(s, grid);
    memcpy(errors, e, (area + s->p.w + s->p.h) * sizeof(int));
    for (int cell = 0; cell < area; cell++) {
        int v = s->grid[cell];
        if (ui->drag_button >= 0)
            v = drag_xform(ui, cell % s->p.w, cell / s->p.w, v);
        values[cell] = v;
    }
    sfree(e);
    sfree(grid);
}
