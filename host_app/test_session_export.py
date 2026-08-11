from __future__ import annotations

import csv
from io import StringIO
import unittest

from session_export import SESSION_CSV_FIELDS, render_session_csv


class SessionExportTests(unittest.TestCase):
    def test_render_session_csv_keeps_fixed_columns_and_values(self) -> None:
        text = render_session_csv(
            (
                {
                    "time": "15:51:00.000",
                    "P": 1,
                    "BED": 1,
                    "BR": 19,
                    "HR": 64,
                    "motion_score": 0,
                    "alerts": "",
                    "ignored": "not exported",
                },
            )
        )
        rows = list(csv.DictReader(StringIO(text)))
        self.assertEqual(tuple(rows[0]), SESSION_CSV_FIELDS)
        self.assertEqual(rows[0]["HR"], "64")
        self.assertEqual(rows[0]["BR"], "19")
        self.assertNotIn("ignored", rows[0])


if __name__ == "__main__":
    unittest.main()
