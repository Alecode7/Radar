from __future__ import annotations

from dataclasses import dataclass, field
import re
import struct
from typing import Iterable


HIF_PHY_MSG_MAGIC = 0xA5
HIF_MSG_TYPE_TO_HOST = 2
HIF_MSG_FLAG_CHECK_BIT = 1 << 2
HIF_MSG_ID_OBJECTS = 0xC3

TL_TYPE_POINTS = 4
TL_FLAG_CART = 1 << 0
TL_FLAG_MICRO = 1 << 1

HIF_HEADER_LEN = 6
HIF_CHECKSUM_LEN = 4
FRAME_UPLOAD_STRUCT = struct.Struct("<III")
TL_HEADER_STRUCT = struct.Struct("<BBH")
POINT_CART_STRUCT = struct.Struct("<hhhhhh")
POINT_POLAR_STRUCT = struct.Struct("<hhhhhh")

_SLEEP_FIELD_RE = re.compile(r"\b([A-Za-z][A-Za-z0-9]*)=(-?\d+)\b")


@dataclass(frozen=True)
class SleepStatus:
    has_person: bool
    is_active: bool
    in_bed: bool
    breath_valid: bool
    breath_rate: int
    sleep_state: int = 0
    sleep_quiet_streak: int = 0
    sleep_active_streak: int = 0
    sleep_breath_streak: int = 0
    motion_intensity: int = 0
    turn_event: bool = False
    turn_count: int = 0
    sleep_turn_count: int = 0
    in_bed_frames: int = 0
    out_bed_frames: int = 0
    bed_enter_count: int = 0
    bed_exit_count: int = 0
    breath_valid_ratio: int = 0
    breath_rate_avg: int = 0
    breath_state: int = 0
    active_frames: int = 0
    quiet_frames: int = 0
    likely_sleep_frames: int = 0
    longest_quiet_frames: int = 0
    sleep_onset_frames: int = 0
    sleep_segment_count: int = 0
    longest_likely_sleep_frames: int = 0
    sleep_interruption_count: int = 0
    active_ratio: int = 0
    quiet_ratio: int = 0
    likely_sleep_ratio: int = 0
    breath_rate_output_ratio: int = 0
    session_end: bool = False
    breath_rate_min: int = 0
    breath_rate_max: int = 0
    motion_points_total: int = 0
    motion_points_bed: int = 0
    micro_points_total: int = 0
    micro_points_bed: int = 0
    active_motion_points: int = 0
    person_present_streak: int = 0
    person_absent_streak: int = 0
    bed_present_streak: int = 0
    bed_absent_streak: int = 0
    breath_present_streak: int = 0
    breath_absent_streak: int = 0
    breath_signal: int = 0
    breath_quality: int = 0
    breath_period_frames: int = 0
    breath_rate_candidate: int = 0
    breath_rate_repeat_count: int = 0
    breath_rate_stable: int = 0
    breath_target_bin: int = 0
    breath_phase_mrad: int = 0
    breath_phase_delta_mrad: int = 0
    breath_phase_amp: int = 0
    breath_phase_quality: int = 0
    raw: dict[str, int] = field(default_factory=dict)


@dataclass(frozen=True)
class HifHeader:
    msg_type: int
    flag: int
    msg_id: int
    length: int
    seq: int
    frag: int


@dataclass(frozen=True)
class Point:
    kind: str
    x_cm: int | None = None
    y_cm: int | None = None
    z_cm: int | None = None
    range_cm: int | None = None
    azimuth_deg: float | None = None
    elevation_deg: float | None = None
    velocity_cm_s: int = 0
    snr_db: float = 0.0
    track_uid: int = 0


@dataclass(frozen=True)
class PointCloudPacket:
    header: HifHeader
    frame_index: int
    frame_len_words: int
    offset_words: int
    points: list[Point]


