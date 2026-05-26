from __future__ import annotations

import csv
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO / 'python' / 'random_quadratic_attractor_screening.py'
SPEC = importlib.util.spec_from_file_location('random_quadratic_attractor_screening', SCRIPT_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC is not None and SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class RandomQuadraticAttractorScreeningTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.results = MODULE.scan_candidates()

    def test_status_counts_stay_deterministic(self) -> None:
        counts = {}
        for result in self.results:
            counts[result.status] = counts.get(result.status, 0) + 1
        self.assertEqual(counts['diverged'], 282)
        self.assertEqual(counts['collapsed'], 93)
        self.assertEqual(counts['nonchaotic'], 15)
        self.assertEqual(counts['accepted'], 10)

    def test_selected_candidates_match_expected_ranking(self) -> None:
        selected_ids = [result.candidate_id for result in self.results if result.selected_rank is not None]
        self.assertEqual(selected_ids, [172, 218, 235, 286, 316, 386])

    def test_csv_contains_all_candidates_and_six_selected_rows(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'screening.csv'
            MODULE.write_csv(self.results, csv_path)
            with csv_path.open() as handle:
                rows = list(csv.DictReader(handle))
        self.assertEqual(len(rows), MODULE.MAX_CANDIDATES)
        selected = [row for row in rows if row['selected_rank']]
        self.assertEqual(len(selected), MODULE.SELECT_COUNT)
        self.assertEqual(selected[0]['candidate_id'], '172')
        self.assertEqual(selected[-1]['candidate_id'], '386')


if __name__ == '__main__':
    unittest.main()
