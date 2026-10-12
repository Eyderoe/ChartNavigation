#ifndef CHARTNAVIGATION_GAMES_SIGNPOSTBACKEND_H
#define CHARTNAVIGATION_GAMES_SIGNPOSTBACKEND_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SignpostBackend SignpostBackend;
typedef struct SignpostConfig {
    int width, height;
    bool cornerEnds;
} SignpostConfig;

int signpost_preset_count(void);
SignpostConfig signpost_preset(int index);
const char *signpost_validate(SignpostConfig config);
SignpostBackend *signpost_new(SignpostConfig config, const char *seed);
SignpostBackend *signpost_copy(const SignpostBackend *state);
void signpost_free(SignpostBackend *state);
bool signpost_can_link(const SignpostBackend *state, int from, int to, bool clever);
SignpostBackend *signpost_link(const SignpostBackend *state, int from, int to);
SignpostBackend *signpost_unlink(const SignpostBackend *state, int cell, bool chain);
SignpostBackend *signpost_solve(const SignpostBackend *initial, const SignpostBackend *state);
int signpost_cells(const SignpostBackend *state);
int signpost_number(const SignpostBackend *state, int cell);
void signpost_label(const SignpostBackend *state, int cell, char *output, size_t capacity);
int signpost_direction(const SignpostBackend *state, int cell);
int signpost_next(const SignpostBackend *state, int cell);
int signpost_prev(const SignpostBackend *state, int cell);
bool signpost_fixed(const SignpostBackend *state, int cell);
bool signpost_error(const SignpostBackend *state, int cell);
bool signpost_won(const SignpostBackend *state);
bool signpost_used_solve(const SignpostBackend *state);

#ifdef __cplusplus
}
#endif
#endif
