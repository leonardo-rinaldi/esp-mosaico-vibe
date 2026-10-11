# SPDX-License-Identifier: Apache-2.0
"""Tests for the lens screen (zoom, freeze, camera status), run through the Host
simulator. The Host has no camera: the picture is a test pattern, and the camera
side is played by calling the game's exported camera functions."""
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

CONTROL_RESET = 3
LENS_TILE = (360, 400)
BACK = (60, 20)
MINUS, PLUS = (50, 452), (250, 452)
FREEZE = (390, 452)
PICTURE = (240, 200)
BTN_EN = (446, 28)

STARTING, LIVE, NO_CAMERA, ERROR = 0, 1, 2, 3


class Request(ctypes.Structure):
    _fields_ = [("active", ctypes.c_bool), ("zoom", ctypes.c_uint8), ("frozen", ctypes.c_bool)]


class LensScreen(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        state = ctypes.c_void_p.from_address(self.rt.context + ctypes.sizeof(ctypes.c_void_p)).value
        self.game = ctypes.c_void_p.from_address(state).value
        self.rt.api.game_set_lens_status.argtypes = [ctypes.c_void_p, ctypes.c_int]
        self.rt.api.game_lens_request.argtypes = [ctypes.c_void_p, ctypes.POINTER(Request)]
        self.step()
        self.assertEqual(self.tap(LENS_TILE), "lente")

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

    def status(self, value: int) -> None:
        self.rt.api.game_set_lens_status(self.game, value)
        self.step()

    def request(self) -> Request:
        request = Request()
        self.rt.api.game_lens_request(self.game, ctypes.byref(request))
        return request

    def test_opens_looking_for_the_camera(self) -> None:
        self.assertEqual(self.lines(), ["X1", "CERCO LA FOTOCAMERA...", "", "FERMA"])

    def test_back_returns_to_the_menu(self) -> None:
        self.assertEqual(self.tap(BACK), "menu")

    def test_request_follows_the_screen(self) -> None:
        request = self.request()
        self.assertEqual((request.active, request.zoom, request.frozen), (True, 1, False))
        self.tap(BACK)
        self.assertFalse(self.request().active)

    def test_live_shows_no_status_text(self) -> None:
        self.status(LIVE)
        self.assertEqual(self.lines(), ["X1", "", "", "FERMA"])

    def test_no_camera_explains_where_to_plug_it(self) -> None:
        self.status(NO_CAMERA)
        self.assertEqual(self.lines()[1:3], ["NESSUNA FOTOCAMERA", "MODULO NELLO SLOT SINISTRO"])

    def test_error_text(self) -> None:
        self.status(ERROR)
        self.assertEqual(self.lines()[1], "ERRORE FOTOCAMERA")

    def test_zoom_steps_and_stops_at_the_limits(self) -> None:
        self.tap(MINUS)
        self.assertEqual(self.lines()[0], "X1")
        for expected in ("X2", "X3", "X4", "X4"):
            self.tap(PLUS)
            self.assertEqual(self.lines()[0], expected)
            self.assertEqual(self.request().zoom, int(expected[1]))
        self.tap(MINUS)
        self.assertEqual(self.lines()[0], "X3")

    def test_zoom_changes_the_picture(self) -> None:
        self.status(LIVE)
        before = self.rt.frame()
        self.tap(PLUS)
        self.assertNotEqual(before, self.rt.frame())

    def test_freeze_button_and_picture_tap_toggle(self) -> None:
        self.status(LIVE)
        self.tap(FREEZE)
        self.assertEqual(self.lines()[1], "IMMAGINE FERMA")
        self.assertEqual(self.lines()[3], "RIPRENDI")
        self.assertTrue(self.request().frozen)
        self.tap(PICTURE)
        self.assertEqual(self.lines()[1], "")
        self.assertFalse(self.request().frozen)
        self.tap(PICTURE)
        self.assertTrue(self.request().frozen)

    def test_reopening_unfreezes_and_keeps_the_zoom(self) -> None:
        self.tap(PLUS)
        self.tap(FREEZE)
        self.tap(BACK)
        self.assertEqual(self.tap(LENS_TILE), "lente")
        self.assertEqual(self.lines()[0], "X2")
        self.assertFalse(self.request().frozen)
        self.assertEqual(self.lines()[1], "CERCO LA FOTOCAMERA...")

    def test_english(self) -> None:
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(LENS_TILE)
        self.assertEqual(self.lines(), ["X1", "LOOKING FOR CAMERA...", "", "FREEZE"])
        self.status(NO_CAMERA)
        self.assertEqual(self.lines()[1:3], ["NO CAMERA", "MODULE IN THE LEFT SLOT"])

    def test_reset_returns_to_the_menu(self) -> None:
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.rt.metadata()["screen"], "menu")


if __name__ == "__main__":
    unittest.main()