def parse_sleep_line(line: str) -> SleepStatus | None:
    if "SLEEP " not in line:
        return None

    fields = {key: int(value) for key, value in _SLEEP_FIELD_RE.findall(line)}
    if not {"P", "A", "BED", "BV", "BR"}.issubset(fields):
        return None

    return SleepStatus(
        has_person=bool(fields["P"]),
        is_active=bool(fields["A"]),
        in_bed=bool(fields["BED"]),
        breath_valid=bool(fields["BV"]),
        breath_rate=fields["BR"],
        sleep_state=fields.get("SS", 0),
        sleep_quiet_streak=fields.get("SQ", 0),
        sleep_active_streak=fields.get("SA", 0),
        sleep_breath_streak=fields.get("SB", 0),
        motion_intensity=fields.get("MI", 0),
        turn_event=bool(fields.get("TE", 0)),
        turn_count=fields.get("TC", 0),
        sleep_turn_count=fields.get("STC", 0),
        in_bed_frames=fields.get("BT", 0),
        out_bed_frames=fields.get("OT", 0),
        bed_enter_count=fields.get("BE", 0),
        bed_exit_count=fields.get("BX", 0),
        breath_valid_ratio=fields.get("BVR", 0),
        breath_rate_avg=fields.get("ABR", 0),
        breath_state=fields.get("BS", 0),
        active_frames=fields.get("AT", 0),
        quiet_frames=fields.get("QT", 0),
        likely_sleep_frames=fields.get("ST", 0),
        longest_quiet_frames=fields.get("LQ", 0),
        sleep_onset_frames=fields.get("SL", 0),
        sleep_segment_count=fields.get("SC", 0),
        longest_likely_sleep_frames=fields.get("LS", 0),
        sleep_interruption_count=fields.get("SI", 0),
        active_ratio=fields.get("AR", 0),
        quiet_ratio=fields.get("QR", 0),
        likely_sleep_ratio=fields.get("SR", 0),
        breath_rate_output_ratio=fields.get("RVR", 0),
        session_end=bool(fields.get("SE", 0)),
        breath_rate_min=fields.get("BRmin", 0),
        breath_rate_max=fields.get("BRmax", 0),
        motion_points_total=fields.get("MT", fields.get("MP", 0)),
        motion_points_bed=fields.get("MB", 0),
        micro_points_total=fields.get("McT", fields.get("MCP", 0)),
        micro_points_bed=fields.get("McB", 0),
        active_motion_points=fields.get("Act", 0),
        person_present_streak=fields.get("PP", 0),
        person_absent_streak=fields.get("PA", 0),
        bed_present_streak=fields.get("BP", 0),
        bed_absent_streak=fields.get("BA", 0),
        breath_present_streak=fields.get("BrP", 0),
        breath_absent_streak=fields.get("BrA", 0),
        breath_signal=fields.get("Bsig", 0),
        breath_quality=fields.get("Bq", 0),
        breath_period_frames=fields.get("Bper", 0),
        breath_rate_candidate=fields.get("Brc", 0),
        breath_rate_repeat_count=fields.get("Brr", 0),
        breath_rate_stable=fields.get("Brs", 0),
        breath_target_bin=fields.get("Bbin", 0),
        breath_phase_mrad=fields.get("Bph", 0),
        breath_phase_delta_mrad=fields.get("Bpd", 0),
        breath_phase_amp=fields.get("Bamp", 0),
        breath_phase_quality=fields.get("Bpq", 0),
        raw=fields,
    )


def encode_hif_msg_word(
    msg_id: int,
    payload_len: int,
    msg_type: int = HIF_MSG_TYPE_TO_HOST,
    flag: int = HIF_MSG_FLAG_CHECK_BIT,
    seq: int = 0,
    frag: int = 0,
) -> int:
    return (
        (msg_type & 0x3)
        | ((flag & 0x3F) << 2)
        | ((msg_id & 0xFF) << 8)
        | ((payload_len & 0xFFF) << 16)
        | ((seq & 0x7) << 28)
        | ((frag & 0x1) << 31)
    )


def decode_hif_msg_word(word: int) -> HifHeader:
    return HifHeader(
        msg_type=word & 0x3,
        flag=(word >> 2) & 0x3F,
        msg_id=(word >> 8) & 0xFF,
        length=(word >> 16) & 0xFFF,
        seq=(word >> 28) & 0x7,
        frag=(word >> 31) & 0x1,
    )


def checksum8(data: Iterable[int]) -> int:
    return sum(data) & 0xFF


