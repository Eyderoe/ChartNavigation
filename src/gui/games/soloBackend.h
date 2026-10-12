#ifndef CHARTNAVIGATION_GAMES_SOLOBACKEND_H
#define CHARTNAVIGATION_GAMES_SOLOBACKEND_H
#include <stdbool.h>
#include <stddef.h>
#include "puzzleInput.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SoloBackend SoloBackend;
typedef struct SoloInput SoloInput;
typedef struct SoloConfig {
    int columns, rows, symmetry, difficulty, killerDifficulty;
    bool diagonal, killer;
} SoloConfig;
SoloConfig solo_default(void);
int solo_preset_count(void);
SoloConfig solo_preset(int index);
void solo_preset_name(int index, char *name, size_t capacity);
void solo_choices(bool difficulty, char *names, size_t capacity);
const char *solo_validate(SoloConfig config);
SoloBackend *solo_new(SoloConfig config, const char *seed);
SoloBackend *solo_copy(const SoloBackend *state);
void solo_free(SoloBackend *state);
SoloBackend *solo_solve(const SoloBackend *initial, const SoloBackend *state);
SoloBackend *solo_marks(const SoloBackend *state);
SoloInput *solo_input_new(const SoloBackend *state);
void solo_input_free(SoloInput *input);
void solo_input_changed(SoloInput *input, const SoloBackend *state);
SoloBackend *solo_input(SoloInput *input, const SoloBackend *state, int x, int y, int button);
void solo_input_select(SoloInput *input, int cell, bool pencil, int order);
void solo_input_keep(SoloInput *input, bool keep);
int solo_cursor(const SoloInput *input, int order);
bool solo_cursor_visible(const SoloInput *input);
bool solo_cursor_pencil(const SoloInput *input);
int solo_order(const SoloBackend *state);
int solo_value(const SoloBackend *state, int cell);
bool solo_fixed(const SoloBackend *state, int cell);
bool solo_pencil(const SoloBackend *state, int cell, int value);
int solo_block(const SoloBackend *state, int cell);
int solo_cage(const SoloBackend *state, int cell);
int solo_cage_sum(const SoloBackend *state, int cell);
void solo_errors(const SoloBackend *state, bool *digits, bool *cages);
bool solo_won(const SoloBackend *state);
bool solo_cheated(const SoloBackend *state);
char solo_symbol(int value);
#ifdef __cplusplus
}
#endif
#endif
