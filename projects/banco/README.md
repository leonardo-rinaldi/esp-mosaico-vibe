# banco

Workshop helper for ESP-Mosaico. The app opens on a home menu with five tiles; tap
one to open that function, and tap "< MENU" in the top-left corner to go back.
The language (Italian or English) is chosen with the IT / EN buttons at the top right
of the menu; it applies to every screen. The language and the last values entered on
every screen are saved in the device's NVS and come back after a restart.

| Tile | Function |
| --- | --- |
| CONVERTITORE | Unit converter (ohm, volt, ampere, farad, watt and prefixes) |
| LEGGE DI OHM | Enter any two of V, I, R, P and get the others |
| CODICE COLORI | Resistor colour code, 4 and 5 bands |
| CAVO | Cable cross-section and voltage drop |
| LIVELLA | Spirit level and inclinometer from the accelerometer: FLAT (bubble vial, X and Y angles) or ON EDGE (angle from vertical, for pipes and walls), slope in percent, ZERO button |
| LENTE | Live view from the camera module (left slot) with digital zoom x1-x4 and a freeze button; tap the picture to freeze too. The camera only runs on this screen |

The battery level (from the BQ27220 fuel gauge) is shown top-left in the menu header; tap it for
the battery screen: percentage, charging or not, voltage, current, temperature and time left or
to full. The gauge is read by a small task every 2 s, started 3 s after boot.

Sources: `main/screen_menu.c` (home), `screen_conv/ohm/color/cable.c` (functions),
`calc.c`, `level.c` and `power_status.c` (pure logic), `screen_lens.c` (lens screen; `lens_present_host.c` is the Host test pattern, `lens_present_device.c` and `lens_camera.c` are the board camera path), `settings.h` (what is remembered), `i18n.c` (all Italian and English texts), `game.c` (screen switching and header).
Tests: `python -m unittest discover -s projects/banco/tests -v`
(`test_calc.py` and `test_level.py` for the maths, `test_menu.py` for navigation and language,
`test_level.py` for the level screen, `test_settings.py` for saving and restoring, `test_battery.py` for the battery).

The axis directions of the accelerometer on the real board are set by `LEVEL_SIGN_X` and
`LEVEL_SIGN_Y` in `main/level.c`; flip them if a tilt shows the wrong way on the device.

From the workspace root:

```sh
python mosaico.py game sim --project projects/banco
python mosaico.py game sim --project projects/banco --headless --frames 120
python mosaico.py game build --project projects/banco
python mosaico.py iris system-update --project projects/banco
```

Run `python mosaico.py recover` before first installation on a blank or unverified
device. New applications, changed layouts or external resources use
`iris system-update`. The immutable Vibe Mode partitions and OTA routing are
already configured; the engine starts Iris and accepts the image after its first
frame. USB High-Speed remains owned by Iris. Physical input/display behavior
still requires device validation after Host checks.

- `main/game.c`: shared state, pointer input, fixed-step update and rendering.
  Add game logic to `game_update()` and Raylib Lite drawing to `game_render()`.
- `main/game.h`: portable interface with an opaque instance owned by its loop.
- `main/game_config.h`: project identity and display/tick constants.
- `main/game_module.c`: Host lifecycle, controls and JSON state.
- `main/main.c`: device callbacks for the shared engine application runtime.
- `game.sim.json`: sources compiled into the native Host module.

Host state exposes `phase`, `tick`, `pointer_x`, `pointer_y`, `pointer_down` and
`state_hash` for automation. Touch/pointer input records coordinates without
drawing a cursor. Pause/resume/reset are wired for Host controls and replay;
there are no gameplay keyboard bindings yet. Add each new input behavior to the
shared model and map it from the device and Host adapters.

Host needs a C compiler and Pillow, and uses the same C renderer as firmware.
Firmware needs the pinned workspace ESP-IDF and initialized BSP, engine and utils.
Before using any other Raylib API, check the engine's supported public headers.
