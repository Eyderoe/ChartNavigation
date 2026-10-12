#ifndef CHARTNAVIGATION_GAMES_UNDEADBACKEND_H
#define CHARTNAVIGATION_GAMES_UNDEADBACKEND_H
#include <stdbool.h>
#include "puzzleInput.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct UndeadBackend UndeadBackend;
typedef struct UndeadInput UndeadInput;
typedef struct UndeadConfig {
    int width, height, difficulty;
} UndeadConfig;
int undead_preset_count(void);
UndeadConfig undead_preset(int index);
const char *undead_validate(UndeadConfig config);
UndeadBackend *undead_new(UndeadConfig config, const char *seed);
UndeadBackend *undead_copy(const UndeadBackend *state);
void undead_free(UndeadBackend *state);
UndeadBackend *undead_solve(const UndeadBackend *initial, const UndeadBackend *state);
UndeadInput *undead_input_new(const UndeadBackend *state);
void undead_input_free(UndeadInput *input);
UndeadBackend *undead_input(UndeadInput *input, const UndeadBackend *state, int x, int y, int button,
                            bool control, bool shift);
const char *undead_difficulty_name(int index);
bool undead_won(const UndeadBackend *state);
bool undead_cheated(const UndeadBackend *state);
int undead_cursor(const UndeadInput *input, int width);
int undead_mirror(const UndeadBackend *state, int cell);
int undead_value(const UndeadBackend *state, int cell);
int undead_pencils(const UndeadBackend *state, int cell);
bool undead_fixed(const UndeadBackend *state, int cell);
bool undead_cell_error(const UndeadBackend *state, int cell);
int undead_clue(const UndeadBackend *state, int x, int y);
bool undead_clue_error(const UndeadBackend *state, int x, int y);
bool undead_clue_done(const UndeadBackend *state, int x, int y);
int undead_total(const UndeadBackend *state, int monster);
int undead_placed(const UndeadBackend *state, int monster);
bool undead_count_error(const UndeadBackend *state, int monster);
bool undead_cursor_pencil(const UndeadInput *input);
void undead_input_changed(UndeadInput *input, const UndeadBackend *oldstate, const UndeadBackend *newstate);
void undead_preferences(UndeadInput *input, bool keep, bool ascii, int style);
bool undead_ascii(const UndeadInput *input);
int undead_count_style(const UndeadInput *input);
UndeadBackend *undead_monster_button(UndeadInput *input, const UndeadBackend *state, int monster, bool right);
#ifdef __cplusplus
}
#endif
#endif
