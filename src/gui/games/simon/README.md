# Simon Tatham puzzle backends

Source: https://www.chiark.greenend.org.uk/~sgtatham/puzzles/puzzles.tar.gz
Snapshot: **20260923.616da16**, the version displayed by the Mines web manual
when integrated on 2026-10-11. Distributed under `LICENCE` (MIT).

All supporting `.c` and `.h` files are unmodified upstream files. The backend
keeps all 40 desktop/web presets and 21 grid types, including aperiodic grids.

`minesCore.inc` contains exact excerpts of upstream `mines.c`: the prefix before
`preset_menu_add_preset_with_title`, the section from `free_params` to before
`game_can_format_as_text_now`, and `execute_move` up to before
`game_compute_size`. Only frontend/menu/drawing functions are omitted.
`miscCore.c` keeps the prefix before `colour_distance`, `swap_regions` and
`shuffle`, `compare_integers`, and the trailing `n_times_root_k` function.
No generation, solver, parameter-validation, or move algorithm is rewritten.

`../minesBackend.c` provides the C/Qt ownership and geometry interface and
replaces midend's description publication callback (there is no midend here).
The first click runs the original deduction/perturbation generator in a worker
with its own entire backend state. The GUI adopts it only after completion.
History and restart states then share its immutable, reference-counted layout,
as in the original game. Presets are read directly from the upstream arrays.

Updating to a new upstream version requires recutting these excerpts and
checking the API, preset menu, grid geometry, RNG, solver and move contracts;
this snapshot does not automatically track future website updates.

## Signpost

`signpostCore.inc` contains the exact prefix of upstream `signpost.c` before
`struct game_ui`, followed by `unlink_cell` through before `game_compute_size`.
This preserves all six presets, parameter validation, description handling,
random generation, deduction solver, number propagation and move execution.
`../signpostBackend.c` adapts ownership and board queries for Qt. It shares the
unmodified RNG, DSF and allocation utilities above. Generation runs in an
isolated worker; superseded results are discarded. Qt provides drawing,
mouse/keyboard input and undo/restart history. No generation or difficulty
algorithm is rewritten. Source snapshot and licence are the same as Mines.

## Solo and Palisade

`soloCore.inc` and `palisadeCore.inc` preserve the upstream prefix through
before `game_compute_size`, including presets, custom configuration, validation,
all generators/solvers, game descriptions, UI interpretation and move execution.
Solo exposes all 16 web/desktop presets (without `SLOW_SYSTEM`), six difficulty
levels and eight symmetries, with Jigsaw, diagonal and Killer variants. Palisade
exposes the four original presets and region-size validation. `divvy.c` is an
unmodified supporting source. `inputCore.c` contains the exact `move_cursor`
function from `misc.c`; environment handling is already in `miscCore.c`.

Two explicitly delimited Solo adapters are inserted into the excerpt:

- The original `precompute_sum_bits` function is macro-renamed and called once
  through a C11 atomic publication guard. The generated tables and algorithms
  remain identical, and separate background generators safely share them.
- The original `grid_encode_space` is macro-renamed and its capacity estimate
  is bounded below by `3*area+1`. Upstream's estimate is too small for some small
  Jigsaw/Killer descriptions (reproduced with a 4x4 Jigsaw+X+Killer puzzle).
  This only reserves more memory; encoded descriptions and RNG calls are unchanged.

The bridges preserve the generation-time auxiliary solution, as upstream
midend does. State/aux snapshots and GUI input objects are separately owned;
reference-counted puzzle structures are only shared after adopting a worker's
result. Generation never runs on the GUI thread, and stale results cannot
replace newer configurations. Qt paints regular/irregular block edges,
diagonals, Killer cages/sums and candidate symbols; Palisade's border/clue
error calculation is adapted from upstream `game_redraw`. Both frontends feed
mouse coordinates and keys to the original `interpret_move` function and offer
undo, redo, restart and reveal-solution actions. All algorithms use the same
snapshot and licence as Mines and Signpost.

## Tents and Undead

Both games use upstream snapshot `20260923.616da16`, the version published by
both web games and their manuals on 2026-10-11. `tentsCore.inc` preserves the
exact prefix of `tents.c` before `game_compute_size`, followed by the exact
`ERR_ADJ_TOPLEFT` enum and `find_errors` function (ending before `draw_err_adj`).
`undeadCore.inc` preserves the exact prefix of `undead.c` before
`game_compute_size`. There are no inserted changes to either excerpt.
`matching.c` and `matching.h` are unmodified upstream sources used by Tents
for generation and its one-to-one tent/tree completion check.

The C bridges expose all six Tents presets and Easy/Tricky difficulty, and all
eight Undead presets and Easy/Normal/Tricky difficulty. Custom configuration
uses the original size validation: Tents has minimum 4x4 and an integer area
bound; Undead has minimum 3x3 and maximum 54 cells. The bridges additionally
reject out-of-range difficulty enum values before accessing source arrays.
No generation or difficulty algorithm is rewritten. Both games generate in
isolated workers with the upstream RNG; the controller discards stale results.
Original auxiliary solutions are retained when supplied (Tents supplies one;
Undead solves the generated puzzle with its original solver).

Qt supplies drawing and history while mouse coordinates and symbolic keys are
fed into the original `interpret_move` and `execute_move` functions. Tents uses
original press/drag/release previews, row/column right-drag, off-grid cancellation,
modifier keys and original error diagnostics. Undead preserves mirror paths,
monster totals, pencil marks, clue completion marks, alphabet/picture preference,
three count styles, monster-count buttons and original error/completion state.
Qt relocates Undead's count buttons to controls above the board but invokes the
original count-button hit regions. Error flags come directly from the upstream
state. Undead's original distinction between a normal win and a revealed answer
is retained. New symbolic drag/release button mappings are shared with the
existing bridges without changing their input mappings.
