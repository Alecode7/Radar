from __future__ import annotations

import argparse

try:
    from .ch347_spi import Ch347Spi, Ch347SpiConfig
except ImportError:
    from ch347_spi import Ch347Spi, Ch347SpiConfig


def _format_value(key: str, value: int) -> str:
    if key in {"chip_select", "default_read_byte"}:
        return f"0x{value:X}"
    return str(value)


def _print_config(title: str, config: Ch347SpiConfig) -> None:
    print(title)
    for key, value in config.as_dict().items():
        print(f"  {key}: {_format_value(key, value)}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Dump CH347 SPI configuration after CH347SPI_Init.")
    parser.add_argument("--index", type=lambda value: int(value, 0), default=0, help="CH347 device index")
    parser.add_argument(
        "--chip-select",
        type=lambda value: int(value, 0),
        default=0x80,
        help="CH347 chip select mask, default: 0x80",
    )
    parser.add_argument("--clock-code", type=lambda value: int(value, 0), default=3, help="CH347 SPI clock code")
    parser.add_argument("--mode", type=lambda value: int(value, 0), default=0, help="CH347 SPI mode/iMode")
    parser.add_argument("--byte-order", type=lambda value: int(value, 0), default=1, help="CH347 byte order")
    parser.add_argument("--irq-bit", type=int, default=6, help="GPIO bit used as notify IRQ")
    args = parser.parse_args(argv)

    requested = Ch347SpiConfig(
        mode=args.mode,
        chip_select=args.chip_select,
        clock_code=args.clock_code,
        byte_order=args.byte_order,
    )
    _print_config("requested:", requested)

    with Ch347Spi(index=args.index, config=requested) as spi:
        actual = spi.get_config()
        _print_config("driver:", actual)

        try:
            direction, data = spi.gpio_get()
            level = 1 if data & (1 << args.irq_bit) else 0
            print(f"gpio: dir=0x{direction:02X} data=0x{data:02X} bit{args.irq_bit}={level}")
        except Exception as exc:
            print(f"gpio: unavailable ({exc})")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
