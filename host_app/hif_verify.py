from __future__ import annotations

import argparse
from dataclasses import dataclass
import time
from typing import Callable

try:
    from .ch347_spi import Ch347Spi, Ch347SpiConfig
    from .hif_spi import (
        HifSpiTimeout,
        build_hif_command,
        build_host_read_poll_frame,
        read_direct_stream_frame,
        read_poll_burst_stream_frame,
    )
    from .parsers import HIF_MSG_ID_OBJECTS, decode_hif_msg_word, parse_hif_stream
except ImportError:
    from ch347_spi import Ch347Spi, Ch347SpiConfig
    from hif_spi import (
        HifSpiTimeout,
        build_hif_command,
        build_host_read_poll_frame,
        read_direct_stream_frame,
        read_poll_burst_stream_frame,
    )
    from parsers import HIF_MSG_ID_OBJECTS, decode_hif_msg_word, parse_hif_stream


HIF_MSG_ID_VERSION = 0x00
HIF_MSG_ID_HIF_MODE_CFG = 0x0A
HIF_MSG_ID_HOST_STATE = 0x0B

HIF_SUBID_SPI_IO_DELAY_GET = 0x05
HIF_SUBID_SPI_POLL_DELAY_GET = 0x06
HIF_HOST_STATE_ACTIVE = 0x00


@dataclass(frozen=True)
class SafeHifCommand:
    msg_id: int
    payload: bytes
    description: str
    msg_type: int = 1
    flag: int | None = None
    include_payload_checksum: bool = True


SAFE_HIF_COMMANDS: dict[str, SafeHifCommand] = {
    "version": SafeHifCommand(HIF_MSG_ID_VERSION, b"\x00", "read HIF version"),
    "spi-io-delay": SafeHifCommand(HIF_MSG_ID_HIF_MODE_CFG, b"\x05", "read SPI RX/TX delay limits"),
    "spi-poll-delay": SafeHifCommand(HIF_MSG_ID_HIF_MODE_CFG, b"\x06", "read SPI poll delay defaults"),
    "host-active": SafeHifCommand(HIF_MSG_ID_HOST_STATE, b"\x00", "mark host active"),
    "dev-sync": SafeHifCommand(
        0x01,
        b"\x00\x01",
        "official RadarAnalysisTool device sync",
        msg_type=0,
        flag=1,
        include_payload_checksum=False,
    ),
    "hif-switch": SafeHifCommand(
        0x01,
        b"\x00\x00",
        "official RadarAnalysisTool HIF switch mode",
        msg_type=1,
        flag=0,
        include_payload_checksum=False,
    ),
}


@dataclass
class HifVerificationIo:
    spi: Ch347Spi
    tx_bytes: int = 0
    rx_bytes: int = 0
    rx_magic_bytes: int = 0
    all_ff_reads: int = 0
    all_zero_reads: int = 0
    last_tx_hex: str = ""
    last_rx_hex: str = ""
    gpio_reads: int = 0
    last_gpio_dir: int | None = None
    last_gpio_data: int | None = None
    last_irq_bit: int | None = None

    def transfer(self, data: bytes) -> bytes:
        received = self.spi.transfer(data)
        self._record_tx(data)
        self._record_rx(received)
        return received

    def gpio_is_high(self, irq_bit: int) -> bool:
        direction, data = self.spi.gpio_get()
        self.gpio_reads += 1
        self.last_gpio_dir = direction
        self.last_gpio_data = data
        self.last_irq_bit = irq_bit
        return bool(data & (1 << irq_bit))

    def format_diagnostics(self) -> str:
        gpio = ""
        if self.last_gpio_dir is not None and self.last_gpio_data is not None and self.last_irq_bit is not None:
            level = 1 if self.last_gpio_data & (1 << self.last_irq_bit) else 0
            gpio = (
                f" gpio=dir:0x{self.last_gpio_dir:02x}"
                f" data:0x{self.last_gpio_data:02x} bit{self.last_irq_bit}={level}"
            )
        return (
            f"tx={self.tx_bytes} rx={self.rx_bytes} A5={self.rx_magic_bytes}"
            f" FF={self.all_ff_reads} 00={self.all_zero_reads}"
            f" last_tx={self.last_tx_hex or '--'} last_rx={self.last_rx_hex or '--'}{gpio}"
        )

    def _record_tx(self, data: bytes) -> None:
        self.tx_bytes += len(data)
        self.last_tx_hex = data[:64].hex(" ")

    def _record_rx(self, data: bytes) -> None:
        self.rx_bytes += len(data)
        self.rx_magic_bytes += data.count(0xA5)
        if data and all(byte == 0xFF for byte in data):
            self.all_ff_reads += 1
        if data and all(byte == 0x00 for byte in data):
            self.all_zero_reads += 1
        self.last_rx_hex = data[:64].hex(" ")


