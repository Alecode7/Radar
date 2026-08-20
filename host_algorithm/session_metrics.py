from __future__ import annotations

from dataclasses import dataclass
import statistics
from typing import Iterable, Sequence


@dataclass(frozen=True)
class HeartSessionStats:
    attempts: int
    valid_count: int
    coverage_percent: int
    average_bpm: float | None
    minimum_bpm: int | None
    maximum_bpm: int | None


@dataclass(frozen=True)
class TenMinuteSummary:
    record_count: int
    sleep_state: int | None
    average_breath_rate: float | None
    average_heart_rate: float | None
    turn_count: int
    large_motion_percent: int
    small_motion_percent: int


@dataclass(frozen=True)
class SleepQualityScore:
    available: bool
    total_score: int | None
    grade: str
    confidence_percent: int
    confidence_label: str
    duration_score: int
    onset_score: int
    continuity_score: int
    motion_score: int
    breath_score: int
    sleep_ratio: int
    interruption_rate: float
    turn_rate: float
    breath_mad: float | None
    note: str


@dataclass
class SleepOnsetTracker:
    quiet_started_at: float | None = None
    estimated_onset_at: float | None = None
    confirmed_at: float | None = None

    def reset(self) -> None:
        self.quiet_started_at = None
        self.estimated_onset_at = None
        self.confirmed_at = None

    def update(self, timestamp: float, in_bed: bool, sleep_state: int) -> None:
        if self.confirmed_at is not None:
            return
        state = int(sleep_state)
        if not in_bed or state not in (2, 3):
            self.quiet_started_at = None
            return
        if state == 2:
            if self.quiet_started_at is None:
                self.quiet_started_at = float(timestamp)
            return
        if state == 3:
            self.estimated_onset_at = (
                self.quiet_started_at
                if self.quiet_started_at is not None
                else float(timestamp)
            )
            self.confirmed_at = float(timestamp)


def stream_health_state(
    *, connected: bool, monitoring: bool, age_seconds: float | None
) -> str:
    if not connected:
        return "disconnected"
    if not monitoring:
        return "stopped"
    if age_seconds is None:
        return "waiting"
    if age_seconds > 10.0:
        return "interrupted"
    if age_seconds > 5.0:
        return "delayed"
    return "normal"


def should_auto_export_session(
    in_bed_frames: int,
    *,
    minimum_frames: int = 150,
) -> bool:
    return max(int(in_bed_frames), 0) >= max(int(minimum_frames), 1)


def split_vital_segments(
    samples: Iterable[tuple[float, float | int]],
    *,
    max_gap_seconds: float,
) -> tuple[tuple[tuple[float, float], ...], ...]:
    segments: list[tuple[tuple[float, float], ...]] = []
    current: list[tuple[float, float]] = []
    previous_timestamp: float | None = None
    for timestamp, value in sorted(samples, key=lambda item: float(item[0])):
        sample_time = float(timestamp)
        sample_value = float(value)
        invalid = sample_value <= 0
        too_far = (
            previous_timestamp is not None
            and sample_time - previous_timestamp > max(float(max_gap_seconds), 0.0)
        )
        if invalid or too_far:
            if current:
                segments.append(tuple(current))
                current = []
            previous_timestamp = None if invalid else sample_time
            if invalid:
                continue
        current.append((sample_time, sample_value))
        previous_timestamp = sample_time
    if current:
        segments.append(tuple(current))
    return tuple(segments)


def calculate_motion_score(motion_intensity: int, active_motion_points: int) -> int:
    """Map the firmware's four motion levels to an engineering 0-100 index."""
    level = max(0, min(int(motion_intensity), 3))
    if level == 0:
        return 0
    base_score = (0, 25, 55, 80)[level]
    point_bonus = min(max(int(active_motion_points), 0) * 2, 20)
    return min(100, base_score + point_bonus)


