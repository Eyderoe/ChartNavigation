#include "minesBackend.h"
#include <stdarg.h>
#include "simon/minesCore.inc"

struct MinesBackend { game_state *state; };

// The Qt controller owns snapshots and restart history instead of a midend.
void midend_supersede_game_desc(midend *me, const char *desc, const char *privdesc) {
    (void)me;
    (void)privdesc;
    sfree((void *)desc);
}

void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    abort();
}

static game_params to_params(MinesConfig config) {
    game_params params = {config.width, config.height, config.proportion,
                          config.mines, config.type, config.soluble, -1};
    return params;
}

static MinesConfig from_params(game_params params) {
    MinesConfig config = {params.w, params.h, params.type, params.nmines_is_proportion,
                           params.nmines, params.unique};
    return config;
}

static MinesBackend *wrap(game_state *state) {
    if (!state) return NULL;
    MinesBackend *result = snew(MinesBackend);
    result->state = state;
    return result;
}

int mines_preset_count(void) {
    return lenof(mines_presets_top) + lenof(mines_presets_more_small) + lenof(mines_presets_more_big);
}

MinesConfig mines_preset(int index) {
    if (index < 0 || index >= mines_preset_count()) index = 0;
    if (index < (int)lenof(mines_presets_top)) return from_params(mines_presets_top[index]);
    index -= lenof(mines_presets_top);
    if (index < (int)lenof(mines_presets_more_small)) return from_params(mines_presets_more_small[index]);
    return from_params(mines_presets_more_big[index - lenof(mines_presets_more_small)]);
}

const char *mines_preset_group(int index) {
    if (index < (int)lenof(mines_presets_top)) return "";
    if (index < (int)(lenof(mines_presets_top) + lenof(mines_presets_more_small))) return "More (small)";
    return "More";
}

int mines_grid_count(void) { return NUM_GRID_TYPES; }
const char *mines_grid_name(int type) { return type >= 0 && type < (int)NUM_GRID_TYPES ? gridnames[type] : ""; }

void mines_count_text(MinesConfig config, char *buffer, size_t size) {
    if (config.proportion) {
        mines_prop_buf buf;
        snprintf(buffer, size, "%s%%", format_mines_proportion(config.mines, &buf));
    } else {
        snprintf(buffer, size, "%d", config.mines);
    }
}

bool mines_parse_count(MinesConfig *config, const char *text) {
    const char *end = text + strlen(text);
    const char *percent = strchr(text, '%');
    const char *limit = percent ? percent : end;
    if (text == limit || (percent && percent != end - 1)) return false;
    bool decimal = false;
    for (const char *p = text; p < limit; p++) {
        if (*p == '.' && percent && !decimal) decimal = true;
        else if (!isdigit((unsigned char)*p)) return false;
    }
    if (percent) {
        // Validate the magnitude before the upstream fixed-point parser.
        double value = strtod(text, NULL);
        if (value <= 0 || value > 100) return false;
        config->mines = parse_mines_proportion(text, percent);
    } else {
        char *tail;
        long value = strtol(text, &tail, 10);
        if (*tail || value <= 0 || value > INT_MAX) return false;
        config->mines = value;
    }
    config->proportion = percent != NULL;
    return true;
}

const char *mines_validate(MinesConfig config) {
    game_params params = to_params(config);
    return validate_params(&params, true);
}

MinesBackend *mines_new(MinesConfig config, const char *seed) {
    if (mines_validate(config)) return NULL;
    game_params params = to_params(config);
    random_state *random = random_new(seed, strlen(seed));
    char *description = new_game_desc(&params, random, NULL, true);
    MinesBackend *result = wrap(new_game(NULL, &params, description));
    random_free(random);
    sfree(description);
    return result;
}

MinesBackend *mines_copy(const MinesBackend *state) { return wrap(dup_game(state->state)); }

MinesBackend *mines_rebase(const MinesBackend *old, const MinesBackend *generated) {
    game_state *result = dup_game(generated->state);
    assert(old->state->grid->ntiles == result->grid->ntiles);
    memcpy(result->board, old->state->board, result->grid->ntiles);
    result->dead = old->state->dead;
    result->won = old->state->won;
    result->used_solve = old->state->used_solve;
    return wrap(result);
}

