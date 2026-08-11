from __future__ import annotations

from dataclasses import dataclass

try:
    from .parsers import (
        HIF_HEADER_LEN,
        HIF_CHECKSUM_LEN,
        HIF_MSG_ID_OBJECTS,
        HIF_MSG_FLAG_CHECK_BIT,
        HIF_PHY_MSG_MAGIC,
        PointCloudPacket,
        checksum8,
        decode_hif_msg_word,
        parse_hif_stream_with_consumed,
    )
except ImportError:
    from parsers import (
        HIF_HEADER_LEN,
        HIF_CHECKSUM_LEN,
        HIF_MSG_ID_OBJECTS,
        HIF_MSG_FLAG_CHECK_BIT,
        HIF_PHY_MSG_MAGIC,
        PointCloudPacket,
        checksum8,
        decode_hif_msg_word,
        parse_hif_stream_with_consumed,
    )


@dataclass
class StreamDiagnostics:
    total_bytes: int = 0
    total_magic_bytes: int = 0
    checksum_failures: int = 0
    parsed_packets: int = 0
    parsed_points: int = 0
    ignored_packets: int = 0
    hif_buffer_len: int = 0
    pending_frame_bytes: int = 0
    pending_frame_expected: int = 0
    last_msg_id: int | None = None
    last_payload_len: int | None = None
    last_chunk_hex: str = ""


class SerialStreamDecoder:
    def __init__(self):
        self._text_buffer = bytearray()
        self._hif_buffer = bytearray()
        self.diagnostics = StreamDiagnostics()

    def feed(self, data: bytes) -> tuple[list[str], list[PointCloudPacket]]:
        self._record_raw_data(data)
        lines = self._feed_text(data)
        packets = self._feed_hif(data)
        return lines, packets

    def _record_raw_data(self, data: bytes) -> None:
        if not data:
            return
        self.diagnostics.total_bytes += len(data)
        self.diagnostics.total_magic_bytes += data.count(HIF_PHY_MSG_MAGIC)
        self.diagnostics.last_chunk_hex = data[:48].hex(" ")

    def _feed_text(self, data: bytes) -> list[str]:
        lines: list[str] = []
        for byte in data:
            if byte in (10, 13):
                if self._text_buffer:
                    line = self._text_buffer.decode("utf-8", errors="replace").strip()
                    self._text_buffer.clear()
                    if line:
                        lines.append(line)
                continue
            if 32 <= byte <= 126 or byte >= 128:
                self._text_buffer.append(byte)
            elif self._text_buffer:
                self._text_buffer.append(ord(" "))

        if len(self._text_buffer) > 4096:
            self._text_buffer.clear()
        return lines

    def _feed_hif(self, data: bytes) -> list[PointCloudPacket]:
        self._hif_buffer.extend(data)
        buffer_before_parse = bytes(self._hif_buffer)
        packets, consumed = parse_hif_stream_with_consumed(buffer_before_parse)
        if consumed:
            self._inspect_consumed_hif(buffer_before_parse, consumed)
        if consumed:
            del self._hif_buffer[:consumed]
        if len(self._hif_buffer) > 8192:
            self._hif_buffer = self._hif_buffer[-2048:]
        if packets:
            self.diagnostics.parsed_packets += len(packets)
            self.diagnostics.parsed_points += sum(len(packet.points) for packet in packets)
        self._update_pending_hif_state()
        return packets

    def _inspect_consumed_hif(self, data: bytes, consumed: int) -> None:
        offset = 0
        while offset < consumed and offset <= len(data) - HIF_HEADER_LEN:
            if data[offset] != HIF_PHY_MSG_MAGIC:
                offset += 1
                continue

            header_bytes = data[offset : offset + HIF_HEADER_LEN]
            if checksum8(header_bytes) != 0xFF:
                self.diagnostics.checksum_failures += 1
                offset += 1
                continue

            header = decode_hif_msg_word(int.from_bytes(data[offset + 2 : offset + HIF_HEADER_LEN], "little"))
            payload_start = offset + HIF_HEADER_LEN
            payload_end = payload_start + header.length
            frame_end = payload_end + (HIF_CHECKSUM_LEN if header.flag & HIF_MSG_FLAG_CHECK_BIT else 0)
            if frame_end > len(data):
                return

            self.diagnostics.last_msg_id = header.msg_id
            self.diagnostics.last_payload_len = header.length
            if header.msg_id != HIF_MSG_ID_OBJECTS:
                self.diagnostics.ignored_packets += 1
            offset = frame_end

    def _update_pending_hif_state(self) -> None:
        self.diagnostics.hif_buffer_len = len(self._hif_buffer)
        self.diagnostics.pending_frame_bytes = 0
        self.diagnostics.pending_frame_expected = 0

        magic_index = self._hif_buffer.find(bytes([HIF_PHY_MSG_MAGIC]))
        if magic_index < 0:
            return

        available = len(self._hif_buffer) - magic_index
        if available < HIF_HEADER_LEN:
            self.diagnostics.pending_frame_bytes = available
            self.diagnostics.pending_frame_expected = HIF_HEADER_LEN
            return

        header_bytes = self._hif_buffer[magic_index : magic_index + HIF_HEADER_LEN]
        if checksum8(header_bytes) != 0xFF:
            return

        header = decode_hif_msg_word(int.from_bytes(self._hif_buffer[magic_index + 2 : magic_index + HIF_HEADER_LEN], "little"))
        expected = HIF_HEADER_LEN + header.length + (HIF_CHECKSUM_LEN if header.flag & HIF_MSG_FLAG_CHECK_BIT else 0)
        if available < expected:
            self.diagnostics.pending_frame_bytes = available
            self.diagnostics.pending_frame_expected = expected
