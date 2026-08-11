from __future__ import annotations

from dataclasses import dataclass
import ctypes
from pathlib import Path


DEFAULT_CH347_DLL = r"C:\Windows\System32\CH347DLLA64.DLL"
DEFAULT_CHIP_SELECT = 0x80


class Ch347Error(RuntimeError):
    pass


class SpiConfigStruct(ctypes.Structure):
    _pack_ = 1
    _fields_ = [
        ("iMode", ctypes.c_ubyte),
        ("iClock", ctypes.c_ubyte),
        ("iByteOrder", ctypes.c_ubyte),
        ("iSpiWriteReadInterval", ctypes.c_ushort),
        ("iSpiOutDefaultData", ctypes.c_ubyte),
        ("iChipSelect", ctypes.c_uint32),
        ("CS1Polarity", ctypes.c_ubyte),
        ("CS2Polarity", ctypes.c_ubyte),
        ("iIsAutoDeativeCS", ctypes.c_ushort),
        ("iActiveDelay", ctypes.c_ushort),
        ("iDelayDeactive", ctypes.c_uint32),
    ]


@dataclass(frozen=True)
class Ch347SpiConfig:
    mode: int = 0
    clock_code: int = 3
    byte_order: int = 1
    write_read_interval: int = 0
    default_read_byte: int = 0xFF
    chip_select: int = DEFAULT_CHIP_SELECT
    cs1_polarity: int = 0
    cs2_polarity: int = 0
    auto_deactivate_cs: int = 1
    active_delay: int = 0
    delay_deactivate: int = 0
    pre_init_gpio_input_mask: int = 0x40

    def to_ctypes(self) -> SpiConfigStruct:
        return SpiConfigStruct(
            self.mode & 0xFF,
            self.clock_code & 0xFF,
            self.byte_order & 0xFF,
            self.write_read_interval & 0xFFFF,
            self.default_read_byte & 0xFF,
            self.chip_select & 0xFFFFFFFF,
            self.cs1_polarity & 0xFF,
            self.cs2_polarity & 0xFF,
            self.auto_deactivate_cs & 0xFFFF,
            self.active_delay & 0xFFFF,
            self.delay_deactivate & 0xFFFFFFFF,
        )

    @classmethod
    def from_ctypes(cls, raw: SpiConfigStruct) -> "Ch347SpiConfig":
        return cls(
            mode=int(raw.iMode),
            clock_code=int(raw.iClock),
            byte_order=int(raw.iByteOrder),
            write_read_interval=int(raw.iSpiWriteReadInterval),
            default_read_byte=int(raw.iSpiOutDefaultData),
            chip_select=int(raw.iChipSelect),
            cs1_polarity=int(raw.CS1Polarity),
            cs2_polarity=int(raw.CS2Polarity),
            auto_deactivate_cs=int(raw.iIsAutoDeativeCS),
            active_delay=int(raw.iActiveDelay),
            delay_deactivate=int(raw.iDelayDeactive),
        )

    def as_dict(self) -> dict[str, int]:
        return {
            "mode": self.mode,
            "clock_code": self.clock_code,
            "byte_order": self.byte_order,
            "write_read_interval": self.write_read_interval,
            "default_read_byte": self.default_read_byte,
            "chip_select": self.chip_select,
            "cs1_polarity": self.cs1_polarity,
            "cs2_polarity": self.cs2_polarity,
            "auto_deactivate_cs": self.auto_deactivate_cs,
            "active_delay": self.active_delay,
            "delay_deactivate": self.delay_deactivate,
        }


