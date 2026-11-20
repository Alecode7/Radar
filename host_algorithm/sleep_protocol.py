from __future__ import annotations

from dataclasses import dataclass, field
import re


_FIELD_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)=(-?\d+)\b")


@dataclass(frozen=True)
class SleepStatus:
    has_person: bool
    is_active: bool
    in_bed: bool
    breath_valid: bool
    breath_rate: int
    sleep_state: int = 0
    motion_intensity: int = 0
    turn_event: bool = False
    turn_count: int = 0
    sleep_turn_count: int = 0
    in_bed_frames: int = 0
    out_bed_frames: int = 0
    breath_valid_ratio: int = 0
    breath_rate_output_ratio: int = 0
    breath_target_bin: int = 0
    breath_phase_mrad: int = 0
    breath_phase_delta_mrad: int = 0
    raw: dict[str, int] = field(default_factory=dict)


def parse_sleep_line(line: str) -> SleepStatus | None:
    """Parse the stable KEY=VALUE subset of a firmware SLEEP line."""
    if "SLEEP " not in line:
        return None

    fields = {key: int(value) for key, value in _FIELD_RE.findall(line)}
    if not {"P", "A", "BED", "BV", "BR"}.issubset(fields):
        return None

    return SleepStatus(
        has_person=bool(fields["P"]),
        is_active=bool(fields["A"]),
        in_bed=bool(fields["BED"]),
        breath_valid=bool(fields["BV"]),
        breath_rate=fields["BR"],
        sleep_state=fields.get("SS", 0),
        motion_intensity=fields.get("MI", 0),
        turn_event=bool(fields.get("TE", 0)),
        turn_count=fields.get("TC", 0),
        sleep_turn_count=fields.get("STC", 0),
        in_bed_frames=fields.get("BT", 0),
        out_bed_frames=fields.get("OT", 0),
        breath_valid_ratio=fields.get("BVR", 0),
        breath_rate_output_ratio=fields.get("RVR", 0),
        breath_target_bin=fields.get("Bbin", 0),
        breath_phase_mrad=fields.get("Bph", 0),
        breath_phase_delta_mrad=fields.get("Bpd", 0),
        raw=fields,
    )