def build_safe_hif_command(name: str, *, seq: int = 0) -> bytes:
    command = SAFE_HIF_COMMANDS[name]
    return build_hif_command(
        command.msg_id,
        command.payload,
        req_en=True,
        seq=seq,
        msg_type=command.msg_type,
        flag=command.flag,
        include_payload_checksum=command.include_payload_checksum,
    )


def run_hif_transaction(
    io: HifVerificationIo,
    command_frame: bytes,
    *,
    wait_irq: bool,
    irq_bit: int = 6,
    timeout_ms: int = 100,
    scan_chunk_size: int = 0,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> bytes:
    deadline = monotonic() + timeout_ms / 1000.0
    io.transfer(command_frame)

    if wait_irq:
        while monotonic() <= deadline:
            if io.gpio_is_high(irq_bit):
                break
            sleep(0.001)
        else:
            raise HifSpiTimeout(f"HIF command IRQ timeout after {timeout_ms} ms")

    remaining_ms = max(1, int((deadline - monotonic()) * 1000))
    return read_direct_stream_frame(
        io.transfer,
        timeout_ms=remaining_ms,
        scan_chunk_size=scan_chunk_size,
        monotonic=monotonic,
        sleep=sleep,
    )


def summarize_frame(frame: bytes) -> str:
    if len(frame) < 6:
        return f"short len={len(frame)}"
    header = decode_hif_msg_word(int.from_bytes(frame[2:6], "little"))
    parts = [
        f"msg=0x{header.msg_id:02X}",
        f"type={header.msg_type}",
        f"flag=0x{header.flag:02X}",
        f"payload={header.length}",
    ]
    payload = frame[6 : 6 + header.length]
    if payload and header.msg_id != HIF_MSG_ID_OBJECTS:
        parts.append(f"status={payload[0]}")
    if header.msg_id == HIF_MSG_ID_VERSION and len(payload) >= 3:
        parts.append(f"hif_proto={payload[1]}")
        parts.append(f"hif_handle={payload[2]}")
    elif header.msg_id == HIF_MSG_ID_HIF_MODE_CFG and len(payload) >= 5:
        subid = payload[1] if payload[0] == 0 else payload[0]
        parts.append(f"subid=0x{subid:02X}")
        if len(payload) >= 6:
            value = int.from_bytes(payload[2:6], "little")
            parts.append(f"value0={value}")
    elif header.msg_id == HIF_MSG_ID_OBJECTS:
        packets = parse_hif_stream(frame)
        if packets:
            packet = packets[-1]
            parts.append(f"frame={packet.frame_index}")
            parts.append(f"points={len(packet.points)}")
        else:
            parts.append("points=unparsed")
    return " ".join(parts)


def run_named_command(
    io: HifVerificationIo,
    name: str,
    *,
    seq: int,
    wait_irq: bool,
    irq_bit: int,
    timeout_ms: int,
    scan_chunk_size: int = 0,
) -> bytes:
    frame = build_safe_hif_command(name, seq=seq)
    return run_hif_transaction(
        io,
        frame,
        wait_irq=wait_irq,
        irq_bit=irq_bit,
        timeout_ms=timeout_ms,
        scan_chunk_size=scan_chunk_size,
    )


def run_point_poll(
    io: HifVerificationIo,
    *,
    seq: int,
    wait_irq: bool,
    irq_bit: int,
    timeout_ms: int,
    scan_chunk_size: int = 0,
) -> bytes:
    poll_frame = build_host_read_poll_frame(seq=seq)
    return read_poll_burst_stream_frame(
        io.transfer,
        irq_is_high=(lambda: io.gpio_is_high(irq_bit)) if wait_irq else None,
        poll_frame=poll_frame,
        timeout_ms=timeout_ms,
        scan_chunk_size=scan_chunk_size,
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verify MRS6130 HIF over CH347 SPI without the GUI.")
    parser.add_argument("--index", type=lambda value: int(value, 0), default=0, help="CH347 device index, default: 0")
    parser.add_argument("--chip-select", type=lambda value: int(value, 0), default=0x80, help="CH347 chip select mask")
    parser.add_argument("--clock-code", type=lambda value: int(value, 0), default=3, help="CH347 SPI clock code")
    parser.add_argument("--mode", type=lambda value: int(value, 0), default=0, help="CH347 SPI mode")
    parser.add_argument("--byte-order", type=lambda value: int(value, 0), default=1, help="CH347 byte order")
    parser.add_argument("--irq-bit", type=int, default=6, help="CH347 GPIO bit used as notify IRQ")
    parser.add_argument("--set-irq-input", action="store_true", help="configure selected IRQ GPIO as input")
    parser.add_argument("--wait-irq", action="store_true", help="wait for notify IRQ before reading ACK/frame")
    parser.add_argument("--timeout-ms", type=int, default=150, help="timeout per HIF command")
    parser.add_argument("--scan-chunk-size", type=int, default=1024, help="scan this many bytes for A5 after a bad header")
    parser.add_argument("--loops", type=int, default=1, help="repeat point-poll command this many times")
    parser.add_argument(
        "commands",
        nargs="*",
        default=["version", "spi-poll-delay", "host-active", "point-poll"],
        help=f"commands: {', '.join(SAFE_HIF_COMMANDS)} or point-poll",
    )
    args = parser.parse_args(argv)

    config = Ch347SpiConfig(
        mode=args.mode,
        chip_select=args.chip_select,
        clock_code=args.clock_code,
        byte_order=args.byte_order,
    )
    seq = 0
    with Ch347Spi(index=args.index, config=config) as spi:
        io = HifVerificationIo(spi)
        if args.set_irq_input:
            spi.gpio_set_input(1 << args.irq_bit)
        for command_name in args.commands:
            repeats = args.loops if command_name == "point-poll" else 1
            for repeat in range(1, repeats + 1):
                try:
                    if command_name == "point-poll":
                        frame = run_point_poll(
                            io,
                            seq=seq,
                            wait_irq=args.wait_irq,
                            irq_bit=args.irq_bit,
                            timeout_ms=args.timeout_ms,
                            scan_chunk_size=args.scan_chunk_size,
                        )
                    else:
                        frame = run_named_command(
                            io,
                            command_name,
                            seq=seq,
                            wait_irq=args.wait_irq,
                            irq_bit=args.irq_bit,
                            timeout_ms=args.timeout_ms,
                            scan_chunk_size=args.scan_chunk_size,
                        )
                    print(f"{command_name}[{repeat}]: {summarize_frame(frame)} hex={frame[:64].hex(' ')}")
                except KeyError:
                    print(f"{command_name}[{repeat}]: unknown command")
                    return 2
                except HifSpiTimeout as exc:
                    print(f"{command_name}[{repeat}]: timeout {exc} {io.format_diagnostics()}")
                except Exception as exc:
                    print(f"{command_name}[{repeat}]: error {type(exc).__name__}: {exc} {io.format_diagnostics()}")
                    return 2
                finally:
                    seq = (seq + 1) & 0x7
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
