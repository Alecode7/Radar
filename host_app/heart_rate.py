from __future__ import annotations

from collections import Counter, deque
from dataclasses import dataclass
import math
import re
import statistics
import time


_FIELD_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)=(-?\d+)\b")


@dataclass(frozen=True)
class HeartRawSample:
    timestamp: float
    frame: int
    valid: bool
    active: bool
    in_bed: bool
    target_bin: int
    antenna_iq: tuple[tuple[int, int], tuple[int, int], tuple[int, int]]
    sample_rate_hz: int = 5
    bin_iq: tuple[
        tuple[int, tuple[tuple[int, int], tuple[int, int], tuple[int, int]]], ...
    ] = ()


@dataclass(frozen=True)
class HeartRateEstimate:
    bpm: int | None
    quality: str
    reason: str
    target_bin: int | None = None
    target_bins: tuple[int, ...] = ()
    bin_share: float = 0.0
    antenna_support: int = 0
    duration_s: float = 0.0
    breath_rate: int = 0
    harmonic_bpm: int = 0
    harmonic_conflict: bool = False
    harmonic_order: int = 0


@dataclass(frozen=True)
class _AntennaPeak:
    bpm: float
    score: float


@dataclass(frozen=True)
class _ConsensusCandidate:
    bpm: float
    support: int
    score: float
    spread: float
    rank: float


def parse_heart_raw_line(line: str, timestamp: float | None = None) -> HeartRawSample | None:
    if not line.startswith("HRRAW "):
        return None

    fields = {key: int(value) for key, value in _FIELD_RE.findall(line)}
    required = {"F", "V", "A", "BED", "BIN", "I0", "Q0", "I1", "Q1", "I2", "Q2"}
    if not required.issubset(fields):
        return None

    center_iq = (
        (fields["I0"], fields["Q0"]),
        (fields["I1"], fields["Q1"]),
        (fields["I2"], fields["Q2"]),
    )
    bin_iq = []
    for slot in range(max(0, fields.get("N", 0))):
        keys = [f"B{slot}"]
        for antenna in range(3):
            keys.extend((f"B{slot}I{antenna}", f"B{slot}Q{antenna}"))
        if not all(key in fields for key in keys):
            continue
        bin_iq.append(
            (
                fields[f"B{slot}"],
                tuple(
                    (fields[f"B{slot}I{antenna}"], fields[f"B{slot}Q{antenna}"])
                    for antenna in range(3)
                ),
            )
        )
    if not bin_iq:
        bin_iq.append((fields["BIN"], center_iq))

    return HeartRawSample(
        timestamp=time.monotonic() if timestamp is None else timestamp,
        frame=fields["F"],
        valid=bool(fields["V"]),
        active=bool(fields["A"]),
        in_bed=bool(fields["BED"]),
        target_bin=fields["BIN"],
        antenna_iq=center_iq,
        sample_rate_hz=max(1, fields.get("FS", 5)),
        bin_iq=tuple(bin_iq),
    )


