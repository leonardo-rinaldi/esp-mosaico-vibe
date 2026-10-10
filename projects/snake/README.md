# snake

Classic Snake on a 20 x 18 board (480 x 480 display, 24 px cells) for ESP-Mosaico,
built on Raylib Lite. Eat the red food to grow and score; the snake speeds up every
three foods. Hitting a wall or your own body ends the round. The snake may move into
the cell its tail is leaving.

## Controls

| Input | Action |
| --- | --- |
| Swipe (touch or mouse drag, 28 px or more) | Turn; also starts or restarts the game |
| Arrow keys / WASD (Host) | Turn; also starts or restarts the game |
| Tap | Start, restart (after a short cooldown) or resume from pause |
| `P` / Host pause button | Pause and resume |
| Host reset button | Back to the ready screen (best score is kept) |

Reversing straight into the neck is ignored, and up to two quick turns are buffered so
fast key presses within one move are not lost. In the Host web preview, touching the
bottom strip of the canvas also emits the Host's generic left/right/up actions, which
turn the snake as well. The best score lives in memory for the current run only.

## Build and run

From the workspace root:

```sh
python mosaico.py game sim --project projects/snake
python mosaico.py game sim --project projects/snake --headless --frames 120
python -m unittest discover -s projects/snake/tests -v
python mosaico.py game build --project projects/snake
python mosaico.py iris system-update --project projects/snake
```

Run `python mosaico.py recover` before first installation on a blank or unverified
device. New applications, changed layouts or external resources use
`iris system-update`. The immutable Vibe Mode partitions and OTA routing are
already configured; the engine starts Iris and accepts the image after its first
frame. USB High-Speed remains owned by Iris. Physical input/display behavior
still requires device validation after Host checks.

## Files

- `main/game.c`: the whole game model: ring-buffer snake body, food placement,
  fixed-step update, swipe/tap handling and Raylib Lite rendering.
- `main/game.h`: portable interface with an opaque instance owned by its loop.
- `main/game_config.h`: project identity, display/tick constants and Host action codes.
- `main/game_module.c`: Host lifecycle, key and control mapping, JSON state.
- `main/main.c`: device callbacks for the shared engine application runtime.
- `game.sim.json`: sources compiled into the native Host module.
- `tests/test_host.py`: behavior tests run through the Host simulator.

Board size, cell size, speed curve and input thresholds are named constants at the top
of `main/game.c`.

## Host state

`--state-output` and the preview expose `phase` (`ready`, `running`, `paused`,
`game_over`, `won`), `tick`, `score`, `best`, `length`, `dir` (0 up, 1 right, 2 down,
3 left), `head_x`, `head_y`, `food_x`, `food_y`, `pointer_*`, `body_hash` and
`state_hash`. The game is deterministic for a given input sequence, so recorded
scenarios replay identically with `--scenario`.

## Tests

`tests/test_host.py` compiles the Host module with the simulator's own flags and
checks start, movement, reversal rejection, swipe and tap input, buffered turns,
walls, self-collision, tail-following, restart cooldown, pause/resume, reset,
growth, scoring and the speed-up. A bot plays up to 25 foods while an independent
Python model of the body is compared with the C game's body hash on every move.
They run with plain `unittest` and are also collected by pytest.

Host needs a C compiler and Pillow, and uses the same C renderer as firmware.
Firmware needs the pinned workspace ESP-IDF and initialized BSP, engine and utils.
Before using any other Raylib API, check the engine's supported public headers.
