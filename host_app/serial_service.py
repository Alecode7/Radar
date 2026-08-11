from __future__ import annotations

from dataclasses import dataclass
import threading
import time
from typing import Callable, Iterable


_DEFAULT = object()


try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover - exercised through dependency injection tests.
    serial = None
    list_ports = None


class SerialDependencyError(RuntimeError):
    pass


@dataclass(frozen=True)
class SerialEvent:
    kind: str
    payload: bytes | str


class SerialService:
    def __init__(
        self,
        serial_factory=_DEFAULT,
        port_lister: Callable[[], Iterable[object]] | None | object = _DEFAULT,
    ):
        self.serial_factory = getattr(serial, "Serial", None) if serial_factory is _DEFAULT else serial_factory
        self.port_lister = (
            list_ports.comports if list_ports is not None else None
        ) if port_lister is _DEFAULT else port_lister
        self.connection = None
        self._reader_thread: threading.Thread | None = None
        self._stop_event = threading.Event()

    def ensure_available(self) -> None:
        if self.serial_factory is None or self.port_lister is None:
            raise SerialDependencyError("缺少 pyserial，请先安装依赖：python -m pip install -r host_app/requirements.txt")

    def list_ports(self) -> list[tuple[str, str]]:
        self.ensure_available()
        ports = []
        for port in self.port_lister():
            device = getattr(port, "device", str(port))
            description = getattr(port, "description", "")
            label = f"{device} - {description}" if description else device
            ports.append((device, label))
        return ports

    def connect(self, port: str, baudrate: int):
        self.ensure_available()
        self.disconnect()
        self.connection = self.serial_factory(
            port=port,
            baudrate=baudrate,
            timeout=0.05,
            write_timeout=0.5,
        )
        return self.connection

    def start_reader(self, on_event: Callable[[SerialEvent], None]) -> None:
        if self.connection is None:
            raise RuntimeError("串口未连接")
        if self._reader_thread and self._reader_thread.is_alive():
            return

        self._stop_event.clear()
        self._reader_thread = threading.Thread(
            target=self._read_loop,
            args=(on_event,),
            name="mrs6130-serial-reader",
            daemon=True,
        )
        self._reader_thread.start()

    def _read_loop(self, on_event: Callable[[SerialEvent], None]) -> None:
        while not self._stop_event.is_set():
            conn = self.connection
            if conn is None:
                return
            try:
                waiting = getattr(conn, "in_waiting", 0)
                size = waiting if waiting else 1
                data = conn.read(size)
                if data:
                    on_event(SerialEvent("rx", data))
                else:
                    time.sleep(0.01)
            except Exception as exc:  # pragma: no cover - hardware dependent path.
                on_event(SerialEvent("error", str(exc)))
                return

    def send_command(self, command: str) -> None:
        if self.connection is None:
            raise RuntimeError("串口未连接")
        command = command.strip()
        if not command:
            return
        self.connection.write((command + "\r\n").encode("ascii", errors="ignore"))

    def disconnect(self) -> None:
        self._stop_event.set()
        if self._reader_thread and self._reader_thread.is_alive():
            self._reader_thread.join(timeout=0.5)
        self._reader_thread = None

        if self.connection is not None:
            try:
                self.connection.close()
            finally:
                self.connection = None
