# SPDX-License-Identifier: Apache-2.0
"""Tests for power saving: the brightness and screen-off choices, the idle timing (dim at half
the time, off at the full time, a touch wakes it) and how the choices are remembered. Time runs
in Host frames: 30 per second."""
from __future__ import annotations

import ctypes
import io
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "banco"
ENGINE_HOST = ROOT / "submodule" / "raylib-lite-engine" / "host"
if not (ENGINE_HOST / "run_game.py").is_file():
    raise unittest.SkipTest("initialize submodule/raylib-lite-engine to run these tests")
sys.path.insert(0, str(ENGINE_HOST))

import run_game  # noqa: E402

FPS = 30
CONTROL_RESET = 3
SETTINGS_SIZE = 122
BACK = (60, 20)
BATTERY = (60, 28)
POWER_BUTTON = (400, 20)
BRIGHT = {"high": (80, 124), "medium": (240, 124), "low": (400, 124)}
SLEEP = {"never": (60, 248), "30": (180, 248), "60": (300, 248), "120": (420, 248)}
CONV_TILE = (120, 117)
BTN_EN = (446, 28)
OFFSET_BRIGHTNESS, OFFSET_SLEEP = 23, 24


class PowerSaving(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        state = ctypes.c_void_p.from_address(self.rt.context + ctypes.sizeof(ctypes.c_void_p)).value
        self.game = ctypes.c_void_p.from_address(state).value
        api = self.rt.api
        api.game_display_brightness.argtypes = [ctypes.c_void_p]
        api.game_display_brightness.restype = ctypes.c_int
        api.game_settings_get.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        api.game_settings_apply.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.step()

    def step(self, count: int = 1) -> None:
        for _ in range(count):
            self.rt.step(False, False, False)

    def tap(self, point) -> str:
        self.rt.pointer(0, point[0], point[1], True)
        self.step()
        self.rt.pointer(0, point[0], point[1], False)
        self.step()
        return self.rt.metadata()["screen"]

    def lines(self) -> list:
        return self.rt.metadata()["lines"]

    def brightness(self) -> int:
        return self.rt.api.game_display_brightness(self.game)

    def open_power(self) -> None:
        self.assertEqual(self.tap(BATTERY), "batteria")
        self.assertEqual(self.tap(POWER_BUTTON), "risparmio")

    def choose(self, brightness=None, sleep=None) -> None:
        self.open_power()
        if brightness:
            self.tap(BRIGHT[brightness])
        if sleep:
            self.tap(SLEEP[sleep])
        self.assertEqual(self.tap(BACK), "batteria")
        self.assertEqual(self.tap(BACK), "menu")

    def settings_blob(self) -> bytes:
        buffer = ctypes.create_string_buffer(SETTINGS_SIZE)
        self.rt.api.game_settings_get(self.game, buffer)
        return bytes(buffer.raw)

    # Navigation and choices.
    def test_navigation_from_the_battery_screen(self) -> None:
        self.open_power()
        self.assertEqual(self.tap(BACK), "batteria")
        self.assertEqual(self.tap(BACK), "menu")

    def test_defaults(self) -> None:
        self.open_power()
        self.assertEqual(self.lines(), ["ALTA", "1 MIN"])
        self.assertEqual(self.brightness(), 100)

    def test_choices_show_their_names(self) -> None:
        self.open_power()
        self.tap(BRIGHT["low"])
        self.tap(SLEEP["never"])
        self.assertEqual(self.lines(), ["BASSA", "MAI"])
        self.tap(BRIGHT["medium"])
        self.tap(SLEEP["120"])
        self.assertEqual(self.lines(), ["MEDIA", "2 MIN"])

    def test_english(self) -> None:
        self.tap(BTN_EN)
        self.open_power()
        self.assertEqual(self.lines(), ["HIGH", "1 MIN"])
        self.tap(SLEEP["never"])
        self.assertEqual(self.lines()[1], "NEVER")

    def test_brightness_levels(self) -> None:
        for name, percent in (("medium", 60), ("low", 30), ("high", 100)):
            with self.subTest(level=name):
                self.choose(brightness=name)
                self.assertEqual(self.brightness(), percent)

    # Idle timing.
    def test_dims_at_half_the_time_and_turns_off_at_the_full_time(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS // 2 - 10)
        self.assertEqual(self.brightness(), 100)
        self.step(20)
        self.assertEqual(self.brightness(), 10)
        self.step(30 * FPS // 2 - 40)
        self.assertEqual(self.brightness(), 10)
        self.step(60)
        self.assertEqual(self.brightness(), 0)

    def test_a_touch_restarts_the_timer(self) -> None:
        self.choose(sleep="30")
        self.step(400)
        self.tap((240, 300))            # between the tiles: only a touch
        self.step(400)
        self.assertEqual(self.brightness(), 100)

    def test_never_keeps_the_screen_on(self) -> None:
        self.choose(sleep="never")
        self.step(30 * FPS * 3)
        self.assertEqual(self.brightness(), 100)

    def test_dim_never_raises_a_low_brightness(self) -> None:
        self.choose(brightness="low", sleep="30")
        self.step(30 * FPS // 2 + 20)
        self.assertEqual(self.brightness(), 10)

    def test_the_waking_touch_does_not_press_anything(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS + 30)
        self.assertEqual(self.brightness(), 0)
        self.assertEqual(self.tap(CONV_TILE), "menu")       # woke the screen, opened nothing
        self.assertEqual(self.brightness(), 100)
        self.assertEqual(self.tap(CONV_TILE), "conv")       # now it works

    def test_a_dim_screen_still_takes_the_touch(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS // 2 + 30)
        self.assertEqual(self.brightness(), 10)
        self.assertEqual(self.tap(CONV_TILE), "conv")
        self.assertEqual(self.brightness(), 100)

    def test_the_idle_timer_runs_on_every_screen(self) -> None:
        self.choose(sleep="30")
        self.tap(CONV_TILE)
        self.step(30 * FPS + 30)
        self.assertEqual(self.brightness(), 0)

    # What is drawn: colours are scaled by the brightness, and the screen goes black.
    def brightest(self) -> int:
        from PIL import Image
        image = Image.open(io.BytesIO(self.rt.frame())).convert("RGB")
        return max(max(pixel) for pixel in image.getdata())

    def test_drawing_follows_the_brightness(self) -> None:
        full = self.brightest()
        self.assertGreaterEqual(full, 240)
        for name, percent in (("medium", 60), ("low", 30)):
            with self.subTest(level=name):
                self.choose(brightness=name)
                self.step(2)
                self.assertAlmostEqual(self.brightest(), full * percent / 100, delta=full * 0.06)

    def test_a_dim_screen_is_drawn_dark(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS // 2 + 30)
        self.assertLessEqual(self.brightest(), 40)

    def test_an_off_screen_shows_pure_black(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS + 30)
        self.assertEqual(self.brightest(), 0)
        self.step(60)
        self.assertEqual(self.brightest(), 0)

    def test_waking_draws_the_interface_again(self) -> None:
        self.choose(sleep="30")
        self.step(30 * FPS + 30)
        self.tap((240, 300))
        self.step(2)
        self.assertGreaterEqual(self.brightest(), 240)

    # Remembering.
    def test_choices_are_part_of_the_saved_settings(self) -> None:
        self.choose(brightness="low", sleep="120")
        blob = self.settings_blob()
        self.assertEqual((blob[OFFSET_BRIGHTNESS], blob[OFFSET_SLEEP]), (2, 3))

    def test_restoring_settings_brings_the_choices_back(self) -> None:
        blob = bytearray(self.settings_blob())
        blob[OFFSET_BRIGHTNESS], blob[OFFSET_SLEEP] = 1, 3
        self.rt.api.game_settings_apply(self.game, ctypes.create_string_buffer(bytes(blob), SETTINGS_SIZE))
        self.open_power()
        self.assertEqual(self.lines(), ["MEDIA", "2 MIN"])
        self.assertEqual(self.brightness(), 60)

    def test_invalid_stored_values_are_ignored(self) -> None:
        self.choose(brightness="low", sleep="30")
        blob = bytearray(self.settings_blob())
        blob[OFFSET_BRIGHTNESS], blob[OFFSET_SLEEP] = 9, 200
        self.rt.api.game_settings_apply(self.game, ctypes.create_string_buffer(bytes(blob), SETTINGS_SIZE))
        self.open_power()
        self.assertEqual(self.lines(), ["BASSA", "30 S"])

    def test_reset_keeps_the_choices_and_wakes_the_screen(self) -> None:
        self.choose(brightness="medium", sleep="30")
        self.step(30 * FPS + 30)
        self.assertEqual(self.brightness(), 0)
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.brightness(), 60)
        self.open_power()
        self.assertEqual(self.lines(), ["MEDIA", "30 S"])


if __name__ == "__main__":
    unittest.main()
