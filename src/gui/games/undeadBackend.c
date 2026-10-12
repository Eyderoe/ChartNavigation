#include "undeadBackend.h"
#include "simon/undeadCore.inc"
#include "simon/inputBridge.inc"
struct UndeadBackend {
    game_state *state;
    char *aux;
};
struct UndeadInput {
    game_ui *ui;
};
static game_params to_params(UndeadConfig c) {
    game_params p = {c.width, c.height, c.difficulty};
    return p;
}
static UndeadBackend *wrap(game_state *state, const char *aux) {
    if (!state)
        return NULL;
    UndeadBackend *b = snew(UndeadBackend);
    b->state = state;
    b->aux = aux ? dupstr(aux) : NULL;
    return b;
}
int undead_preset_count(void) { return lenof(undead_presets); }
UndeadConfig undead_preset(int index) {
    if (index < 0 || index >= undead_preset_count())
        index = 0;
    game_params p = undead_presets[index];
    UndeadConfig c = {p.w, p.h, p.diff};
    return c;
}
const char *undead_validate(UndeadConfig c) {
    game_params p = to_params(c);
    if (c.difficulty < 0 || c.difficulty >= DIFFCOUNT)
        return "Unknown difficulty rating";
    return validate_params(&p, true);
}
UndeadBackend *undead_new(UndeadConfig c, const char *seed) {
    if (undead_validate(c))
        return NULL;
    game_params p = to_params(c);
    random_state *rs = random_new(seed, strlen(seed));
    char *aux = NULL, *desc = new_game_desc(&p, rs, &aux, true);
    UndeadBackend *b = wrap(new_game(NULL, &p, desc), aux);
    sfree(desc);
    sfree(aux);
    random_free(rs);
    return b;
}
UndeadBackend *undead_copy(const UndeadBackend *b) { return b ? wrap(dup_game(b->state), b->aux) : NULL; }
void undead_free(UndeadBackend *b) {
    if (b) {
        free_game(b->state);
        sfree(b->aux);
        sfree(b);
    }
}
UndeadBackend *undead_solve(const UndeadBackend *initial, const UndeadBackend *b) {
    if (!initial || !b)
        return NULL;
    const char *error = NULL;
    char *move = solve_game(initial->state, b->state, initial->aux, &error);
    if (!move)
        return NULL;
    UndeadBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
UndeadInput *undead_input_new(const UndeadBackend *b) {
    UndeadInput *i = snew(UndeadInput);
    i->ui = new_ui(b->state);
    return i;
}
void undead_input_free(UndeadInput *i) {
    if (i) {
        free_ui(i->ui);
        sfree(i);
    }
}
UndeadBackend *undead_input(UndeadInput *i, const UndeadBackend *b, int x, int y, int button, bool control,
                            bool shift) {
    if (!i || !b)
        return NULL;
    game_drawstate ds = {0};
    ds.tilesize = 100;
    ds.w = b->state->common->params.w;
    ds.h = b->state->common->params.h;
    ds.count_style = i->ui->count_style;
    calculate_count_layout(&ds);
    char *move = interpret_move(b->state, i->ui, &ds, x, y, upstream_button(button, control, shift));
    if (!real_move(move))
        return NULL;
    UndeadBackend *result = wrap(execute_move(b->state, move), b->aux);
    sfree(move);
    return result;
}
const char *undead_difficulty_name(int index) {
    return index >= 0 && index < DIFFCOUNT ? undead_diffnames[index] : "";
}
static int cell_xy(const UndeadBackend *b, int cell) {
    int w = b->state->common->params.w;
    return cell % w + 1 + (cell / w + 1) * (w + 2);
}
int undead_mirror(const UndeadBackend *b, int cell) {
    int xy = cell_xy(b, cell);
    return b->state->common->xinfo[xy] < 0 ? b->state->common->grid[xy] : 0;
}
int undead_value(const UndeadBackend *b, int cell) {
    int xi = b->state->common->xinfo[cell_xy(b, cell)];
    return xi < 0 ? 0 : b->state->guess[xi];
}
int undead_pencils(const UndeadBackend *b, int cell) {
    int xi = b->state->common->xinfo[cell_xy(b, cell)];
    return xi < 0 || b->state->guess[xi] != 7 ? 0 : b->state->pencils[xi];
}
bool undead_cell_error(const UndeadBackend *b, int cell) { return b->state->cell_errors[cell_xy(b, cell)]; }
bool undead_fixed(const UndeadBackend *b, int cell) {
    int xi = b->state->common->xinfo[cell_xy(b, cell)];
    return xi >= 0 && b->state->common->fixed[xi];
}
int undead_clue(const UndeadBackend *b, int x, int y) {
    return b->state->common->grid[x + y * (b->state->common->params.w + 2)];
}
bool undead_clue_error(const UndeadBackend *b, int x, int y) {
    return b->state->hint_errors[clue_index(b->state, x, y)];
}
bool undead_clue_done(const UndeadBackend *b, int x, int y) {
    return b->state->hints_done[clue_index(b->state, x, y)];
}
int undead_total(const UndeadBackend *b, int monster) {
    return monster == 0   ? b->state->common->num_ghosts
           : monster == 1 ? b->state->common->num_vampires
                          : b->state->common->num_zombies;
}
int undead_placed(const UndeadBackend *b, int monster) {
    int count = 0;
    for (int i = 0; i < b->state->common->num_total; i++)
        if (b->state->guess[i] == (1 << monster))
            count++;
    return count;
}
bool undead_count_error(const UndeadBackend *b, int monster) { return b->state->count_errors[monster]; }
bool undead_won(const UndeadBackend *b) { return b && b->state->solved; }
bool undead_cheated(const UndeadBackend *b) { return b && b->state->cheated; }
int undead_cursor(const UndeadInput *i, int width) {
    return i && i->ui->hshow ? i->ui->hx - 1 + width * (i->ui->hy - 1) : -1;
}
bool undead_cursor_pencil(const UndeadInput *i) { return i && i->ui->hpencil; }
void undead_input_changed(UndeadInput *i, const UndeadBackend *old, const UndeadBackend *next) {
    if (i && next)
        game_changed_state(i->ui, old ? old->state : next->state, next->state);
}
void undead_preferences(UndeadInput *i, bool keep, bool ascii, int style) {
    if (i) {
        i->ui->pencil_keep_highlight = keep;
        i->ui->ascii = ascii;
        i->ui->count_style = style;
    }
}
bool undead_ascii(const UndeadInput *i) { return i && i->ui->ascii; }
int undead_count_style(const UndeadInput *i) { return i ? i->ui->count_style : 0; }
UndeadBackend *undead_monster_button(UndeadInput *i, const UndeadBackend *b, int monster, bool right) {
    if (!i || !b || monster < 0 || monster > 2)
        return NULL;
    game_drawstate storage = {0}, *ds = &storage;
    ds->tilesize = 100;
    ds->w = b->state->common->params.w;
    ds->h = b->state->common->params.h;
    ds->count_style = i->ui->count_style;
    calculate_count_layout(ds);
    return undead_input(i, b, COUNT_X(monster) + COUNT_W / 2, COUNT_Y + COUNT_H / 2,
                        right ? PuzzleRight : PuzzleLeft, false, false);
}
