# SPDX-License-Identifier: Apache-2.0
"""Behavior tests for the shared Snake C model, run through the Host simulator.

Run from the workspace root (works with plain unittest and with pytest):

    python -m unittest discover -s projects/snake/tests -v

The Host module is compiled once from the project sources with the same flags as
`mosaico.py game sim`. A bot plays against an independent Python model of the
snake body, so the C game logic is checked against a second implementation.
"""
from __future__ import annotations

import sys
import tempfile
import unittest
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
PROJECT = ROOT / "projects" / "snake"
ENGINE_HOST = ROOT / "submodule" / "raylib-lite-engine" / "host"
if not (ENGINE_HOST / "run_game.py").is_file():
    raise unittest.SkipTest("initialize submodule/raylib-lite-engine to run these tests")
sys.path.insert(0, str(ENGINE_HOST))

import run_game  # noqa: E402

COLS, ROWS = 20, 18
UP, RIGHT, DOWN, LEFT = range(4)
VECTOR = {UP: (0, -1), RIGHT: (1, 0), DOWN: (0, 1), LEFT: (-1, 0)}
# Host action codes from main/game_config.h.
ACTION_FOR_DIR = {UP: 2, RIGHT: 1, DOWN: 5, LEFT: 0}
ACTION_RESET = 4
CONTROL_PAUSE, CONTROL_RESUME = 1, 2
START_STEP_TICKS = 8
MIN_STEP_TICKS = 3
RESTART_COOLDOWN_TICKS = 20
CENTRE = (9, 8)


def opposite(direction: int) -> int:
    return (direction + 2) % 4


class Game:
    """Drives one Host game instance; every call advances whole frames."""

    def __init__(self, runtime) -> None:
        self.rt = runtime

    def state(self) -> dict:
        return self.rt.metadata()

    def step(self) -> dict:
        self.rt.step(False, False, False)
        return self.state()

    def frames(self, count: int) -> dict:
        for _ in range(count):
            self.rt.step(False, False, False)
        return self.state()

    def press(self, direction: int) -> None:
        """A key press: the turn registers on the press itself."""
        self.rt.action(ACTION_FOR_DIR[direction], True)
        self.rt.action(ACTION_FOR_DIR[direction], False)

    def pointer(self, x: int, y: int, pressed: bool) -> None:
        self.rt.pointer(0, x, y, pressed)

    def tap(self, x: int = 240, y: int = 240) -> None:
        self.pointer(x, y, True)
        self.pointer(x, y, False)


class Model:
    """Re-derives the body from observed head moves and checks the C hash."""

    def __init__(self) -> None:
        self.body = deque([(9, 9), (10, 9), (11, 9)])  # tail first
        self.direction = RIGHT

    @property
    def head(self) -> tuple[int, int]:
        return self.body[-1]

    @staticmethod
    def body_hash(cells) -> str:
        value = 2166136261
        for x, y in cells:
            value = ((value ^ (y * COLS + x)) * 16777619) & 0xFFFFFFFF
        return f"{value:08x}"

    def observe(self, state: dict) -> None:
        new_head = (state["head_x"], state["head_y"])
        if new_head != self.head:
            dx, dy = new_head[0] - self.head[0], new_head[1] - self.head[1]
            assert abs(dx) + abs(dy) == 1, f"head jumped to {new_head}"
            grew = state["length"] > len(self.body)
            self.body.append(new_head)
            if not grew:
                self.body.popleft()
            self.direction = next(d for d, v in VECTOR.items() if v == (dx, dy))
        assert state["length"] == len(self.body), (state["length"], len(self.body))
        assert len(set(self.body)) == len(self.body), "snake overlaps itself"
        assert state["body_hash"] == self.body_hash(self.body), "body hash mismatch"

    def blocked(self) -> set[tuple[int, int]]:
        return set(list(self.body)[1:])  # the tail cell is vacated by a move

    @staticmethod
    def inside(cell) -> bool:
        return 0 <= cell[0] < COLS and 0 <= cell[1] < ROWS

    def neighbours(self, cell, blocked):
        for d, (dx, dy) in VECTOR.items():
            nxt = (cell[0] + dx, cell[1] + dy)
            if self.inside(nxt) and nxt not in blocked:
                yield d, nxt

    def next_direction(self, target) -> int | None:
        """First move of a shortest safe path to target, else the roomiest move."""
        blocked = self.blocked()
        seen = {self.head}
        queue = deque()
        for d, n in self.neighbours(self.head, blocked):
            if d != opposite(self.direction):
                seen.add(n)
                queue.append((d, n))
        while queue:
            d, cell = queue.popleft()
            if cell == target:
                return d
            for _, n in self.neighbours(cell, blocked):
                if n not in seen:
                    seen.add(n)
                    queue.append((d, n))
        best, best_room = None, -1
        for d, n in self.neighbours(self.head, blocked):
            if d == opposite(self.direction):
                continue
            room = self.room(n, blocked | {self.head})
            if room > best_room:
                best, best_room = d, room
        return best

    def room(self, start, blocked) -> int:
        seen, queue = {start}, deque([start])
        while queue:
            for _, n in self.neighbours(queue.popleft(), blocked):
                if n not in seen:
                    seen.add(n)
                    queue.append(n)
        return len(seen)


