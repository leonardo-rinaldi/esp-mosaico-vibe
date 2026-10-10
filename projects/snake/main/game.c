// SPDX-License-Identifier: Apache-2.0
/* Snake. All state lives in one instance owned by the loop that calls the
 * public methods, so no locking is needed. Host and device share this file. */
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "game_config.h"
#include "mosaico_raylib_fast.h"

enum {
    BOARD_COLS = 20,
    BOARD_ROWS = 18,
    CELL_PX = 24,
    CELL_INSET = 1,                 /* Gap between drawn segments. */
    CELL_COUNT = BOARD_COLS * BOARD_ROWS,
    BOARD_W = BOARD_COLS * CELL_PX,
    BOARD_H = BOARD_ROWS * CELL_PX,
    BOARD_X = (GAME_WIDTH - BOARD_W) / 2,
    BOARD_Y = GAME_HEIGHT - BOARD_H, /* The strip above the board is the HUD. */
    HUD_HEIGHT = BOARD_Y,

    START_LENGTH = 3,
    START_STEP_TICKS = 8,           /* Ticks per move at score 0. */
    MIN_STEP_TICKS = 3,             /* Fastest speed. */
    FOODS_PER_SPEEDUP = 3,
    TURN_QUEUE_SIZE = 2,            /* Buffered turns, so quick taps are not lost. */
    SWIPE_MIN_PX = 28,
    RESTART_COOLDOWN_TICKS = 20,    /* Ignore input briefly after a crash. */

    HUD_FONT = 24,
    TITLE_FONT = 48,
    HINT_FONT = 20,
    READY_TITLE_Y = 330,            /* Below the starting snake's row. */
};

_Static_assert(BOARD_X >= 0 && BOARD_Y >= 0, "board must fit the display");
_Static_assert(CELL_COUNT <= UINT16_MAX, "cell indices are 16 bit");
_Static_assert(START_LENGTH < BOARD_COLS, "start snake must fit one row");

static const Color COLOR_HUD = {18, 24, 36, 255};
static const Color COLOR_TILE_A = {22, 30, 44, 255};
static const Color COLOR_TILE_B = {26, 36, 52, 255};
static const Color COLOR_BORDER = {70, 90, 120, 255};
static const Color COLOR_HEAD = {120, 255, 140, 255};
static const Color COLOR_BODY_A = {50, 200, 90, 255};
static const Color COLOR_BODY_B = {40, 170, 80, 255};
static const Color COLOR_DEAD = {150, 80, 80, 255};
static const Color COLOR_FOOD = {240, 60, 70, 255};
static const Color COLOR_FOOD_SHINE = {255, 190, 190, 255};
static const Color COLOR_TEXT = {235, 240, 250, 255};
static const Color COLOR_MUTED = {150, 165, 190, 255};
static const Color COLOR_EYE = {255, 255, 255, 255};
static const Color COLOR_PUPIL = {10, 10, 10, 255};

static const int8_t s_dir_dx[GAME_DIR_COUNT] = {0, 1, 0, -1};
static const int8_t s_dir_dy[GAME_DIR_COUNT] = {-1, 0, 1, 0};

struct game_t {
    game_config_t config;

    uint32_t tick;
    int32_t pointer_x;
    int32_t pointer_y;
    bool pointer_down;
    bool paused;

    game_phase_t phase;
    bool won;
    game_dir_t dir;
    uint16_t score;
    uint16_t best;
    uint32_t over_tick;
    uint8_t step_timer;

    /* The body is a ring of cell indices (y * BOARD_COLS + x), tail first. */
    uint16_t body[CELL_COUNT];
    uint16_t body_start;
    uint16_t body_len;
    uint8_t occupied[CELL_COUNT];   /* Mirrors the ring for O(1) collision tests. */
    uint16_t food;

    game_dir_t turn_queue[TURN_QUEUE_SIZE];
    uint8_t turn_count;

    int32_t anchor_x;               /* Where the current touch or last swipe began. */
    int32_t anchor_y;
    bool swiped;                    /* The current touch already turned the snake. */

    uint32_t rng;
};

static game_dir_t opposite(game_dir_t dir)
{
    return (game_dir_t)((dir + 2) % GAME_DIR_COUNT);
}

static uint32_t rng_next(game_handle_t game)
{
    uint32_t x = game->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    game->rng = x;
    return x;
}

static uint16_t body_at(const struct game_t *game, uint16_t index)
{
    return game->body[(game->body_start + index) % CELL_COUNT];
}

static uint16_t body_head(const struct game_t *game)
{
    return body_at(game, game->body_len - 1);
}

static void body_push_head(game_handle_t game, uint16_t cell)
{
    game->body[(game->body_start + game->body_len) % CELL_COUNT] = cell;
    ++game->body_len;
    game->occupied[cell] = 1;
}

