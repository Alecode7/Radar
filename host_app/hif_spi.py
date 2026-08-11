from __future__ import annotations

from dataclasses import dataclass
import struct
import time
from typing import Callable

try:
    from .parsers import (
        HIF_CHECKSUM_LEN,
        HIF_HEADER_LEN,
        HIF_MSG_FLAG_CHECK_BIT,
        HIF_PHY_MSG_MAGIC,
        checksum8,
        decode_hif_msg_word,
        encode_hif_msg_word,
    )
except ImportError:
    from parsers import (
        HIF_CHECKSUM_LEN,
        HIF_HEADER_LEN,
        HIF_MSG_FLAG_CHECK_BIT,
        HIF_PHY_MSG_MAGIC,
        checksum8,
        decode_hif_msg_word,
        encode_hif_msg_word,
    )


HIF_MSG_TYPE_TO_DEVICE = 1
HIF_MSG_ID_HOST_READ = 0x0C
HOST_POLL_TYPE_ACK = 1
HOST_POLL_TYPE_APP_CUBE = 2
DEFAULT_POLL_TYPE = HOST_POLL_TYPE_APP_CUBE
DEFAULT_POLL_ERR_NUM = 0
DEFAULT_POLL_BURST_NUM = 256
HIF_SPI_HEADER_PAD_LEN = 1
HIF_SPI_CHECKSUM_PAD_LEN = 1


class HifSpiTimeout(TimeoutError):
    pass


def checksum32(data: bytes) -> int:
    total = 0
    for offset in range(0, len(data), 4):
        chunk = data[offset : offset + 4]
        if len(chunk) < 4:
            chunk = chunk + b"\x00" * (4 - len(chunk))
        total = (total + int.from_bytes(chunk, "little")) & 0xFFFFFFFF
    return total


def check32(message_header: bytes, payload: bytes) -> int:
    return (~(checksum32(message_header) + checksum32(payload))) & 0xFFFFFFFF


def build_hif_command(
    msg_id: int,
    payload: bytes = b"",
    *,
    req_en: bool = False,
    seq: int = 0,
    msg_type: int = HIF_MSG_TYPE_TO_DEVICE,
    flag: int | None = None,
    include_payload_checksum: bool = True,
) -> bytes:
    if flag is None:
        flag = 1 if req_en else 0
    if payload and include_payload_checksum:
        flag |= HIF_MSG_FLAG_CHECK_BIT

    msg_word = encode_hif_msg_word(
        msg_id,
        len(payload),
        msg_type=msg_type,
        flag=flag,
        seq=seq,
    )
    msg_header = struct.pack("<I", msg_word)
    phy_checksum = (~checksum8(bytes([HIF_PHY_MSG_MAGIC]) + msg_header)) & 0xFF
    frame = bytes([HIF_PHY_MSG_MAGIC, phy_checksum]) + msg_header + payload
    if payload and include_payload_checksum:
        frame += struct.pack("<I", check32(msg_header, payload))
    return frame


def build_host_read_poll_frame(
    *,
    poll_type: int = DEFAULT_POLL_TYPE,
    err_num: int = DEFAULT_POLL_ERR_NUM,
    burst_num: int = DEFAULT_POLL_BURST_NUM,
    seq: int = 0,
) -> bytes:
    payload = struct.pack("<BBH", poll_type & 0xFF, err_num & 0xFF, burst_num & 0xFFFF)
    return build_hif_command(HIF_MSG_ID_HOST_READ, payload, req_en=True, seq=seq)


