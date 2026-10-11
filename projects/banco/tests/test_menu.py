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
TILES = [((120, 140), "conv"), ((360, 140), "ohm"), ((120, 270), "colori"), ((360, 270), "cavo"),
         ((120, 400), "livella"), ((360, 400), "lente")]
BACK = (60, 20)
TITLE = (300, 20)
BTN_IT = (394, 28)
BTN_EN = (446, 28)


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
        self.assertEqual(self.tap((240, 270)), "menu")

    def test_reset_returns_to_menu(self) -> None:
        self.tap(TILES[1][0])
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.rt.metadata()["screen"], "menu")


class Language(MenuNavigation):
    def lines(self) -> list:
        return self.rt.metadata()["lines"]

    def test_starts_in_italian(self) -> None:
        self.assertEqual(self.lines()[:2], ["CONVERTITORE", "LEGGE DI OHM"])

    def test_english_button_translates_menu_and_italian_restores_it(self) -> None:
        self.assertEqual(self.tap(BTN_EN), "menu")
        self.assertEqual(self.lines()[:6], ["CONVERTER", "OHMS LAW", "COLOR CODE", "CABLE", "LEVEL", "LENS"])
        self.tap(BTN_IT)
        self.assertEqual(self.lines()[0], "CONVERTITORE")

    def test_language_buttons_do_not_open_a_tile(self) -> None:
        self.assertEqual(self.tap(BTN_EN), "menu")
        self.assertEqual(self.tap(BTN_IT), "menu")

    def test_english_hint_on_ohm_screen(self) -> None:
        self.tap(BTN_EN)
        self.tap(TILES[1][0])
        self.assertIn("ENTER TWO VALUES", self.lines())

    def test_english_cable_and_colour_texts(self) -> None:
        self.tap(BTN_EN)
        self.tap(TILES[3][0])
        self.assertIn("ENTER CURRENT AND LENGTH", self.lines())
        self.tap(BACK)
        self.tap(TILES[2][0])
        self.assertIn("BAND 1: DIGIT", self.lines())

    def test_switching_language_keeps_entered_values(self) -> None:
        self.tap(TILES[1][0])
        self.tap((60, 400))     # a keypad key; any digit press changes the first row
        before = self.lines()[:4]
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(TILES[1][0])
        self.assertEqual(self.lines()[:4], before)

    def test_reset_returns_to_italian(self) -> None:
        self.tap(BTN_EN)
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.lines()[0], "CONVERTITORE")


if __name__ == "__main__":
    unittest.main()
