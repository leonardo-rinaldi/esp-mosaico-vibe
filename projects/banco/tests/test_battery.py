# SPDX-License-Identifier: Apache-2.0
"""Tests for the battery indicator: the judging functions in main/power_status.c
(through ctypes) and the menu header and battery screen, fed with readings through
the game's exported game_set_battery.

    python -m unittest discover -s projects/banco/tests -v
"""
from __future__ import annotations

import ctypes
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "banco"
MAIN = PROJECT / "main"
ENGINE_HOST = ROOT / "submodule" / "raylib-lite-engine" / "host"

UNKNOWN, ABSENT, CHARGING, DISCHARGING, IDLE = range(5)
OK, LOW, CRITICAL = range(3)
TIME_UNKNOWN = 0xFFFF


class Sample(ctypes.Structure):
    _fields_ = [("valid", ctypes.c_bool), ("percent", ctypes.c_uint8),
                ("voltage_mv", ctypes.c_int16), ("current_ma", ctypes.c_int16),
                ("temperature_dc", ctypes.c_int16), ("minutes_to_empty", ctypes.c_uint16),
                ("minutes_to_full", ctypes.c_uint16)]


def sample(percent=80, mv=3900, ma=-100, temp=245, empty=150, full=TIME_UNKNOWN, valid=True) -> Sample:
    return Sample(valid, percent, mv, ma, temp, empty, full)