static void body_pop_tail(game_handle_t game)
{
    game->occupied[game->body[game->body_start]] = 0;
    game->body_start = (game->body_start + 1) % CELL_COUNT;
    --game->body_len;
}

/* Puts food on a uniformly chosen free cell; false when the board is full. */
static bool place_food(game_handle_t game)
{
    const uint32_t free_cells = CELL_COUNT - game->body_len;
    if (free_cells == 0) return false;
    uint32_t pick = rng_next(game) % free_cells;
    for (uint16_t cell = 0; cell < CELL_COUNT; ++cell) {
        if (game->occupied[cell]) continue;
        if (pick-- == 0) {
            game->food = cell;
            return true;
        }
    }
    return false;
}

static void board_reset(game_handle_t game)
{
    memset(game->occupied, 0, sizeof(game->occupied));
    game->body_start = 0;
    game->body_len = 0;
    const int row = BOARD_ROWS / 2;
    const int first_col = BOARD_COLS / 2 - START_LENGTH / 2;
    for (int i = 0; i < START_LENGTH; ++i)
        body_push_head(game, (uint16_t)(row * BOARD_COLS + first_col + i));
    game->dir = GAME_DIR_RIGHT;
    game->turn_count = 0;
    game->step_timer = 0;
    game->score = 0;
    game->won = false;
    game->swiped = false;
    /* Vary later rounds with the time the player took to start them. */
    game->rng ^= (game->tick + 1U) * 2654435761U;
    if (!game->rng) game->rng = 1;
    (void)place_food(game); /* Cannot fail: the board has free cells. */
}

static void finish_round(game_handle_t game, bool won)
{
    game->phase = GAME_PHASE_OVER;
    game->won = won;
    game->over_tick = game->tick;
    game->turn_count = 0;
}

static unsigned step_interval(const struct game_t *game)
{
    const unsigned faster = game->score / FOODS_PER_SPEEDUP;
    return faster >= START_STEP_TICKS - MIN_STEP_TICKS
        ? MIN_STEP_TICKS : START_STEP_TICKS - faster;
}

/* Queues a turn against the direction the snake will have when it is applied. */
static void queue_turn(game_handle_t game, game_dir_t dir)
{
    const game_dir_t reference = game->turn_count
        ? game->turn_queue[game->turn_count - 1] : game->dir;
    if (dir == reference || dir == opposite(reference)) return;
    if (game->turn_count < TURN_QUEUE_SIZE) game->turn_queue[game->turn_count++] = dir;
}

static void apply_queued_turn(game_handle_t game)
{
    while (game->turn_count) {
        const game_dir_t next = game->turn_queue[0];
        game->turn_queue[0] = game->turn_queue[1];
        --game->turn_count;
        if (next != game->dir && next != opposite(game->dir)) {
            game->dir = next;
            return;
        }
    }
}

static void step_snake(game_handle_t game)
{
    apply_queued_turn(game);
    const uint16_t head = body_head(game);
    const int x = head % BOARD_COLS + s_dir_dx[game->dir];
    const int y = head / BOARD_COLS + s_dir_dy[game->dir];
    if (x < 0 || x >= BOARD_COLS || y < 0 || y >= BOARD_ROWS) {
        finish_round(game, false);
        return;
    }
    const uint16_t next = (uint16_t)(y * BOARD_COLS + x);
    const bool grow = next == game->food;
    /* The tail cell is vacated by this move unless the snake grows. */
    const bool into_tail = next == game->body[game->body_start];
    if (game->occupied[next] && (grow || !into_tail)) {
        finish_round(game, false);
        return;
    }
    if (!grow) body_pop_tail(game);
    body_push_head(game, next);
    if (!grow) return;
    ++game->score;
    if (game->score > game->best) game->best = game->score;
    if (!place_food(game)) finish_round(game, true);
}

static void start_round(game_handle_t game)
{
    if (game->phase == GAME_PHASE_OVER) board_reset(game);
    game->phase = GAME_PHASE_RUNNING;
    game->step_timer = 0;
}

static bool restart_allowed(const struct game_t *game)
{
    return game->tick - game->over_tick >= RESTART_COOLDOWN_TICKS;
}

/* A tap that did not swipe: resume, start or restart. */
static void handle_tap(game_handle_t game)
{
    if (game->paused) {
        game->paused = false;
    } else if (game->phase == GAME_PHASE_READY) {
        start_round(game);
    } else if (game->phase == GAME_PHASE_OVER && restart_allowed(game)) {
        start_round(game);
    }
}

