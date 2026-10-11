# SPDX-License-Identifier: Apache-2.0
"""Tests for the remembered settings (language and last entries), through the
Host simulator. The device keeps the same blob in NVS; here the blob is read and
written through the game's settings functions, which the Host module exports.

    python -m unittest discover -s projects/banco/tests -v
"""
from __future__ import annotations

import ctypes
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

SETTINGS_SIZE = 120
CONTROL_RESET = 3
BACK = (60, 20)
BTN_EN = (446, 28)
TILES = {"conv": (120, 140), "ohm": (360, 140), "colori": (120, 270), "cavo": (360, 270),
         "livella": (240, 400)}
# Keypad keys by position on the screens that use it (see keypad.c layout).
KEYS = {"7": (60, 332), "8": (180, 332), "9": (300, 332)}


class Settings(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        api = self.rt.api
        # The host bridge's instance is {module, state}; state starts with the game handle.
        state = ctypes.c_void_p.from_address(self.rt.context + ctypes.sizeof(ctypes.c_void_p)).value
        self.game = ctypes.c_void_p.from_address(state).value
        api.game_settings_get.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        api.game_settings_apply.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        api.game_settings_poll.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        api.game_settings_poll.restype = ctypes.c_bool
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

    def get(self) -> bytes:
        buffer = ctypes.create_string_buffer(SETTINGS_SIZE)
        self.rt.api.game_settings_get(self.game, buffer)
        return buffer.raw

    def apply(self, blob: bytes) -> None:
        self.rt.api.game_settings_apply(self.game, ctypes.create_string_buffer(blob, SETTINGS_SIZE))
        self.step()

    def poll(self) -> bool:
        return bool(self.rt.api.game_settings_poll(self.game, ctypes.create_string_buffer(SETTINGS_SIZE)))

    def snapshot(self) -> dict:
        """The text of every function screen, taken by visiting each one."""
        result = {}
        for name in ("conv", "ohm", "colori", "cavo"):
            self.tap(TILES[name])
            result[name] = list(self.lines())
            self.tap(BACK)
        return result

    def enter_values(self) -> None:
        self.tap(BTN_EN)
        self.tap(TILES["conv"])
        for key in "78":
            self.tap(KEYS[key])
        self.tap(BACK)
        self.tap(TILES["ohm"])
        self.tap(KEYS["9"])
        self.tap(BACK)

    def test_blob_has_the_documented_size(self) -> None:
        self.assertEqual(len(self.get()), SETTINGS_SIZE)

    def test_untouched_app_reports_no_change(self) -> None:
        self.step(5)
        self.assertFalse(self.poll())

    def test_change_is_reported_once(self) -> None:
        self.tap(TILES["conv"])
        self.tap(KEYS["7"])
        self.assertTrue(self.poll())
        self.assertFalse(self.poll())

    def test_language_change_is_a_change(self) -> None:
        self.tap(BTN_EN)
        self.assertTrue(self.poll())

    def test_values_survive_a_restart(self) -> None:
        self.enter_values()
        blob = self.get()
        before = self.snapshot()
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.lines()[0], "CONVERTITORE")       # back to the defaults
        self.apply(blob)
        self.assertEqual(self.lines()[0], "CONVERTER")           # language restored
        self.assertEqual(self.snapshot(), before)

    def test_applying_is_not_reported_as_a_change(self) -> None:
        self.enter_values()
        blob = self.get()
        self.assertTrue(self.poll())
        self.rt.control(CONTROL_RESET)
        self.apply(blob)
        self.assertFalse(self.poll())

    def test_erased_flash_values_are_ignored(self) -> None:
        before = self.snapshot()
        self.apply(b"\xff" * SETTINGS_SIZE)
        self.assertEqual(self.snapshot(), before)
        self.assertEqual(self.lines()[0], "CONVERTITORE")

    def test_zero_blob_does_not_break_the_app(self) -> None:
        self.apply(bytes(SETTINGS_SIZE))
        for name in TILES:
            self.assertEqual(self.tap(TILES[name]), name)
            self.tap(BACK)

    def test_text_that_is_not_a_number_is_dropped(self) -> None:
        blob = bytearray(self.get())
        offset = SETTINGS_SIZE - 88              # the first entry text (converter)
        blob[offset:offset + 11] = b"12abc\x00\x00\x00\x00\x00\x00"
        self.apply(bytes(blob))
        self.tap(TILES["conv"])
        entry = self.lines()[4]
        self.assertEqual(entry, "")              # not a number: the whole text is ignored

    def test_unterminated_text_is_cut(self) -> None:
        blob = bytearray(self.get())
        offset = SETTINGS_SIZE - 88
        blob[offset:offset + 11] = b"12345678901"      # fills all 11 bytes: no terminator
        self.apply(bytes(blob))
        self.tap(TILES["conv"])
        self.assertEqual(len(self.lines()[4].replace(" ", "")), 10)

    def test_level_zero_and_mode_are_restored(self) -> None:
        import math
        self.tap(TILES["livella"])
        sx, cz = math.sin(math.radians(5)), math.cos(math.radians(5))
        self.rt.imu(sx, 0.0, cz)
        self.step(40)
        self.tap((120, 438))                     # zero
        self.tap((360, 70))                      # edge mode
        self.tap(BACK)
        blob = self.get()
        self.rt.control(CONTROL_RESET)
        self.apply(blob)
        self.tap(TILES["livella"])
        self.rt.imu(0.0, 1.0, 0.0)
        self.step(40)
        self.assertEqual(self.lines()[4], "VERTICALE")    # edge mode came back
        self.tap((120, 70))                      # flat mode, zero kept
        self.rt.imu(sx, 0.0, cz)
        self.step(40)
        self.assertEqual(self.lines()[0], "0,0°")

    def test_impossible_colours_are_ignored(self) -> None:
        blob = bytearray(self.get())
        # colour_bands is at offset 15; set an out-of-range band colour at offset 17+2.
        blob[19] = 200
        self.apply(bytes(blob))
        self.tap(TILES["colori"])
        self.assertEqual(self.lines()[0], "4,7 kΩ")        # the default resistor


if __name__ == "__main__":
    unittest.main()