class Judging(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc")) or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.tmp = tempfile.TemporaryDirectory()
        library = Path(cls.tmp.name) / "libpower.so"
        subprocess.run([compiler, "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        str(MAIN / "power_status.c"), "-o", str(library)],
                       check=True, capture_output=True, text=True)
        lib = ctypes.CDLL(str(library))
        lib.battery_state.argtypes = [ctypes.POINTER(Sample)]
        lib.battery_state.restype = ctypes.c_int
        lib.battery_level.argtypes = [ctypes.POINTER(Sample)]
        lib.battery_level.restype = ctypes.c_int
        lib.battery_minutes_remaining.argtypes = [ctypes.POINTER(Sample), ctypes.POINTER(ctypes.c_uint16)]
        lib.battery_minutes_remaining.restype = ctypes.c_bool
        cls.lib = lib

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def state(self, s): return self.lib.battery_state(ctypes.byref(s))
    def level(self, s): return self.lib.battery_level(ctypes.byref(s))

    def minutes(self, s):
        out = ctypes.c_uint16()
        return (self.lib.battery_minutes_remaining(ctypes.byref(s), ctypes.byref(out)), out.value)

    def test_layout_matches_the_c_struct(self) -> None:
        self.assertEqual(ctypes.sizeof(Sample), 12)

    def test_no_reading_is_unknown(self) -> None:
        self.assertEqual(self.state(sample(valid=False)), UNKNOWN)

    def test_missing_cell_is_absent(self) -> None:
        self.assertEqual(self.state(sample(mv=0)), ABSENT)
        self.assertEqual(self.state(sample(mv=2499)), ABSENT)
        self.assertNotEqual(self.state(sample(mv=2500)), ABSENT)

    def test_current_decides_charging_or_discharging(self) -> None:
        self.assertEqual(self.state(sample(ma=120)), CHARGING)
        self.assertEqual(self.state(sample(ma=-120)), DISCHARGING)
        self.assertEqual(self.state(sample(ma=6)), CHARGING)
        self.assertEqual(self.state(sample(ma=-6)), DISCHARGING)

    def test_tiny_current_is_idle(self) -> None:
        for ma in (0, 5, -5, 3, -3):
            self.assertEqual(self.state(sample(ma=ma)), IDLE, ma)

    def test_levels(self) -> None:
        self.assertEqual(self.level(sample(percent=100)), OK)
        self.assertEqual(self.level(sample(percent=20)), OK)
        self.assertEqual(self.level(sample(percent=19)), LOW)
        self.assertEqual(self.level(sample(percent=10)), LOW)
        self.assertEqual(self.level(sample(percent=9)), CRITICAL)
        self.assertEqual(self.level(sample(percent=0)), CRITICAL)

    def test_no_level_judgement_without_a_cell(self) -> None:
        self.assertEqual(self.level(sample(percent=0, mv=0)), OK)
        self.assertEqual(self.level(sample(valid=False, percent=0)), OK)

    def test_minutes_follow_the_direction(self) -> None:
        self.assertEqual(self.minutes(sample(ma=-100, empty=95, full=7)), (True, 95))
        self.assertEqual(self.minutes(sample(ma=100, empty=95, full=42)), (True, 42))
        self.assertEqual(self.minutes(sample(ma=0, empty=95, full=42))[0], False)

    def test_unknown_minutes_are_hidden(self) -> None:
        self.assertFalse(self.minutes(sample(ma=-100, empty=TIME_UNKNOWN))[0])
        self.assertFalse(self.minutes(sample(ma=100, full=TIME_UNKNOWN))[0])


HOST_AVAILABLE = (ENGINE_HOST / "run_game.py").is_file()
if HOST_AVAILABLE:
    sys.path.insert(0, str(ENGINE_HOST))
    import run_game  # noqa: E402

BATTERY_BUTTON = (60, 28)
BACK = (60, 20)
BTN_EN = (446, 28)
CONTROL_RESET = 3


@unittest.skipUnless(HOST_AVAILABLE, "initialize submodule/raylib-lite-engine to run these tests")
class BatteryScreen(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        state = ctypes.c_void_p.from_address(self.rt.context + ctypes.sizeof(ctypes.c_void_p)).value
        self.game = ctypes.c_void_p.from_address(state).value
        self.rt.api.game_set_battery.argtypes = [ctypes.c_void_p, ctypes.POINTER(Sample)]
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

    def feed(self, s: Sample) -> None:
        self.rt.api.game_set_battery(self.game, ctypes.byref(s))
        self.step()

    def test_menu_shows_no_data_before_a_reading(self) -> None:
        self.assertEqual(self.lines()[5], "-")

    def test_menu_shows_the_percentage(self) -> None:
        self.feed(sample(percent=78))
        self.assertEqual(self.lines()[5], "78 %")

    def test_menu_marks_charging(self) -> None:
        self.feed(sample(percent=78, ma=250))
        self.assertEqual(self.lines()[5], "+78 %")

    def test_menu_shows_nothing_for_a_missing_cell(self) -> None:
        self.feed(sample(mv=0, percent=0))
        self.assertEqual(self.lines()[5], "-")

    def test_tapping_the_indicator_opens_the_screen_and_back_returns(self) -> None:
        self.assertEqual(self.tap(BATTERY_BUTTON), "batteria")
        self.assertEqual(self.tap(BACK), "menu")

    def test_screen_without_reading(self) -> None:
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[:2], ["-", "NESSUN DATO"])

    def test_screen_shows_every_number(self) -> None:
        self.feed(sample(percent=64, mv=3870, ma=-120, temp=245, empty=185))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines(), ["64 %", "A BATTERIA", "3,87 V", "-120 mA", "24,5 °C",
                                         "3 H 05 MIN", ""])

    def test_charging_screen_shows_time_to_full(self) -> None:
        self.feed(sample(percent=40, ma=300, full=45, empty=TIME_UNKNOWN))
        self.tap(BATTERY_BUTTON)
        lines = self.lines()
        self.assertEqual(lines[1], "IN CARICA")
        self.assertEqual(lines[3], "+300 mA")
        self.assertEqual(lines[5], "45 MIN")

    def test_full_battery_on_the_charger(self) -> None:
        self.feed(sample(percent=100, ma=0))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[1], "CARICA COMPLETA")

    def test_low_and_critical_warnings(self) -> None:
        self.feed(sample(percent=15))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[6], "BATTERIA QUASI SCARICA")
        self.feed(sample(percent=5))
        self.assertEqual(self.lines()[6], "COLLEGA IL CARICATORE")

    def test_no_warning_while_charging(self) -> None:
        self.feed(sample(percent=5, ma=300))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[6], "")

    def test_missing_cell_message(self) -> None:
        self.feed(sample(mv=0, percent=0))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[:2], ["-", "NESSUNA BATTERIA"])

    def test_screen_updates_while_open(self) -> None:
        self.tap(BATTERY_BUTTON)
        self.feed(sample(percent=50))
        self.assertEqual(self.lines()[0], "50 %")
        self.feed(sample(percent=49))
        self.assertEqual(self.lines()[0], "49 %")

    def test_english_texts(self) -> None:
        self.tap(BTN_EN)
        self.feed(sample(percent=64, ma=-120, empty=185))
        self.tap(BATTERY_BUTTON)
        self.assertEqual(self.lines()[1], "ON BATTERY")
        self.assertEqual(self.lines()[5], "3 H 05 MIN")

    def test_reset_keeps_the_last_reading(self) -> None:
        self.feed(sample(percent=64))
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.lines()[5], "64 %")

    def test_percentage_is_clamped(self) -> None:
        self.feed(sample(percent=250))
        self.assertEqual(self.lines()[5], "100 %")


if __name__ == "__main__":
    unittest.main()