def read_direct_stream_frame(
    transfer: Callable[[bytes], bytes],
    *,
    timeout_ms: int = 80,
    scan_chunk_size: int = 0,
    strip_spi_padding: bool = True,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> bytes:
    deadline = monotonic() + timeout_ms / 1000.0

    while monotonic() <= deadline:
        header_bytes = transfer(b"\x00" * HIF_HEADER_LEN)
        if len(header_bytes) < HIF_HEADER_LEN:
            sleep(0.001)
            continue
        if header_bytes[0] != HIF_PHY_MSG_MAGIC or checksum8(header_bytes) != 0xFF:
            if scan_chunk_size > 0:
                scan_bytes = transfer(b"\x00" * scan_chunk_size)
                frame = _try_extract_frame_from_buffer(
                    header_bytes + scan_bytes,
                    transfer,
                    strip_spi_padding=strip_spi_padding,
                )
                if frame is not None:
                    return frame
            sleep(0.001)
            continue

        header = decode_hif_msg_word(int.from_bytes(header_bytes[2:HIF_HEADER_LEN], "little"))
        tail_len = header.length
        if header.flag & HIF_MSG_FLAG_CHECK_BIT:
            tail_len += HIF_CHECKSUM_LEN
        if tail_len <= 0:
            return header_bytes

        tail = transfer(b"\x00" * tail_len)
        if len(tail) < tail_len:
            sleep(0.001)
            continue
        return header_bytes + tail[:tail_len]

    raise HifSpiTimeout(f"direct SPI HIF frame timeout after {timeout_ms} ms")


def _physical_tail_len(payload_len: int, has_checksum: bool, strip_spi_padding: bool) -> int:
    length = payload_len + (HIF_CHECKSUM_LEN if has_checksum else 0)
    if strip_spi_padding:
        length += HIF_SPI_HEADER_PAD_LEN
        if has_checksum:
            length += HIF_SPI_CHECKSUM_PAD_LEN
    return length


def _physical_frame_len(payload_len: int, has_checksum: bool, strip_spi_padding: bool) -> int:
    return HIF_HEADER_LEN + _physical_tail_len(payload_len, has_checksum, strip_spi_padding)


def _canonical_frame_from_parts(
    header_bytes: bytes,
    tail: bytes,
    *,
    payload_len: int,
    has_checksum: bool,
    strip_spi_padding: bool,
) -> bytes:
    if not strip_spi_padding:
        return header_bytes + tail

    payload_start = HIF_SPI_HEADER_PAD_LEN
    payload_end = payload_start + payload_len
    checksum_end = payload_end + (HIF_CHECKSUM_LEN if has_checksum else 0)
    return header_bytes + tail[payload_start:checksum_end]


def _try_extract_frame_from_buffer(
    buffer: bytes,
    transfer: Callable[[bytes], bytes],
    *,
    strip_spi_padding: bool,
) -> bytes | None:
    offset = 0
    while offset <= len(buffer) - HIF_HEADER_LEN:
        magic_index = buffer.find(bytes([HIF_PHY_MSG_MAGIC]), offset)
        if magic_index < 0:
            return None

        header_bytes = buffer[magic_index : magic_index + HIF_HEADER_LEN]
        if len(header_bytes) < HIF_HEADER_LEN:
            return None
        if checksum8(header_bytes) != 0xFF:
            offset = magic_index + 1
            continue

        header = decode_hif_msg_word(int.from_bytes(header_bytes[2:HIF_HEADER_LEN], "little"))
        has_checksum = bool(header.flag & HIF_MSG_FLAG_CHECK_BIT)
        frame_len = _physical_frame_len(header.length, has_checksum, strip_spi_padding)

        available = len(buffer) - magic_index
        if available >= frame_len:
            tail = buffer[magic_index + HIF_HEADER_LEN : magic_index + frame_len]
            return _canonical_frame_from_parts(
                header_bytes,
                tail,
                payload_len=header.length,
                has_checksum=has_checksum,
                strip_spi_padding=strip_spi_padding,
            )

        missing = frame_len - available
        tail = transfer(b"\x00" * missing)
        if len(tail) < missing:
            return None
        physical_frame = buffer[magic_index:] + tail[:missing]
        physical_tail = physical_frame[HIF_HEADER_LEN:]
        return _canonical_frame_from_parts(
            header_bytes,
            physical_tail,
            payload_len=header.length,
            has_checksum=has_checksum,
            strip_spi_padding=strip_spi_padding,
        )

    return None


def read_poll_burst_stream_frame(
    transfer: Callable[[bytes], bytes],
    *,
    irq_is_high: Callable[[], bool] | None = None,
    poll_frame: bytes | None = None,
    timeout_ms: int = 80,
    scan_chunk_size: int = 0,
    strip_spi_padding: bool = True,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> bytes:
    deadline = monotonic() + timeout_ms / 1000.0
    transfer(poll_frame if poll_frame is not None else build_host_read_poll_frame())

    if irq_is_high is not None:
        while monotonic() <= deadline:
            if irq_is_high():
                break
            sleep(0.001)
        else:
            raise HifSpiTimeout(f"poll-burst SPI IRQ timeout after {timeout_ms} ms")

    remaining_ms = max(1, int((deadline - monotonic()) * 1000))
    return read_direct_stream_frame(
        transfer,
        timeout_ms=remaining_ms,
        scan_chunk_size=scan_chunk_size,
        strip_spi_padding=strip_spi_padding,
        monotonic=monotonic,
        sleep=sleep,
    )


@dataclass
class HifSpiFrameReader:
    read: Callable[[int], bytes]
    chunk_size: int = 64
    strip_spi_padding: bool = True
    monotonic: Callable[[], float] = time.monotonic
    sleep: Callable[[float], None] = time.sleep

    def read_frame(self, timeout_ms: int = 80) -> bytes:
        deadline = self.monotonic() + timeout_ms / 1000.0
        buffer = bytearray()

        while self.monotonic() <= deadline:
            frame = self._try_extract_frame(buffer)
            if frame is not None:
                return frame

            buffer.extend(self.read(max(1, self.chunk_size)))
            if len(buffer) > 8192:
                del buffer[:-HIF_HEADER_LEN]
            self.sleep(0.001)

        raise HifSpiTimeout(f"SPI HIF frame timeout after {timeout_ms} ms")

    def _try_extract_frame(self, buffer: bytearray) -> bytes | None:
        while True:
            magic_index = buffer.find(bytes([HIF_PHY_MSG_MAGIC]))
            if magic_index < 0:
                if len(buffer) > HIF_HEADER_LEN:
                    del buffer[:-HIF_HEADER_LEN]
                return None
            if magic_index:
                del buffer[:magic_index]

            if len(buffer) < HIF_HEADER_LEN:
                return None

            header_bytes = bytes(buffer[:HIF_HEADER_LEN])
            if checksum8(header_bytes) != 0xFF:
                del buffer[0]
                continue

            header = decode_hif_msg_word(int.from_bytes(buffer[2:HIF_HEADER_LEN], "little"))
            has_checksum = bool(header.flag & HIF_MSG_FLAG_CHECK_BIT)
            frame_len = _physical_frame_len(header.length, has_checksum, self.strip_spi_padding)
            if len(buffer) < frame_len:
                return None

            physical_frame = bytes(buffer[:frame_len])
            del buffer[:frame_len]
            return _canonical_frame_from_parts(
                header_bytes,
                physical_frame[HIF_HEADER_LEN:],
                payload_len=header.length,
                has_checksum=has_checksum,
                strip_spi_padding=self.strip_spi_padding,
            )


class HifSpiSession:
    def __init__(
        self,
        transfer: Callable[[bytes], bytes],
        *,
        read: Callable[[int], bytes] | None = None,
        monotonic: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], None] = time.sleep,
    ):
        self.transfer = transfer
        self.reader = HifSpiFrameReader(
            read=read if read is not None else self._read,
            monotonic=monotonic,
            sleep=sleep,
        )
        self._seq = 0

    def send_host_read_poll(self) -> None:
        frame = build_host_read_poll_frame(seq=self._seq)
        self._seq = (self._seq + 1) & 0x7
        self.transfer(frame)

    def poll_frame(self, timeout_ms: int = 80) -> bytes:
        self.send_host_read_poll()
        return self.reader.read_frame(timeout_ms=timeout_ms)

    def _read(self, size: int) -> bytes:
        return self.transfer(b"\xff" * size)