bool game_create(const game_config_t *config, game_handle_t *ret_handle)
{
    if (!ret_handle) return false;
    *ret_handle = NULL;
    if (!config || !config->width || !config->height) return false;
    if (config->width < BOARD_X + BOARD_W || config->height < BOARD_Y + BOARD_H) return false;
    game_handle_t game = calloc(1, sizeof(*game));
    if (!game) return false;
    game->config = *config;
    game->rng = 0x2545F491U;
    board_reset(game);
    *ret_handle = game;
    return true;
}

void game_delete(game_handle_t game)
{
    free(game);
}

void game_reset(game_handle_t game)
{
    if (!game) return;
    game->paused = false;
    game->pointer_down = false;
    game->phase = GAME_PHASE_READY;
    board_reset(game);
}

void game_set_paused(game_handle_t game, bool paused)
{
    if (game) game->paused = paused;
}

void game_set_direction(game_handle_t game, game_dir_t dir)
{
    if (!game || game->paused || (unsigned)dir >= GAME_DIR_COUNT) return;
    switch (game->phase) {
    case GAME_PHASE_READY:
        queue_turn(game, dir);
        start_round(game);
        break;
    case GAME_PHASE_RUNNING:
        queue_turn(game, dir);
        break;
    case GAME_PHASE_OVER:
        if (restart_allowed(game)) start_round(game);
        break;
    }
}

void game_set_pointer(game_handle_t game, int32_t x, int32_t y, bool pressed)
{
    if (!game) return;
    const bool was_down = game->pointer_down;
    game->pointer_x = x < 0 ? 0 : (x >= game->config.width ? game->config.width - 1 : x);
    game->pointer_y = y < 0 ? 0 : (y >= game->config.height ? game->config.height - 1 : y);
    game->pointer_down = pressed;

    if (pressed && !was_down) {
        game->anchor_x = game->pointer_x;
        game->anchor_y = game->pointer_y;
        game->swiped = false;
    } else if (pressed) {
        const int32_t dx = game->pointer_x - game->anchor_x;
        const int32_t dy = game->pointer_y - game->anchor_y;
        const int32_t adx = dx < 0 ? -dx : dx;
        const int32_t ady = dy < 0 ? -dy : dy;
        if (adx < SWIPE_MIN_PX && ady < SWIPE_MIN_PX) return;
        const game_dir_t dir = adx > ady
            ? (dx > 0 ? GAME_DIR_RIGHT : GAME_DIR_LEFT)
            : (dy > 0 ? GAME_DIR_DOWN : GAME_DIR_UP);
        game->swiped = true;
        game->anchor_x = game->pointer_x; /* Continue the gesture from here. */
        game->anchor_y = game->pointer_y;
        game_set_direction(game, dir);
    } else if (was_down && !game->swiped) {
        handle_tap(game);
    }
}

void game_update(game_handle_t game)
{
    if (!game || game->paused) return;
    ++game->tick;
    if (game->phase != GAME_PHASE_RUNNING) return;
    if (++game->step_timer < step_interval(game)) return;
    game->step_timer = 0;
    step_snake(game);
}

static void cell_origin(uint16_t cell, int *px, int *py)
{
    *px = BOARD_X + (cell % BOARD_COLS) * CELL_PX;
    *py = BOARD_Y + (cell / BOARD_COLS) * CELL_PX;
}

static void draw_centered(const char *text, int y, int font, Color color)
{
    DrawText(text, (GAME_WIDTH - MeasureText(text, font)) / 2, y, font, color);
}

static void draw_board(void)
{
    DrawRectangle(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, COLOR_TILE_A);
    for (int row = 0; row < BOARD_ROWS; ++row) {
        for (int col = (row & 1); col < BOARD_COLS; col += 2)
            DrawRectangle(BOARD_X + col * CELL_PX, BOARD_Y + row * CELL_PX,
                          CELL_PX, CELL_PX, COLOR_TILE_B);
    }
    DrawRectangleLines(BOARD_X, BOARD_Y, BOARD_W, BOARD_H, COLOR_BORDER);
}

static void draw_food(const struct game_t *game)
{
    int px, py;
    cell_origin(game->food, &px, &py);
    const int cx = px + CELL_PX / 2;
    const int cy = py + CELL_PX / 2;
    DrawCircle(cx, cy, CELL_PX / 2 - 3, COLOR_FOOD);
    DrawCircle(cx - 3, cy - 3, 3, COLOR_FOOD_SHINE);
}

static void draw_head_eyes(int px, int py, game_dir_t dir)
{
    const int cx = px + CELL_PX / 2;
    const int cy = py + CELL_PX / 2;
    const int forward_x = s_dir_dx[dir] * 4;
    const int forward_y = s_dir_dy[dir] * 4;
    const int side_x = -s_dir_dy[dir] * 5; /* Perpendicular to the heading. */
    const int side_y = s_dir_dx[dir] * 5;
    for (int sign = -1; sign <= 1; sign += 2) {
        const int ex = cx + forward_x + sign * side_x;
        const int ey = cy + forward_y + sign * side_y;
        DrawCircle(ex, ey, 3, COLOR_EYE);
        DrawCircle(ex + s_dir_dx[dir], ey + s_dir_dy[dir], 1.5f, COLOR_PUPIL);
    }
}

