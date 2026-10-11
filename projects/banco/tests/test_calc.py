# SPDX-License-Identifier: Apache-2.0
"""Tests for main/calc.c, the pure calculation layer of the bench assistant.

Run from the workspace root:

    python -m unittest discover -s projects/banco/tests -v

calc.c is compiled once into a shared library with strict warnings and called
through ctypes. Expected values are worked out by hand in the comments.
"""
from __future__ import annotations

import ctypes
import math
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

MAIN = Path(__file__).resolve().parents[1] / "main"

# Mirrors of the enums in calc.h.
QTY_RESISTANCE, QTY_VOLTAGE, QTY_CURRENT, QTY_CAPACITANCE, QTY_POWER, QTY_COUNT = range(6)
UNITS_PER_QTY = 4
OHM_V, OHM_I, OHM_R, OHM_P, OHM_COUNT = range(5)
OHM_OK, OHM_NEED_TWO, OHM_UNDEFINED = range(3)
(BLACK, BROWN, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET, GREY, WHITE, GOLD, SILVER,
 COLOR_COUNT) = range(13)
BAND_DIGIT, BAND_MULTIPLIER, BAND_TOLERANCE = range(3)
COPPER, ALUMINIUM = range(2)
SINGLE_PHASE, THREE_PHASE = range(2)


class Unit(ctypes.Structure):
    _fields_ = [("symbol", ctypes.c_char_p), ("factor", ctypes.c_double)]


class Entry(ctypes.Structure):
    _fields_ = [("text", ctypes.c_char * 11)]


class CableInput(ctypes.Structure):
    _fields_ = [("current_a", ctypes.c_double), ("length_m", ctypes.c_double),
                ("limit_percent", ctypes.c_double), ("material", ctypes.c_int),
                ("system", ctypes.c_int)]


def load_library(directory: Path) -> ctypes.CDLL:
    compiler = shutil.which(os.environ.get("CC", "cc")) or shutil.which("gcc")
    if not compiler:
        raise unittest.SkipTest("a C compiler is required")
    library = directory / "libcalc.so"
    subprocess.run([compiler, "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra",
                    "-Werror", str(MAIN / "calc.c"), "-lm", "-o", str(library)],
                   check=True, capture_output=True, text=True)
    lib = ctypes.CDLL(str(library))
    d, i, b, sz = ctypes.c_double, ctypes.c_int, ctypes.c_bool, ctypes.c_size_t
    lib.calc_unit.restype = ctypes.POINTER(Unit)
    lib.calc_unit.argtypes = [i, i]
    lib.calc_default_unit.argtypes = [i]
    lib.calc_convert.restype = d
    lib.calc_convert.argtypes = [i, d, i, i]
    lib.calc_best_unit.argtypes = [i, d]
    for name in ("clear",):
        getattr(lib, f"calc_entry_{name}").argtypes = [ctypes.POINTER(Entry)]
    for name in ("is_empty", "double_zero", "point", "backspace", "toggle_sign"):
        fn = getattr(lib, f"calc_entry_{name}")
        fn.restype = b
        fn.argtypes = [ctypes.POINTER(Entry)]
    lib.calc_entry_digit.restype = b
    lib.calc_entry_digit.argtypes = [ctypes.POINTER(Entry), ctypes.c_char]
    lib.calc_entry_value.restype = b
    lib.calc_entry_value.argtypes = [ctypes.POINTER(Entry), ctypes.POINTER(d)]
    lib.calc_format.restype = sz
    lib.calc_format.argtypes = [d, ctypes.c_char_p, sz]
    lib.calc_ohm.restype = i
    lib.calc_ohm.argtypes = [ctypes.POINTER(b), ctypes.POINTER(d), ctypes.POINTER(d)]
    lib.calc_band_kind.argtypes = [i, i]
    lib.calc_band_allows.restype = b
    lib.calc_band_allows.argtypes = [i, i]
    lib.calc_resistor.restype = b
    lib.calc_resistor.argtypes = [i, ctypes.POINTER(i), ctypes.POINTER(d), ctypes.POINTER(d)]
    lib.calc_cable_section_count.argtypes = []
    lib.calc_cable_section.restype = d
    lib.calc_cable_section.argtypes = [i]
    lib.calc_cable_nominal_voltage.restype = d
    lib.calc_cable_nominal_voltage.argtypes = [i]
    lib.calc_cable_drop_v.restype = d
    lib.calc_cable_drop_v.argtypes = [ctypes.POINTER(CableInput), d]
    lib.calc_cable_min_section.argtypes = [ctypes.POINTER(CableInput), ctypes.POINTER(d),
                                           ctypes.POINTER(d)]
    return lib