def build_hif_header(msg_id: int, payload: bytes) -> bytes:
    word = encode_hif_msg_word(msg_id, len(payload))
    msg_header = struct.pack("<I", word)
    checksum = (~checksum8(bytes([HIF_PHY_MSG_MAGIC, 0]) + msg_header)) & 0xFF
    return bytes([HIF_PHY_MSG_MAGIC, checksum]) + msg_header


def parse_hif_stream(data: bytes) -> list[PointCloudPacket]:
    packets, _consumed = parse_hif_stream_with_consumed(data)
    return packets


def parse_hif_stream_with_consumed(data: bytes) -> tuple[list[PointCloudPacket], int]:
    packets: list[PointCloudPacket] = []
    offset = 0
    consumed = 0
    while offset <= len(data) - HIF_HEADER_LEN:
        if data[offset] != HIF_PHY_MSG_MAGIC:
            offset += 1
            consumed = offset
            continue

        header_bytes = data[offset : offset + HIF_HEADER_LEN]
        if checksum8(header_bytes) != 0xFF:
            offset += 1
            consumed = offset
            continue

        msg_word = struct.unpack_from("<I", data, offset + 2)[0]
        header = decode_hif_msg_word(msg_word)
        payload_start = offset + HIF_HEADER_LEN
        payload_end = payload_start + header.length
        frame_end = payload_end + (HIF_CHECKSUM_LEN if header.flag & HIF_MSG_FLAG_CHECK_BIT else 0)
        if frame_end > len(data):
            break

        payload = data[payload_start:payload_end]
        if header.msg_id == HIF_MSG_ID_OBJECTS:
            packet = parse_pointcloud_payload(header, payload)
            if packet is not None:
                packets.append(packet)

        offset = frame_end
        consumed = offset
    return packets, consumed


def parse_pointcloud_payload(header: HifHeader, payload: bytes) -> PointCloudPacket | None:
    if len(payload) < FRAME_UPLOAD_STRUCT.size:
        return None

    frame_index, frame_len_words, offset_words = FRAME_UPLOAD_STRUCT.unpack_from(payload, 0)
    cursor = FRAME_UPLOAD_STRUCT.size
    points: list[Point] = []

    while cursor + TL_HEADER_STRUCT.size <= len(payload):
        tl_type, tl_flag, tl_len = TL_HEADER_STRUCT.unpack_from(payload, cursor)
        cursor += TL_HEADER_STRUCT.size
        tl_payload = payload[cursor : cursor + tl_len]
        if len(tl_payload) < tl_len:
            break

        if tl_type == TL_TYPE_POINTS:
            points.extend(parse_points_tl(tl_flag, tl_payload))
        cursor += tl_len

    return PointCloudPacket(
        header=header,
        frame_index=frame_index,
        frame_len_words=frame_len_words,
        offset_words=offset_words,
        points=points,
    )


def parse_points_tl(tl_flag: int, payload: bytes) -> list[Point]:
    is_micro = bool(tl_flag & TL_FLAG_MICRO)
    is_cart = bool(tl_flag & TL_FLAG_CART)
    kind = "micro" if is_micro else "motion"
    point_struct = POINT_CART_STRUCT if is_cart else POINT_POLAR_STRUCT
    points: list[Point] = []

    for offset in range(0, len(payload) - point_struct.size + 1, point_struct.size):
        values = point_struct.unpack_from(payload, offset)
        if is_cart:
            x, y, z, velocity, snr, track_uid = values
            points.append(
                Point(
                    kind=kind,
                    x_cm=x,
                    y_cm=y,
                    z_cm=z,
                    velocity_cm_s=velocity,
                    snr_db=snr / 100.0,
                    track_uid=track_uid,
                )
            )
        else:
            range_cm, azim, elev, velocity, snr, track_uid = values
            points.append(
                Point(
                    kind=kind,
                    range_cm=range_cm,
                    azimuth_deg=azim / 100.0,
                    elevation_deg=elev / 100.0,
                    velocity_cm_s=velocity,
                    snr_db=snr / 100.0,
                    track_uid=track_uid,
                )
            )

    return points
