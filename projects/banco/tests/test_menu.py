# SPDX-License-Identifier: Apache-2.0
"""Navigation tests for the banco home menu, run through the Host simulator.

    python -m unittest discover -s projects/banco/tests -v
"""
from __future__ import annotations

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

CONTROL_RESET = 3
TILES = [((120, 180), "conv"), ((360, 180), "ohm"), ((120, 380), "colori"), ((360, 380), "cavo")]
BACK = (60, 20)
TITLE = (300, 20)


class MenuNavigation(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        self.step()

    def step(self) -> None:
        self.rt.step(False, False, False)

    def tap(self, point) -> str:
        self.rt.pointer(0, point[0], point[1], True)
        self.step()
        self.rt.pointer(0, point[0], point[1], False)
        self.step()
        return self.rt.metadata()["screen"]

    def test_starts_on_menu(self) -> None:
        self.assertEqual(self.rt.metadata()["screen"], "menu")

    def test_each_tile_opens_its_function_and_back_returns(self) -> None:
        for point, name in TILES:
            with self.subTest(tile=name):
                self.assertEqual(self.tap(point), name)
                self.assertEqual(self.tap(BACK), "menu")

    def test_title_area_does_not_navigate(self) -> None:
        self.assertEqual(self.tap(TITLE), "menu")
        self.tap(TILES[0][0])
        self.assertEqual(self.tap(TITLE), "conv")

    def test_gap_between_tiles_does_nothing(self) -> None:
        self.assertEqual(self.tap((240, 280)), "menu")

    def test_reset_returns_to_menu(self) -> None:
        self.tap(TILES[1][0])
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.rt.metadata()["screen"], "menu")


if __name__ == "__main__":
    unittest.main()