def loop_cells(head, direction, turn):
    """First three cells entered when the head circles a 2x2 block."""
    cells, heading, cell = [], direction, head
    for _ in range(3):
        cell = (cell[0] + VECTOR[heading][0], cell[1] + VECTOR[heading][1])
        cells.append(cell)
        heading = (heading + turn) % 4
    return cells


class SnakeHostTests(unittest.TestCase):
    runtime = None
    tmp = None

    @classmethod
    def setUpClass(cls) -> None:
        cls.tmp = tempfile.TemporaryDirectory(prefix="snake-host-")
        cls.runtime = run_game.GenericHostRuntime(PROJECT, Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.runtime.close()
        cls.tmp.cleanup()

    def setUp(self) -> None:
        """A fresh game instance: same seed, no carried-over best score."""
        rt = self.runtime
        if rt.context:
            rt.api.mosaico_host_game_destroy_v1(rt.context)
        rt.context = rt.api.mosaico_host_game_create_v1(
            str(PROJECT / "assets/generated").encode())
        self.assertTrue(rt.context)
        self.game = Game(rt)

    # --- Bot helpers -------------------------------------------------------

    def chase(self, model: Model, state: dict, target_of, until, frames: int = 20000,
              intervals: list | None = None) -> dict:
        """Steer toward target_of(state) until until(state) or the round ends."""
        game = self.game
        last_move_tick = state["tick"]
        for _ in range(frames):
            if state["phase"] != "running" or until(state):
                break
            wanted = model.next_direction(target_of(state))
            if wanted is not None and wanted != model.direction:
                game.press(wanted)
            previous_head = model.head
            state = game.step()
            model.observe(state)
            if intervals is not None and model.head != previous_head:
                intervals.append((state["score"], state["tick"] - last_move_tick))
                last_move_tick = state["tick"]
        return state

    def play_until_score(self, score: int, intervals: list | None = None):
        model = Model()
        self.game.tap()
        state = self.chase(
            model, self.game.state(), lambda s: (s["food_x"], s["food_y"]),
            lambda s: s["score"] >= score, intervals=intervals)
        return state, model

    def circle(self, model: Model, state: dict, turn: int, moves: int) -> dict:
        """Turn after every move so the head circles a 2x2 block."""
        heading = model.direction
        for _ in range(moves):
            heading = (heading + turn) % 4
            self.game.press(heading)
            before = model.head
            for _ in range(START_STEP_TICKS * 2):
                state = self.game.step()
                model.observe(state)
                if model.head != before or state["phase"] != "running":
                    break
            if state["phase"] != "running":
                break
        return state

    def open_loop_turn(self, score: int):
        """Grow to `score`, move to open space, pick a free 2x2 loop direction."""
        state, model = self.play_until_score(score)
        self.assertEqual(state["phase"], "running")
        state = self.chase(model, state, lambda s: CENTRE, lambda s: model.head == CENTRE)
        self.assertEqual(model.head, CENTRE)
        food = (state["food_x"], state["food_y"])
        for turn in (1, 3):
            cells = loop_cells(model.head, model.direction, turn)
            if all(model.inside(c) and c != food and c not in model.blocked() for c in cells):
                return state, model, turn
        self.fail("no free 2x2 loop at the board centre")

    # --- Start, movement and input ----------------------------------------

    def test_starts_ready_with_a_three_cell_snake(self):
        state = self.game.state()
        self.assertEqual(state["phase"], "ready")
        self.assertEqual((state["length"], state["score"], state["dir"]), (3, 0, RIGHT))
        self.assertEqual((state["head_x"], state["head_y"]), (11, 9))
        self.assertTrue(0 <= state["food_x"] < COLS and 0 <= state["food_y"] < ROWS)

    def test_ready_game_does_not_move(self):
        before = self.game.state()
        after = self.game.frames(60)
        self.assertEqual((after["head_x"], after["head_y"]), (before["head_x"], before["head_y"]))
        self.assertEqual(after["phase"], "ready")

    def test_arrow_starts_game_and_snake_moves_one_cell_per_interval(self):
        self.game.press(UP)
        self.assertEqual(self.game.state()["phase"], "running")
        self.assertEqual(self.game.frames(START_STEP_TICKS - 1)["head_y"], 9)  # not yet
        state = self.game.frames(1)
        self.assertEqual((state["head_x"], state["head_y"], state["dir"]), (11, 8, UP))

    def test_reversing_into_the_neck_is_ignored(self):
        self.game.press(LEFT)  # snake heads right, so left is a reversal
        state = self.game.frames(START_STEP_TICKS)
        self.assertEqual(state["phase"], "running")
        self.assertEqual((state["head_x"], state["dir"]), (12, RIGHT))

    def test_tap_starts_game(self):
        self.game.tap()
        self.assertEqual(self.game.state()["phase"], "running")

    def test_swipe_turns_snake(self):
        self.game.tap()
        self.game.pointer(200, 300, True)
        self.game.pointer(200, 260, True)  # drag up by more than the swipe threshold
        self.game.pointer(200, 260, False)
        state = self.game.frames(START_STEP_TICKS)
        self.assertEqual((state["dir"], state["head_y"]), (UP, 8))

    def test_short_drag_does_not_turn_snake(self):
        self.game.tap()
        self.game.pointer(200, 300, True)
        self.game.pointer(200, 290, True)
        self.game.pointer(200, 290, False)
        self.assertEqual(self.game.frames(START_STEP_TICKS)["dir"], RIGHT)

    def test_swipe_in_ready_state_starts_and_turns(self):
        self.game.pointer(200, 300, True)
        self.game.pointer(200, 340, True)
        self.game.pointer(200, 340, False)
        state = self.game.frames(START_STEP_TICKS)
        self.assertEqual((state["phase"], state["dir"], state["head_y"]), ("running", DOWN, 10))

    def test_two_quick_turns_are_both_applied(self):
        self.game.press(UP)
        self.game.press(LEFT)  # buffered: UP first, then LEFT one step later
        state = self.game.frames(START_STEP_TICKS)
        self.assertEqual((state["head_x"], state["head_y"]), (11, 8))
        state = self.game.frames(START_STEP_TICKS)
        self.assertEqual((state["head_x"], state["head_y"], state["dir"]), (10, 8, LEFT))

    def test_holding_a_key_turns_only_once(self):
        self.game.press(UP)
        self.game.rt.action(ACTION_FOR_DIR[RIGHT], True)  # held: Host repeats it every frame
        for _ in range(START_STEP_TICKS * 2):
            self.game.rt.action(ACTION_FOR_DIR[RIGHT], True)
            self.game.rt.step(False, False, False)
        state = self.game.state()
        # UP step, then RIGHT step; the repeated RIGHT presses are not re-queued as new turns.
        self.assertEqual((state["head_x"], state["head_y"], state["dir"]), (12, 8, RIGHT))

    # --- Round end, restart, pause ----------------------------------------

    def test_wall_ends_round_and_freezes_board(self):
        self.game.press(UP)
        state = self.game.frames(START_STEP_TICKS * 12)
        self.assertEqual(state["phase"], "game_over")
        self.assertEqual((state["head_y"], state["score"]), (0, 0))
        self.assertEqual(self.game.frames(60)["body_hash"], state["body_hash"])

    def test_restart_waits_for_cooldown_then_starts_fresh_round(self):
        self.game.press(UP)
        self.game.frames(START_STEP_TICKS * 12)
        self.assertEqual(self.game.state()["phase"], "game_over")
        self.game.press(DOWN)
        self.game.tap()
        self.assertEqual(self.game.state()["phase"], "game_over")  # still in the cooldown
        self.game.frames(RESTART_COOLDOWN_TICKS)
        self.game.tap()
        state = self.game.state()
        self.assertEqual(state["phase"], "running")
        self.assertEqual((state["length"], state["score"], state["head_x"], state["head_y"]),
                         (3, 0, 11, 9))

    def test_reset_returns_to_ready_and_keeps_best_score(self):
        state, _ = self.play_until_score(1)
        self.assertEqual(state["best"], 1)
        self.game.rt.action(ACTION_RESET, True)
        self.game.rt.action(ACTION_RESET, False)
        state = self.game.state()
        self.assertEqual((state["phase"], state["score"], state["length"], state["best"]),
                         ("ready", 0, 3, 1))

    def test_pause_freezes_game_and_ignores_input(self):
        self.game.press(UP)
        self.game.rt.control(CONTROL_PAUSE)
        before = self.game.frames(30)
        self.assertEqual(before["phase"], "paused")
        self.game.press(LEFT)
        after = self.game.frames(60)
        self.assertEqual((after["tick"], after["body_hash"]), (before["tick"], before["body_hash"]))
        self.game.rt.control(CONTROL_RESUME)
        self.assertEqual(self.game.frames(START_STEP_TICKS)["dir"], UP)

    def test_tap_resumes_a_paused_game(self):
        self.game.press(UP)
        self.game.rt.control(CONTROL_PAUSE)
        self.game.tap()
        self.assertEqual(self.game.state()["phase"], "running")

    # --- Scoring, growth, speed and collisions (bot vs. Python model) -----

    def test_eating_food_grows_snake_and_scores(self):
        state, model = self.play_until_score(1)
        self.assertEqual((state["score"], state["length"]), (1, 4))
        self.assertEqual(state["phase"], "running")
        self.assertNotIn((state["food_x"], state["food_y"]), model.body)

    def test_long_game_keeps_body_consistent_and_speeds_up(self):
        intervals: list[tuple[int, int]] = []
        state, model = self.play_until_score(25, intervals=intervals)
        self.assertGreaterEqual(state["score"], 25, state)
        self.assertEqual(state["length"], 3 + state["score"])
        self.assertNotIn((state["food_x"], state["food_y"]), model.body)
        by_score = {}
        for score, interval in intervals:
            by_score[score] = interval
        self.assertEqual(by_score[0], START_STEP_TICKS)
        self.assertLess(by_score[max(by_score)], START_STEP_TICKS)
        self.assertGreaterEqual(min(by_score.values()), MIN_STEP_TICKS)

    def test_snake_may_follow_its_own_tail_around_a_loop(self):
        """A length-4 snake circling a 2x2 block always enters the cell its tail just left."""
        state, model, turn = self.open_loop_turn(score=1)
        state = self.circle(model, state, turn, moves=12)
        self.assertEqual(state["phase"], "running")
        self.assertEqual((state["score"], state["length"]), (1, 4))

    def test_self_collision_ends_round(self):
        """With length 5 the same loop runs into the body on its fourth move."""
        state, model, turn = self.open_loop_turn(score=2)
        state = self.circle(model, state, turn, moves=8)
        self.assertEqual(state["phase"], "game_over")
        self.assertEqual((state["score"], state["length"]), (2, 5))
        dx, dy = VECTOR[(model.direction + turn) % 4]
        crashed_into = (model.head[0] + dx, model.head[1] + dy)
        self.assertIn(crashed_into, model.body)
        self.assertEqual(self.game.frames(40)["body_hash"], state["body_hash"])


if __name__ == "__main__":
    unittest.main()
