from __future__ import annotations

import argparse
from dataclasses import dataclass
import time

try:
    from .ch347_spi import Ch347Spi, Ch347SpiConfig
    from .hif_spi import (
        HifSpiSession,
        HifSpiTimeout,
        build_host_read_poll_frame,
        read_direct_stream_frame,
        read_poll_burst_stream_frame,
    )
    from .parsers import HIF_MSG_ID_OBJECTS, decode_hif_msg_word, parse_hif_stream
except ImportError:
    from ch347_spi import Ch347Spi, Ch347SpiConfig
    from hif_spi import (
        HifSpiSession,
        HifSpiTimeout,
        build_host_read_poll_frame,
        read_direct_stream_frame,
        read_poll_burst_stream_frame,
    )
    from parsers import HIF_MSG_ID_OBJECTS, decode_hif_msg_word, parse_hif_stream


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
    if header.msg_id == HIF_MSG_ID_OBJECTS:
        packets = parse_hif_stream(frame)
        if packets:
            packet = packets[-1]
            parts.append(f"frame={packet.frame_index}")
            parts.append(f"points={len(packet.points)}")
        else:
            parts.append("points=unparsed")
    return " ".join(parts)


@dataclass
class ProbeIo:
    spi: Ch347Spi
    io_mode: str = "split"
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

    def write(self, data: bytes) -> bytes:
        if self.io_mode == "split":
            result = self.spi.write(data)
            self._record_tx(data)
            return result or b""
        if self.io_mode == "write-read":
            received = self.spi.write_read(data)
            self._record_tx(data)
            self._record_rx(received)
            return received
        received = self.spi.transfer(data)
        self._record_tx(data)
        self._record_rx(received)
        return received

    def read(self, size: int) -> bytes:
        if self.io_mode == "split":
            received = self.spi.read(size)
        elif self.io_mode == "write-read":
            received = self.spi.write_read(b"\xff" * size)
        else:
            received = self.spi.transfer(b"\xff" * size)
        self._record_rx(received)
        return received

    def direct_transfer(self, data: bytes) -> bytes:
        received = self.spi.transfer(data)
        self._record_tx(data)
        self._record_rx(received)
        return received

    def configure_irq_input(self, irq_bit: int) -> None:
        self.spi.gpio_set_input(1 << irq_bit)

    def gpio_is_high(self, irq_bit: int) -> bool:
        direction, data = self.spi.gpio_get()
        self.gpio_reads += 1
        self.last_gpio_dir = direction
        self.last_gpio_data = data
        self.last_irq_bit = irq_bit
        return bool(data & (1 << irq_bit))

    def format_timeout(self) -> str:
        gpio = ""
        if self.last_gpio_dir is not None and self.last_gpio_data is not None and self.last_irq_bit is not None:
            level = 1 if self.last_gpio_data & (1 << self.last_irq_bit) else 0
            gpio = (
                f" gpio=dir:0x{self.last_gpio_dir:02x} "
                f"data:0x{self.last_gpio_data:02x} bit{self.last_irq_bit}={level}"
            )
        return (
            f"timeout tx={self.tx_bytes} rx={self.rx_bytes} A5={self.rx_magic_bytes} "
            f"FF={self.all_ff_reads} 00={self.all_zero_reads} "
            f"last_tx={self.last_tx_hex or '--'} last_rx={self.last_rx_hex or '--'}{gpio}"
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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Probe MRS6130 HIF point cloud over CH347 SPI.")
    parser.add_argument("--index", type=lambda value: int(value, 0), default=0, help="CH347 device index, default: 0")
    parser.add_argument(
        "--chip-select",
        type=lambda value: int(value, 0),
        default=0x80,
        help="CH347 chip select mask, default: 0x80",
    )
    parser.add_argument("--clock-code", type=lambda value: int(value, 0), default=3, help="CH347 SPI clock code")
    parser.add_argument("--mode", type=lambda value: int(value, 0), default=0, help="CH347 SPI mode/iMode, default: 0")
    parser.add_argument(
        "--byte-order",
        type=lambda value: int(value, 0),
        default=1,
        help="CH347 byte order, default: 1",
    )
    parser.add_argument(
        "--io-mode",
        choices=("direct", "split", "stream", "write-read", "poll-burst"),
        default="direct",
        help="SPI API mode: direct=raw StreamSPI4, poll-burst=official style HOST_READ+GPIO+StreamSPI4, split=CH347SPI_Write then CH347SPI_Read, stream=HOST_READ over CH347StreamSPI4, write-read=CH347SPI_WriteRead",
    )
    parser.add_argument("--irq-bit", type=int, default=6, help="CH347 GPIO bit used as SPI notify IRQ, default: 6")
    parser.add_argument(
        "--set-irq-input",
        action="store_true",
        help="configure selected IRQ GPIO as input with CH347GPIO_Set(index, mask, 0, 0)",
    )
    parser.add_argument("--no-gpio-wait", action="store_true", help="poll-burst mode: skip GPIO wait")
    parser.add_argument("--loops", type=int, default=50, help="poll attempts")
    parser.add_argument("--timeout-ms", type=int, default=80, help="timeout for one HIF frame")
    parser.add_argument("--scan-chunk-size", type=int, default=1024, help="scan this many bytes for A5 after a bad header")
    parser.add_argument("--interval-ms", type=int, default=50, help="delay after an empty poll")
    args = parser.parse_args(argv)

    config = Ch347SpiConfig(
        mode=args.mode,
        chip_select=args.chip_select,
        clock_code=args.clock_code,
        byte_order=args.byte_order,
    )
    with Ch347Spi(index=args.index, config=config) as spi:
        probe_io = ProbeIo(spi, args.io_mode)
        if args.set_irq_input:
            probe_io.configure_irq_input(args.irq_bit)
        session = None if args.io_mode in ("direct", "poll-burst") else HifSpiSession(probe_io.write, read=probe_io.read)
        seq = 0
        for attempt in range(1, args.loops + 1):
            try:
                if args.io_mode == "direct":
                    frame = read_direct_stream_frame(
                        probe_io.direct_transfer,
                        timeout_ms=args.timeout_ms,
                        scan_chunk_size=args.scan_chunk_size,
                    )
                elif args.io_mode == "poll-burst":
                    poll_frame = build_host_read_poll_frame(seq=seq)
                    seq = (seq + 1) & 0x7
                    frame = read_poll_burst_stream_frame(
                        probe_io.direct_transfer,
                        irq_is_high=None if args.no_gpio_wait else lambda: probe_io.gpio_is_high(args.irq_bit),
                        poll_frame=poll_frame,
                        timeout_ms=args.timeout_ms,
                        scan_chunk_size=args.scan_chunk_size,
                    )
                else:
                    frame = session.poll_frame(timeout_ms=args.timeout_ms)
            except HifSpiTimeout:
                print(f"{attempt:03d}: {probe_io.format_timeout()}")
                time.sleep(args.interval_ms / 1000.0)
                continue
            except Exception as exc:
                print(f"{attempt:03d}: error {type(exc).__name__}: {exc} {probe_io.format_timeout()}")
                return 2

            print(f"{attempt:03d}: {summarize_frame(frame)} hex={frame[:64].hex(' ')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