class HeartRateEstimator:
    """Experimental heart-rate estimator for 5 Hz and 20 Hz HRRAW streams."""

    _RESULT_EXPIRE_S = 120.0
    _RESULT_HOLD_S = 45.0
    _LARGE_CHANGE_BPM = 5.0
    _LARGE_CHANGE_CONFIRMATIONS = 7
    _MAX_LARGE_STEP_BPM = 3.0
    _MIN_CLUSTER_SCORE = 3.5
    _CONSENSUS_ANCHOR_RANGE_BPM = 8.0
    _CONSENSUS_RANK_MARGIN = 3.5
    _CONSENSUS_DISTANCE_PENALTY = 0.35

    def __init__(self) -> None:
        self.samples: deque[HeartRawSample] = deque(maxlen=2200)
        self._last_frame: int | None = None
        self._last_estimate_at = 0.0
        self._smoothed_bpm: float | None = None
        self._candidate_history: deque[tuple[float, float, int]] = deque(maxlen=20)
        self._breath_history: deque[tuple[float, int]] = deque(maxlen=24)
        self._last_accepted_at = 0.0
        self._consistent_updates = 0
        self._out_of_bed_since: float | None = None
        self._out_of_bed_reset = False

    def reset(self) -> None:
        self.samples.clear()
        self._last_frame = None
        self._last_estimate_at = 0.0
        self._smoothed_bpm = None
        self._candidate_history.clear()
        self._breath_history.clear()
        self._last_accepted_at = 0.0
        self._consistent_updates = 0
        self._out_of_bed_since = None
        self._out_of_bed_reset = False

    def add_line(self, line: str, timestamp: float | None = None) -> HeartRawSample | None:
        sample = parse_heart_raw_line(line, timestamp)
        if sample is None:
            return None
        if self._last_frame is not None and sample.frame < self._last_frame:
            self.reset()
        if sample.in_bed:
            self._out_of_bed_since = None
            self._out_of_bed_reset = False
        else:
            if self._out_of_bed_since is None:
                self._out_of_bed_since = sample.timestamp
            elif not self._out_of_bed_reset and sample.timestamp - self._out_of_bed_since >= 2.0:
                self.samples.clear()
                self._smoothed_bpm = None
                self._candidate_history.clear()
                self._breath_history.clear()
                self._last_accepted_at = 0.0
                self._consistent_updates = 0
                self._out_of_bed_reset = True
        self.samples.append(sample)
        self._last_frame = sample.frame
        return sample

    def maybe_estimate(self, breath_rate: int = 0, now: float | None = None) -> HeartRateEstimate | None:
        current = time.monotonic() if now is None else now
        if current - self._last_estimate_at < 5.0:
            return None
        self._last_estimate_at = current
        return self.estimate(breath_rate, current)

    def estimate(self, breath_rate: int = 0, now: float | None = None) -> HeartRateEstimate:
        if not self.samples:
            return HeartRateEstimate(None, "--", "等待在床数据", breath_rate=breath_rate)

        current = self.samples[-1].timestamp if now is None else now
        self._observe_breath_rate(current, breath_rate)
        effective_breath_rate = self._median_breath_rate(current)
        if self._smoothed_bpm is not None and current - self._last_accepted_at > self._RESULT_EXPIRE_S:
            self._smoothed_bpm = None
            self._consistent_updates = 0
        if effective_breath_rate < 8:
            self._candidate_history.clear()
            return HeartRateEstimate(None, "--", "等待稳定呼吸数据", breath_rate=0)
        recent = [sample for sample in self.samples if current - sample.timestamp <= 75.0]
        if any(sample.active for sample in recent if current - sample.timestamp <= 3.0):
            return HeartRateEstimate(None, "低", "活动干扰，暂停估计", breath_rate=effective_breath_rate)

        quiet = [sample for sample in recent if sample.valid and sample.in_bed and not sample.active]
        quiet_seconds = self._sample_duration(quiet)
        if len(quiet) < 80 or quiet_seconds < 20.0:
            seconds = quiet_seconds
            return HeartRateEstimate(
                None,
                "--",
                f"安静数据采集中 {seconds:.0f}/20秒",
                duration_s=seconds,
                breath_rate=effective_breath_rate,
            )

        bin_counts = Counter(sample.target_bin for sample in quiet)
        target_bin, target_count = bin_counts.most_common(1)[0]
        simultaneous_bins = any(len(sample.bin_iq) > 1 for sample in quiet)
        if simultaneous_bins:
            available_counts = Counter(
                bin_index
                for sample in quiet
                for bin_index, _antenna_iq in sample.bin_iq
            )
            target_bins = self._select_simultaneous_bins(available_counts, len(quiet), target_bin)
            bin_share = (
                sum(available_counts[target] for target in target_bins)
                / (len(quiet) * len(target_bins))
                if target_bins
                else 0.0
            )
        else:
            target_bins = self._select_adjacent_bins(bin_counts)
            bin_share = sum(bin_counts[target] for target in target_bins) / len(quiet)
        if bin_share < 0.65:
            self._candidate_history.clear()
            return HeartRateEstimate(
                None,
                "低",
                "目标距离不稳定",
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                breath_rate=effective_breath_rate,
            )

        cutoff = current - 60.0
        window = [
            sample
            for sample in quiet
            if sample.timestamp >= cutoff
            and any(bin_index in target_bins for bin_index, _antenna_iq in sample.bin_iq)
        ]
        segments = self._split_bin_segments(window, target_bins)
        duration_by_bin: dict[int, float] = {}
        samples_by_bin: Counter[int] = Counter()
        for bin_index, segment in segments:
            duration_by_bin[bin_index] = duration_by_bin.get(bin_index, 0.0) + self._sample_duration(segment)
            samples_by_bin[bin_index] += len(segment)
        if simultaneous_bins:
            duration_s = max(duration_by_bin.values(), default=0.0)
            usable_samples = max(samples_by_bin.values(), default=0)
        else:
            duration_s = sum(duration_by_bin.values())
            usable_samples = sum(samples_by_bin.values())
        if usable_samples < 80 or duration_s < 30.0:
            bin_text = "/".join(str(value) for value in target_bins)
            return HeartRateEstimate(
                None,
                "低",
                f"锁定bin {bin_text}中 {duration_s:.0f}/35秒",
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
            )

        peaks_by_antenna = [
            self._antenna_peaks_from_segments(segments, antenna, effective_breath_rate)
            for antenna in range(3)
        ]
        bpm, support, cluster_score, spread = self._select_consensus(peaks_by_antenna)
        if bpm is None or support < 2:
            self._consistent_updates = 0
            self._candidate_history.clear()
            return HeartRateEstimate(
                None,
                "低",
                "三路天线结果不一致",
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
            )

        strong_candidate = self._is_strong_candidate(support, cluster_score, spread, bin_share)
        if not strong_candidate:
            return HeartRateEstimate(
                None,
                "低",
                "心率频谱峰值不足",
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                antenna_support=support,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
            )

        high_order, high_harmonic_bpm = self._nearest_high_order_harmonic(bpm, effective_breath_rate)
        if high_order > 0:
            return HeartRateEstimate(
                None,
                "低",
                f"呼吸{high_order}次谐波冲突",
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                antenna_support=support,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
                harmonic_bpm=high_harmonic_bpm,
                harmonic_conflict=True,
                harmonic_order=high_order,
            )

        self._candidate_history.append((current, bpm, effective_breath_rate))
        while self._candidate_history and current - self._candidate_history[0][0] > 90.0:
            self._candidate_history.popleft()

        candidate_harmonic_conflict = self._has_harmonic_conflict(bpm, effective_breath_rate)
        required = self._required_confirmations(bpm)
        if candidate_harmonic_conflict:
            required = max(required, 6)
        confirmed = self._confirmed_candidate(required)
        if confirmed is None:
            held_bpm = self._held_bpm(current)
            harmonic_bpm = self._nearest_harmonic_bpm(bpm, effective_breath_rate)
            harmonic_conflict = self._has_harmonic_conflict(bpm, effective_breath_rate)
            reason = f"候选{int(round(bpm))}确认中"
            if harmonic_conflict:
                reason = f"候选{int(round(bpm))}谐波冲突确认中"
            return HeartRateEstimate(
                held_bpm,
                "低",
                reason,
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                antenna_support=support,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
                harmonic_bpm=harmonic_bpm,
                harmonic_conflict=harmonic_conflict,
            )

        confirmed_harmonic_bpm = self._nearest_harmonic_bpm(confirmed, effective_breath_rate)
        confirmed_harmonic_conflict = self._has_harmonic_conflict(confirmed, effective_breath_rate)
        accept_harmonic_overlap = self._can_accept_harmonic_overlap(confirmed, strong_candidate)
        if confirmed_harmonic_conflict and not accept_harmonic_overlap:
            held_bpm = self._held_bpm(current)
            reason = "呼吸谐波冲突，保持上次结果" if held_bpm is not None else "呼吸谐波冲突，等待分离"
            return HeartRateEstimate(
                held_bpm,
                "低",
                reason,
                target_bin=target_bin,
                target_bins=target_bins,
                bin_share=bin_share,
                antenna_support=support,
                duration_s=duration_s,
                breath_rate=effective_breath_rate,
                harmonic_bpm=confirmed_harmonic_bpm,
                harmonic_conflict=True,
                harmonic_order=3,
            )

        self._accept_candidate(confirmed, current, strong_candidate=strong_candidate)

        self._consistent_updates = self._trailing_consistent_count(confirmed)

        quality = "低"
        if support == 3 and bin_share >= 0.80 and spread <= 4.0 and cluster_score >= 5.0:
            quality = "中"
        if (
            support == 3
            and bin_share >= 0.90
            and spread <= 2.5
            and cluster_score >= 8.0
            and self._consistent_updates >= 2
        ):
            quality = "较高"

        reported_bpm = int(round(self._smoothed_bpm))
        harmonic_bpm = self._nearest_harmonic_bpm(reported_bpm, effective_breath_rate)
        harmonic_conflict = self._has_harmonic_conflict(reported_bpm, effective_breath_rate)
        if harmonic_conflict:
            quality = "低"

        return HeartRateEstimate(
            bpm=reported_bpm,
            quality=quality,
            reason="谐波重叠候选" if harmonic_conflict else "工程候选值",
            target_bin=target_bin,
            target_bins=target_bins,
            bin_share=bin_share,
            antenna_support=support,
            duration_s=duration_s,
            breath_rate=effective_breath_rate,
            harmonic_bpm=harmonic_bpm,
            harmonic_conflict=harmonic_conflict,
            harmonic_order=3 if harmonic_conflict else 0,
        )

    def _held_bpm(self, current: float) -> int | None:
        if self._smoothed_bpm is None or current - self._last_accepted_at > self._RESULT_HOLD_S:
            return None
        return int(round(self._smoothed_bpm))

    def _required_confirmations(self, candidate: float) -> int:
        if self._smoothed_bpm is None:
            return 5
        if abs(candidate - self._smoothed_bpm) > self._LARGE_CHANGE_BPM:
            return self._LARGE_CHANGE_CONFIRMATIONS
        return 3

    def _accept_candidate(
        self,
        candidate: float,
        current: float,
        harmonic_conflict: bool = False,
        strong_candidate: bool = False,
    ) -> bool:
        if harmonic_conflict:
            return False
        if self._smoothed_bpm is None:
            self._smoothed_bpm = candidate
        else:
            delta = candidate - self._smoothed_bpm
            if abs(delta) > self._LARGE_CHANGE_BPM:
                if strong_candidate:
                    self._smoothed_bpm = candidate
                else:
                    step = max(-self._MAX_LARGE_STEP_BPM, min(self._MAX_LARGE_STEP_BPM, delta))
                    self._smoothed_bpm += step
            else:
                self._smoothed_bpm = self._smoothed_bpm * 0.65 + candidate * 0.35
        self._last_accepted_at = current
        return True

    def _can_accept_harmonic_overlap(self, candidate: float, strong_candidate: bool) -> bool:
        if not strong_candidate or not 60.0 <= candidate <= 90.0:
            return False
        consistent = self._trailing_consistent_count(candidate)
        if self._smoothed_bpm is None:
            return consistent >= 6
        return abs(candidate - self._smoothed_bpm) <= self._LARGE_CHANGE_BPM or consistent >= 7

    @classmethod
    def _is_strong_candidate(
        cls, support: int, cluster_score: float, spread: float, bin_share: float
    ) -> bool:
        return (
            support == 3
            and cluster_score >= cls._MIN_CLUSTER_SCORE
            and spread <= 3.0
            and bin_share >= 0.80
        )

    def _observe_breath_rate(self, timestamp: float, breath_rate: int) -> None:
        if 8 <= breath_rate <= 35:
            self._breath_history.append((timestamp, breath_rate))
        while self._breath_history and timestamp - self._breath_history[0][0] > 75.0:
            self._breath_history.popleft()

    def _median_breath_rate(self, timestamp: float) -> int:
        values = [
            value
            for sample_time, value in self._breath_history
            if timestamp - sample_time <= 60.0
        ]
        return int(round(statistics.median(values))) if values else 0

    def _has_harmonic_conflict(self, bpm: float, breath_rate: int) -> bool:
        if breath_rate < 8 or abs(bpm - breath_rate * 3.0) > 6.0:
            return False
        harmonic_bpm = self._nearest_harmonic_bpm(bpm, breath_rate)
        if harmonic_bpm == 0:
            return False

        history = [
            (candidate, rate)
            for _timestamp, candidate, rate in self._candidate_history
            if rate >= 8
        ]
        if len(history) < 10:
            return True

        heart_values = [candidate for candidate, _rate in history]
        harmonic_values = [rate * 3.0 for _candidate, rate in history]
        harmonic_range = max(harmonic_values) - min(harmonic_values)
        correlation = self._correlation(heart_values, harmonic_values)
        heart_spread = statistics.pstdev(heart_values)
        independent = harmonic_range >= 9.0 and abs(correlation) < 0.20 and heart_spread <= 2.5
        return not independent

    def _nearest_harmonic_bpm(self, bpm: float, breath_rate: int) -> int:
        if breath_rate < 8:
            return 0
        recent_rates = [rate for _timestamp, rate in self._breath_history if rate >= 8]
        if not recent_rates and breath_rate >= 8:
            recent_rates = [breath_rate]
        if not recent_rates:
            return 0

        sorted_rates = sorted(recent_rates)
        lower_index = int((len(sorted_rates) - 1) * 0.15)
        upper_index = int((len(sorted_rates) - 1) * 0.85)
        lower_rate = sorted_rates[lower_index]
        upper_rate = sorted_rates[upper_index]
        reference_rates = [rate for rate in recent_rates if lower_rate <= rate <= upper_rate]
        if breath_rate >= 8:
            reference_rates.append(breath_rate)
        return int(round(min((rate * 3.0 for rate in reference_rates), key=lambda value: abs(bpm - value))))

    def _nearest_high_order_harmonic(self, bpm: float, breath_rate: int) -> tuple[int, int]:
        if bpm < 85.0 or breath_rate < 8:
            return 0, 0
        recent_rates = [rate for _timestamp, rate in self._breath_history if 8 <= rate <= 35]
        if breath_rate not in recent_rates:
            recent_rates.append(breath_rate)
        candidates = [
            (abs(bpm - rate * order), order, int(round(rate * order)))
            for rate in recent_rates
            for order in (4, 5, 6)
        ]
        distance, order, harmonic_bpm = min(candidates)
        return (order, harmonic_bpm) if distance <= 4.0 else (0, 0)

    @staticmethod
    def _correlation(left: list[float], right: list[float]) -> float:
        left_mean = statistics.mean(left)
        right_mean = statistics.mean(right)
        numerator = sum(
            (left_value - left_mean) * (right_value - right_mean)
            for left_value, right_value in zip(left, right)
        )
        left_energy = sum((value - left_mean) ** 2 for value in left)
        right_energy = sum((value - right_mean) ** 2 for value in right)
        if left_energy <= 1e-9 and right_energy > 1e-9:
            return 0.0
        denominator = math.sqrt(left_energy * right_energy)
        return numerator / denominator if denominator > 0.0 else 1.0

    def _confirmed_candidate(self, required: int) -> float | None:
        if len(self._candidate_history) < required:
            return None
        values = [value for _timestamp, value, _breath in list(self._candidate_history)[-required:]]
        median = statistics.median(values)
        if max(abs(value - median) for value in values) > 6.0:
            return None
        return median

    def _trailing_consistent_count(self, center: float) -> int:
        count = 0
        for _timestamp, value, _breath in reversed(self._candidate_history):
            if abs(value - center) > 6.0:
                break
            count += 1
        return count

    @staticmethod
    def _sample_duration(samples: list[HeartRawSample]) -> float:
        if len(samples) < 2:
            return 0.0
        return max(0.0, samples[-1].timestamp - samples[0].timestamp)

    @staticmethod
    def _select_adjacent_bins(bin_counts: Counter[int]) -> tuple[int, ...]:
        dominant, dominant_count = bin_counts.most_common(1)[0]
        neighbors = [
            (value, bin_counts[value])
            for value in (dominant - 1, dominant + 1)
            if bin_counts[value] > 0
        ]
        neighbors.sort(key=lambda item: item[1], reverse=True)
        selected = [dominant]
        if neighbors and neighbors[0][1] >= max(12, int(dominant_count * 0.25)):
            selected.append(neighbors[0][0])
        return tuple(sorted(selected))

    @staticmethod
    def _select_simultaneous_bins(
        bin_counts: Counter[int], sample_count: int, center_bin: int
    ) -> tuple[int, ...]:
        minimum_count = max(1, int(sample_count * 0.65))
        selected = [bin_index for bin_index, count in bin_counts.items() if count >= minimum_count]
        selected.sort(key=lambda bin_index: (abs(bin_index - center_bin), -bin_counts[bin_index]))
        return tuple(sorted(selected[:5]))

    @classmethod
    def _split_bin_segments(
        cls, samples: list[HeartRawSample], target_bins: tuple[int, ...]
    ) -> list[tuple[int, list[HeartRawSample]]]:
        by_bin: dict[int, list[HeartRawSample]] = {bin_index: [] for bin_index in target_bins}
        for sample in samples:
            available = {bin_index for bin_index, _antenna_iq in sample.bin_iq}
            for bin_index in target_bins:
                if bin_index in available:
                    by_bin[bin_index].append(sample)

        segments: list[tuple[int, list[HeartRawSample]]] = []
        for bin_index, bin_samples in by_bin.items():
            current: list[HeartRawSample] = []
            for sample in bin_samples:
                if current:
                    frame_gap = sample.frame - current[-1].frame
                    time_gap = sample.timestamp - current[-1].timestamp
                    max_frame_gap = max(3, int(round(sample.sample_rate_hz * 0.8)))
                    if frame_gap > max_frame_gap or time_gap > 0.8:
                        if len(current) >= 12 and cls._sample_duration(current) >= 2.0:
                            segments.append((bin_index, current))
                        current = []
                current.append(sample)
            if len(current) >= 12 and cls._sample_duration(current) >= 2.0:
                segments.append((bin_index, current))
        return segments

    @staticmethod
    def _antenna_iq_for_bin(
        sample: HeartRawSample, bin_index: int
    ) -> tuple[tuple[int, int], tuple[int, int], tuple[int, int]]:
        for sample_bin, antenna_iq in sample.bin_iq:
            if sample_bin == bin_index:
                return antenna_iq
        return sample.antenna_iq

    @staticmethod
    def _unwrap_phase(values: list[tuple[int, int]]) -> list[float]:
        phases: list[float] = []
        offset = 0.0
        previous: float | None = None
        for real, imag in values:
            phase = math.atan2(imag, real)
            if previous is not None:
                delta = phase - previous
                if delta > math.pi:
                    offset -= 2.0 * math.pi
                elif delta < -math.pi:
                    offset += 2.0 * math.pi
            phases.append(phase + offset)
            previous = phase
        return phases

    @staticmethod
    def _detrend(values: list[float], timestamps: list[float]) -> list[float]:
        count = len(values)
        x_mean = sum(timestamps) / count
        y_mean = sum(values) / count
        denominator = sum((timestamp - x_mean) ** 2 for timestamp in timestamps)
        slope = 0.0
        if denominator > 0.0:
            slope = sum(
                (timestamp - x_mean) * (value - y_mean)
                for timestamp, value in zip(timestamps, values)
            ) / denominator
        return [
            value - (y_mean + slope * (timestamp - x_mean))
            for timestamp, value in zip(timestamps, values)
        ]

    @staticmethod
    def _high_pass(values: list[float], sample_rate: float) -> list[float]:
        width = max(3, int(round(sample_rate * 1.5)))
        half = width // 2
        prefix = [0.0]
        for value in values:
            prefix.append(prefix[-1] + value)
        output: list[float] = []
        for index, value in enumerate(values):
            left = max(0, index - half)
            right = min(len(values), index + half + 1)
            average = (prefix[right] - prefix[left]) / (right - left)
            output.append(value - average)
        return output

    @staticmethod
    def _power_at_bpm(values: list[float], timestamps: list[float], bpm: float) -> float:
        omega = 2.0 * math.pi * (bpm / 60.0)
        real = 0.0
        imag = 0.0
        last = len(values) - 1
        for index, (timestamp, value) in enumerate(zip(timestamps, values)):
            window = 1.0 if last <= 0 else 0.5 - 0.5 * math.cos(2.0 * math.pi * index / last)
            angle = omega * timestamp
            weighted = value * window
            real += weighted * math.cos(angle)
            imag -= weighted * math.sin(angle)
        return real * real + imag * imag

    def _antenna_peaks_from_segments(
        self,
        segments: list[tuple[int, list[HeartRawSample]]],
        antenna: int,
        breath_rate: int,
    ) -> list[_AntennaPeak]:
        bpms = [48.0 + index * 0.5 for index in range(145)]
        powers = [0.0 for _bpm in bpms]
        total_weight = 0.0

        for bin_index, samples in segments:
            segment_powers = self._segment_powers(samples, bin_index, antenna, bpms)
            baseline = statistics.median(segment_powers) + 1e-9
            duration_s = self._sample_duration(samples)
            weight = min(duration_s, 10.0)
            total_weight += weight
            for index, power in enumerate(segment_powers):
                powers[index] += min(power / baseline, 100.0) * weight

        if total_weight <= 0.0:
            return []
        powers = [power / total_weight for power in powers]
        baseline = statistics.median(powers) + 1e-9
        return self._find_peaks(bpms, powers, baseline, breath_rate)

    def _segment_powers(
        self,
        samples: list[HeartRawSample],
        bin_index: int,
        antenna: int,
        bpms: list[float],
    ) -> list[float]:
        duration_s = self._sample_duration(samples)
        sample_rate = (len(samples) - 1) / duration_s
        timestamps = [sample.timestamp - samples[0].timestamp for sample in samples]
        phase = self._unwrap_phase(
            [self._antenna_iq_for_bin(sample, bin_index)[antenna] for sample in samples]
        )
        filtered = self._high_pass(self._detrend(phase, timestamps), sample_rate)
        return [self._power_at_bpm(filtered, timestamps, bpm) for bpm in bpms]

    def _find_peaks(
        self,
        bpms: list[float],
        powers: list[float],
        baseline: float,
        breath_rate: int,
    ) -> list[_AntennaPeak]:
        peak_indices = [
            index
            for index in range(1, len(powers) - 1)
            if powers[index] >= powers[index - 1] and powers[index] >= powers[index + 1]
        ]
        peak_indices.sort(key=lambda index: powers[index], reverse=True)

        peaks: list[_AntennaPeak] = []
        for index in peak_indices:
            bpm = bpms[index]
            if any(abs(bpm - peak.bpm) < 4.0 for peak in peaks):
                continue
            score = math.log1p(powers[index] / baseline)
            score *= self._breath_harmonic_weight(bpm, breath_rate)
            peaks.append(_AntennaPeak(bpm, score))
            if len(peaks) >= 6:
                break
        return peaks

    @staticmethod
    def _breath_harmonic_weight(bpm: float, breath_rate: int) -> float:
        if breath_rate < 8:
            return 1.0
        weight = 1.0
        # Keep the third harmonic as a candidate because it overlaps normal
        # resting heart rates; it is downgraded later by HCON instead.
        for harmonic in (2, 4):
            distance = abs(bpm - breath_rate * harmonic)
            if distance <= 2.0:
                weight = min(weight, 0.20)
            elif distance <= 4.0:
                weight = min(weight, 0.55)
        return weight

    def _select_consensus(
        self, peaks_by_antenna: list[list[_AntennaPeak]]
    ) -> tuple[float | None, int, float, float]:
        candidates = self._rank_consensus_candidates(peaks_by_antenna)
        if not candidates:
            return None, 0, 0.0, 999.0

        selected = candidates[0]
        anchor = self._consensus_anchor()
        if anchor is not None:
            nearby = [
                candidate
                for candidate in candidates
                if candidate.support == 3
                and candidate.score >= self._MIN_CLUSTER_SCORE
                and candidate.spread <= 3.0
                and abs(candidate.bpm - anchor) <= self._CONSENSUS_ANCHOR_RANGE_BPM
                and candidate.rank >= selected.rank - self._CONSENSUS_RANK_MARGIN
            ]
            if nearby:
                selected = max(
                    nearby,
                    key=lambda candidate: candidate.rank
                    - abs(candidate.bpm - anchor) * self._CONSENSUS_DISTANCE_PENALTY,
                )

        return selected.bpm, selected.support, selected.score, selected.spread

    def _consensus_anchor(self) -> float | None:
        if self._smoothed_bpm is not None and 58.0 <= self._smoothed_bpm <= 95.0:
            return self._smoothed_bpm

        recent = [
            candidate
            for _timestamp, candidate, _breath in list(self._candidate_history)[-6:]
            if 60.0 <= candidate <= 90.0
        ]
        if len(recent) < 3:
            return None
        center = statistics.median(recent)
        consistent = [candidate for candidate in recent if abs(candidate - center) <= 6.0]
        return statistics.median(consistent) if len(consistent) >= 3 else None

    @staticmethod
    def _rank_consensus_candidates(
        peaks_by_antenna: list[list[_AntennaPeak]],
    ) -> list[_ConsensusCandidate]:
        centers = [peak.bpm for peaks in peaks_by_antenna for peak in peaks]
        candidates_by_bpm: dict[int, _ConsensusCandidate] = {}
        for center in centers:
            matches: list[_AntennaPeak] = []
            for antenna_peaks in peaks_by_antenna:
                nearby = [peak for peak in antenna_peaks if abs(peak.bpm - center) <= 4.0]
                if nearby:
                    matches.append(max(nearby, key=lambda peak: peak.score))
            if len(matches) < 2:
                continue
            total_score = sum(peak.score for peak in matches)
            weighted_bpm = sum(peak.bpm * peak.score for peak in matches) / max(total_score, 1e-9)
            spread = max(peak.bpm for peak in matches) - min(peak.bpm for peak in matches)
            rank = total_score + 1.5 * (len(matches) - 1) - spread * 0.25
            rounded_bpm = int(round(weighted_bpm))
            candidate = _ConsensusCandidate(
                weighted_bpm, len(matches), total_score, spread, rank
            )
            previous = candidates_by_bpm.get(rounded_bpm)
            if previous is None or candidate.rank > previous.rank:
                candidates_by_bpm[rounded_bpm] = candidate
        candidates = list(candidates_by_bpm.values())
        candidates.sort(key=lambda candidate: candidate.rank, reverse=True)
        return candidates
