from __future__ import annotations

import math
import unittest

from heart_rate import HeartRateEstimator, _AntennaPeak, parse_heart_raw_line


def synthetic_line(
    frame: int,
    bpm: float = 80.0,
    breath_rate: float = 18.0,
    target_bin: int = 11,
) -> str:
    timestamp = frame / 5.0
    respiration = 0.9 * math.sin(2.0 * math.pi * breath_rate / 60.0 * timestamp)
    heartbeat = 0.22 * math.sin(2.0 * math.pi * bpm / 60.0 * timestamp)
    fields = []
    for antenna, offset in enumerate((0.1, 0.6, -0.4)):
        bin_phase_offset = 0.7 if target_bin == 12 else 0.0
        phase = respiration + heartbeat + offset + bin_phase_offset
        amplitude = 2500 + antenna * 300
        fields.extend(
            (
                f"I{antenna}={round(amplitude * math.cos(phase))}",
                f"Q{antenna}={round(amplitude * math.sin(phase))}",
            )
        )
    return (
        f"HRRAW F={frame} V=1 P=1 A=0 BED=1 BIN={target_bin} ANT=0 "
        f"I=0 Q=0 AMP=1 PH=0 DPH=0 {' '.join(fields)}"
    )


def synthetic_multibin_line(frame: int, bpm: float = 80.0, breath_rate: float = 18.0) -> str:
    timestamp = frame / 20.0
    respiration = 0.9 * math.sin(2.0 * math.pi * breath_rate / 60.0 * timestamp)
    heartbeat = 0.22 * math.sin(2.0 * math.pi * bpm / 60.0 * timestamp)
    bins = []
    center_iq = []
    for slot, target_bin in enumerate((10, 11, 12)):
        fields = [f"B{slot}={target_bin}"]
        for antenna, offset in enumerate((0.1, 0.6, -0.4)):
            phase = respiration + heartbeat + offset + (target_bin - 11) * 0.35
            amplitude = 2500 + antenna * 300
            real = round(amplitude * math.cos(phase))
            imag = round(amplitude * math.sin(phase))
            fields.extend((f"B{slot}I{antenna}={real}", f"B{slot}Q{antenna}={imag}"))
            if target_bin == 11:
                center_iq.extend((f"I{antenna}={real}", f"Q{antenna}={imag}"))
        bins.extend(fields)
    return (
        f"HRRAW F={frame} FS=20 V=1 P=1 A=0 BED=1 BIN=11 ANT=0 "
        f"I=0 Q=0 AMP=1 PH=0 DPH=0 {' '.join(center_iq)} N=3 {' '.join(bins)}"
    )


