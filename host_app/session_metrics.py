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


def calculate_motion_score(motion_intensity: int, active_motion_points: int) -> int:
    """Map the firmware's four motion levels to an engineering 0-100 index."""
    level = max(0, min(int(motion_intensity), 3))
    if level == 0:
        return 0
    base_score = (0, 25, 55, 80)[level]
    point_bonus = min(max(int(active_motion_points), 0) * 2, 20)
    return min(100, base_score + point_bonus)


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
