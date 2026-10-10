// SPDX-License-Identifier: Apache-2.0
#pragma once

#define GAME_NAME "snake"
enum { GAME_WIDTH = 480, GAME_HEIGHT = 480, GAME_TICK_HZ = 30 };

/* Host action codes. 0/1/2/5 are the Host's left/right/jump/back inputs
 * (A/D/W/S or the arrow keys); 3 and 4 are pause and restart. */
enum {
    GAME_ACTION_LEFT = 0,
    GAME_ACTION_RIGHT = 1,
    GAME_ACTION_UP = 2,
    GAME_ACTION_PAUSE = 3,
    GAME_ACTION_RESET = 4,
    GAME_ACTION_DOWN = 5,
};
