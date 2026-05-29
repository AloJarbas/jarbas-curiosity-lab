from __future__ import annotations

import csv
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO / 'python' / 'rule30_left_right_asymmetry.py'
SPEC = importlib.util.spec_from_file_location('rule30_left_right_asymmetry', SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC is not None and SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class Rule30LeftRightAsymmetryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.study = MODULE.study_asymmetry()
        cls.left = {metric.depth: metric for metric in cls.study.left_metrics}
        cls.right = {metric.depth: metric for metric in cls.study.right_metrics}

    def test_left_depths_keep_short_periods_long_after_right_side_breaks(self) -> None:
        self.assertEqual(sum(metric.detected_period > 0 for metric in self.left.values()), 24)
        self.assertEqual(sum(metric.detected_period > 0 for metric in self.right.values()), 8)
        self.assertEqual(self.left[16].detected_period, 4)
        self.assertEqual(self.right[16].detected_period, 0)
        self.assertEqual(self.right[8].detected_period, 32)

    def test_left_tail_entropy_stays_much_lower_than_right(self) -> None:
        left_mean = sum(metric.normalized_block_entropy for metric in self.left.values()) / len(self.left)
        right_mean = sum(metric.normalized_block_entropy for metric in self.right.values()) / len(self.right)
        self.assertLess(left_mean, 0.35)
        self.assertGreater(right_mean, 0.70)
        self.assertLess(self.left[8].normalized_block_entropy, 0.45)
        self.assertGreater(self.right[8].normalized_block_entropy, 0.75)

    def test_right_tail_transition_rate_stays_higher(self) -> None:
        left_mean = sum(metric.transition_rate for metric in self.left.values()) / len(self.left)
        right_mean = sum(metric.transition_rate for metric in self.right.values()) / len(self.right)
        self.assertLess(left_mean, 0.55)
        self.assertGreater(right_mean, 0.70)
        self.assertEqual(self.left[4].transition_rate, 0.0)
        self.assertGreater(self.right[4].transition_rate, 0.70)

    def test_csv_contains_both_sides_and_all_depths(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'asymmetry.csv'
            MODULE.write_csv(self.study, csv_path)
            with csv_path.open() as handle:
                rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), 2 * MODULE.DEFAULT_MAX_DEPTH)
        self.assertEqual(rows[0]['side'], 'left')
        self.assertEqual(rows[0]['depth'], '1')
        self.assertEqual(rows[-1]['side'], 'right')
        self.assertEqual(rows[-1]['depth'], str(MODULE.DEFAULT_MAX_DEPTH))


if __name__ == '__main__':
    unittest.main()
