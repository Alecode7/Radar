from __future__ import annotations

import unittest

from host_algorithm.session_metrics import (
    calculate_heart_session_stats,
    calculate_motion_score,
    calculate_ten_minute_summary,
    evaluate_monitoring_alerts,
    session_duration_notice,
)


class SessionMetricsTests(unittest.TestCase):
    def test_heart_session_stats(self) -> None:
        stats = calculate_heart_session_stats(10, (0, 62, 63, 64, 65, 0))
        self.assertEqual(stats.valid_count, 4)
        self.assertEqual(stats.coverage_percent, 40)
        self.assertEqual(stats.minimum_bpm, 62)
        self.assertEqual(stats.maximum_bpm, 65)
        self.assertAlmostEqual(stats.average_bpm or 0.0, 63.5)

    def test_ten_minute_summary_filters_old_and_out_of_bed_rows(self) -> None:
        now = 1000.0
        sleep_history = (
            (350.0, 1, 1, 0, 2, 30, 1, 3),
            (500.0, 0, 0, 0, 0, 0, 1, 0),
            (700.0, 1, 1, 0, 2, 18, 0, 0),
            (800.0, 1, 1, 0, 2, 20, 1, 1),
            (900.0, 1, 1, 1, 1, 22, 2, 2),
        )
        heart_history = ((650.0, 80), (750.0, 62), (850.0, 64))
        summary = calculate_ten_minute_summary(
            sleep_history,
            heart_history,
            now,
            session_start=600.0,
        )
        self.assertEqual(summary.record_count, 3)
        self.assertEqual(summary.sleep_state, 1)
        self.assertEqual(summary.turn_count, 2)
        self.assertEqual(summary.large_motion_percent, 33)
        self.assertEqual(summary.small_motion_percent, 33)
        self.assertAlmostEqual(summary.average_breath_rate or 0.0, 20.0)
        self.assertAlmostEqual(summary.average_heart_rate or 0.0, 63.0)

    def test_ten_minute_turns_use_prior_window_value_as_baseline(self) -> None:
        summary = calculate_ten_minute_summary(
            (
                (300.0, 1, 1, 0, 3, 17, 4, 0),
                (500.0, 1, 1, 0, 3, 17, 5, 0),
                (900.0, 1, 1, 0, 3, 16, 7, 0),
            ),
            (),
            1000.0,
            session_start=100.0,
        )
        self.assertEqual(summary.turn_count, 3)

    def test_session_duration_notice(self) -> None:
        self.assertIn("不足4小时", session_duration_notice(3 * 3600 * 5))
        self.assertIn("满足", session_duration_notice(8 * 3600 * 5))
        self.assertIn("超过12小时", session_duration_notice(13 * 3600 * 5))

    def test_motion_score_uses_level_and_active_points(self) -> None:
        self.assertEqual(calculate_motion_score(0, 20), 0)
        self.assertEqual(calculate_motion_score(1, 2), 29)
        self.assertEqual(calculate_motion_score(2, 5), 65)
        self.assertEqual(calculate_motion_score(3, 20), 100)

    def test_monitoring_alerts_ignore_normal_warmup(self) -> None:
        alerts = evaluate_monitoring_alerts(
            in_bed=True,
            is_active=False,
            in_bed_frames=179 * 5,
            out_bed_frames=0,
            had_bed_session=True,
            breath_rate=0,
            breath_absent_frames=400,
            breath_output_ratio=0,
            heart_attempts=0,
            heart_valid_count=0,
            heart_age_seconds=None,
        )
        self.assertEqual(alerts, ())

    def test_monitoring_alerts_report_signal_and_coverage_problems(self) -> None:
        alerts = evaluate_monitoring_alerts(
            in_bed=True,
            is_active=False,
            in_bed_frames=6 * 60 * 5,
            out_bed_frames=0,
            had_bed_session=True,
            breath_rate=0,
            breath_absent_frames=70 * 5,
            breath_output_ratio=35,
            heart_attempts=20,
            heart_valid_count=4,
            heart_age_seconds=75.0,
        )
        self.assertIn("心率连续60秒无新有效值", alerts)
        self.assertIn("呼吸信号连续60秒无效", alerts)
        self.assertIn("心率覆盖率低于40%", alerts)
        self.assertIn("呼吸率覆盖率低于50%", alerts)

    def test_monitoring_alerts_keep_stable_breath_rate_when_bv_is_intermittent(self) -> None:
        alerts = evaluate_monitoring_alerts(
            in_bed=True,
            is_active=False,
            in_bed_frames=6 * 60 * 5,
            out_bed_frames=0,
            had_bed_session=True,
            breath_rate=19,
            breath_absent_frames=70 * 5,
            breath_output_ratio=80,
            heart_attempts=20,
            heart_valid_count=15,
            heart_age_seconds=5.0,
        )
        self.assertEqual(alerts, ())

    def test_monitoring_alerts_report_long_out_of_bed_time(self) -> None:
        alerts = evaluate_monitoring_alerts(
            in_bed=False,
            is_active=False,
            in_bed_frames=0,
            out_bed_frames=30 * 60 * 5,
            had_bed_session=True,
            breath_rate=0,
            breath_absent_frames=0,
            breath_output_ratio=0,
            heart_attempts=0,
            heart_valid_count=0,
            heart_age_seconds=None,
        )
        self.assertEqual(alerts, ("离床已超过30分钟",))


if __name__ == "__main__":
    unittest.main()
