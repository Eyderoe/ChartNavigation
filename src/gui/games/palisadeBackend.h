#ifndef CHARTNAVIGATION_GAMES_PALISADEBACKEND_H
#define CHARTNAVIGATION_GAMES_PALISADEBACKEND_H
#include <stdbool.h>
#include "puzzleInput.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PalisadeBackend PalisadeBackend;
typedef struct PalisadeInput PalisadeInput;
typedef struct PalisadeConfig {
    int width, height, region;
} PalisadeConfig;
int palisade_preset_count(void);
PalisadeConfig palisade_preset(int index);
const char *palisade_validate(PalisadeConfig config);
PalisadeBackend *palisade_new(PalisadeConfig config, const char *seed);
PalisadeBackend *palisade_copy(const PalisadeBackend *state);
void palisade_free(PalisadeBackend *state);
PalisadeBackend *palisade_solve(const PalisadeBackend *initial, const PalisadeBackend *state);
PalisadeInput *palisade_input_new(const PalisadeBackend *state);
void palisade_input_free(PalisadeInput *input);
PalisadeBackend *palisade_input(PalisadeInput *input, const PalisadeBackend *state, int x, int y, int button,
                                bool control, bool shift);
void palisade_input_mode(PalisadeInput *input, bool full, bool clear);
bool palisade_input_visible(const PalisadeInput *input);
void palisade_cursor(const PalisadeInput *input, int *x, int *y);
int palisade_clue(const PalisadeBackend *state, int cell);
int palisade_border(const PalisadeBackend *state, int cell);
void palisade_display(const PalisadeBackend *state, const PalisadeInput *input, int *flags);
bool palisade_won(const PalisadeBackend *state);
bool palisade_cheated(const PalisadeBackend *state);
#ifdef __cplusplus
}
#endif
#endif
