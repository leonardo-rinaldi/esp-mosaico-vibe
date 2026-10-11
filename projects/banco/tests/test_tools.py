# SPDX-License-Identifier: Apache-2.0
"""Tests for the bench tools: AWG, LED resistor, voltage divider and SMD codes. The maths in
main/calc_bench.c is checked through ctypes against known reference values; the four screens
run in the Host simulator.

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

SMD_OK, SMD_INCOMPLETE, SMD_INVALID = range(3)
dbl = ctypes.c_double


class LedInput(ctypes.Structure):
    _fields_ = [("supply_v", dbl), ("led_v", dbl), ("current_a", dbl)]


class LedResult(ctypes.Structure):
    _fields_ = [("ideal_ohm", dbl), ("std_ohm", dbl), ("actual_a", dbl), ("power_w", dbl)]


class Maths(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        compiler = shutil.which(os.environ.get("CC", "cc")) or shutil.which("gcc")
        if not compiler:
            raise unittest.SkipTest("a C compiler is required")
        cls.tmp = tempfile.TemporaryDirectory()
        library = Path(cls.tmp.name) / "libbench.so"
        subprocess.run([compiler, "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        str(MAIN / "calc_bench.c"), "-o", str(library), "-lm"],
                       check=True, capture_output=True, text=True)
        lib = ctypes.CDLL(str(library))
        for name in ("awg_diameter_mm", "awg_area_mm2"):
            getattr(lib, name).argtypes = [ctypes.c_int]
            getattr(lib, name).restype = dbl
        lib.awg_nearest.argtypes = [dbl]
        lib.awg_nearest.restype = ctypes.c_int
        for name in ("metric_section_at_least", "e24_at_least", "e24_nearest"):
            getattr(lib, name).argtypes = [dbl]
            getattr(lib, name).restype = dbl
        lib.power_rating_label.argtypes = [dbl]
        lib.power_rating_label.restype = ctypes.c_char_p
        lib.calc_led_resistor.argtypes = [ctypes.POINTER(LedInput), ctypes.POINTER(LedResult)]
        lib.calc_led_resistor.restype = ctypes.c_bool
        lib.divider_vout.argtypes = [dbl, dbl, dbl]
        lib.divider_vout.restype = dbl
        lib.divider_r2.argtypes = [dbl, dbl, dbl, ctypes.POINTER(dbl)]
        lib.divider_r2.restype = ctypes.c_bool
        lib.eia96_base.argtypes = [ctypes.c_int]
        lib.eia96_base.restype = dbl
        lib.eia96_multiplier.argtypes = [ctypes.c_char]
        lib.eia96_multiplier.restype = dbl
        for name in ("smd_resistor", "smd_capacitor"):
            getattr(lib, name).argtypes = [ctypes.c_char_p, ctypes.POINTER(dbl)]
            getattr(lib, name).restype = ctypes.c_int
        lib.smd_eia96.argtypes = [ctypes.c_char_p, ctypes.c_char, ctypes.POINTER(dbl)]
        lib.smd_eia96.restype = ctypes.c_int
        cls.lib = lib

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    def resistor(self, text):
        out = dbl()
        status = self.lib.smd_resistor(text.encode(), ctypes.byref(out))
        return status, out.value

    def capacitor(self, text):
        out = dbl()
        status = self.lib.smd_capacitor(text.encode(), ctypes.byref(out))
        return status, out.value

    def eia96(self, digits, letter):
        out = dbl()
        status = self.lib.smd_eia96(digits.encode(), letter.encode(), ctypes.byref(out))
        return status, out.value

    # AWG: reference table values (mm and mm²).
    def test_awg_reference_sizes(self) -> None:
        reference = {40: (0.0799, 0.0050), 30: (0.2546, 0.0509), 20: (0.8118, 0.5176), 14: (1.6277, 2.0809),
                     12: (2.0525, 3.3088), 10: (2.5882, 5.2612)}
        for awg, (diameter, area) in reference.items():
            with self.subTest(awg=awg):
                self.assertAlmostEqual(self.lib.awg_diameter_mm(awg), diameter, delta=0.0005)
                self.assertAlmostEqual(self.lib.awg_area_mm2(awg), area, delta=max(0.0005, area * 0.002))

    def test_awg_large_sizes(self) -> None:
        self.assertAlmostEqual(self.lib.awg_area_mm2(0), 53.49, delta=0.05)       # 1/0
        self.assertAlmostEqual(self.lib.awg_area_mm2(-1), 67.43, delta=0.05)      # 2/0
        self.assertAlmostEqual(self.lib.awg_area_mm2(-3), 107.2, delta=0.1)       # 4/0

    def test_awg_nearest_and_metric_size(self) -> None:
        self.assertEqual(self.lib.awg_nearest(2.5), 13)
        self.assertEqual(self.lib.awg_nearest(self.lib.awg_area_mm2(8)), 8)
        self.assertEqual(self.lib.awg_nearest(1000.0), -3)
        self.assertEqual(self.lib.awg_nearest(0.0001), 40)
        self.assertEqual(self.lib.metric_section_at_least(2.0809), 2.5)
        self.assertEqual(self.lib.metric_section_at_least(2.5), 2.5)
        self.assertEqual(self.lib.metric_section_at_least(400.0), 0.0)

    # Standard values and ratings.
    def test_e24(self) -> None:
        self.assertAlmostEqual(self.lib.e24_at_least(143), 150, places=6)
        self.assertAlmostEqual(self.lib.e24_at_least(150), 150, places=6)
        self.assertAlmostEqual(self.lib.e24_at_least(951), 1000, places=6)
        self.assertAlmostEqual(self.lib.e24_at_least(0.047), 0.047, places=9)
        self.assertEqual(self.lib.e24_at_least(0), 0)
        self.assertAlmostEqual(self.lib.e24_nearest(19411.8), 20000, places=3)
        self.assertAlmostEqual(self.lib.e24_nearest(9300), 9100, places=3)

    def test_power_rating(self) -> None:
        self.assertEqual(self.lib.power_rating_label(0.06), b"1/8 W")
        self.assertEqual(self.lib.power_rating_label(0.1), b"1/4 W")
        self.assertEqual(self.lib.power_rating_label(0.4), b"1 W")
        self.assertEqual(self.lib.power_rating_label(10.0), b">5 W")

    # LED resistor.
    def test_led_resistor(self) -> None:
        out = LedResult()
        self.assertTrue(self.lib.calc_led_resistor(ctypes.byref(LedInput(5, 2, 0.02)), ctypes.byref(out)))
        self.assertAlmostEqual(out.ideal_ohm, 150)
        self.assertAlmostEqual(out.std_ohm, 150)
        self.assertAlmostEqual(out.power_w, 0.06)

    def test_led_resistor_rounds_up_so_the_current_stays_below(self) -> None:
        out = LedResult()
        self.assertTrue(self.lib.calc_led_resistor(ctypes.byref(LedInput(12, 3.2, 0.015)), ctypes.byref(out)))
        self.assertAlmostEqual(out.ideal_ohm, 586.667, places=2)
        self.assertAlmostEqual(out.std_ohm, 620)
        self.assertLess(out.actual_a, 0.015)

    def test_led_resistor_rejects_impossible_inputs(self) -> None:
        out = LedResult()
        for supply, led, current in ((2, 2, 0.02), (1.8, 2, 0.02), (5, 2, 0), (0, 2, 0.02)):
            with self.subTest(supply=supply, led=led, current=current):
                self.assertFalse(self.lib.calc_led_resistor(ctypes.byref(LedInput(supply, led, current)),
                                                            ctypes.byref(out)))

    # Divider.
    def test_divider(self) -> None:
        self.assertAlmostEqual(self.lib.divider_vout(5, 10000, 10000), 2.5)
        self.assertAlmostEqual(self.lib.divider_vout(12, 30000, 10000), 3.0)
        r2 = dbl()
        self.assertTrue(self.lib.divider_r2(5, 3.3, 10000, ctypes.byref(r2)))
        self.assertAlmostEqual(r2.value, 19411.76, places=1)
        self.assertAlmostEqual(self.lib.divider_vout(5, 10000, r2.value), 3.3)

    def test_divider_r2_rejects_impossible_outputs(self) -> None:
        r2 = dbl()
        for vin, vout, r1 in ((5, 5, 1000), (5, 6, 1000), (5, 0, 1000), (5, 2, 0)):
            with self.subTest(vin=vin, vout=vout, r1=r1):
                self.assertFalse(self.lib.divider_r2(vin, vout, r1, ctypes.byref(r2)))

    # SMD resistors.
    def test_smd_resistor_three_and_four_digits(self) -> None:
        self.assertEqual(self.resistor("103"), (SMD_OK, 10000))
        self.assertEqual(self.resistor("472"), (SMD_OK, 4700))
        self.assertEqual(self.resistor("1002"), (SMD_OK, 10000))
        self.assertEqual(self.resistor("4701"), (SMD_OK, 4700))
        self.assertEqual(self.resistor("000"), (SMD_OK, 0))
        self.assertEqual(self.resistor("100"), (SMD_OK, 10))
        self.assertEqual(self.resistor("509")[1], 5.0)         # last digit 9 means x0.1

    def test_smd_resistor_r_notation(self) -> None:
        self.assertEqual(self.resistor("4,7"), (SMD_OK, 4.7))
        self.assertEqual(self.resistor("22,"), (SMD_OK, 22.0))
        status, value = self.resistor(",047")
        self.assertEqual(status, SMD_OK)
        self.assertAlmostEqual(value, 0.047)

    def test_smd_incomplete_and_invalid(self) -> None:
        self.assertEqual(self.resistor("")[0], SMD_INCOMPLETE)
        self.assertEqual(self.resistor("4")[0], SMD_INCOMPLETE)
        self.assertEqual(self.resistor("47")[0], SMD_INCOMPLETE)
        self.assertEqual(self.resistor(",")[0], SMD_INCOMPLETE)
        self.assertEqual(self.resistor("12345")[0], SMD_INVALID)
        self.assertEqual(self.resistor("1,2,3")[0], SMD_INVALID)
        self.assertEqual(self.resistor("1A3")[0], SMD_INVALID)

    # EIA-96.
    def test_eia96_table_and_known_codes(self) -> None:
        self.assertEqual([self.lib.eia96_base(i) for i in (1, 2, 48, 96)], [100, 102, 309, 976])
        self.assertEqual(self.lib.eia96_base(0), 0)
        self.assertEqual(self.lib.eia96_base(97), 0)
        bases = [self.lib.eia96_base(i) for i in range(1, 97)]
        self.assertEqual(bases, sorted(bases))
        self.assertEqual(len(set(bases)), 96)
        self.assertEqual(self.eia96("01", "A"), (SMD_OK, 100))
        self.assertEqual(self.eia96("68", "X")[0], SMD_OK)
        self.assertAlmostEqual(self.eia96("68", "X")[1], 49.9)
        self.assertAlmostEqual(self.eia96("39", "B")[1], 2490)     # 249 x 10
        self.assertAlmostEqual(self.eia96("96", "F")[1], 97600000)

    def test_eia96_multipliers_and_errors(self) -> None:
        multipliers = {"Z": 0.001, "Y": 0.01, "R": 0.01, "X": 0.1, "S": 0.1, "A": 1, "B": 10, "H": 10,
                       "C": 100, "D": 1000, "E": 10000, "F": 100000}
        for letter, value in multipliers.items():
            with self.subTest(letter=letter):
                self.assertAlmostEqual(self.lib.eia96_multiplier(letter.encode()), value)
        self.assertEqual(self.lib.eia96_multiplier(b"Q"), 0)
        self.assertEqual(self.eia96("0", "A")[0], SMD_INCOMPLETE)
        self.assertEqual(self.eia96("97", "A")[0], SMD_INVALID)
        self.assertEqual(self.eia96("00", "A")[0], SMD_INVALID)
        self.assertEqual(self.eia96("123", "A")[0], SMD_INVALID)
        self.assertEqual(self.eia96("68", "Q")[0], SMD_INVALID)

    # SMD capacitors (picofarads).
    def test_smd_capacitor(self) -> None:
        self.assertEqual(self.capacitor("104"), (SMD_OK, 100000))
        self.assertEqual(self.capacitor("473"), (SMD_OK, 47000))
        self.assertEqual(self.capacitor("101"), (SMD_OK, 100))
        self.assertEqual(self.capacitor("4,7"), (SMD_OK, 4.7))
        self.assertEqual(self.capacitor("10")[0], SMD_INCOMPLETE)
        self.assertEqual(self.capacitor("1004")[0], SMD_INVALID)


HOST_AVAILABLE = (ENGINE_HOST / "run_game.py").is_file()
if HOST_AVAILABLE:
    sys.path.insert(0, str(ENGINE_HOST))
    import run_game  # noqa: E402

CONTROL_RESET = 3
BACK = (60, 20)
TILE = {"led": (360, 275), "partitore": (120, 354), "smd": (360, 354), "awg": (240, 433)}
KEY = {"7": (60, 340), "8": (180, 340), "9": (300, 340), "<": (420, 340), "4": (60, 380), "5": (180, 380),
       "6": (300, 380), "C": (420, 380), "1": (60, 420), "2": (180, 420), "3": (300, 420), "S": (420, 420),
       "0": (120, 460), ",": (300, 460), "D": (420, 460)}
BTN_EN = (446, 28)


class ScreenCase:
    screen = ""

    def setUp(self) -> None:
        if not HOST_AVAILABLE:
            self.skipTest("initialize submodule/raylib-lite-engine to run these tests")
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.rt = run_game.GenericHostRuntime(PROJECT, Path(self.tmp.name))
        self.step()
        self.assertEqual(self.tap(TILE[self.screen]), self.screen)

    def step(self, count: int = 1) -> None:
        for _ in range(count):
            self.rt.step(False, False, False)

    def tap(self, point) -> str:
        self.rt.pointer(0, point[0], point[1], True)
        self.step()
        self.rt.pointer(0, point[0], point[1], False)
        self.step()
        return self.rt.metadata()["screen"]

    def type(self, text: str) -> None:
        for char in text:
            self.tap(KEY[char])

    def lines(self) -> list:
        return self.rt.metadata()["lines"]

    def test_back_returns_to_the_menu(self) -> None:
        self.assertEqual(self.tap(BACK), "menu")

    def test_reset_returns_to_the_menu(self) -> None:
        self.rt.control(CONTROL_RESET)
        self.step()
        self.assertEqual(self.rt.metadata()["screen"], "menu")


class LedScreen(ScreenCase, unittest.TestCase):
    screen = "led"
    FIELD = {"supply": (240, 68), "led": (240, 112), "current": (240, 156)}

    def test_defaults_wait_for_the_supply(self) -> None:
        self.assertEqual(self.lines()[5], "INSERISCI I TRE VALORI")
        self.assertEqual(self.lines()[6:], ["", "2", "20"])
        self.assertEqual(self.lines()[:5], ["", "", "", "", ""])

    def test_five_volts_gives_150_ohm(self) -> None:
        self.tap(self.FIELD["supply"])
        self.type("5")
        self.assertEqual(self.lines()[:6], ["150 Ω", "150 Ω", "20 mA", "60 mW", "1/8 W", ""])

    def test_standard_value_is_rounded_up(self) -> None:
        self.tap(self.FIELD["supply"])
        self.type("12")
        self.tap(self.FIELD["led"])
        self.type("<")
        self.type("32")
        self.assertEqual(self.lines()[6:], ["12", "32", "20"])

    def test_supply_below_led_voltage(self) -> None:
        self.tap(self.FIELD["supply"])
        self.type("1")
        self.assertEqual(self.lines()[5], "ALIMENTAZIONE TROPPO BASSA")
        self.assertEqual(self.lines()[0], "")

    def test_english(self) -> None:
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(TILE["led"])
        self.assertEqual(self.lines()[5], "ENTER ALL THREE VALUES")


class DividerScreen(ScreenCase, unittest.TestCase):
    screen = "partitore"
    MODE_VOUT, MODE_R2, UNIT = (80, 66), (240, 66), (400, 66)
    FIELD = [(240, 110), (240, 154), (240, 198)]

    def test_find_vout(self) -> None:
        self.tap(self.FIELD[0])
        self.type("5")
        self.tap(self.FIELD[1])
        self.type("10")
        self.tap(self.FIELD[2])
        self.type("10")
        self.assertEqual(self.lines()[:3], ["2,5 V", "CORRENTE 250 µA", ""])

    def test_unit_button_changes_the_resistor_unit(self) -> None:
        for field, text in zip(self.FIELD, ("5", "10", "10")):
            self.tap(field)
            self.type(text)
        self.tap(self.UNIT)             # kΩ -> MΩ: the same ratio, a thousandth of the current
        self.assertEqual(self.lines()[:2], ["2,5 V", "CORRENTE 0,25 µA"])

    def test_find_r2(self) -> None:
        self.tap(self.MODE_R2)
        self.tap(self.FIELD[0])
        self.type("5")
        self.tap(self.FIELD[1])
        self.type("3,3")
        self.tap(self.FIELD[2])
        self.type("10")
        self.assertEqual(self.lines()[:2], ["19,41 kΩ", "E24 20 kΩ = 3,333 V"])

    def test_vout_not_below_vin(self) -> None:
        self.tap(self.MODE_R2)
        self.tap(self.FIELD[0])
        self.type("5")
        self.tap(self.FIELD[1])
        self.type("6")
        self.tap(self.FIELD[2])
        self.type("10")
        self.assertEqual(self.lines()[2], "VOUT DEVE ESSERE MINORE DI VIN")
        self.assertEqual(self.lines()[0], "")

    def test_each_mode_keeps_its_own_values(self) -> None:
        self.tap(self.FIELD[0])
        self.type("9")
        self.tap(self.MODE_R2)
        self.assertEqual(self.lines()[3:], ["", "", ""])
        self.tap(self.MODE_VOUT)
        self.assertEqual(self.lines()[3], "9")


class SmdScreen(ScreenCase, unittest.TestCase):
    screen = "smd"
    MODE_RES, MODE_E96, MODE_CAP = (80, 68), (240, 68), (400, 68)
    LETTER_X = {"Z": 27, "Y": 80, "X": 133, "A": 186, "B": 240, "C": 293, "D": 346, "E": 400, "F": 453}

    def test_three_digit_resistor(self) -> None:
        self.type("103")
        self.assertEqual(self.lines()[:2], ["103", "10 kΩ"])

    def test_four_digit_resistor(self) -> None:
        self.type("1002")
        self.assertEqual(self.lines()[:2], ["1002", "10 kΩ"])

    def test_r_notation_with_the_comma_key(self) -> None:
        self.type("4,7")
        self.assertEqual(self.lines()[:2], ["4R7", "4,7 Ω"])
        self.tap(KEY["C"])
        self.type(",047")
        self.assertEqual(self.lines()[:2], ["R047", "47 mΩ"])

    def test_zero_ohm_jumper(self) -> None:
        self.type("000")
        self.assertEqual(self.lines()[1], "0 Ω")

    def test_waits_and_flags_bad_codes(self) -> None:
        self.assertEqual(self.lines()[3], "SCRIVI IL CODICE")
        self.type("47")
        self.assertEqual(self.lines()[1], "")
        self.assertEqual(self.lines()[3], "SCRIVI IL CODICE")

    def test_five_digits_are_not_accepted(self) -> None:
        self.type("100256")
        self.assertEqual(self.lines()[0], "1002")

    def test_sign_key_is_ignored(self) -> None:
        self.type("1S")
        self.assertEqual(self.lines()[0], "1")

    def test_e96_with_letter(self) -> None:
        self.tap(self.MODE_E96)
        self.type("68")
        self.tap((self.LETTER_X["X"], 170))
        self.assertEqual(self.lines()[:3], ["68X", "49,9 Ω", "499 X 0,1"])
        self.tap((self.LETTER_X["D"], 170))
        self.assertEqual(self.lines()[:3], ["68D", "499 kΩ", "499 X 1000"])

    def test_e96_takes_two_digits_and_no_comma(self) -> None:
        self.tap(self.MODE_E96)
        self.type("123")
        self.assertEqual(self.lines()[0], "12A")
        self.tap(KEY["C"])
        self.type(",")
        self.assertEqual(self.lines()[0], "")

    def test_e96_invalid_code(self) -> None:
        self.tap(self.MODE_E96)
        self.type("97")
        self.assertEqual(self.lines()[3], "CODICE NON VALIDO")
        self.assertEqual(self.lines()[1], "")

    def test_capacitor(self) -> None:
        self.tap(self.MODE_CAP)
        self.type("104")
        self.assertEqual(self.lines()[:2], ["104", "100 nF"])
        self.tap(KEY["C"])
        self.type("4,7")
        self.assertEqual(self.lines()[:2], ["4R7", "4,7 pF"])
        self.tap(KEY["C"])
        self.type("225")
        self.assertEqual(self.lines()[1], "2,2 µF")

    def test_changing_mode_clears_the_code(self) -> None:
        self.type("103")
        self.tap(self.MODE_CAP)
        self.assertEqual(self.lines()[0], "")

    def test_english(self) -> None:
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(TILE["smd"])
        self.assertEqual(self.lines()[3], "TYPE THE CODE")


class AwgScreen(ScreenCase, unittest.TestCase):
    screen = "awg"
    TO_MM2, TO_AWG = (120, 68), (360, 68)

    def test_awg_14(self) -> None:
        self.type("14")
        self.assertEqual(self.lines()[:6], ["14", "2,081 mm²", "1,628 mm", "2,5 mm²", "", ""])

    def test_awg_10_and_4(self) -> None:
        self.type("10")
        self.assertEqual(self.lines()[1:4], ["5,261 mm²", "2,588 mm", "6 mm²"])
        self.tap(KEY["C"])
        self.type("4")
        self.assertEqual(self.lines()[1], "21,15 mm²")

    def test_double_zero_aliases(self) -> None:
        self.type("1S")
        self.assertEqual(self.lines()[0], "-1")
        self.assertEqual(self.lines()[5], "= 00")
        self.assertEqual(self.lines()[1], "67,43 mm²")
        self.tap(KEY["C"])
        self.type("3S")
        self.assertEqual(self.lines()[5], "= 0000")
        self.assertEqual(self.lines()[1], "107,2 mm²")

    def test_out_of_range_and_fractions_are_refused(self) -> None:
        self.type("41")
        self.assertEqual(self.lines()[4], "AWG INTERO DA 0000 A 40")
        self.assertEqual(self.lines()[1], "")
        self.tap(KEY["C"])
        self.type("4,5")
        self.assertEqual(self.lines()[4], "AWG INTERO DA 0000 A 40")
        self.tap(KEY["C"])
        self.type("4S")
        self.assertEqual(self.lines()[4], "AWG INTERO DA 0000 A 40")

    def test_mm2_to_awg(self) -> None:
        self.tap(self.TO_AWG)
        self.type("2,5")
        self.assertEqual(self.lines()[:5], ["2,5", "13", "2,624 mm²", "1,828 mm", ""])

    def test_modes_keep_their_own_value(self) -> None:
        self.type("14")
        self.tap(self.TO_AWG)
        self.assertEqual(self.lines()[0], "")
        self.tap(self.TO_MM2)
        self.assertEqual(self.lines()[0], "14")

    def test_empty_shows_the_hint(self) -> None:
        self.assertEqual(self.lines()[4], "TASTO +/- PER 00 000 0000")

    def test_english(self) -> None:
        self.tap(BACK)
        self.tap(BTN_EN)
        self.tap(TILE["awg"])
        self.assertEqual(self.lines()[4], "USE +/- FOR 00 000 0000")


if __name__ == "__main__":
    unittest.main()
