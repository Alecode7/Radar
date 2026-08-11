from __future__ import annotations

import csv
from datetime import datetime
from pathlib import Path

import matplotlib.dates as mdates
import matplotlib.pyplot as plt
from matplotlib import font_manager


LOG_DIR = Path.home() / "Desktop" / "MRS6130睡眠雷达日志"
OUTPUT = LOG_DIR / "latest_session_trends.png"


def read_latest_csv() -> tuple[Path, list[dict[str, str]]]:
    paths = sorted(LOG_DIR.glob("mrs6130_session_*.csv"), key=lambda p: p.stat().st_mtime)
    if not paths:
        raise FileNotFoundError("没有找到会话CSV")
    path = paths[-1]
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return path, list(csv.DictReader(stream))


def to_float(row: dict[str, str], key: str) -> float | None:
    value = row.get(key, "").strip()
    if not value:
        return None
    number = float(value)
    return number if number > 0 else None


def main() -> None:
    csv_path, rows = read_latest_csv()
    if not rows:
        raise ValueError("最新CSV没有数据")

    font_path = Path(r"C:\Windows\Fonts\msyh.ttc")
    if font_path.exists():
        font_manager.fontManager.addfont(str(font_path))
        plt.rcParams["font.family"] = "Microsoft YaHei"
    plt.rcParams["axes.unicode_minus"] = False

    times = [datetime.strptime(row["time"], "%Y-%m-%d %H:%M:%S.%f") for row in rows]
    heart = [to_float(row, "HR") for row in rows]
    breath = [to_float(row, "BR") for row in rows]
    motion = [float(row.get("motion_score") or 0) for row in rows]
    bed = [float(row.get("BED") or 0) for row in rows]

    fig, axes = plt.subplots(3, 1, figsize=(13, 8), sharex=True, constrained_layout=True)
    fig.suptitle(
        f"MRS6130 最新测试趋势\n{csv_path.name}",
        fontsize=15,
        fontweight="bold",
    )

    ax = axes[0]
    ax.plot(times, heart, color="#2563eb", marker="o", markersize=2.5, linewidth=1.4, label="雷达心率")
    ax.axhspan(66, 67, color="#16a34a", alpha=0.12, label="手测最后两分钟 66-67")
    ax.set_ylabel("心率 (次/分)")
    ax.set_ylim(45, 82)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="upper right")

    ax = axes[1]
    ax.plot(times, breath, color="#ea580c", marker="o", markersize=2.5, linewidth=1.3, label="呼吸率")
    ax.set_ylabel("呼吸 (次/分)")
    ax.set_ylim(0, 30)
    ax.grid(True, alpha=0.25)
    ax.legend(loc="upper right")

    ax = axes[2]
    ax.plot(times, motion, color="#7c3aed", linewidth=1.2, label="体动指数 0-100")
    ax.fill_between(times, motion, 0, color="#7c3aed", alpha=0.12)
    ax.step(times, [value * 100 for value in bed], where="post", color="#16a34a", alpha=0.65, label="在床")
    ax.set_ylabel("体动 / 在床")
    ax.set_ylim(0, 110)
    ax.set_yticks([0, 25, 50, 75, 100])
    ax.grid(True, alpha=0.25)
    ax.legend(loc="upper right")

    axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
    axes[-1].set_xlabel("时间")
    fig.savefig(OUTPUT, dpi=180)
    print(f"CSV={csv_path}")
    print(f"PNG={OUTPUT}")


if __name__ == "__main__":
    main()
