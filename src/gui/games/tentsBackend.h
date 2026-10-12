#ifndef CHARTNAVIGATION_GAMES_TENTSBACKEND_H
#define CHARTNAVIGATION_GAMES_TENTSBACKEND_H
#include <stdbool.h>
#include "puzzleInput.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct TentsBackend TentsBackend;
typedef struct TentsInput TentsInput;
typedef struct TentsConfig {
    int width, height, difficulty;
} TentsConfig;
int tents_preset_count(void);
TentsConfig tents_preset(int index);
const char *tents_validate(TentsConfig config);
TentsBackend *tents_new(TentsConfig config, const char *seed);
TentsBackend *tents_copy(const TentsBackend *state);
void tents_free(TentsBackend *state);
TentsBackend *tents_reveal(const TentsBackend *initial, const TentsBackend *state);
TentsInput *tents_input_new(const TentsBackend *state);
void tents_input_free(TentsInput *input);
TentsBackend *tents_input(TentsInput *input, const TentsBackend *state, int x, int y, int button,
                          bool control, bool shift);
const char *tents_difficulty_name(int index);
bool tents_won(const TentsBackend *state);
bool tents_cheated(const TentsBackend *state);
int tents_cursor(const TentsInput *input, int width);
enum TentsCell { TentsBlank, TentsTree, TentsTent, TentsGrass };
int tents_cell(const TentsBackend *state, int cell);
int tents_clue(const TentsBackend *state, int index);
void tents_display(const TentsBackend *state, const TentsInput *input, int *values, int *errors);
#ifdef __cplusplus
}
#endif
#endif