def calculate_sleep_quality_score(
    *,
    in_bed_frames: int,
    likely_sleep_frames: int,
    sleep_interruptions: int,
    sleep_turns: int,
    active_ratio: int,
    breath_valid_ratio: int,
    breath_output_ratio: int,
    breath_rates: Iterable[int],
    onset_latency_seconds: float | None,
    stream_gap_count: int = 0,
    frame_rate_hz: int = 5,
) -> SleepQualityScore:
    """Calculate a conservative engineering score for complete sleep sessions."""
    frame_rate = max(int(frame_rate_hz), 1)
    in_bed_seconds = max(int(in_bed_frames), 0) / frame_rate
    in_bed_hours = in_bed_seconds / 3600.0
    sleep_seconds = max(int(likely_sleep_frames), 0) / frame_rate
    sleep_hours = sleep_seconds / 3600.0
    sleep_ratio = (
        round(min(sleep_seconds * 100 / in_bed_seconds, 100.0))
        if in_bed_seconds > 0
        else 0
    )
    interruption_rate = max(int(sleep_interruptions), 0) / max(sleep_hours, 1.0)
    turn_rate = max(int(sleep_turns), 0) / max(sleep_hours, 1.0)

    valid_ratio = max(0, min(int(breath_valid_ratio), 100))
    output_ratio = max(0, min(int(breath_output_ratio), 100))
    confidence = round(valid_ratio * 0.55 + output_ratio * 0.45)
    confidence = max(0, confidence - min(max(int(stream_gap_count), 0) * 15, 45))
    confidence_label = "高" if confidence >= 80 else "中" if confidence >= 60 else "低"

    if in_bed_hours < 4.0:
        return SleepQualityScore(
            available=False,
            total_score=None,
            grade="未评分",
            confidence_percent=confidence,
            confidence_label=confidence_label,
            duration_score=0,
            onset_score=0,
            continuity_score=0,
            motion_score=0,
            breath_score=0,
            sleep_ratio=sleep_ratio,
            interruption_rate=round(interruption_rate, 2),
            turn_rate=round(turn_rate, 2),
            breath_mad=None,
            note="记录不足4小时，仅保留入睡和生命体征分析",
        )
    if in_bed_hours > 12.0:
        return SleepQualityScore(
            available=False,
            total_score=None,
            grade="未评分",
            confidence_percent=confidence,
            confidence_label=confidence_label,
            duration_score=0,
            onset_score=0,
            continuity_score=0,
            motion_score=0,
            breath_score=0,
            sleep_ratio=sleep_ratio,
            interruption_rate=round(interruption_rate, 2),
            turn_rate=round(turn_rate, 2),
            breath_mad=None,
            note="记录超过12小时，请先核对在床和离床判断",
        )

    if in_bed_hours < 5:
        duration_score = 15
    elif in_bed_hours < 6:
        duration_score = 19
    elif in_bed_hours < 7:
        duration_score = 22
    elif in_bed_hours <= 9:
        duration_score = 25
    elif in_bed_hours <= 10:
        duration_score = 22
    else:
        duration_score = 18

    if onset_latency_seconds is None or onset_latency_seconds < 0:
        onset_score = 0
    else:
        onset_minutes = onset_latency_seconds / 60.0
        if onset_minutes <= 20:
            onset_score = 10
        elif onset_minutes <= 30:
            onset_score = 8
        elif onset_minutes <= 45:
            onset_score = 5
        elif onset_minutes <= 60:
            onset_score = 2
        else:
            onset_score = 0

    if sleep_ratio >= 85:
        sleep_ratio_score = 15
    elif sleep_ratio >= 75:
        sleep_ratio_score = 13
    elif sleep_ratio >= 65:
        sleep_ratio_score = 10
    elif sleep_ratio >= 50:
        sleep_ratio_score = 6
    else:
        sleep_ratio_score = 2

    if interruption_rate <= 0.25:
        interruption_score = 10
    elif interruption_rate <= 0.5:
        interruption_score = 8
    elif interruption_rate <= 1.0:
        interruption_score = 5
    elif interruption_rate <= 2.0:
        interruption_score = 2
    else:
        interruption_score = 0
    continuity_score = sleep_ratio_score + interruption_score

    activity = max(0, min(int(active_ratio), 100))
    if activity <= 5:
        activity_score = 10
    elif activity <= 10:
        activity_score = 8
    elif activity <= 15:
        activity_score = 6
    elif activity <= 25:
        activity_score = 3
    else:
        activity_score = 0

    if turn_rate <= 2:
        turn_score = 10
    elif turn_rate <= 4:
        turn_score = 8
    elif turn_rate <= 6:
        turn_score = 5
    elif turn_rate <= 10:
        turn_score = 2
    else:
        turn_score = 0
    motion_score = activity_score + turn_score

    valid_breath_rates = [int(value) for value in breath_rates if int(value) > 0]
    breath_mad: float | None = None
    if valid_breath_rates:
        median_rate = statistics.median(valid_breath_rates)
        breath_mad = float(
            statistics.median(abs(value - median_rate) for value in valid_breath_rates)
        )
        if breath_mad <= 1:
            breath_score = 20
        elif breath_mad <= 2:
            breath_score = 17
        elif breath_mad <= 3:
            breath_score = 13
        elif breath_mad <= 4:
            breath_score = 8
        else:
            breath_score = 4
    else:
        breath_score = 0

    total_score = (
        duration_score
        + onset_score
        + continuity_score
        + motion_score
        + breath_score
    )
    if total_score >= 85:
        grade = "优秀"
    elif total_score >= 70:
        grade = "良好"
    elif total_score >= 60:
        grade = "一般"
    else:
        grade = "待改善"

    note = "工程试验评分，仅用于同一设备和安装条件下的趋势对比"
    if confidence < 60:
        note += "；当前数据可信度较低，分数仅供参考"
    return SleepQualityScore(
        available=True,
        total_score=total_score,
        grade=grade,
        confidence_percent=confidence,
        confidence_label=confidence_label,
        duration_score=duration_score,
        onset_score=onset_score,
        continuity_score=continuity_score,
        motion_score=motion_score,
        breath_score=breath_score,
        sleep_ratio=sleep_ratio,
        interruption_rate=round(interruption_rate, 2),
        turn_rate=round(turn_rate, 2),
        breath_mad=round(breath_mad, 1) if breath_mad is not None else None,
        note=note,
    )


