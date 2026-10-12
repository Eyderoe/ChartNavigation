#ifndef CHARTNAVIGATION_GAMES_MINESBACKEND_H
#define CHARTNAVIGATION_GAMES_MINESBACKEND_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MinesBackend MinesBackend;
typedef struct MinesConfig {
    int width, height, type;
    bool proportion;
    int mines; // Proportions use the upstream denominator 10000.
    bool soluble;
} MinesConfig;

int mines_preset_count(void);
MinesConfig mines_preset(int index);
const char *mines_preset_group(int index);
int mines_grid_count(void);
const char *mines_grid_name(int type);
void mines_count_text(MinesConfig config, char *buffer, size_t size);
bool mines_parse_count(MinesConfig *config, const char *text);
const char *mines_validate(MinesConfig config);

MinesBackend *mines_new(MinesConfig config, const char *seed);
MinesBackend *mines_copy(const MinesBackend *state);
MinesBackend *mines_rebase(const MinesBackend *old, const MinesBackend *generated);
MinesBackend *mines_first_open(MinesConfig config, const char *seed, const int *board, int cell);
MinesBackend *mines_action(const MinesBackend *state, int cell, bool flag);
MinesBackend *mines_solve(const MinesBackend *state);
void mines_free(MinesBackend *state);

int mines_cells(const MinesBackend *state);
int mines_cell(const MinesBackend *state, int cell);
bool mines_generated(const MinesBackend *state);
bool mines_dead(const MinesBackend *state);
bool mines_won(const MinesBackend *state);
bool mines_used_solve(const MinesBackend *state);
int mines_nominal_count(const MinesBackend *state);
int mines_start_cell(const MinesBackend *state);
int mines_neighbours(const MinesBackend *state, int cell, int *output, int capacity);
int mines_cursor(const MinesBackend *state, int cell, int direction);
int mines_corners(const MinesBackend *state, int cell);
void mines_corner(const MinesBackend *state, int cell, int corner, double *x, double *y);
void mines_centre(const MinesBackend *state, int cell, double *x, double *y, double *radius);

#ifdef __cplusplus
}
#endif

#endif // CHARTNAVIGATION_GAMES_MINESBACKEND_H
