from __future__ import annotations

from dataclasses import dataclass
import threading
import time
from typing import Callable

try:
    from .ch347_spi import Ch347Error, Ch347Spi, Ch347SpiConfig
    from .hif_spi import HifSpiTimeout, build_host_read_poll_frame, read_poll_burst_stream_frame
    from .serial_service import SerialEvent
except ImportError:
    from ch347_spi import Ch347Error, Ch347Spi, Ch347SpiConfig
    from hif_spi import HifSpiTimeout, build_host_read_poll_frame, read_poll_burst_stream_frame
    from serial_service import SerialEvent


@dataclass
class SpiDiagnostics:
    poll_attempts: int = 0
    timeouts: int = 0
    frames: int = 0
    tx_bytes: int = 0
    rx_bytes: int = 0
    rx_magic_bytes: int = 0
    all_ff_reads: int = 0
    all_zero_reads: int = 0
    last_tx_hex: str = ""
    last_rx_hex: str = ""
    last_error: str = ""
    gpio_reads: int = 0
    last_gpio_dir: int = 0
    last_gpio_data: int = 0
    irq_bit: int = 6


class SpiHifService:
    def __init__(
        self,
        *,
        spi_factory: Callable[..., Ch347Spi] = Ch347Spi,
        frame_reader: Callable[..., bytes] = read_poll_burst_stream_frame,
    ):
        self.spi_factory = spi_factory
        self.frame_reader = frame_reader
        self.spi: Ch347Spi | None = None
        self._reader_thread: threading.Thread | None = None
        self._stop_event = threading.Event()
        self.diagnostics = SpiDiagnostics()
        self._seq = 0
        self._irq_bit = 6

    @property
    def connection(self):
        return self.spi

    def connect(
        self,
        *,
        index: int = 0,
        chip_select: int = 0x80,
        clock_code: int = 3,
        irq_bit: int = 6,
    ) -> None:
        self.disconnect()
        self.diagnostics = SpiDiagnostics()
        self.diagnostics.irq_bit = irq_bit
        self._irq_bit = irq_bit
        self._seq = 0
        config = Ch347SpiConfig(chip_select=chip_select, clock_code=clock_code)
        self.spi = self.spi_factory(index=index, config=config)
        self.spi.open()
        if hasattr(self.spi, "gpio_set_input"):
            self.spi.gpio_set_input(1 << irq_bit)

    def poll_once(self, timeout_ms: int = 80) -> bytes | None:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        self.diagnostics.poll_attempts += 1
        try:
            poll_frame = build_host_read_poll_frame(seq=self._seq)
            self._seq = (self._seq + 1) & 0x7
            frame = self.frame_reader(
                self._transfer_with_diagnostics,
                irq_is_high=self._irq_is_high,
                poll_frame=poll_frame,
                timeout_ms=timeout_ms,
                scan_chunk_size=1024,
            )
            self.diagnostics.frames += 1
            self.diagnostics.last_error = ""
            return frame
        except HifSpiTimeout:
            self.diagnostics.timeouts += 1
            return None
        except Exception as exc:
            self.diagnostics.last_error = str(exc)
            raise

    def _irq_is_high(self) -> bool:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        if not hasattr(self.spi, "gpio_get"):
            return True
        direction, data = self.spi.gpio_get()
        self.diagnostics.gpio_reads += 1
        self.diagnostics.last_gpio_dir = direction
        self.diagnostics.last_gpio_data = data
        return bool(data & (1 << self._irq_bit))

    def _write_with_diagnostics(self, data: bytes) -> bytes:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        try:
            if hasattr(self.spi, "write"):
                result = self.spi.write(data)
            else:
                result = self.spi.transfer(data)
        except Exception as exc:
            self.diagnostics.last_error = str(exc)
            raise
        self._record_write(data)
        return result or b""

    def _read_with_diagnostics(self, size: int) -> bytes:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        try:
            if hasattr(self.spi, "read"):
                received = self.spi.read(size)
            else:
                received = self.spi.transfer(b"\xff" * size)
        except Exception as exc:
            self.diagnostics.last_error = str(exc)
            raise
        self._record_read(received)
        return received

    def _transfer_with_diagnostics(self, data: bytes) -> bytes:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        try:
            received = self.spi.transfer(data)
        except Exception as exc:
            self.diagnostics.last_error = str(exc)
            raise
        self._record_transfer(data, received)
        return received

    def _record_write(self, transmitted: bytes) -> None:
        self.diagnostics.tx_bytes += len(transmitted)
        self.diagnostics.last_tx_hex = self._format_hex(transmitted)
        self.diagnostics.last_error = ""

    def _record_read(self, received: bytes) -> None:
        self.diagnostics.rx_bytes += len(received)
        self.diagnostics.rx_magic_bytes += received.count(0xA5)
        if received and all(byte == 0xFF for byte in received):
            self.diagnostics.all_ff_reads += 1
        if received and all(byte == 0x00 for byte in received):
            self.diagnostics.all_zero_reads += 1
        self.diagnostics.last_rx_hex = self._format_hex(received)
        self.diagnostics.last_error = ""

    def _record_transfer(self, transmitted: bytes, received: bytes) -> None:
        self.diagnostics.tx_bytes += len(transmitted)
        self.diagnostics.rx_bytes += len(received)
        self.diagnostics.rx_magic_bytes += received.count(0xA5)
        if received and all(byte == 0xFF for byte in received):
            self.diagnostics.all_ff_reads += 1
        if received and all(byte == 0x00 for byte in received):
            self.diagnostics.all_zero_reads += 1
        self.diagnostics.last_tx_hex = self._format_hex(transmitted)
        self.diagnostics.last_rx_hex = self._format_hex(received)
        self.diagnostics.last_error = ""

    @staticmethod
    def _format_hex(data: bytes, limit: int = 64) -> str:
        if not data:
            return ""
        suffix = " ..." if len(data) > limit else ""
        return data[:limit].hex(" ") + suffix

    def start_reader(
        self,
        on_event: Callable[[SerialEvent], None],
        *,
        timeout_ms: int = 80,
        idle_sleep_s: float = 0.02,
        status_interval_s: float = 1.0,
    ) -> None:
        if self.spi is None:
            raise Ch347Error("SPI数据口未连接")
        if self._reader_thread and self._reader_thread.is_alive():
            return

        self._stop_event.clear()
        self._reader_thread = threading.Thread(
            target=self._read_loop,
            args=(on_event, timeout_ms, idle_sleep_s, status_interval_s),
            name="mrs6130-spi-reader",
            daemon=True,
        )
        self._reader_thread.start()

    def _read_loop(
        self,
        on_event: Callable[[SerialEvent], None],
        timeout_ms: int,
        idle_sleep_s: float,
        status_interval_s: float,
    ) -> None:
        last_status_at = 0.0
        while not self._stop_event.is_set():
            try:
                frame = self.poll_once(timeout_ms=timeout_ms)
                if frame:
                    on_event(SerialEvent("rx", frame))
                else:
                    now = time.monotonic()
                    if status_interval_s <= 0 or now - last_status_at >= status_interval_s:
                        on_event(SerialEvent("status", "spi"))
                        last_status_at = now
                    time.sleep(idle_sleep_s)
            except Exception as exc:  # pragma: no cover - hardware dependent path.
                on_event(SerialEvent("error", str(exc)))
                return

    def disconnect(self) -> None:
        self._stop_event.set()
        if self._reader_thread and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=0.5)
        self._reader_thread = None

        if self.spi is not None:
            try:
                self.spi.close()
            finally:
                self.spi = None
