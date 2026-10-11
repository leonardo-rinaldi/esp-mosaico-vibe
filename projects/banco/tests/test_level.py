# SPDX-License-Identifier: Apache-2.0
"""Tests for the spirit level: the pure maths in main/level.c and the screen,
fed with simulated accelerometer samples through the Host simulator.

    python -m unittest discover -s projects/banco/tests -v
"""
from __future__ import annotations

import ctypes
import math
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


def tilt(degrees: float) -> tuple[float, float]:
    """Gravity components (g) for a board leaning by `degrees`."""
    return math.sin(math.radians(degrees)), math.cos(math.radians(degrees))


class LevelMaths(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc")) or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.tmp = tempfile.TemporaryDirectory()
        library = Path(cls.tmp.name) / "liblevel.so"
        subprocess.run([compiler, "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra",
                        "-Werror", str(MAIN / "level.c"), "-lm", "-o", str(library)],
                       check=True, capture_output=True, text=True)
        lib = ctypes.CDLL(str(library))
        d = ctypes.c_double
        lib.level_flat_angles.argtypes = [d, d, d, ctypes.POINTER(d), ctypes.POINTER(d)]
        lib.level_flat_angles.restype = ctypes.c_bool
        lib.level_edge_angle.argtypes = [d, d, d, ctypes.POINTER(d)]
        lib.level_edge_angle.restype = ctypes.c_bool
        lib.level_slope_percent.argtypes = [d]
        lib.level_slope_percent.restype = d
        cls.lib = lib

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def flat(self, ax, ay, az):
        roll, pitch = ctypes.c_double(), ctypes.c_double()
        ok = self.lib.level_flat_angles(ax, ay, az, ctypes.byref(roll), ctypes.byref(pitch))
        return ok, roll.value, pitch.value

    def edge(self, ax, ay, az):
        angle = ctypes.c_double()
        return self.lib.level_edge_angle(ax, ay, az, ctypes.byref(angle)), angle.value

    def test_flat_board_reads_zero(self) -> None:
        ok, roll, pitch = self.flat(0.0, 0.0, 1.0)
        self.assertTrue(ok)
        self.assertAlmostEqual(roll, 0.0)
        self.assertAlmostEqual(pitch, 0.0)

    def test_flat_tilt_angles(self) -> None:
        # 5 degrees about each axis: atan2(sin 5, cos 5) is exactly 5.
        sx, cz = tilt(5.0)
        ok, roll, pitch = self.flat(sx, 0.0, cz)
        self.assertTrue(ok)
        self.assertAlmostEqual(roll, 5.0, places=6)
        self.assertAlmostEqual(pitch, 0.0, places=6)
        ok, roll, pitch = self.flat(0.0, -sx, cz)
        self.assertAlmostEqual(pitch, -5.0, places=6)

    def test_flat_upside_down_board_reads_small_angles(self) -> None:
        sx, cz = tilt(5.0)
        ok, roll, _ = self.flat(sx, 0.0, -cz)
        self.assertTrue(ok)
        self.assertAlmostEqual(roll, 5.0, places=6)

    def test_no_gravity_is_rejected(self) -> None:
        self.assertFalse(self.flat(0.0, 0.0, 0.0)[0])
        self.assertFalse(self.flat(0.1, 0.1, 0.1)[0])
        self.assertFalse(self.edge(0.0, 0.0, 0.0)[0])

    def test_edge_angle_from_vertical(self) -> None:
        sx, cy = tilt(10.0)
        ok, angle = self.edge(sx, cy, 0.0)
        self.assertTrue(ok)
        self.assertAlmostEqual(angle, 10.0, places=6)
        ok, angle = self.edge(-sx, cy, 0.0)
        self.assertAlmostEqual(angle, -10.0, places=6)

    def test_edge_angle_folds_when_held_upside_down(self) -> None:
        sx, cy = tilt(10.0)
        ok, angle = self.edge(-sx, -cy, 0.0)        # same pipe, board turned around
        self.assertAlmostEqual(angle, 10.0, places=6)
        ok, angle = self.edge(sx, -cy, 0.0)
        self.assertAlmostEqual(angle, -10.0, places=6)

    def test_slope_percent(self) -> None:
        self.assertAlmostEqual(self.lib.level_slope_percent(0.0), 0.0)
        self.assertAlmostEqual(self.lib.level_slope_percent(45.0), 100.0, places=6)
        self.assertAlmostEqual(self.lib.level_slope_percent(-45.0), -100.0, places=6)
        self.assertAlmostEqual(self.lib.level_slope_percent(math.degrees(math.atan(0.02))), 2.0, places=6)

    def test_slope_is_clamped_near_vertical(self) -> None:
        self.assertEqual(self.lib.level_slope_percent(90.0), 9999.0)
        self.assertEqual(self.lib.level_slope_percent(-89.5), -9999.0)


if not (ENGINE_HOST / "run_game.py").is_file():
    HOST_AVAILABLE = False
else:
    HOST_AVAILABLE = True
    sys.path.insert(0, str(ENGINE_HOST))
    import run_game  # noqa: E402

CONTROL_RESET = 3
LEVEL_TILE = (240, 400)
BACK = (60, 20)
MODE_FLAT, MODE_EDGE = (120, 70), (360, 70)
ZERO, RESET_ZERO = (120, 438), (360, 438)
BTN_EN = (446, 28)


@unittest.skipUnless(HOST_AVAILABLE, "initialize submodule/raylib-lite-engine to run these tests")
class LevelScreen(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        self.step()
        self.assertEqual(self.tap(LEVEL_TILE), "livella")

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

    def gravity(self, ax: float, ay: float, az: float, frames: int = 40) -> None:
        self.rt.imu(ax, ay, az)
        self.step(frames)

    def test_no_sensor_data_is_reported_not_shown_as_level(self) -> None:
        self.step(5)
        lines = self.lines()
        self.assertEqual(lines[0], "")
        self.assertEqual(lines[4], "NESSUN DATO DAL SENSORE")

    def test_flat_and_level(self) -> None:
        self.gravity(0.0, 0.0, 1.0)
        self.assertEqual(self.lines(), ["0,0°", "0,0°", "0,0 %", "0,0 %", "IN LIVELLA"])

    def test_tilt_shows_angles_and_slope(self) -> None:
        sx, cz = tilt(5.0)
        self.gravity(sx, 0.0, cz)
        lines = self.lines()
        self.assertEqual(lines[0], "+5,0°")
        self.assertEqual(lines[1], "0,0°")
        self.assertEqual(lines[2], "+8,7 %")
        self.assertEqual(lines[4], "")              # not level, so no status text

    def test_reading_follows_the_filter_not_a_jump(self) -> None:
        self.gravity(0.0, 0.0, 1.0, frames=5)
        sx, cz = tilt(10.0)
        self.rt.imu(sx, 0.0, cz)
        self.step(1)
        first = self.lines()[0]
        self.step(60)
        self.assertNotEqual(first, "+10,0°")        # still on its way after one frame
        self.assertEqual(self.lines()[0], "+10,0°")

    def test_zero_button_takes_the_current_tilt_as_reference(self) -> None:
        sx, cz = tilt(5.0)
        self.gravity(sx, 0.0, cz)
        self.tap(ZERO)
        self.step(2)
        self.assertEqual(self.lines()[0], "0,0°")
        self.assertEqual(self.lines()[4], "IN LIVELLA")
        self.gravity(0.0, 0.0, 1.0)
        self.assertEqual(self.lines()[0], "-5,0°")
        self.tap(RESET_ZERO)
        self.step(2)
        self.assertEqual(self.lines()[0], "0,0°")

    def test_edge_mode_reads_one_angle_from_vertical(self) -> None:
        self.tap(MODE_EDGE)
        sx, cy = tilt(10.0)
        self.gravity(sx, cy, 0.0)
        lines = self.lines()
        self.assertEqual(lines[0], "+10,0°")
        self.assertEqual(lines[1], "")
        self.assertEqual(lines[2], "+17,6 %")
        self.gravity(0.0, 1.0, 0.0)
        self.assertEqual(self.lines()[4], "VERTICALE")

    def test_zero_is_kept_per_mode(self) -> None:
        sx, cz = tilt(5.0)
        self.gravity(sx, 0.0, cz)
        self.tap(ZERO)
        self.tap(MODE_EDGE)
        self.gravity(0.0, 1.0, 0.0)
        self.assertEqual(self.lines()[0], "0,0°")
        self.tap(MODE_FLAT)
        self.gravity(sx, 0.0, cz)
        self.assertEqual(self.lines()[0], "0,0°")   # the flat zero is still stored

    def test_sensor_dropout_goes_back_to_no_data(self) -> None:
        self.gravity(0.0, 0.0, 1.0)
        self.gravity(0.0, 0.0, 0.0, frames=3)
        self.assertEqual(self.lines()[4], "NESSUN DATO DAL SENSORE")

    def test_english_texts(self) -> None:
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(LEVEL_TILE)
        self.step(2)
        self.assertEqual(self.lines()[4], "NO SENSOR DATA")
        self.gravity(0.0, 0.0, 1.0)
        self.assertEqual(self.lines()[4], "LEVEL")

    def test_back_returns_to_menu(self) -> None:
        self.assertEqual(self.tap(BACK), "menu")

    def test_level_ignores_the_sensor_on_other_screens(self) -> None:
        self.tap(BACK)
        self.rt.imu(0.3, 0.3, 0.9)
        self.step(30)
        self.assertEqual(self.rt.metadata()["screen"], "menu")


if __name__ == "__main__":
    unittest.main()
