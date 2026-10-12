#include "puzzles.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
char *move_cursor(int button, int *x, int *y, int maxw, int maxh, bool wrap,
    bool *visible)
{
    int dx = 0, dy = 0, ox = *x, oy = *y;
    switch (button) {
    case CURSOR_UP:         dy = -1; break;
    case CURSOR_DOWN:       dy = 1; break;
    case CURSOR_RIGHT:      dx = 1; break;
    case CURSOR_LEFT:       dx = -1; break;
    default: return MOVE_UNUSED;
    }
    if (wrap) {
        *x = (*x + dx + maxw) % maxw;
        *y = (*y + dy + maxh) % maxh;
    } else {
        *x = min(max(*x+dx, 0), maxw - 1);
        *y = min(max(*y+dy, 0), maxh - 1);
    }
    if (visible != NULL && !*visible) {
        *visible = true;
        return MOVE_UI_UPDATE;
    }
    if (*x != ox || *y != oy)
        return MOVE_UI_UPDATE;
    return MOVE_NO_EFFECT;
}