static void draw_snake(const struct game_t *game)
{
    const bool dead = game->phase == GAME_PHASE_OVER && !game->won;
    for (uint16_t i = 0; i < game->body_len; ++i) {
        int px, py;
        cell_origin(body_at(game, i), &px, &py);
        const bool is_head = i == game->body_len - 1;
        Color color = (i & 1) ? COLOR_BODY_A : COLOR_BODY_B;
        if (dead) color = COLOR_DEAD;
        else if (is_head) color = COLOR_HEAD;
        DrawRectangle(px + CELL_INSET, py + CELL_INSET,
                      CELL_PX - 2 * CELL_INSET, CELL_PX - 2 * CELL_INSET, color);
        if (is_head) draw_head_eyes(px, py, game->dir);
    }
}

static void draw_hud(const struct game_t *game)
{
    DrawRectangle(0, 0, GAME_WIDTH, HUD_HEIGHT, COLOR_HUD);
    const int y = (HUD_HEIGHT - HUD_FONT) / 2;
    DrawText(TextFormat("SCORE %u", (unsigned)game->score), 16, y, HUD_FONT, COLOR_TEXT);
    const char *best = TextFormat("BEST %u", (unsigned)game->best);
    DrawText(best, GAME_WIDTH - 16 - MeasureText(best, HUD_FONT), y, HUD_FONT, COLOR_MUTED);
}

static void draw_overlay(const struct game_t *game)
{
    const char *title = NULL;
    const char *line = NULL;
    Color title_color = COLOR_TEXT;
    if (game->paused) {
        title = "PAUSED";
        line = "Tap to resume";
    } else if (game->phase == GAME_PHASE_READY) {
        title = "SNAKE";
        line = "Swipe or press an arrow to start";
    } else if (game->phase == GAME_PHASE_OVER) {
        title = game->won ? "YOU WIN!" : "GAME OVER";
        title_color = game->won ? COLOR_HEAD : COLOR_FOOD;
        line = restart_allowed(game) ? "Tap to play again" : NULL;
    }
    if (!title) return;
    if (!game->paused && game->phase == GAME_PHASE_READY) {
        /* Keep the starting snake visible: text sits below its row, no dimming. */
        draw_centered(title, READY_TITLE_Y, TITLE_FONT, title_color);
        draw_centered(line, READY_TITLE_Y + 62, HINT_FONT, COLOR_MUTED);
        return;
    }
    const int mid = BOARD_Y + BOARD_H / 2;
    DrawRectangle(BOARD_X, mid - 70, BOARD_W, 140, Fade(BLACK, 0.7f));
    draw_centered(title, mid - 50, TITLE_FONT, title_color);
    if (game->phase == GAME_PHASE_OVER && !game->paused)
        draw_centered(TextFormat("Score %u", (unsigned)game->score), mid + 6, HUD_FONT, COLOR_TEXT);
    if (line) draw_centered(line, mid + 38, HINT_FONT, COLOR_MUTED);
}

bool game_render(game_handle_t game)
{
    if (!game) return false;
    BeginDrawing();
    if (!MosaicoFastFrameAvailable()) {
        EndDrawing();
        return false;
    }
    ClearBackground(BLACK);
    draw_hud(game);
    draw_board();
    draw_food(game);
    draw_snake(game);
    draw_overlay(game);
    EndDrawing();
    return true;
}

game_snapshot_t game_read(game_handle_t game)
{
    if (!game) return (game_snapshot_t){0};
    const uint16_t head = body_head(game);
    uint32_t hash = 2166136261U; /* FNV-1a over the ring, tail to head. */
    for (uint16_t i = 0; i < game->body_len; ++i)
        hash = (hash ^ body_at(game, i)) * 16777619U;
    return (game_snapshot_t){
        .tick = game->tick,
        .pointer_x = game->pointer_x,
        .pointer_y = game->pointer_y,
        .pointer_down = game->pointer_down,
        .paused = game->paused,
        .phase = game->phase,
        .won = game->won,
        .dir = game->dir,
        .score = game->score,
        .best = game->best,
        .length = game->body_len,
        .head_x = (int16_t)(head % BOARD_COLS),
        .head_y = (int16_t)(head / BOARD_COLS),
        .food_x = (int16_t)(game->food % BOARD_COLS),
        .food_y = (int16_t)(game->food / BOARD_COLS),
        .body_hash = hash,
    };
}