MinesBackend *mines_first_open(MinesConfig config, const char *seed, const int *board, int cell) {
    MinesBackend *before = mines_new(config, seed);
    if (!before) return NULL;
    for (int i = 0; i < before->state->grid->ntiles; i++) before->state->board[i] = board[i];
    MinesBackend *after = mines_action(before, cell, false);
    mines_free(before);
    return after;
}

static void append_command(char **end, game_state *state, const char *command, int cell) {
    if (state->grid->type == MINES_GRID_SQUARE)
        *end += sprintf(*end, "%s%d,%d;", command, cell % state->w, cell / state->w);
    else
        *end += sprintf(*end, "%s%d;", command, cell);
}

MinesBackend *mines_action(const MinesBackend *backend, int cell, bool flag) {
    game_state *state = backend->state;
    if (cell < 0 || cell >= state->grid->ntiles || state->dead || state->won || state->used_solve) return NULL;
    char command[1024], *end = command;
    if (flag) {
        if (state->board[cell] != -2 && state->board[cell] != -1) return NULL;
        append_command(&end, state, "F", cell);
    } else if (state->board[cell] == -2) {
        append_command(&end, state, "O", cell);
    } else if (state->board[cell] > 0 && state->board[cell] <= MAX_NEIGHBOURS) {
        int flags = 0, covered = 0;
        for (int j = 0; j < state->grid->nsize; j++) {
            int next = state->grid->neighbours[cell * state->grid->nsize + j];
            if (next >= 0) {
                flags += state->board[next] == -1;
                covered += state->board[next] == -2;
            }
        }
        if (flags != state->board[cell] || !covered) return NULL;
        // As in interpret_move: on an incorrect chord reveal only hit mines.
        for (int j = 0; j < state->grid->nsize; j++) {
            int next = state->grid->neighbours[cell * state->grid->nsize + j];
            if (next >= 0 && state->board[next] != -1 && state->layout->mines[next])
                append_command(&end, state, "O", next);
        }
        if (end == command) append_command(&end, state, "C", cell);
    } else {
        return NULL;
    }
    *end = '\0';
    return wrap(execute_move(state, command));
}

MinesBackend *mines_solve(const MinesBackend *state) { return wrap(execute_move(state->state, "S")); }
void mines_free(MinesBackend *state) { if (state) { free_game(state->state); sfree(state); } }
int mines_cells(const MinesBackend *state) { return state->state->grid->ntiles; }
int mines_cell(const MinesBackend *state, int cell) { return state->state->board[cell]; }
bool mines_generated(const MinesBackend *state) { return state->state->layout->mines != NULL; }
bool mines_dead(const MinesBackend *state) { return state->state->dead; }
bool mines_won(const MinesBackend *state) { return state->state->won; }
bool mines_used_solve(const MinesBackend *state) { return state->state->used_solve; }
int mines_nominal_count(const MinesBackend *state) { return state->state->layout->nmines; }
int mines_start_cell(const MinesBackend *state) { return state->state->layout->start_tile; }

int mines_neighbours(const MinesBackend *backend, int cell, int *output, int capacity) {
    struct grid_info *grid = backend->state->grid;
    int count = 0;
    for (int i = 0; i < grid->nsize; i++) {
        int next = grid->neighbours[cell * grid->nsize + i];
        if (next >= 0 && count < capacity) output[count++] = next;
    }
    return count;
}

int mines_cursor(const MinesBackend *backend, int cell, int direction) {
    struct grid_info *grid = backend->state->grid;
    int *cursor = direction == 0 ? grid->cursor_up : direction == 1 ? grid->cursor_right
                   : direction == 2 ? grid->cursor_down : grid->cursor_left;
    return cursor ? cursor[cell] : -1;
}

int mines_corners(const MinesBackend *state, int cell) { return state->state->grid->game_grid->faces[cell]->order; }
void mines_corner(const MinesBackend *state, int cell, int corner, double *x, double *y) {
    grid_dot *dot = state->state->grid->game_grid->faces[cell]->dots[corner];
    *x = dot->x; *y = dot->y;
}
void mines_centre(const MinesBackend *state, int cell, double *x, double *y, double *radius) {
    grid_face *face = state->state->grid->game_grid->faces[cell];
    *x = face->ix; *y = face->iy; *radius = face->iradius;
}
