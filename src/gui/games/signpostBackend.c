#include "signpostBackend.h"
#include "simon/signpostCore.inc"

struct SignpostBackend { game_state *state; };

static game_params to_params(SignpostConfig config) {
    game_params result = {config.width, config.height, config.cornerEnds};
    return result;
}

static SignpostBackend *wrap(game_state *state) {
    if (!state) return NULL;
    SignpostBackend *result = snew(SignpostBackend);
    result->state = state;
    return result;
}

int signpost_preset_count(void) { return lenof(signpost_presets); }
SignpostConfig signpost_preset(int index) {
    if (index < 0 || index >= signpost_preset_count()) index = 0;
    game_params p = signpost_presets[index];
    SignpostConfig result = {p.w, p.h, p.force_corner_start};
    return result;
}
const char *signpost_validate(SignpostConfig config) {
    game_params p = to_params(config);
    return validate_params(&p, true);
}
SignpostBackend *signpost_new(SignpostConfig config, const char *seed) {
    if (signpost_validate(config)) return NULL;
    game_params p = to_params(config);
    random_state *rs = random_new(seed, strlen(seed));
    char *aux = NULL;
    char *desc = new_game_desc(&p, rs, &aux, true);
    SignpostBackend *result = wrap(new_game(NULL, &p, desc));
    sfree(desc); sfree(aux); random_free(rs);
    return result;
}
SignpostBackend *signpost_copy(const SignpostBackend *state) {
    return state ? wrap(dup_game(state->state)) : NULL;
}
void signpost_free(SignpostBackend *state) {
    if (!state) return;
    free_game(state->state); sfree(state);
}
static bool in_range(const SignpostBackend *state, int cell) {
    return state && cell >= 0 && cell < state->state->n;
}
bool signpost_can_link(const SignpostBackend *state, int from, int to, bool clever) {
    if (!in_range(state, from) || !in_range(state, to)) return false;
    int w = state->state->w;
    return isvalidmove(state->state, clever, from%w, from/w, to%w, to/w);
}
SignpostBackend *signpost_link(const SignpostBackend *state, int from, int to) {
    if (!signpost_can_link(state, from, to, false)) return NULL;
    int w = state->state->w;
    char move[96];
    snprintf(move, sizeof(move), "L%d,%d-%d,%d", from%w, from/w, to%w, to/w);
    return wrap(execute_move(state->state, move));
}
SignpostBackend *signpost_unlink(const SignpostBackend *state, int cell, bool chain) {
    if (!in_range(state, cell)) return NULL;
    int w = state->state->w;
    char move[64];
    snprintf(move, sizeof(move), "%c%d,%d", chain ? 'X' : 'C', cell%w, cell/w);
    return wrap(execute_move(state->state, move));
}
SignpostBackend *signpost_solve(const SignpostBackend *initial, const SignpostBackend *state) {
    if (!initial || !state) return NULL;
    const char *error = NULL;
    char *move = solve_game(initial->state, state->state, NULL, &error);
    if (!move) return NULL;
    SignpostBackend *result = wrap(execute_move(state->state, move));
    sfree(move); return result;
}
int signpost_cells(const SignpostBackend *state) { return state ? state->state->n : 0; }
int signpost_number(const SignpostBackend *state, int cell) { return state->state->nums[cell]; }
void signpost_label(const SignpostBackend *state, int cell, char *output, size_t capacity) {
    int num = state->state->nums[cell], n = state->state->n;
    if (num == 0 && state->state->next[cell] == -1 && state->state->prev[cell] == -1) {
        snprintf(output, capacity, "%s", ""); return;
    }
    int set = num <= 0 ? 0 : num / (n+1);
    if (set == 0) { snprintf(output, capacity, "%d", num); return; }
    // Match upstream's base-26 chain names, including a+1 and aa+2.
    char letters[32]; int count = 0;
    do { --set; letters[count++] = 'a' + set % 26; set /= 26; } while (set);
    char name[32];
    for (int i = 0; i < count; ++i) name[i] = letters[count - i - 1];
    name[count] = '\0';
    int offset = num % (n+1);
    if (offset) snprintf(output, capacity, "%s+%d", name, offset);
    else snprintf(output, capacity, "%s", name);
}
int signpost_direction(const SignpostBackend *state, int cell) { return state->state->dirs[cell]; }
int signpost_next(const SignpostBackend *state, int cell) { return state->state->next[cell]; }
int signpost_prev(const SignpostBackend *state, int cell) { return state->state->prev[cell]; }
bool signpost_fixed(const SignpostBackend *state, int cell) { return state->state->flags[cell] & FLAG_IMMUTABLE; }
bool signpost_error(const SignpostBackend *state, int cell) {
    return state->state->impossible || state->state->nums[cell] < 0 ||
           (state->state->flags[cell] & FLAG_ERROR);
}
bool signpost_won(const SignpostBackend *state) { return state && state->state->completed; }
bool signpost_used_solve(const SignpostBackend *state) { return state && state->state->used_solve; }