class CalcTests(unittest.TestCase):
    lib = None
    tmp = None

    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory(prefix="banco-calc-")
        cls.lib = load_library(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tmp.cleanup()

    # --- helpers -----------------------------------------------------------

    def entry(self, keys: str) -> Entry:
        """Type keys: digits, ',' point, '0' x2 as 'Z', '<' backspace, '~' sign."""
        e = Entry()
        self.lib.calc_entry_clear(ctypes.byref(e))
        for key in keys:
            if key.isdigit():
                self.lib.calc_entry_digit(ctypes.byref(e), key.encode())
            elif key == ",":
                self.lib.calc_entry_point(ctypes.byref(e))
            elif key == "Z":
                self.lib.calc_entry_double_zero(ctypes.byref(e))
            elif key == "<":
                self.lib.calc_entry_backspace(ctypes.byref(e))
            elif key == "~":
                self.lib.calc_entry_toggle_sign(ctypes.byref(e))
        return e

    def text(self, e: Entry) -> str:
        return e.text.decode()

    def value(self, e: Entry):
        out = ctypes.c_double()
        return out.value if self.lib.calc_entry_value(ctypes.byref(e), ctypes.byref(out)) else None

    def fmt(self, value: float, size: int = 32) -> str:
        buffer = ctypes.create_string_buffer(size)
        length = self.lib.calc_format(value, buffer, size)
        self.assertEqual(length, len(buffer.value))
        return buffer.value.decode()

    def symbol(self, qty: int, index: int) -> str:
        return self.lib.calc_unit(qty, index).contents.symbol.decode()

    def convert(self, qty, value, src, dst) -> float:
        return self.lib.calc_convert(qty, value, src, dst)

    def ohm(self, **known):
        index = {"V": OHM_V, "I": OHM_I, "R": OHM_R, "P": OHM_P}
        flags = (ctypes.c_bool * OHM_COUNT)()
        values = (ctypes.c_double * OHM_COUNT)()
        for name, number in known.items():
            flags[index[name]] = True
            values[index[name]] = number
        out = (ctypes.c_double * OHM_COUNT)()
        status = self.lib.calc_ohm(flags, values, out)
        return status, {name: out[i] for name, i in index.items()}

    def resistor(self, *colors):
        array = (ctypes.c_int * 5)(*colors, *([0] * (5 - len(colors))))
        ohms, tolerance = ctypes.c_double(), ctypes.c_double()
        ok = self.lib.calc_resistor(len(colors), array, ctypes.byref(ohms), ctypes.byref(tolerance))
        return ok, ohms.value, tolerance.value

    def cable(self, **kwargs):
        defaults = dict(current_a=16, length_m=30, limit_percent=4, material=COPPER,
                        system=SINGLE_PHASE)
        defaults.update(kwargs)
        return CableInput(**defaults)

    # --- units -------------------------------------------------------------

    def test_unit_tables_are_ordered_and_have_symbols(self):
        for qty in range(QTY_COUNT):
            factors = [self.lib.calc_unit(qty, i).contents.factor for i in range(UNITS_PER_QTY)]
            self.assertEqual(factors, sorted(factors), qty)
            self.assertTrue(all(self.symbol(qty, i) for i in range(UNITS_PER_QTY)))
            default = self.lib.calc_default_unit(qty)
            self.assertTrue(0 <= default < UNITS_PER_QTY)
        self.assertFalse(self.lib.calc_unit(QTY_COUNT, 0))
        self.assertFalse(self.lib.calc_unit(QTY_RESISTANCE, UNITS_PER_QTY))

    def test_resistance_symbols(self):
        self.assertEqual([self.symbol(QTY_RESISTANCE, i) for i in range(4)],
                         ["mΩ", "Ω", "kΩ", "MΩ"])

    def test_ohm_to_kilo_ohm(self):
        self.assertAlmostEqual(self.convert(QTY_RESISTANCE, 4700, 1, 2), 4.7)
        self.assertAlmostEqual(self.convert(QTY_RESISTANCE, 0.33, 2, 1), 330)
        self.assertAlmostEqual(self.convert(QTY_RESISTANCE, 2.2, 3, 2), 2200)
        self.assertAlmostEqual(self.convert(QTY_RESISTANCE, 100, 0, 3), 1e-7)

    def test_conversions_are_exact_where_a_decimal_result_exists(self):
        self.assertEqual(self.convert(QTY_RESISTANCE, 4700, 1, 2), 4.7)
        self.assertEqual(self.convert(QTY_RESISTANCE, 4.7, 2, 1), 4700.0)
        self.assertEqual(self.convert(QTY_CAPACITANCE, 100, 1, 2), 0.1)  # 100 nF = 0.1 µF

    def test_other_quantities(self):
        self.assertAlmostEqual(self.convert(QTY_CURRENT, 250, 1, 2), 0.25)      # mA -> A
        self.assertAlmostEqual(self.convert(QTY_VOLTAGE, 3.3, 2, 1), 3300)      # V -> mV
        self.assertAlmostEqual(self.convert(QTY_CAPACITANCE, 4.7, 2, 0), 4.7e6)  # µF -> pF
        self.assertAlmostEqual(self.convert(QTY_POWER, 1.5, 2, 1), 1500)         # kW -> W

    def test_convert_rejects_bad_indices(self):
        self.assertTrue(math.isnan(self.convert(QTY_RESISTANCE, 1, 0, 9)))
        self.assertTrue(math.isnan(self.convert(99, 1, 0, 1)))

    def test_conversion_round_trip(self):
        for qty in range(QTY_COUNT):
            for a in range(UNITS_PER_QTY):
                for b in range(UNITS_PER_QTY):
                    there = self.convert(qty, 123.456, a, b)
                    back = self.convert(qty, there, b, a)
                    self.assertAlmostEqual(back / 123.456, 1.0, places=12, msg=(qty, a, b))

    def test_best_unit_picks_a_readable_prefix(self):
        best = lambda base: self.lib.calc_best_unit(QTY_RESISTANCE, base)  # noqa: E731
        self.assertEqual(best(4700), 2)       # 4.7 kΩ
        self.assertEqual(best(470), 1)        # 470 Ω
        self.assertEqual(best(0.47), 0)       # 470 mΩ
        self.assertEqual(best(2.2e6), 3)      # 2.2 MΩ
        self.assertEqual(best(1000), 2)       # 1 kΩ, not 1000 Ω
        self.assertEqual(best(999.9999999999), 2)
        self.assertEqual(best(1e9), 3)        # no larger unit: stays at MΩ
        self.assertEqual(best(0), 0)
        self.assertEqual(best(-4700), 2)
        self.assertEqual(best(float("nan")), 0)

    # --- number entry ------------------------------------------------------

    def test_entry_builds_numbers(self):
        self.assertEqual(self.text(self.entry("4700")), "4700")
        self.assertEqual(self.text(self.entry("4,7")), "4,7")
        self.assertEqual(self.value(self.entry("4,7")), 4.7)
        self.assertEqual(self.value(self.entry("4700")), 4700.0)

    def test_entry_has_no_leading_zeros(self):
        self.assertEqual(self.text(self.entry("007")), "7")
        self.assertEqual(self.text(self.entry("0")), "0")
        self.assertEqual(self.text(self.entry("00")), "0")
        self.assertEqual(self.text(self.entry("Z")), "0")
        self.assertEqual(self.text(self.entry("5Z")), "500")
        self.assertEqual(self.text(self.entry("0Z")), "0")

    def test_entry_point_rules(self):
        self.assertEqual(self.text(self.entry(",")), "0,")
        self.assertEqual(self.text(self.entry("1,2,3")), "1,23")  # second point ignored
        self.assertEqual(self.text(self.entry("~,5")), "-0,5")
        self.assertEqual(self.value(self.entry("~,5")), -0.5)

    def test_entry_backspace_and_sign(self):
        self.assertEqual(self.text(self.entry("123<")), "12")
        self.assertEqual(self.text(self.entry("1<")), "")
        self.assertEqual(self.text(self.entry("<")), "")
        self.assertEqual(self.text(self.entry("~5<")), "")        # "-" alone is cleared
        self.assertEqual(self.text(self.entry("5~")), "-5")
        self.assertEqual(self.text(self.entry("5~~")), "5")

    def test_entry_value_needs_a_digit(self):
        for keys in ("", "~"):
            self.assertIsNone(self.value(self.entry(keys)), keys)
        self.assertEqual(self.value(self.entry(",")), 0.0)  # a lone point becomes "0,"

    def test_entry_stops_at_maximum_length(self):
        e = self.entry("1" * 20)
        self.assertEqual(self.text(e), "1" * 10)
        self.assertFalse(self.lib.calc_entry_digit(ctypes.byref(e), b"1"))
        self.assertFalse(self.lib.calc_entry_point(ctypes.byref(e)))
        self.assertEqual(self.text(self.entry("123456789Z")), "123456789")  # "00" does not fit

    def test_entry_digit_rejects_non_digits(self):
        e = self.entry("5")
        self.assertFalse(self.lib.calc_entry_digit(ctypes.byref(e), b"x"))
        self.assertEqual(self.text(e), "5")

    # --- display formatting ------------------------------------------------

    def test_format_numbers(self):
        cases = {
            0: "0", 4.7: "4,7", -0.5: "-0,5", 4700: "4700", 12345: "12 345",
            1234567: "1 234 567", -1234567: "-1 234 567", 0.001: "0,001", 100: "100",
            0.0001: "0,0001", 1e-5: "1e-5", 1.5e-7: "1,5e-7", 1e10: "1e10",
            1.2345678901: "1,234568", 2.5e12: "2,5e12",
        }
        for number, expected in cases.items():
            self.assertEqual(self.fmt(number), expected, number)

    def test_format_special_values(self):
        self.assertEqual(self.fmt(float("nan")), "-")
        self.assertEqual(self.fmt(float("inf")), "inf")
        self.assertEqual(self.fmt(float("-inf")), "-inf")

    def test_format_never_overflows_its_buffer(self):
        for size in (1, 2, 5, 8):
            buffer = ctypes.create_string_buffer(b"\xff" * 64, 64)
            length = self.lib.calc_format(1234567.891, buffer, size)
            self.assertLess(length, size)
            self.assertEqual(len(buffer.value), length)
            self.assertEqual(buffer.raw[size:], b"\xff" * (64 - size))  # nothing written past size

    # --- Ohm's law ---------------------------------------------------------

    def test_ohm_from_voltage_and_current(self):
        status, r = self.ohm(V=12, I=2)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["R"], 6)
        self.assertAlmostEqual(r["P"], 24)

    def test_ohm_from_voltage_and_resistance(self):
        status, r = self.ohm(V=230, R=46)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["I"], 5)
        self.assertAlmostEqual(r["P"], 1150)

    def test_ohm_from_voltage_and_power(self):
        status, r = self.ohm(V=230, P=2300)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["I"], 10)
        self.assertAlmostEqual(r["R"], 23)

    def test_ohm_from_current_and_resistance(self):
        status, r = self.ohm(I=0.02, R=330)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["V"], 6.6)
        self.assertAlmostEqual(r["P"], 0.132)

    def test_ohm_from_current_and_power(self):
        status, r = self.ohm(I=4, P=64)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["V"], 16)
        self.assertAlmostEqual(r["R"], 4)

    def test_ohm_from_resistance_and_power(self):
        status, r = self.ohm(R=100, P=4)
        self.assertEqual(status, OHM_OK)
        self.assertAlmostEqual(r["V"], 20)
        self.assertAlmostEqual(r["I"], 0.2)

    def test_ohm_results_are_consistent_for_every_pair(self):
        truth = dict(V=12.0, I=0.5, R=24.0, P=6.0)
        names = list(truth)
        for a in range(4):
            for b in range(a + 1, 4):
                status, r = self.ohm(**{names[a]: truth[names[a]], names[b]: truth[names[b]]})
                self.assertEqual(status, OHM_OK, (names[a], names[b]))
                for name in names:
                    self.assertAlmostEqual(r[name], truth[name], msg=(names[a], names[b], name))

    def test_ohm_needs_exactly_two_values(self):
        self.assertEqual(self.ohm(V=12)[0], OHM_NEED_TWO)
        self.assertEqual(self.ohm(V=12, I=1, R=12)[0], OHM_NEED_TWO)
        self.assertEqual(self.ohm()[0], OHM_NEED_TWO)

    def test_ohm_undefined_cases(self):
        self.assertEqual(self.ohm(V=12, I=0)[0], OHM_UNDEFINED)      # R = V / 0
        self.assertEqual(self.ohm(V=0, R=0)[0], OHM_UNDEFINED)       # I = 0 / 0
        self.assertEqual(self.ohm(V=0, P=5)[0], OHM_UNDEFINED)       # I = P / 0
        self.assertEqual(self.ohm(R=-10, P=5)[0], OHM_UNDEFINED)     # sqrt of a negative

    def test_ohm_known_values_are_copied(self):
        _, r = self.ohm(V=5, R=1000)
        self.assertEqual((r["V"], r["R"]), (5, 1000))

    # --- resistor colour code ----------------------------------------------

    def test_four_band_resistors(self):
        ok, ohms, tolerance = self.resistor(BROWN, BLACK, RED, GOLD)
        self.assertTrue(ok)
        self.assertAlmostEqual(ohms, 1000)
        self.assertEqual(tolerance, 5.0)
        _, ohms, tolerance = self.resistor(YELLOW, VIOLET, RED, SILVER)   # 4,7 kΩ ±10 %
        self.assertAlmostEqual(ohms, 4700)
        self.assertEqual(tolerance, 10.0)
        _, ohms, _ = self.resistor(ORANGE, ORANGE, ORANGE, GOLD)          # 33 kΩ
        self.assertAlmostEqual(ohms, 33000)

    def test_five_band_resistors(self):
        ok, ohms, tolerance = self.resistor(BROWN, BLACK, BLACK, RED, BROWN)   # 100 x 100
        self.assertTrue(ok)
        self.assertAlmostEqual(ohms, 10000)
        self.assertEqual(tolerance, 1.0)
        _, ohms, tolerance = self.resistor(RED, RED, BLACK, BLACK, GREEN)      # 220 x 1
        self.assertAlmostEqual(ohms, 220)
        self.assertEqual(tolerance, 0.5)
        _, ohms, _ = self.resistor(GREEN, BLUE, BLACK, BROWN, BROWN)           # 560 x 10
        self.assertAlmostEqual(ohms, 5600)

    def test_fractional_multipliers(self):
        _, ohms, _ = self.resistor(RED, RED, GOLD, GOLD)       # 22 x 0,1 = 2,2 Ω
        self.assertAlmostEqual(ohms, 2.2)
        _, ohms, _ = self.resistor(RED, RED, SILVER, GOLD)     # 22 x 0,01 = 0,22 Ω
        self.assertAlmostEqual(ohms, 0.22)

    def test_all_tolerance_colours(self):
        expected = {BROWN: 1, RED: 2, GREEN: 0.5, BLUE: 0.25, VIOLET: 0.1, GREY: 0.05,
                    GOLD: 5, SILVER: 10}
        for color, percent in expected.items():
            ok, _, tolerance = self.resistor(BROWN, BLACK, BLACK, color)
            self.assertTrue(ok, color)
            self.assertEqual(tolerance, percent, color)

    def test_invalid_colours_are_rejected(self):
        self.assertFalse(self.resistor(GOLD, BLACK, RED, GOLD)[0])        # gold is no digit
        self.assertFalse(self.resistor(BROWN, BLACK, RED, WHITE)[0])      # white is no tolerance
        self.assertFalse(self.resistor(BROWN, BLACK, RED, BLACK)[0])      # black is no tolerance
        self.assertFalse(self.resistor(BROWN, BLACK, RED, ORANGE)[0])
        self.assertFalse(self.resistor(BROWN, BLACK, 99, GOLD)[0])
        self.assertFalse(self.resistor(BROWN, BLACK, COLOR_COUNT, GOLD)[0])

    def test_band_count_limits(self):
        for count in (0, 3, 6):
            colors = (ctypes.c_int * 5)(BROWN, BLACK, RED, GOLD, BROWN)
            self.assertFalse(self.lib.calc_resistor(count, colors, None, None), count)

    def test_band_kinds(self):
        four = [self.lib.calc_band_kind(4, i) for i in range(4)]
        five = [self.lib.calc_band_kind(5, i) for i in range(5)]
        self.assertEqual(four, [BAND_DIGIT, BAND_DIGIT, BAND_MULTIPLIER, BAND_TOLERANCE])
        self.assertEqual(five, [BAND_DIGIT, BAND_DIGIT, BAND_DIGIT, BAND_MULTIPLIER,
                                BAND_TOLERANCE])

    def test_band_allowed_colours(self):
        digits = [c for c in range(COLOR_COUNT) if self.lib.calc_band_allows(BAND_DIGIT, c)]
        multipliers = [c for c in range(COLOR_COUNT) if self.lib.calc_band_allows(BAND_MULTIPLIER, c)]
        tolerances = [c for c in range(COLOR_COUNT) if self.lib.calc_band_allows(BAND_TOLERANCE, c)]
        self.assertEqual(digits, list(range(BLACK, WHITE + 1)))
        self.assertEqual(multipliers, list(range(BLACK, SILVER + 1)))
        self.assertEqual(tolerances, [BROWN, RED, GREEN, BLUE, VIOLET, GREY, GOLD, SILVER])

    # --- cable voltage drop ------------------------------------------------

    def test_standard_sections(self):
        count = self.lib.calc_cable_section_count()
        sections = [self.lib.calc_cable_section(i) for i in range(count)]
        self.assertEqual(sections[:7], [1.5, 2.5, 4, 6, 10, 16, 25])
        self.assertEqual(sections, sorted(sections))
        self.assertTrue(math.isnan(self.lib.calc_cable_section(count)))
        self.assertTrue(math.isnan(self.lib.calc_cable_section(-1)))

    def test_drop_single_phase_copper(self):
        # 2 x 0,0225 x 30 m x 16 A / 2,5 mm² = 8,64 V
        drop = self.lib.calc_cable_drop_v(ctypes.byref(self.cable()), 2.5)
        self.assertAlmostEqual(drop, 8.64)

    def test_drop_three_phase_aluminium(self):
        # sqrt(3) x 0,036 x 50 m x 32 A / 10 mm² = 9,977 V
        cable = self.cable(current_a=32, length_m=50, material=ALUMINIUM, system=THREE_PHASE)
        drop = self.lib.calc_cable_drop_v(ctypes.byref(cable), 10)
        self.assertAlmostEqual(drop, math.sqrt(3) * 0.036 * 50 * 32 / 10)

    def test_drop_rejects_bad_input(self):
        self.assertTrue(math.isnan(self.lib.calc_cable_drop_v(ctypes.byref(self.cable()), 0)))
        self.assertTrue(math.isnan(self.lib.calc_cable_drop_v(None, 2.5)))

    def test_nominal_voltage(self):
        self.assertEqual(self.lib.calc_cable_nominal_voltage(SINGLE_PHASE), 230)
        self.assertEqual(self.lib.calc_cable_nominal_voltage(THREE_PHASE), 400)

    def min_section(self, cable):
        drop, percent = ctypes.c_double(), ctypes.c_double()
        index = self.lib.calc_cable_min_section(ctypes.byref(cable), ctypes.byref(drop),
                                                ctypes.byref(percent))
        return index, drop.value, percent.value

    def test_min_section_single_phase(self):
        # Limit 4 % of 230 V = 9,2 V. 1,5 mm² drops 14,4 V, 2,5 mm² drops 8,64 V.
        index, drop, percent = self.min_section(self.cable())
        self.assertEqual(self.lib.calc_cable_section(index), 2.5)
        self.assertAlmostEqual(drop, 8.64)
        self.assertAlmostEqual(percent, 8.64 / 230 * 100)

    def test_min_section_three_phase_aluminium(self):
        # Limit 3 % of 400 V = 12 V. 10 mm² drops 9,977 V; 6 mm² drops 16,6 V.
        cable = self.cable(current_a=32, length_m=50, limit_percent=3, material=ALUMINIUM,
                           system=THREE_PHASE)
        index, drop, percent = self.min_section(cable)
        self.assertEqual(self.lib.calc_cable_section(index), 10)
        self.assertLessEqual(percent, 3)

    def test_min_section_is_minimal(self):
        cable = self.cable(current_a=40, length_m=80, limit_percent=3)
        index, _, percent = self.min_section(cable)
        self.assertGreater(index, 0)
        self.assertLessEqual(percent, 3)
        smaller = self.lib.calc_cable_drop_v(ctypes.byref(cable), self.lib.calc_cable_section(index - 1))
        self.assertGreater(smaller / 230 * 100, 3)

    def test_min_section_none_fits(self):
        index, _, _ = self.min_section(self.cable(current_a=1000, length_m=500))
        self.assertEqual(index, -1)
        self.assertEqual(self.lib.calc_cable_min_section(None, None, None), -1)

    def test_min_section_without_output_pointers(self):
        cable = self.cable()
        self.assertEqual(self.lib.calc_cable_min_section(ctypes.byref(cable), None, None), 1)


if __name__ == "__main__":
    unittest.main()
