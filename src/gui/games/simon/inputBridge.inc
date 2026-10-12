static int upstream_button(int button, bool control, bool shift) {
    switch (button) {
        case PuzzleLeft: button = LEFT_BUTTON; break;
        case PuzzleRight: button = RIGHT_BUTTON; break;
        case PuzzleUp: button = CURSOR_UP; break;
        case PuzzleDown: button = CURSOR_DOWN; break;
        case PuzzleArrowLeft: button = CURSOR_LEFT; break;
        case PuzzleArrowRight: button = CURSOR_RIGHT; break;
        case PuzzleEnter: button = CURSOR_SELECT; break;
        case PuzzleLeftDrag: button = LEFT_DRAG; break;
        case PuzzleRightDrag: button = RIGHT_DRAG; break;
        case PuzzleLeftRelease: button = LEFT_RELEASE; break;
        case PuzzleRightRelease: button = RIGHT_RELEASE; break;
        case PuzzleSpace: button = CURSOR_SELECT2; break;
    }
    if (control) button |= MOD_CTRL;
    if (shift) button |= MOD_SHFT;
    return button;
}
static bool real_move(const char *move) {
    return move && move != MOVE_UI_UPDATE && move != MOVE_NO_EFFECT && move != MOVE_UNUSED;
}