class HeartRateEstimatorTests(unittest.TestCase):
    def test_parse_hrraw(self) -> None:
        sample = parse_heart_raw_line(synthetic_line(1), timestamp=0.2)
        self.assertIsNotNone(sample)
        assert sample is not None
        self.assertEqual(sample.frame, 1)
        self.assertEqual(sample.target_bin, 11)
        self.assertEqual(len(sample.antenna_iq), 3)

    def test_parse_multibin_hrraw(self) -> None:
        sample = parse_heart_raw_line(synthetic_multibin_line(1), timestamp=0.05)
        self.assertIsNotNone(sample)
        assert sample is not None
        self.assertEqual(sample.sample_rate_hz, 20)
        self.assertEqual(tuple(bin_index for bin_index, _iq in sample.bin_iq), (10, 11, 12))

    def test_detects_multibin_20hz_heart_rate(self) -> None:
        estimator = HeartRateEstimator()
        for frame in range(1000):
            estimator.add_line(synthetic_multibin_line(frame), frame / 20.0)
        estimate = None
        for now in (50.0, 55.0, 60.0, 65.0, 70.0):
            estimate = estimator.estimate(18, now)
        assert estimate is not None
        self.assertIsNotNone(estimate.bpm)
        assert estimate.bpm is not None
        self.assertLessEqual(abs(estimate.bpm - 80), 2)
        self.assertEqual(estimate.target_bins, (10, 11, 12))

    def test_detects_synthetic_heart_rate(self) -> None:
        estimator = HeartRateEstimator()
        estimates = []
        for frame in range(300):
            timestamp = frame / 5.0
            estimator.add_line(synthetic_line(frame), timestamp)
            estimate = estimator.maybe_estimate(18, timestamp)
            if estimate is not None and estimate.bpm is not None:
                estimates.append(estimate)
        self.assertTrue(estimates)
        self.assertLessEqual(abs(estimates[-1].bpm - 80), 2)
        self.assertEqual(estimates[-1].antenna_support, 3)

    def test_fuses_adjacent_bins_without_joining_phase(self) -> None:
        estimator = HeartRateEstimator()
        estimates = []
        for frame in range(360):
            timestamp = frame / 5.0
            target_bin = 11 if (frame // 20) % 2 == 0 else 12
            estimator.add_line(synthetic_line(frame, target_bin=target_bin), timestamp)
            estimate = estimator.maybe_estimate(18, timestamp)
            if estimate is not None and estimate.bpm is not None:
                estimates.append(estimate)
        self.assertTrue(estimates)
        self.assertLessEqual(abs(estimates[-1].bpm - 80), 2)
        self.assertEqual(estimates[-1].target_bins, (11, 12))

    def test_harmonic_conflict_when_candidate_tracks_breathing(self) -> None:
        estimator = HeartRateEstimator()
        estimator._candidate_history.extend(
            (index * 5.0, breath_rate * 3.0, breath_rate)
            for index, breath_rate in enumerate((17, 18, 19, 20, 21))
        )
        self.assertTrue(estimator._has_harmonic_conflict(63.0, 21))

    def test_no_harmonic_conflict_when_candidate_is_independent(self) -> None:
        estimator = HeartRateEstimator()
        estimator._candidate_history.extend(
            (index * 5.0, 60.0, breath_rate)
            for index, breath_rate in enumerate((17, 18, 19, 20, 21) * 2)
        )
        estimator._breath_history.extend(
            (index * 5.0, breath_rate)
            for index, breath_rate in enumerate((17, 18, 19, 20, 21) * 2)
        )
        self.assertFalse(estimator._has_harmonic_conflict(60.0, 20))

    def test_historical_harmonic_band_does_not_block_current_candidate(self) -> None:
        estimator = HeartRateEstimator()
        rates = (17, 18, 19, 20, 21, 22, 23, 22, 21, 20)
        candidates = (61, 60, 59, 58, 57, 56, 55, 54, 53, 52)
        estimator._candidate_history.extend(
            (index * 5.0, candidate, rate)
            for index, (candidate, rate) in enumerate(zip(candidates, rates))
        )
        estimator._breath_history.extend(
            (index * 5.0, rate)
            for index, rate in enumerate(rates)
        )
        self.assertFalse(estimator._has_harmonic_conflict(52.0, 20))

    def test_harmonic_conflict_uses_current_breath_rate(self) -> None:
        estimator = HeartRateEstimator()
        estimator._breath_history.extend(
            (index * 5.0, breath_rate)
            for index, breath_rate in enumerate((18, 19, 20, 21) * 2)
        )
        self.assertTrue(estimator._has_harmonic_conflict(59.0, 20))
        self.assertTrue(estimator._has_harmonic_conflict(66.0, 20))
        self.assertFalse(estimator._has_harmonic_conflict(67.0, 20))

    def test_no_harmonic_conflict_without_current_breath_rate(self) -> None:
        estimator = HeartRateEstimator()
        estimator._breath_history.extend(((0.0, 22), (5.0, 22)))
        self.assertEqual(estimator._nearest_harmonic_bpm(66.0, 0), 0)
        self.assertFalse(estimator._has_harmonic_conflict(66.0, 0))

    def test_waits_for_breath_rate_before_estimating(self) -> None:
        estimator = HeartRateEstimator()
        estimator.add_line(synthetic_line(1), 100.0)
        estimate = estimator.estimate(0, 100.0)
        self.assertIsNone(estimate.bpm)
        self.assertEqual(estimate.reason, "等待稳定呼吸数据")

    def test_expires_stale_smoothed_result(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 80.0
        estimator._last_accepted_at = 60.0
        estimator.add_line(synthetic_line(1), 241.0)
        estimator.estimate(18, 241.0)
        self.assertIsNone(estimator._smoothed_bpm)

    def test_out_of_bed_resets_continuity_anchor(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        estimator._last_accepted_at = 100.0
        estimator.add_line(synthetic_line(1).replace("BED=1", "BED=0"), 101.0)
        estimator.add_line(synthetic_line(2).replace("BED=1", "BED=0"), 103.1)
        self.assertIsNone(estimator._smoothed_bpm)
        self.assertFalse(estimator._candidate_history)

    def test_harmonic_candidate_does_not_replace_stable_result(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        estimator._last_accepted_at = 100.0
        accepted = estimator._accept_candidate(54.0, 105.0, harmonic_conflict=True)
        self.assertFalse(accepted)
        self.assertEqual(estimator._smoothed_bpm, 64.0)
        self.assertEqual(estimator._last_accepted_at, 100.0)

    def test_large_change_requires_long_confirmation(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        self.assertEqual(estimator._required_confirmations(79.0), 7)
        self.assertEqual(estimator._required_confirmations(69.0), 3)
        self.assertEqual(estimator._required_confirmations(69.5), 7)

    def test_initial_candidate_requires_five_updates(self) -> None:
        estimator = HeartRateEstimator()
        self.assertEqual(estimator._required_confirmations(64.0), 5)

    def test_large_change_is_rate_limited_after_confirmation(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        estimator._accept_candidate(79.0, 100.0)
        self.assertEqual(estimator._smoothed_bpm, 67.0)

    def test_strong_candidate_can_replace_wrong_anchor(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 106.0
        estimator._accept_candidate(64.0, 100.0, strong_candidate=True)
        self.assertEqual(estimator._smoothed_bpm, 64.0)

    def test_held_result_expires_before_continuity_anchor(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        estimator._last_accepted_at = 100.0
        self.assertEqual(estimator._held_bpm(144.0), 64)
        self.assertIsNone(estimator._held_bpm(146.0))
        self.assertEqual(estimator._smoothed_bpm, 64.0)

    def test_rejects_weak_consensus_candidate(self) -> None:
        self.assertFalse(HeartRateEstimator._is_strong_candidate(3, 2.8, 1.0, 0.90))
        self.assertTrue(HeartRateEstimator._is_strong_candidate(3, 4.2, 2.0, 0.85))

    def test_detects_high_order_breath_harmonic(self) -> None:
        estimator = HeartRateEstimator()
        estimator._breath_history.extend(((0.0, 18), (5.0, 21)))
        self.assertEqual(estimator._nearest_high_order_harmonic(106.0, 21), (5, 105))
        self.assertEqual(estimator._nearest_high_order_harmonic(64.0, 21), (0, 0))

    def test_stable_sixty_bpm_harmonic_overlap_can_be_reported(self) -> None:
        estimator = HeartRateEstimator()
        estimator._candidate_history.extend(
            (index * 5.0, candidate, 21)
            for index, candidate in enumerate((65.0, 65.0, 64.5, 64.5, 64.0, 64.0))
        )
        self.assertTrue(estimator._can_accept_harmonic_overlap(64.0, strong_candidate=True))
        self.assertFalse(estimator._can_accept_harmonic_overlap(55.0, strong_candidate=True))

    def test_third_breath_harmonic_remains_a_candidate(self) -> None:
        self.assertEqual(HeartRateEstimator._breath_harmonic_weight(66.0, 22), 1.0)
        self.assertEqual(HeartRateEstimator._breath_harmonic_weight(88.0, 22), 0.20)

    def test_consensus_continuity_protects_stable_result(self) -> None:
        estimator = HeartRateEstimator()
        estimator._smoothed_bpm = 64.0
        peaks = [
            [_AntennaPeak(50.0, 3.5), _AntennaPeak(66.0, 2.7)]
            for _antenna in range(3)
        ]
        bpm, support, _score, _spread = estimator._select_consensus(peaks)
        self.assertEqual(support, 3)
        self.assertAlmostEqual(bpm or 0.0, 66.0)

    def test_recent_candidates_seed_consensus_continuity(self) -> None:
        estimator = HeartRateEstimator()
        estimator._candidate_history.extend(
            (index * 5.0, 70.0, 22) for index in range(3)
        )
        peaks = [
            [_AntennaPeak(50.0, 3.5), _AntennaPeak(66.0, 2.7)]
            for _antenna in range(3)
        ]
        bpm, _support, _score, _spread = estimator._select_consensus(peaks)
        self.assertAlmostEqual(bpm or 0.0, 66.0)

    def test_unanchored_consensus_keeps_strongest_peak(self) -> None:
        estimator = HeartRateEstimator()
        peaks = [
            [_AntennaPeak(50.0, 3.5), _AntennaPeak(66.0, 2.7)]
            for _antenna in range(3)
        ]
        bpm, _support, _score, _spread = estimator._select_consensus(peaks)
        self.assertAlmostEqual(bpm or 0.0, 50.0)


if __name__ == "__main__":
    unittest.main()