class Ch347Spi:
    def __init__(
        self,
        *,
        index: int = 0,
        dll_path: str | Path = DEFAULT_CH347_DLL,
        dll=None,
        config: Ch347SpiConfig | None = None,
    ):
        self.index = index
        self.dll_path = Path(dll_path)
        self.dll = dll
        self.config = config or Ch347SpiConfig()
        self.handle = None

    @property
    def is_open(self) -> bool:
        return self.handle is not None

    def open(self) -> None:
        if self.is_open:
            return
        self._load_dll()
        self._bind_functions()

        handle = self.dll.CH347OpenDevice(self.index)
        if self._invalid_handle(handle):
            raise Ch347Error(f"打开 CH347 设备失败 index={self.index}")

        config = self.config.to_ctypes()
        self._configure_pre_init_gpio()
        ok = self.dll.CH347SPI_Init(self.index, ctypes.pointer(config))
        if not ok:
            self.dll.CH347CloseDevice(self.index)
            raise Ch347Error(f"初始化 CH347 SPI 失败 index={self.index}")

        self.handle = handle

    def close(self) -> None:
        if self.dll is not None and self.is_open:
            self.dll.CH347CloseDevice(self.index)
        self.handle = None

    def transfer(self, data: bytes) -> bytes:
        if not self.is_open:
            raise Ch347Error("CH347 SPI 未连接")
        if not data:
            return b""

        buffer_type = ctypes.c_ubyte * len(data)
        buffer = buffer_type.from_buffer_copy(data)
        ok = self.dll.CH347StreamSPI4(self.index, self.config.chip_select, len(data), buffer)
        if not ok:
            raise Ch347Error(f"CH347 SPI transfer 失败 len={len(data)}")
        return bytes(buffer)

    def write(self, data: bytes) -> bytes:
        if not self.is_open:
            raise Ch347Error("CH347 SPI 未连接")
        if not data:
            return b""

        buffer_type = ctypes.c_ubyte * len(data)
        buffer = buffer_type.from_buffer_copy(data)
        write_step = len(data)
        ok = self.dll.CH347SPI_Write(self.index, self.config.chip_select, len(data), write_step, buffer)
        if not ok:
            raise Ch347Error(f"CH347 SPI write 失败 len={len(data)}")
        return b""

    def read(self, size: int, *, prefix: bytes = b"") -> bytes:
        if not self.is_open:
            raise Ch347Error("CH347 SPI 未连接")
        if size <= 0:
            return b""

        total_len = len(prefix) + size
        initial = prefix + bytes([self.config.default_read_byte]) * size
        buffer_type = ctypes.c_ubyte * total_len
        buffer = buffer_type.from_buffer_copy(initial)
        read_len = ctypes.c_ulong(size)
        ok = self.dll.CH347SPI_Read(
            self.index,
            self.config.chip_select,
            len(prefix),
            ctypes.pointer(read_len),
            buffer,
        )
        if not ok:
            raise Ch347Error(f"CH347 SPI read 失败 len={size}")
        return bytes(buffer[: read_len.value])

    def write_read(self, data: bytes) -> bytes:
        if not self.is_open:
            raise Ch347Error("CH347 SPI 未连接")
        if not data:
            return b""

        buffer_type = ctypes.c_ubyte * len(data)
        buffer = buffer_type.from_buffer_copy(data)
        ok = self.dll.CH347SPI_WriteRead(self.index, self.config.chip_select, len(data), buffer)
        if not ok:
            raise Ch347Error(f"CH347 SPI write_read 失败 len={len(data)}")
        return bytes(buffer)

    def gpio_get(self) -> tuple[int, int]:
        if not self.is_open:
            raise Ch347Error("CH347 未连接")
        if not hasattr(self.dll, "CH347GPIO_Get"):
            raise Ch347Error("当前 CH347 DLL 不支持 GPIO_Get")

        direction = ctypes.c_ubyte(0)
        data = ctypes.c_ubyte(0)
        ok = self.dll.CH347GPIO_Get(self.index, ctypes.pointer(direction), ctypes.pointer(data))
        if not ok:
            raise Ch347Error("CH347 GPIO_Get 失败")
        return int(direction.value), int(data.value)

    def get_config(self) -> Ch347SpiConfig:
        if not self.is_open:
            raise Ch347Error("CH347 SPI 未连接")
        if not hasattr(self.dll, "CH347SPI_GetCfg"):
            raise Ch347Error("当前 CH347 DLL 不支持 SPI_GetCfg")

        raw = SpiConfigStruct()
        ok = self.dll.CH347SPI_GetCfg(self.index, ctypes.pointer(raw))
        if not ok:
            raise Ch347Error(f"CH347 SPI_GetCfg 失败 index={self.index}")
        return Ch347SpiConfig.from_ctypes(raw)

    def gpio_set_input(self, mask: int) -> None:
        if not self.is_open:
            raise Ch347Error("CH347 未连接")
        if mask == self.config.pre_init_gpio_input_mask:
            return
        if not hasattr(self.dll, "CH347GPIO_Set"):
            raise Ch347Error("当前 CH347 DLL 不支持 GPIO_Set")

        ok = self.dll.CH347GPIO_Set(self.index, mask & 0xFF, 0, 0)
        if not ok:
            raise Ch347Error(f"CH347 GPIO_Set 输入模式失败 mask=0x{mask:X}")

    def _configure_pre_init_gpio(self) -> None:
        mask = self.config.pre_init_gpio_input_mask
        if not mask:
            return
        if not hasattr(self.dll, "CH347GPIO_Set"):
            return

        ok = self.dll.CH347GPIO_Set(self.index, mask & 0xFF, 0, 0)
        if not ok:
            self.dll.CH347CloseDevice(self.index)
            raise Ch347Error(f"CH347 GPIO_Set 输入模式失败 mask=0x{mask:X}")

    def __enter__(self) -> "Ch347Spi":
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.close()

    def _load_dll(self) -> None:
        if self.dll is not None:
            return
        if not self.dll_path.exists():
            raise Ch347Error(f"找不到 CH347 DLL: {self.dll_path}")
        self.dll = ctypes.WinDLL(str(self.dll_path))

    def _bind_functions(self) -> None:
        self.dll.CH347OpenDevice.argtypes = [ctypes.c_ulong]
        self.dll.CH347OpenDevice.restype = ctypes.c_void_p
        self.dll.CH347CloseDevice.argtypes = [ctypes.c_ulong]
        self.dll.CH347CloseDevice.restype = ctypes.c_bool
        self.dll.CH347SPI_Init.argtypes = [ctypes.c_ulong, ctypes.POINTER(SpiConfigStruct)]
        self.dll.CH347SPI_Init.restype = ctypes.c_bool
        if hasattr(self.dll, "CH347SPI_GetCfg"):
            self.dll.CH347SPI_GetCfg.argtypes = [ctypes.c_ulong, ctypes.POINTER(SpiConfigStruct)]
            self.dll.CH347SPI_GetCfg.restype = ctypes.c_bool
        self.dll.CH347StreamSPI4.argtypes = [
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ubyte),
        ]
        self.dll.CH347StreamSPI4.restype = ctypes.c_bool
        self.dll.CH347SPI_Write.argtypes = [
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ubyte),
        ]
        self.dll.CH347SPI_Write.restype = ctypes.c_bool
        self.dll.CH347SPI_Read.argtypes = [
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.c_ubyte),
        ]
        self.dll.CH347SPI_Read.restype = ctypes.c_bool
        self.dll.CH347SPI_WriteRead.argtypes = [
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.c_ulong,
            ctypes.POINTER(ctypes.c_ubyte),
        ]
        self.dll.CH347SPI_WriteRead.restype = ctypes.c_bool
        if hasattr(self.dll, "CH347GPIO_Get"):
            self.dll.CH347GPIO_Get.argtypes = [
                ctypes.c_ulong,
                ctypes.POINTER(ctypes.c_ubyte),
                ctypes.POINTER(ctypes.c_ubyte),
            ]
            self.dll.CH347GPIO_Get.restype = ctypes.c_bool
        if hasattr(self.dll, "CH347GPIO_Set"):
            self.dll.CH347GPIO_Set.argtypes = [
                ctypes.c_ulong,
                ctypes.c_ubyte,
                ctypes.c_ubyte,
                ctypes.c_ubyte,
            ]
            self.dll.CH347GPIO_Set.restype = ctypes.c_bool

    @staticmethod
    def _invalid_handle(handle) -> bool:
        if handle is None:
            return True
        if handle == 0:
            return True
        return int(handle) == (2 ** (ctypes.sizeof(ctypes.c_void_p) * 8) - 1)