def evaluate_monitoring_alerts(
    *,
    in_bed: bool,
    is_active: bool,
    in_bed_frames: int,
    out_bed_frames: int,
    had_bed_session: bool,
    breath_rate: int,
    breath_absent_frames: int,
    breath_output_ratio: int,
    heart_attempts: int,
    heart_valid_count: int,
    heart_age_seconds: float | None,
    frame_rate_hz: int = 5,
) -> tuple[str, ...]:
    alerts: list[str] = []
    frame_rate = max(int(frame_rate_hz), 1)
    in_bed_seconds = max(int(in_bed_frames), 0) / frame_rate

    if in_bed:
        if in_bed_seconds >= 180:
            if heart_valid_count == 0:
                alerts.append("心率尚无有效输出")
            elif not is_active and heart_age_seconds is not None and heart_age_seconds >= 60:
                alerts.append("心率连续60秒无新有效值")

            if (
                int(breath_rate) <= 0
                and max(int(breath_absent_frames), 0) / frame_rate >= 60
            ):
                alerts.append("呼吸信号连续60秒无效")

        if in_bed_seconds >= 300:
            if heart_attempts >= 12 and heart_valid_count > 0:
                heart_coverage = round(heart_valid_count * 100 / heart_attempts)
                if heart_coverage < 40:
                    alerts.append("心率覆盖率低于40%")
            if int(breath_output_ratio) < 50:
                alerts.append("呼吸率覆盖率低于50%")
    elif had_bed_session and max(int(out_bed_frames), 0) / frame_rate >= 30 * 60:
        alerts.append("离床已超过30分钟")

    return tuple(alerts)


def calculate_heart_session_stats(
    attempts: int, values: Iterable[int]
) -> HeartSessionStats:
    valid_values = [int(value) for value in values if value > 0]
    valid_count = len(valid_values)
    safe_attempts = max(int(attempts), valid_count, 0)
    coverage = round(valid_count * 100 / safe_attempts) if safe_attempts else 0
    return HeartSessionStats(
        attempts=safe_attempts,
        valid_count=valid_count,
        coverage_percent=coverage,
        average_bpm=statistics.mean(valid_values) if valid_values else None,
        minimum_bpm=min(valid_values) if valid_values else None,
        maximum_bpm=max(valid_values) if valid_values else None,
    )


def calculate_ten_minute_summary(
    sleep_history: Iterable[Sequence[float | int]],
    heart_history: Iterable[tuple[float, int]],
    now: float,
    *,
    window_seconds: float = 600.0,
    session_start: float | None = None,
) -> TenMinuteSummary:
    cutoff = now - window_seconds
    if session_start is not None:
        cutoff = max(cutoff, session_start)

    history_rows = sorted(
        (row for row in sleep_history if len(row) >= 6),
        key=lambda row: float(row[0]),
    )
    in_bed_rows = [
        row
        for row in history_rows
        if cutoff <= float(row[0]) <= now and int(row[2]) == 1
    ]
    breath_values = [int(row[5]) for row in in_bed_rows if int(row[5]) > 0]
    heart_cutoff = max(cutoff, float(in_bed_rows[0][0])) if in_bed_rows else now + 1.0
    heart_values = [
        int(bpm)
        for timestamp, bpm in heart_history
        if heart_cutoff <= timestamp <= now and bpm > 0
    ]
    turn_values = [int(row[6]) for row in in_bed_rows if len(row) >= 7]
    prior_turn_rows = [
        row
        for row in history_rows
        if len(row) >= 7
        and int(row[2]) == 1
        and (session_start is None or float(row[0]) >= session_start)
        and float(row[0]) < cutoff
    ]
    turn_baseline = int(prior_turn_rows[-1][6]) if prior_turn_rows else 0
    motion_levels = [int(row[7]) for row in in_bed_rows if len(row) >= 8]
    motion_count = len(motion_levels)

    return TenMinuteSummary(
        record_count=len(in_bed_rows),
        sleep_state=int(in_bed_rows[-1][4]) if in_bed_rows else None,
        average_breath_rate=statistics.mean(breath_values) if breath_values else None,
        average_heart_rate=statistics.mean(heart_values) if heart_values else None,
        turn_count=max(0, max(turn_values) - turn_baseline) if turn_values else 0,
        large_motion_percent=(
            round(sum(level >= 2 for level in motion_levels) * 100 / motion_count)
            if motion_count
            else 0
        ),
        small_motion_percent=(
            round(sum(level == 1 for level in motion_levels) * 100 / motion_count)
            if motion_count
            else 0
        ),
    )


def session_duration_notice(in_bed_frames: int, *, frame_rate_hz: int = 5) -> str:
    seconds = max(in_bed_frames, 0) / max(frame_rate_hz, 1)
    if seconds < 4 * 3600:
        return "记录不足4小时，暂不生成睡眠评分"
    if seconds > 12 * 3600:
        return "记录超过12小时，请核对离床判断"
    return "时长满足整晚分析条件"
