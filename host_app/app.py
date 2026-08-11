from __future__ import annotations

from collections import deque
from datetime import datetime
from pathlib import Path
import queue
import time
import tkinter as tk
from tkinter import messagebox, ttk

try:
    from .heart_rate import HeartRateEstimator
    from .parsers import HIF_MSG_ID_OBJECTS
    from .parsers import Point, parse_sleep_line
    from .session_export import write_session_csv
    from .session_metrics import (
        calculate_heart_session_stats,
        calculate_motion_score,
        calculate_ten_minute_summary,
        evaluate_monitoring_alerts,
        session_duration_notice,
    )
    from .serial_service import SerialDependencyError, SerialEvent, SerialService
    from .spi_service import SpiHifService
    from .stream_decoder import SerialStreamDecoder
except ImportError:  # Allows `uv run python main.py` from the host_app project root.
    from heart_rate import HeartRateEstimator
    from parsers import HIF_MSG_ID_OBJECTS
    from parsers import Point, parse_sleep_line
    from session_export import write_session_csv
    from session_metrics import (
        calculate_heart_session_stats,
        calculate_motion_score,
        calculate_ten_minute_summary,
        evaluate_monitoring_alerts,
        session_duration_notice,
    )
    from serial_service import SerialDependencyError, SerialEvent, SerialService
    from spi_service import SpiHifService
    from stream_decoder import SerialStreamDecoder


DEFAULT_BAUDRATES = ("115200", "460800", "921600", "1000000", "2000000")
MAX_LOG_LINES = 600
MIN_SESSION_SUMMARY_FRAMES = 150
TEN_MINUTE_FRAMES = 10 * 60 * 5
LOG_DIR = Path.home() / "Desktop" / "MRS6130睡眠雷达日志"
DISPLAY_MODE_MOTION = "运动点"
DISPLAY_MODE_SCENE = "场景图"
DISPLAY_MODE_MIXED = "混合图"
DISPLAY_MODES = (DISPLAY_MODE_MOTION, DISPLAY_MODE_SCENE, DISPLAY_MODE_MIXED)
MOTION_POINT_FILL = "#2563eb"
SCENE_POINT_FILL = "#94a3b8"
COMMON_COMMANDS = (
    ("一键开始测试", "mmwc stop\nmmwc report points 0\nmmwc start"),
    ("配置连续上报", "mmwc report points 0"),
    ("启动雷达", "mmwc start"),
    ("停止雷达", "mmwc stop"),
    ("关闭上报", "mmwc report disable"),
)


def format_frames(frames: int) -> str:
    total_seconds = max(frames, 0) // 5
    hours, remainder = divmod(total_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours}时{minutes}分{seconds}秒"
    if minutes:
        return f"{minutes}分{seconds}秒"
    return f"{seconds}秒"


def format_frames_short(frames: int) -> str:
    total_seconds = max(frames, 0) // 5
    hours, remainder = divmod(total_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    if hours:
        return f"{hours}:{minutes:02d}:{seconds:02d}"
    return f"{minutes:02d}:{seconds:02d}"


def format_clock_time(value: datetime | None) -> str:
    return value.strftime("%H:%M:%S") if value is not None else "--"


class HostApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("MRS6130 睡眠雷达测试上位机")
        self.geometry("1060x720")
        self.minsize(900, 620)

        self.control_serial = SerialService()
        self.data_serial = SerialService()
        self.spi_service = SpiHifService()
        self.event_queue: queue.Queue[tuple[str, SerialEvent]] = queue.Queue()
        self.control_decoder = SerialStreamDecoder()
        self.data_decoder = SerialStreamDecoder()
        self.latest_points: list[Point] = []
        self.scene_points: deque[tuple[float, Point]] = deque(maxlen=2500)
        self.sleep_history: deque[tuple[float, int, int, int, int, int, int, int]] = deque(
            maxlen=3600
        )
        self.heart_history: deque[tuple[float, int]] = deque(maxlen=1200)
        self._sleep_history_window_s = 3600.0
        self._last_pointcloud_at = 0.0
        self._pointcloud_hold_s = 0.75
        self._scene_hold_s = 30.0
        self._monotonic = time.monotonic
        self._wall_time = time.time
        self._last_status_in_bed = False
        self._last_status_sleep_state = 0
        self._session_bed_enter_at: datetime | None = None
        self._session_first_sleep_at: datetime | None = None
        self._session_last_sleep_exit_at: datetime | None = None
        self._session_bed_exit_at: datetime | None = None
        self.control_log_lines = deque(maxlen=MAX_LOG_LINES)
        self.spi_log_lines = deque(maxlen=MAX_LOG_LINES)
        self._last_data_diag_log_at = 0.0
        self._last_data_diag_bytes = 0
        self._last_spi_diag_log_at = 0.0
        self._last_spi_diag_signature = None
        self._last_session_summary_exit_count = 0
        self._heart_raw_sample_count = 0
        self._latest_breath_rate = 0
        self._session_heart_attempts = 0
        self._session_heart_values: list[int] = []
        self._latest_heart_rate = 0
        self._latest_heart_rate_at: float | None = None
        self._last_ten_minute_report_bucket = 0
        self._last_monitoring_alerts: tuple[str, ...] = ()
        self._session_records: list[dict[str, object]] = []
        self._session_csv_path: Path | None = None
        self.heart_rate_estimator = HeartRateEstimator()
        self.log_dir = LOG_DIR
        self.log_file_path = self.log_dir / f"mrs6130_sleep_{datetime.now():%Y%m%d_%H%M%S}.txt"
        self._log_file = self._open_session_log_file()

        self.control_port_var = tk.StringVar()
        self.control_baud_var = tk.StringVar(value="115200")
        self.data_port_var = tk.StringVar()
        self.data_baud_var = tk.StringVar(value="921600")
        self.spi_index_var = tk.StringVar(value="0")
        self.spi_clock_var = tk.StringVar(value="3")
        self.display_mode_var = tk.StringVar(value=DISPLAY_MODE_MOTION)
        self.connection_var = tk.StringVar(value="控制口未连接")
        self.current_time_var = tk.StringVar(value="当前时间 --")
        self.latest_data_time_var = tk.StringVar(value="最新数据 --")
        self.board_var = tk.StringVar(value="BOARD_MRS6130_P1806/P1812")
        self.show_diag_var = tk.BooleanVar(value=False)

        self.status_vars = {
            "person": tk.StringVar(value="--"),
            "active": tk.StringVar(value="--"),
            "bed": tk.StringVar(value="--"),
            "breath": tk.StringVar(value="--"),
            "breath_rate": tk.StringVar(value="--"),
            "sleep_state": tk.StringVar(value="--"),
            "motion": tk.StringVar(value="0"),
            "micro": tk.StringVar(value="0"),
            "active_points": tk.StringVar(value="0"),
            "motion_level": tk.StringVar(value="--"),
            "turn": tk.StringVar(value="--"),
            "session": tk.StringVar(value="--"),
            "state_duration": tk.StringVar(value="--"),
            "state_ratio": tk.StringVar(value="--"),
            "longest_quiet": tk.StringVar(value="--"),
            "sleep_process": tk.StringVar(value="--"),
            "event_times": tk.StringVar(value="--"),
            "streak": tk.StringVar(value="--"),
            "breath_debug": tk.StringVar(value="--"),
            "breath_quality": tk.StringVar(value="--"),
            "breath_period": tk.StringVar(value="--"),
            "breath_candidate": tk.StringVar(value="--"),
            "breath_summary": tk.StringVar(value="--"),
            "heart_rate": tk.StringVar(value="等待安静数据"),
            "heart_summary": tk.StringVar(value="--"),
            "heart_raw": tk.StringVar(value="等待在床数据"),
            "ten_minute_summary": tk.StringVar(value="等待在床数据"),
            "session_alert": tk.StringVar(value="--"),
        }
        self.data_diag_vars = {
            "bytes": tk.StringVar(value="0"),
            "spi": tk.StringVar(value="--"),
            "magic": tk.StringVar(value="0"),
            "packets": tk.StringVar(value="0"),
            "last_msg": tk.StringVar(value="--"),
            "parse": tk.StringVar(value="--"),
            "hex": tk.StringVar(value="--"),
        }

        self._configure_style()
        self._build_layout()
        if self._log_file is not None:
            self._log_control(f"日志文件: {self.log_file_path}")
        else:
            self._log_control(f"日志文件创建失败: {self.log_file_path}")
        self._update_clock()
        self.refresh_ports()
        self.after(40, self._drain_events)

    def _open_session_log_file(self):
        try:
            self.log_dir.mkdir(parents=True, exist_ok=True)
            log_file = self.log_file_path.open("a", encoding="utf-8", buffering=1)
            log_file.write(f"==== MRS6130 sleep radar log {datetime.now():%Y-%m-%d %H:%M:%S} ====\n")
            return log_file
        except OSError:
            return None

    def _configure_style(self) -> None:
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TFrame", background="#f6f7f9")
        style.configure("Panel.TFrame", background="#ffffff", borderwidth=1, relief="solid")
        style.configure("TLabel", background="#f6f7f9", foreground="#1f2933", font=("Microsoft YaHei UI", 10))
        style.configure("Panel.TLabel", background="#ffffff", foreground="#1f2933", font=("Microsoft YaHei UI", 10))
        style.configure("Title.TLabel", background="#f6f7f9", foreground="#101828", font=("Microsoft YaHei UI", 16, "bold"))
        style.configure("CardTitle.TLabel", background="#ffffff", foreground="#667085", font=("Microsoft YaHei UI", 9))
        style.configure("CardValue.TLabel", background="#ffffff", foreground="#101828", font=("Microsoft YaHei UI", 20, "bold"))
        style.configure("TButton", font=("Microsoft YaHei UI", 10), padding=(10, 6))
        style.configure("Accent.TButton", font=("Microsoft YaHei UI", 10, "bold"), padding=(10, 6))
        style.configure("TCombobox", padding=(6, 4))

    def _build_layout(self) -> None:
        root = ttk.Frame(self, padding=14)
        root.grid(row=0, column=0, sticky="nsew")
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(0, weight=1)
        root.grid_rowconfigure(2, weight=1)
        root.grid_columnconfigure(1, weight=1)

        header = ttk.Frame(root)
        header.grid(row=0, column=0, columnspan=2, sticky="ew", pady=(0, 12))
        header.grid_columnconfigure(0, weight=1)
        ttk.Label(header, text="MRS6130 睡眠雷达测试上位机", style="Title.TLabel").grid(row=0, column=0, sticky="w")
        header_status = ttk.Frame(header)
        header_status.grid(row=0, column=1, sticky="e")
        ttk.Label(header_status, textvariable=self.connection_var).grid(
            row=0, column=0, columnspan=2, sticky="e"
        )
        ttk.Label(header_status, textvariable=self.current_time_var).grid(
            row=1, column=0, sticky="e", padx=(0, 12)
        )
        ttk.Label(header_status, textvariable=self.latest_data_time_var).grid(
            row=1, column=1, sticky="e"
        )

        controls_shell = ttk.Frame(root, style="Panel.TFrame", padding=0)
        controls_shell.grid(row=1, column=0, rowspan=2, sticky="nsew", padx=(0, 12))
        controls_shell.grid_rowconfigure(0, weight=1)
        controls_shell.grid_columnconfigure(0, weight=1)

        self.controls_canvas = tk.Canvas(
            controls_shell,
            background="#ffffff",
            highlightthickness=0,
            width=310,
        )
        self.controls_canvas.grid(row=0, column=0, sticky="nsew")
        self.controls_scrollbar = ttk.Scrollbar(
            controls_shell,
            orient="vertical",
            command=self.controls_canvas.yview,
        )
        self.controls_scrollbar.grid(row=0, column=1, sticky="ns")
        self.controls_canvas.configure(yscrollcommand=self.controls_scrollbar.set)

        controls = ttk.Frame(self.controls_canvas, style="Panel.TFrame", padding=12)
        self.controls_content = controls
        self.controls_window = self.controls_canvas.create_window((0, 0), window=controls, anchor="nw")
        self.controls_canvas.bind("<Configure>", self._resize_controls_canvas)
        controls.bind("<Configure>", self._update_controls_scrollregion)
        self.controls_canvas.bind("<Enter>", self._bind_controls_mousewheel)
        self.controls_canvas.bind("<Leave>", self._unbind_controls_mousewheel)
        controls.grid_columnconfigure(0, weight=1)

        row = 0
        ttk.Label(controls, text="设备连接", style="Panel.TLabel").grid(row=row, column=0, sticky="w")
        row += 1
        ttk.Label(controls, text="控制/日志口", style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=(10, 0))
        row += 1
        self.control_port_combo = ttk.Combobox(
            controls, textvariable=self.control_port_var, width=28, state="readonly"
        )
        self.control_port_combo.grid(row=row, column=0, sticky="ew", pady=(8, 4))
        row += 1
        self.control_baud_combo = ttk.Combobox(
            controls, textvariable=self.control_baud_var, width=28, values=DEFAULT_BAUDRATES
        )
        self.control_baud_combo.grid(row=row, column=0, sticky="ew", pady=4)
        row += 1

        button_row = ttk.Frame(controls, style="Panel.TFrame")
        button_row.grid(row=row, column=0, sticky="ew", pady=(8, 12))
        button_row.grid_columnconfigure((0, 1), weight=1)
        ttk.Button(button_row, text="刷新", command=self.refresh_ports).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        self.one_click_connect_button = ttk.Button(
            button_row,
            text="一键连接",
            style="Accent.TButton",
            command=self.toggle_device_connection,
        )
        self.one_click_connect_button.grid(row=0, column=1, sticky="ew", padx=(4, 0))
        row += 1

        ttk.Separator(controls).grid(row=row, column=0, sticky="ew", pady=8)
        row += 1
        ttk.Label(controls, text="常用命令", style="Panel.TLabel").grid(row=row, column=0, sticky="w")
        row += 1
        for idx, (label, command) in enumerate(COMMON_COMMANDS, start=row):
            ttk.Button(controls, text=label, command=lambda c=command: self.send_command(c)).grid(
                row=idx, column=0, sticky="ew", pady=3
            )
        row += len(COMMON_COMMANDS)

        ttk.Separator(controls).grid(row=row, column=0, sticky="ew", pady=8)
        row += 1
        ttk.Label(controls, text="手动命令", style="Panel.TLabel").grid(row=row, column=0, sticky="w")
        row += 1
        self.command_entry = ttk.Entry(controls)
        self.command_entry.grid(row=row, column=0, sticky="ew", pady=(8, 4))
        self.command_entry.bind("<Return>", lambda _event: self.send_manual_command())
        row += 1
        ttk.Button(controls, text="发送", command=self.send_manual_command).grid(row=row, column=0, sticky="ew")
        row += 1

        ttk.Separator(controls).grid(row=row, column=0, sticky="ew", pady=8)
        row += 1
        ttk.Label(controls, text="板型", style="Panel.TLabel").grid(row=row, column=0, sticky="w")
        row += 1
        ttk.Entry(controls, textvariable=self.board_var).grid(row=row, column=0, sticky="ew", pady=(8, 4))

        content_shell = ttk.Frame(root)
        content_shell.grid(row=1, column=1, rowspan=2, sticky="nsew")
        content_shell.grid_rowconfigure(0, weight=1)
        content_shell.grid_columnconfigure(0, weight=1)
        self.content_canvas = tk.Canvas(
            content_shell,
            background="#f6f7f9",
            highlightthickness=0,
        )
        self.content_canvas.grid(row=0, column=0, sticky="nsew")
        self.content_scrollbar = ttk.Scrollbar(
            content_shell,
            orient="vertical",
            command=self.content_canvas.yview,
        )
        self.content_scrollbar.grid(row=0, column=1, sticky="ns")
        self.content_canvas.configure(yscrollcommand=self.content_scrollbar.set)

        content = ttk.Frame(self.content_canvas)
        self.content_frame = content
        self.content_window = self.content_canvas.create_window((0, 0), window=content, anchor="nw")
        self.content_canvas.bind("<Configure>", self._resize_content_canvas)
        content.bind("<Configure>", self._update_content_scrollregion)
        self.content_canvas.bind("<Enter>", self._bind_content_mousewheel)
        self.content_canvas.bind("<Leave>", self._unbind_content_mousewheel)
        content.grid_columnconfigure(0, weight=1)
        content.grid_rowconfigure(1, weight=1)

        status = ttk.Frame(content)
        status.grid(row=0, column=0, sticky="ew")
        for col in range(6):
            status.grid_columnconfigure(col, weight=1, uniform="status")
        self._status_card(status, 0, "人员", self.status_vars["person"])
        self._status_card(status, 1, "活动", self.status_vars["active"])
        self._status_card(status, 2, "在床", self.status_vars["bed"])
        self._status_card(status, 3, "呼吸", self.status_vars["breath"])
        self._status_card(status, 4, "呼吸率", self.status_vars["breath_rate"])
        self._status_card(status, 5, "睡眠", self.status_vars["sleep_state"])

        main = ttk.Frame(content)
        main.grid(row=1, column=0, sticky="nsew", pady=(12, 0))
        main.grid_columnconfigure(0, weight=2)
        main.grid_columnconfigure(1, weight=3)
        main.grid_rowconfigure(0, weight=1)

        side = ttk.Frame(main)
        side.grid(row=0, column=0, sticky="nsew", padx=(0, 12))
        side.grid_columnconfigure(0, weight=1)

        metrics = ttk.Frame(side, style="Panel.TFrame", padding=10)
        metrics.grid(row=0, column=0, sticky="ew", pady=(0, 12))
        metrics.grid_columnconfigure(1, weight=1)
        metric_rows = (
            ("运动点 MT/MB", "motion"),
            ("微动点 McT/McB", "micro"),
            ("活动点 Act", "active_points"),
            ("体动指数 0-100", "motion_level"),
            ("翻身/大动作", "turn"),
            ("在床统计", "session"),
            ("状态时长", "state_duration"),
            ("状态占比", "state_ratio"),
            ("最长连续安静", "longest_quiet"),
            ("睡眠过程", "sleep_process"),
            ("关键时间", "event_times"),
            ("十分钟综合", "ten_minute_summary"),
            ("会话提示", "session_alert"),
            ("稳定计数", "streak"),
            ("呼吸调试", "breath_debug"),
        )
        for row, (label, key) in enumerate(metric_rows):
            ttk.Label(metrics, text=label, style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=4)
            ttk.Label(metrics, textvariable=self.status_vars[key], style="Panel.TLabel").grid(
                row=row, column=1, sticky="e", pady=4
            )

        breath_panel = ttk.Frame(side, style="Panel.TFrame", padding=10)
        breath_panel.grid(row=1, column=0, sticky="ew", pady=(0, 12))
        breath_panel.grid_columnconfigure(1, weight=1)
        breath_rows = (
            ("目标/质量", "breath_quality"),
            ("相位/周期", "breath_period"),
            ("候选/稳定", "breath_candidate"),
            ("呼吸统计", "breath_summary"),
            ("候选心率", "heart_rate"),
            ("心率统计", "heart_summary"),
            ("心率原始采样", "heart_raw"),
        )
        for row, (label, key) in enumerate(breath_rows):
            ttk.Label(breath_panel, text=label, style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=4)
            ttk.Label(breath_panel, textvariable=self.status_vars[key], style="Panel.TLabel").grid(
                row=row, column=1, sticky="e", pady=4
            )

        logs = ttk.Frame(main)
        logs.grid(row=0, column=1, sticky="nsew")
        logs.grid_rowconfigure(2, weight=1)
        logs.grid_columnconfigure(0, weight=1)
        log_actions = ttk.Frame(logs)
        log_actions.grid(row=0, column=0, sticky="ew", pady=(0, 6))
        log_actions.grid_columnconfigure(0, weight=1)
        ttk.Button(log_actions, text="导出CSV", command=self.export_session_csv).grid(
            row=0, column=1, sticky="e", padx=(0, 6)
        )
        ttk.Button(log_actions, text="删除日志", command=self.clear_logs).grid(row=0, column=2, sticky="e")

        trend_panel = ttk.Frame(logs, style="Panel.TFrame", padding=10)
        trend_panel.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        trend_panel.grid_columnconfigure(0, weight=1)
        ttk.Label(trend_panel, text="最近60分钟趋势", style="Panel.TLabel").grid(
            row=0, column=0, sticky="w"
        )
        self.sleep_trend_canvas = tk.Canvas(
            trend_panel,
            height=190,
            background="#ffffff",
            highlightthickness=0,
        )
        self.sleep_trend_canvas.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        self.sleep_trend_canvas.bind("<Configure>", lambda _event: self._draw_sleep_trend())

        self.control_log_text, self.control_log_scrollbar = self._create_log_box(logs, 2, "控制日志")

    def _toggle_diagnostics_panel(self) -> None:
        if self.show_diag_var.get():
            self.data_diag.grid(row=2, column=0, sticky="ew", pady=(0, 12))
        else:
            self.data_diag.grid_remove()

    def _status_card(self, parent: ttk.Frame, col: int, title: str, value: tk.StringVar) -> None:
        card = ttk.Frame(parent, style="Panel.TFrame", padding=10)
        card.grid(row=0, column=col, sticky="ew", padx=(0 if col == 0 else 6, 0 if col == 5 else 6))
        ttk.Label(card, text=title, style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(card, textvariable=value, style="CardValue.TLabel").grid(row=1, column=0, sticky="w", pady=(4, 0))

    def _create_log_box(self, parent: ttk.Frame, row: int, title: str) -> tuple[tk.Text, ttk.Scrollbar]:
        panel = ttk.Frame(parent, style="Panel.TFrame", padding=10)
        panel.grid(row=row, column=0, sticky="nsew", pady=(0, 8 if row == 0 else 0))
        panel.grid_rowconfigure(1, weight=1)
        panel.grid_columnconfigure(0, weight=1)
        ttk.Label(panel, text=title, style="Panel.TLabel").grid(row=0, column=0, sticky="w")
        scrollbar = ttk.Scrollbar(panel, orient="vertical")
        text = tk.Text(
            panel,
            height=8,
            wrap="none",
            relief="flat",
            background="#0f172a",
            foreground="#e5e7eb",
            insertbackground="#e5e7eb",
            font=("Consolas", 9),
            yscrollcommand=scrollbar.set,
        )
        text.grid(row=1, column=0, sticky="nsew", pady=(8, 0))
        text.bind("<MouseWheel>", lambda event, widget=text: self._scroll_log_text(event, widget))
        scrollbar.configure(command=text.yview)
        scrollbar.grid(row=1, column=1, sticky="ns", pady=(8, 0))
        return text, scrollbar

    def _resize_controls_canvas(self, event: tk.Event) -> None:
        self.controls_canvas.itemconfigure(self.controls_window, width=event.width)

    def _update_controls_scrollregion(self, _event: tk.Event) -> None:
        self.controls_canvas.configure(scrollregion=self.controls_canvas.bbox("all"))

    def _resize_content_canvas(self, event: tk.Event) -> None:
        self.content_canvas.itemconfigure(self.content_window, width=event.width)

    def _update_content_scrollregion(self, _event: tk.Event) -> None:
        self.content_canvas.configure(scrollregion=self.content_canvas.bbox("all"))

    def _bind_controls_mousewheel(self, _event: tk.Event) -> None:
        self.controls_canvas.bind_all("<MouseWheel>", self._scroll_controls)

    def _unbind_controls_mousewheel(self, _event: tk.Event) -> None:
        self.controls_canvas.unbind_all("<MouseWheel>")

    def _scroll_controls(self, event: tk.Event) -> None:
        self.controls_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def _bind_content_mousewheel(self, _event: tk.Event) -> None:
        self.content_canvas.bind_all("<MouseWheel>", self._scroll_content)

    def _unbind_content_mousewheel(self, _event: tk.Event) -> None:
        self.content_canvas.unbind_all("<MouseWheel>")

    def _scroll_content(self, event: tk.Event) -> None:
        self.content_canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

    def _scroll_log_text(self, event: tk.Event, text: tk.Text) -> str:
        text.yview_scroll(int(-1 * (event.delta / 120)), "units")
        return "break"

    def clear_logs(self) -> None:
        self.control_log_lines.clear()
        self.spi_log_lines.clear()
        log_widgets = [self.control_log_text]
        if hasattr(self, "spi_log_text"):
            log_widgets.append(self.spi_log_text)
        for text in log_widgets:
            text.configure(state="normal")
            text.delete("1.0", "end")
            text.configure(state="disabled")
        self._last_data_diag_log_at = 0.0
        self._last_spi_diag_log_at = 0.0
        self._last_spi_diag_signature = None
        self._write_session_log("SYS", "UI log cleared")

    def refresh_ports(self) -> None:
        try:
            ports = self.control_serial.list_ports()
        except SerialDependencyError as exc:
            self._log_control(str(exc))
            self.control_port_combo.configure(values=())
            return
        labels = [label for _device, label in ports]
        self._ports = dict(zip(labels, [device for device, _label in ports], strict=True))
        self.control_port_combo.configure(values=labels)
        if labels and not self.control_port_var.get():
            preferred = next((label for label in labels if "SERIAL-A" in label.upper()), labels[0])
            self.control_port_var.set(preferred)

    def toggle_device_connection(self) -> None:
        if (
            self.control_serial.connection is not None
            or self.data_serial.connection is not None
            or self.spi_service.connection is not None
        ):
            self.control_serial.disconnect()
            self.data_serial.disconnect()
            self.spi_service.disconnect()
            self._log_control("已断开设备")
            self._update_data_diagnostics()
            self._update_connection_status()
            return

        label = self.control_port_var.get()
        port = getattr(self, "_ports", {}).get(label, label)
        if not port:
            messagebox.showwarning("连接设备", "请先选择控制/日志口")
            return

        try:
            baudrate = int(self.control_baud_var.get())
            self.control_serial.connect(port, baudrate)
            self.control_decoder = SerialStreamDecoder()
            self.control_serial.start_reader(lambda event: self.event_queue.put(("control", event)))
        except Exception as exc:
            self.control_serial.disconnect()
            self.data_serial.disconnect()
            self.spi_service.disconnect()
            self._update_data_diagnostics()
            self._update_connection_status()
            messagebox.showerror("连接失败", str(exc))
            return

        self._log_control(f"设备已连接 控制口 {port} @ {baudrate}")
        self._update_connection_status()

    def toggle_control_connection(self) -> None:
        if self.control_serial.connection is not None:
            self.control_serial.disconnect()
            self._log_control("已断开控制口")
            self._update_connection_status()
            return

        label = self.control_port_var.get()
        port = getattr(self, "_ports", {}).get(label, label)
        if not port:
            messagebox.showwarning("控制口", "请先选择控制/日志口")
            return
        try:
            baudrate = int(self.control_baud_var.get())
            self.control_serial.connect(port, baudrate)
            self.control_decoder = SerialStreamDecoder()
            self.control_serial.start_reader(lambda event: self.event_queue.put(("control", event)))
        except Exception as exc:
            messagebox.showerror("连接失败", str(exc))
            return

        self._log_control(f"控制口已连接 {port} @ {baudrate}")
        self._update_connection_status()

    def toggle_data_connection(self) -> None:
        if self.data_serial.connection is not None:
            self.data_serial.disconnect()
            self._log_spi("已断开HIF串口")
            self._update_connection_status()
            return

        label = self.data_port_var.get()
        port = getattr(self, "_ports", {}).get(label, label)
        if not port:
            messagebox.showwarning("HIF串口", "请先选择HIF串口")
            return
        try:
            baudrate = int(self.data_baud_var.get())
            if self.spi_service.connection is not None:
                self.spi_service.disconnect()
                self._log_spi("已断开SPI HIF")
            self.data_serial.connect(port, baudrate)
            self.data_decoder = SerialStreamDecoder()
            self._last_data_diag_log_at = 0.0
            self._last_data_diag_bytes = 0
            self._last_spi_diag_log_at = 0.0
            self._last_spi_diag_signature = None
            self._update_data_diagnostics()
            self.data_serial.start_reader(lambda event: self.event_queue.put(("data", event)))
        except Exception as exc:
            messagebox.showerror("连接失败", str(exc))
            return

        self._log_spi(f"HIF串口已连接 {port} @ {baudrate}")
        self._update_connection_status()

    def toggle_spi_connection(self) -> None:
        if self.spi_service.connection is not None:
            self.spi_service.disconnect()
            self._log_spi("已断开SPI HIF")
            self._update_data_diagnostics()
            self._update_connection_status()
            return

        try:
            index = int(self.spi_index_var.get(), 0)
            clock_code = int(self.spi_clock_var.get(), 0)
            if self.data_serial.connection is not None:
                self.data_serial.disconnect()
                self._log_spi("已断开UART HIF")
            self.spi_service.connect(index=index, clock_code=clock_code)
            self.data_decoder = SerialStreamDecoder()
            self._last_data_diag_log_at = 0.0
            self._last_data_diag_bytes = 0
            self._last_spi_diag_log_at = 0.0
            self._last_spi_diag_signature = None
            self._update_data_diagnostics()
            self.spi_service.start_reader(lambda event: self.event_queue.put(("data", event)))
        except Exception as exc:
            messagebox.showerror("SPI连接失败", str(exc))
            return

        self._log_spi(f"SPI HIF已连接 CH347 index={index} clock_code={clock_code}")
        self._update_connection_status()

    def _update_connection_status(self) -> None:
        control = self.control_serial.connection.port if self.control_serial.connection is not None else "未连接"
        self.connection_var.set(f"控制口 {control}")
        if hasattr(self, "one_click_connect_button"):
            connected = (
                self.control_serial.connection is not None
                or self.data_serial.connection is not None
                or self.spi_service.connection is not None
            )
            self.one_click_connect_button.configure(text="断开设备" if connected else "一键连接")

    def send_manual_command(self) -> None:
        command = self.command_entry.get().strip()
        if command:
            self.send_command(command)
            self.command_entry.delete(0, "end")

    def send_command(self, command: str) -> None:
        try:
            commands = [part.strip() for part in command.splitlines() if part.strip()]
            for item in commands:
                self.control_serial.send_command(item)
        except Exception as exc:
            messagebox.showwarning("发送失败", str(exc))
            return
        for item in commands:
            self._log_control(f"发→ {item}")

    def _drain_events(self) -> None:
        try:
            while True:
                event = self.event_queue.get_nowait()
                source, serial_event = event
                if serial_event.kind == "rx" and isinstance(serial_event.payload, bytes):
                    self._handle_rx(source, serial_event.payload)
                elif serial_event.kind == "status" and source == "data":
                    self._update_data_diagnostics()
                    self._log_spi_diagnostics_if_needed()
                elif serial_event.kind == "error":
                    label = "控制口" if source == "control" else "HIF"
                    if source == "control":
                        self._log_control(f"{label}错误: {serial_event.payload}")
                    else:
                        self._log_spi(f"{label}错误: {serial_event.payload}")
        except queue.Empty:
            pass
        self._expire_pointcloud_if_needed()
        self.after(40, self._drain_events)

    def _handle_rx(self, source: str, data: bytes) -> None:
        decoder = self.control_decoder if source == "control" else self.data_decoder
        lines, packets = decoder.feed(data)
        if source == "data":
            self._update_data_diagnostics()
            self._log_data_diagnostics_if_needed()
        if source == "control":
            for line in lines:
                if line.startswith("HRRAW "):
                    self._handle_heart_raw_line(line)
                else:
                    self._log_control(f"控制收← {line}")
                    self._update_sleep_status(line)
        if packets:
            latest = packets[-1]
            self._handle_pointcloud_packet(latest)

    def _update_data_diagnostics(self) -> None:
        diag = self.data_decoder.diagnostics
        spi_active = self.spi_service.connection is not None
        spi_diag = self.spi_service.diagnostics
        self.data_diag_vars["bytes"].set(str(diag.total_bytes))
        self.data_diag_vars["spi"].set(self._format_spi_diagnostics() if spi_active else "--")
        self.data_diag_vars["magic"].set(str(diag.total_magic_bytes))
        self.data_diag_vars["packets"].set(f"{diag.parsed_packets}/{diag.parsed_points}")
        if diag.last_msg_id is None:
            self.data_diag_vars["last_msg"].set("--")
        else:
            marker = "点云" if diag.last_msg_id == HIF_MSG_ID_OBJECTS else "非点云"
            self.data_diag_vars["last_msg"].set(f"0x{diag.last_msg_id:02X} {marker} len={diag.last_payload_len}")

        parse_parts = []
        if diag.checksum_failures:
            parse_parts.append(f"校验失败 {diag.checksum_failures}")
        if diag.ignored_packets:
            parse_parts.append(f"非点云包 {diag.ignored_packets}")
        if diag.pending_frame_expected:
            parse_parts.append(f"等待 {diag.pending_frame_bytes}/{diag.pending_frame_expected}")
        if spi_active and not diag.total_bytes and spi_diag.timeouts:
            if spi_diag.all_ff_reads:
                parse_parts.append(f"SPI全FF {spi_diag.all_ff_reads}")
            elif spi_diag.all_zero_reads:
                parse_parts.append(f"SPI全00 {spi_diag.all_zero_reads}")
            else:
                parse_parts.append(f"SPI超时 {spi_diag.timeouts}")
        if not parse_parts:
            parse_parts.append(f"缓存 {diag.hif_buffer_len}")
        self.data_diag_vars["parse"].set(" / ".join(parse_parts))
        last_hex = diag.last_chunk_hex
        if not last_hex and spi_active:
            last_hex = spi_diag.last_rx_hex
        self.data_diag_vars["hex"].set(last_hex or "--")

    def _format_spi_diagnostics(self) -> str:
        diag = self.spi_service.diagnostics
        parts = [
            f"poll={diag.poll_attempts}",
            f"timeout={diag.timeouts}",
            f"frame={diag.frames}",
            f"tx={diag.tx_bytes}",
            f"raw={diag.rx_bytes}",
            f"A5={diag.rx_magic_bytes}",
        ]
        if diag.all_ff_reads:
            parts.append(f"FF={diag.all_ff_reads}")
        if diag.all_zero_reads:
            parts.append(f"00={diag.all_zero_reads}")
        if diag.last_error:
            parts.append(f"err={diag.last_error[:36]}")
        return " ".join(parts)

    def _log_spi_diagnostics_if_needed(self) -> None:
        if self.spi_service.connection is None:
            return
        diag = self.spi_service.diagnostics
        signature = (
            diag.poll_attempts,
            diag.timeouts,
            diag.frames,
            diag.tx_bytes,
            diag.rx_bytes,
            diag.rx_magic_bytes,
            diag.all_ff_reads,
            diag.all_zero_reads,
            diag.last_rx_hex,
            diag.last_error,
        )
        if signature == self._last_spi_diag_signature:
            return
        now = time.monotonic()
        if not diag.last_error and now - self._last_spi_diag_log_at < 1.0:
            return
        self._last_spi_diag_signature = signature
        self._last_spi_diag_log_at = now
        self._log_spi(f"SPI状态 {self._format_spi_diagnostics()} last_rx={diag.last_rx_hex or '--'}")

    def _log_data_diagnostics_if_needed(self) -> None:
        diag = self.data_decoder.diagnostics
        if diag.total_bytes == self._last_data_diag_bytes:
            return
        now = time.monotonic()
        should_log = diag.parsed_packets > 0 or now - self._last_data_diag_log_at >= 1.0
        if not should_log:
            return
        self._last_data_diag_log_at = now
        self._last_data_diag_bytes = diag.total_bytes
        self._log_spi(
            "HIF RX "
            f"bytes={diag.total_bytes} A5={diag.total_magic_bytes} "
            f"hif={diag.parsed_packets} points={diag.parsed_points} "
            f"bad_checksum={diag.checksum_failures} hex={diag.last_chunk_hex or '--'}"
        )

    def _update_clock(self) -> None:
        current = datetime.fromtimestamp(self._wall_time())
        self.current_time_var.set(f"当前时间 {current:%Y-%m-%d %H:%M:%S}")
        self.after(1000, self._update_clock)

    def _reset_session_metrics(self, bed_exit_count: int = 0) -> None:
        self._last_session_summary_exit_count = max(int(bed_exit_count), 0)
        self._session_heart_attempts = 0
        self._session_heart_values.clear()
        self.heart_history.clear()
        self._latest_heart_rate = 0
        self._latest_heart_rate_at = None
        self._last_ten_minute_report_bucket = 0
        self._last_monitoring_alerts = ()
        self._session_records.clear()
        self._session_csv_path = None
        self.status_vars["heart_summary"].set("等待有效心率")
        self.status_vars["ten_minute_summary"].set("数据积累中")
        self.status_vars["session_alert"].set("监测中，暂不生成睡眠评分")

    def _update_heart_summary(self) -> None:
        stats = calculate_heart_session_stats(
            self._session_heart_attempts, self._session_heart_values
        )
        if stats.valid_count == 0:
            self.status_vars["heart_summary"].set(
                f"有效0/{stats.attempts} | 覆盖{stats.coverage_percent}%"
            )
            return
        self.status_vars["heart_summary"].set(
            f"均{stats.average_bpm:.1f} | {stats.minimum_bpm}-{stats.maximum_bpm} | "
            f"{stats.valid_count}/{stats.attempts}({stats.coverage_percent}%)"
        )

    def _record_heart_estimate(self, bpm: int | None, timestamp: float) -> None:
        self._session_heart_attempts += 1
        if bpm is not None and bpm > 0:
            self._session_heart_values.append(bpm)
            self.heart_history.append((timestamp, bpm))
            self._latest_heart_rate = bpm
            self._latest_heart_rate_at = timestamp
        cutoff = timestamp - self._sleep_history_window_s
        while self.heart_history and self.heart_history[0][0] < cutoff:
            self.heart_history.popleft()
        self._update_heart_summary()
        self._update_ten_minute_summary(timestamp=timestamp)

    def _update_ten_minute_summary(
        self,
        status=None,
        *,
        timestamp: float | None = None,
        log_if_due: bool = False,
    ) -> None:
        now = self._wall_time() if timestamp is None else timestamp
        session_start = (
            self._session_bed_enter_at.timestamp()
            if self._session_bed_enter_at is not None
            else None
        )
        summary = calculate_ten_minute_summary(
            self.sleep_history,
            self.heart_history,
            now,
            session_start=session_start,
        )
        if summary.record_count == 0:
            self.status_vars["ten_minute_summary"].set("等待在床数据")
            return

        state_text = {
            0: "离床",
            1: "活动",
            2: "安静",
            3: "入睡",
        }.get(summary.sleep_state, "--")
        breath_text = (
            str(round(summary.average_breath_rate))
            if summary.average_breath_rate is not None
            else "--"
        )
        heart_text = (
            str(round(summary.average_heart_rate))
            if summary.average_heart_rate is not None
            else "--"
        )
        text = (
            f"{state_text} | BR{breath_text} HR{heart_text} | 翻{summary.turn_count} | "
            f"大{summary.large_motion_percent}% 小{summary.small_motion_percent}%"
        )
        self.status_vars["ten_minute_summary"].set(text)

        if not log_if_due or status is None or not status.in_bed:
            return
        report_bucket = status.in_bed_frames // TEN_MINUTE_FRAMES
        if report_bucket <= self._last_ten_minute_report_bucket:
            return
        self._last_ten_minute_report_bucket = report_bucket
        self._log_control(f"十分钟综合[{report_bucket * 10}分钟]: {text}")

    def _update_monitoring_alert(self, status, timestamp: float) -> str:
        heart_stats = calculate_heart_session_stats(
            self._session_heart_attempts, self._session_heart_values
        )
        heart_age = (
            max(timestamp - self._latest_heart_rate_at, 0.0)
            if self._latest_heart_rate_at is not None
            else None
        )
        alerts = evaluate_monitoring_alerts(
            in_bed=status.in_bed,
            is_active=status.is_active,
            in_bed_frames=status.in_bed_frames,
            out_bed_frames=status.out_bed_frames,
            had_bed_session=status.bed_enter_count > 0,
            breath_rate=status.breath_rate,
            breath_absent_frames=status.breath_absent_streak,
            breath_output_ratio=status.breath_rate_output_ratio,
            heart_attempts=heart_stats.attempts,
            heart_valid_count=heart_stats.valid_count,
            heart_age_seconds=heart_age,
        )

        if alerts:
            text = "提醒：" + "；".join(alerts)
        elif status.in_bed:
            text = "监测正常，暂不生成睡眠评分"
        elif status.session_end:
            text = session_duration_notice(status.in_bed_frames)
        else:
            text = "等待入床"
        self.status_vars["session_alert"].set(text)

        if alerts != self._last_monitoring_alerts:
            if alerts:
                self._log_control(f"监测提醒: {'；'.join(alerts)}")
            elif self._last_monitoring_alerts:
                self._log_control("监测提醒已恢复")
            self._last_monitoring_alerts = alerts
        return text

    def _record_session_row(
        self,
        status,
        timestamp: float,
        motion_score: int,
        alert_text: str,
    ) -> None:
        is_first_exit_row = (
            bool(self._session_records)
            and status.session_end
            and status.bed_exit_count != self._last_session_summary_exit_count
        )
        if not status.in_bed and not is_first_exit_row:
            return

        current = datetime.fromtimestamp(timestamp)
        if self._session_csv_path is None:
            self._session_csv_path = self.log_dir / (
                f"mrs6130_session_{current:%Y%m%d_%H%M%S}.csv"
            )
        heart_age = (
            round(max(timestamp - self._latest_heart_rate_at, 0.0), 1)
            if self._latest_heart_rate_at is not None
            else -1
        )
        self._session_records.append(
            {
                "time": current.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3],
                "elapsed_s": round(max(status.in_bed_frames, 0) / 5, 1),
                "P": int(status.has_person),
                "A": int(status.is_active),
                "BED": int(status.in_bed),
                "SS": status.sleep_state,
                "BV": int(status.breath_valid),
                "BR": status.breath_rate,
                "HR": self._latest_heart_rate,
                "HR_age_s": heart_age,
                "MI": status.motion_intensity,
                "motion_score": motion_score,
                "TE": int(status.turn_event),
                "TC": status.turn_count,
                "STC": status.sleep_turn_count,
                "MT": status.motion_points_total,
                "MB": status.motion_points_bed,
                "McT": status.micro_points_total,
                "McB": status.micro_points_bed,
                "Act": status.active_motion_points,
                "BVR": status.breath_valid_ratio,
                "RVR": status.breath_rate_output_ratio,
                "alerts": alert_text if alert_text.startswith("提醒") else "",
            }
        )

    def export_session_csv(self, show_message: bool = True) -> Path | None:
        if not self._session_records:
            if show_message:
                messagebox.showwarning("导出CSV", "当前还没有在床会话数据")
            return None
        if self._session_csv_path is None:
            current = self._session_bed_enter_at or datetime.now()
            self._session_csv_path = self.log_dir / (
                f"mrs6130_session_{current:%Y%m%d_%H%M%S}.csv"
            )
        try:
            path = write_session_csv(self._session_csv_path, self._session_records)
        except (OSError, ValueError) as exc:
            self._log_control(f"会话CSV导出失败: {exc}")
            if show_message:
                messagebox.showerror("导出CSV失败", str(exc))
            return None
        self._log_control(f"会话CSV: {path}")
        if show_message:
            messagebox.showinfo("导出CSV", f"已保存到：\n{path}")
        return path

    def _update_session_event_times(self, status, current: datetime) -> None:
        if status.in_bed and not self._last_status_in_bed:
            self._session_bed_enter_at = current
            self._session_first_sleep_at = None
            self._session_last_sleep_exit_at = None
            self._session_bed_exit_at = None

        if status.in_bed and status.sleep_state == 3 and self._last_status_sleep_state != 3:
            if self._session_first_sleep_at is None:
                self._session_first_sleep_at = current

        if self._last_status_sleep_state == 3 and status.sleep_state != 3:
            self._session_last_sleep_exit_at = current

        if not status.in_bed and self._last_status_in_bed:
            self._session_bed_exit_at = current

        self._last_status_in_bed = status.in_bed
        self._last_status_sleep_state = status.sleep_state
        self.status_vars["event_times"].set(
            f"入床 {format_clock_time(self._session_bed_enter_at)} | "
            f"入睡 {format_clock_time(self._session_first_sleep_at)} | "
            f"醒来 {format_clock_time(self._session_last_sleep_exit_at)} | "
            f"离床 {format_clock_time(self._session_bed_exit_at)}"
        )

    def _update_sleep_status(self, line: str) -> None:
        status = parse_sleep_line(line)
        if status is None:
            return
        current_epoch = self._wall_time()
        current = datetime.fromtimestamp(current_epoch)
        if status.in_bed and not self._last_status_in_bed:
            self._reset_session_metrics(status.bed_exit_count)
        self.latest_data_time_var.set(f"最新数据 {current:%H:%M:%S}")
        self._update_session_event_times(status, current)
        self.status_vars["person"].set("有人" if status.has_person else "无人")
        self.status_vars["active"].set("在动" if status.is_active else "安静")
        self.status_vars["bed"].set("在床" if status.in_bed else "离床")
        if not status.breath_valid:
            breath_text = "无效"
        elif status.breath_period_frames <= 0:
            breath_text = "估计中"
        else:
            breath_text = "有效"
        breath_state_text = {
            0: "暂无分类",
            1: "偏慢",
            2: "常见区间",
            3: "偏快",
        }.get(status.breath_state, f"未知{status.breath_state}")
        if status.breath_state > 0:
            breath_text = f"{breath_text}·{breath_state_text}"
        self.status_vars["breath"].set(breath_text)
        self.status_vars["breath_rate"].set(str(status.breath_rate) if status.breath_rate > 0 else "--")
        if status.breath_rate > 0:
            self._latest_breath_rate = status.breath_rate
        if not status.in_bed:
            self._latest_breath_rate = 0
            self._latest_heart_rate = 0
            self._latest_heart_rate_at = None
            self.heart_rate_estimator.reset()
            self.status_vars["heart_rate"].set("等待安静数据")
        sleep_state_text = {
            0: "离床",
            1: "在床活动",
            2: "在床安静",
            3: "已入睡",
        }.get(status.sleep_state, f"未知{status.sleep_state}")
        self.status_vars["sleep_state"].set(sleep_state_text)
        self.status_vars["motion"].set(f"{status.motion_points_total}/{status.motion_points_bed}")
        self.status_vars["micro"].set(f"{status.micro_points_total}/{status.micro_points_bed}")
        self.status_vars["active_points"].set(str(status.active_motion_points))
        motion_level_text = {
            0: "安静",
            1: "轻微",
            2: "明显",
            3: "大动作",
        }.get(status.motion_intensity, str(status.motion_intensity))
        motion_score = calculate_motion_score(
            status.motion_intensity, status.active_motion_points
        )
        self.status_vars["motion_level"].set(
            f"{motion_level_text} | {motion_score}/100"
        )
        self.status_vars["turn"].set(f"睡眠中累计 {status.sleep_turn_count}")
        self.status_vars["session"].set(
            f"入{status.bed_enter_count}/离{status.bed_exit_count}  "
            f"床{format_frames_short(status.in_bed_frames)}  离{format_frames_short(status.out_bed_frames)}"
        )
        self.status_vars["state_duration"].set(
            f"动 {format_frames_short(status.active_frames)} / 静 {format_frames_short(status.quiet_frames)} / "
            f"睡 {format_frames_short(status.likely_sleep_frames)}"
        )
        self.status_vars["state_ratio"].set(
            f"动 {status.active_ratio}% / 静 {status.quiet_ratio}% / 睡 {status.likely_sleep_ratio}%"
        )
        self.status_vars["longest_quiet"].set(format_frames_short(status.longest_quiet_frames))
        onset_text = (
            format_frames_short(status.sleep_onset_frames)
            if status.sleep_onset_frames > 0
            else "--"
        )
        longest_sleep_text = (
            format_frames_short(status.longest_likely_sleep_frames)
            if status.longest_likely_sleep_frames > 0
            else "--"
        )
        self.status_vars["sleep_process"].set(
            f"入床→入睡 {onset_text} | {status.sleep_segment_count}段 | "
            f"中断 {status.sleep_interruption_count} | 最长 {longest_sleep_text}"
        )
        self.status_vars["streak"].set(
            f"P {status.person_present_streak}/{status.person_absent_streak}  "
            f"B {status.bed_present_streak}/{status.bed_absent_streak}  "
            f"S {status.sleep_quiet_streak}/{status.sleep_breath_streak}"
        )
        self.status_vars["breath_debug"].set(
            f"Bsig={status.breath_signal}  Bq={status.breath_quality}  Bper={status.breath_period_frames}  "
            f"Brc={status.breath_rate_candidate}  Brr={status.breath_rate_repeat_count}  Brs={status.breath_rate_stable}"
        )
        self.status_vars["breath_quality"].set(
            f"bin {status.breath_target_bin} / Bpq {status.breath_phase_quality} / Bq {status.breath_quality}"
        )
        self.status_vars["breath_period"].set(
            f"Bph {status.breath_phase_mrad} / Bpd {status.breath_phase_delta_mrad} / "
            f"{status.breath_period_frames}帧"
            if status.breath_period_frames > 0
            else f"Bph {status.breath_phase_mrad} / Bpd {status.breath_phase_delta_mrad}"
        )
        self.status_vars["breath_candidate"].set(
            f"Brc {status.breath_rate_candidate} / Brs {status.breath_rate_stable} / amp {status.breath_phase_amp}"
        )
        self.status_vars["breath_summary"].set(
            f"{breath_state_text} | 信号{status.breath_valid_ratio}% | "
            f"数值{status.breath_rate_output_ratio}% | "
            f"均{status.breath_rate_avg if status.breath_rate_avg > 0 else '--'} | "
            f"{status.breath_rate_min if status.breath_rate_min > 0 else '--'}-"
            f"{status.breath_rate_max if status.breath_rate_max > 0 else '--'}"
        )
        self._record_sleep_history(status, current_epoch)
        self._update_ten_minute_summary(
            status,
            timestamp=current_epoch,
            log_if_due=True,
        )
        alert_text = self._update_monitoring_alert(status, current_epoch)
        self._record_session_row(status, current_epoch, motion_score, alert_text)

        if status.session_end and status.bed_exit_count != self._last_session_summary_exit_count:
            self._last_session_summary_exit_count = status.bed_exit_count
            self.export_session_csv(show_message=False)
            if status.in_bed_frames < MIN_SESSION_SUMMARY_FRAMES:
                return
            heart_stats = calculate_heart_session_stats(
                self._session_heart_attempts, self._session_heart_values
            )
            heart_average = (
                f"{heart_stats.average_bpm:.1f}" if heart_stats.average_bpm is not None else "--"
            )
            heart_range = (
                f"{heart_stats.minimum_bpm}-{heart_stats.maximum_bpm}"
                if heart_stats.minimum_bpm is not None
                else "--"
            )
            self._log_control(
                "会话总结: "
                f"在床{format_frames(status.in_bed_frames)}，"
                f"活动{format_frames(status.active_frames)}({status.active_ratio}%)，"
                f"安静{format_frames(status.quiet_frames)}({status.quiet_ratio}%)，"
                f"估计睡眠{format_frames(status.likely_sleep_frames)}({status.likely_sleep_ratio}%)，"
                f"最长连续安静{format_frames(status.longest_quiet_frames)}，"
                f"入床至估计入睡{format_frames(status.sleep_onset_frames) if status.sleep_onset_frames > 0 else '未检测到'}，"
                f"估计睡眠{status.sleep_segment_count}段，"
                f"最长连续估计睡眠{format_frames(status.longest_likely_sleep_frames)}，"
                f"睡眠中断{status.sleep_interruption_count}次，"
                f"睡眠中翻身{status.sleep_turn_count}次，"
                f"入床时间{format_clock_time(self._session_bed_enter_at)}，"
                f"首次入睡{format_clock_time(self._session_first_sleep_at)}，"
                f"最后醒来{format_clock_time(self._session_last_sleep_exit_at)}，"
                f"离床时间{format_clock_time(self._session_bed_exit_at)}，"
                f"呼吸信号有效率{status.breath_valid_ratio}%，"
                f"呼吸率输出覆盖率{status.breath_rate_output_ratio}%，"
                f"呼吸率均值{status.breath_rate_avg if status.breath_rate_avg > 0 else '--'}，"
                f"范围{status.breath_rate_min if status.breath_rate_min > 0 else '--'}-"
                f"{status.breath_rate_max if status.breath_rate_max > 0 else '--'}，"
                f"心率输出覆盖率{heart_stats.coverage_percent}%，"
                f"心率均值{heart_average}，"
                f"心率范围{heart_range}，"
                f"会话提示{session_duration_notice(status.in_bed_frames)}"
            )

    def _record_sleep_history(self, status, timestamp: float | None = None) -> None:
        now = self._wall_time() if timestamp is None else timestamp
        self.sleep_history.append(
            (
                now,
                1 if status.has_person else 0,
                1 if status.in_bed else 0,
                1 if status.is_active else 0,
                status.sleep_state,
                status.breath_rate,
                status.sleep_turn_count,
                status.motion_intensity,
            )
        )
        cutoff = now - self._sleep_history_window_s
        while self.sleep_history and self.sleep_history[0][0] < cutoff:
            self.sleep_history.popleft()
        self._draw_sleep_trend()

    def _draw_sleep_trend(self) -> None:
        if not hasattr(self, "sleep_trend_canvas"):
            return

        canvas = self.sleep_trend_canvas
        canvas.delete("all")
        width = max(canvas.winfo_width(), 1)
        height = max(canvas.winfo_height(), 190)
        left = 48
        right = max(width - 12, left + 1)
        canvas.create_rectangle(0, 0, width, height, fill="#ffffff", outline="")

        rail_rows = (("P", 20, "#16a34a"), ("BED", 30, "#2563eb"), ("A", 40, "#dc2626"))
        for label, y_coord, _color in rail_rows:
            canvas.create_text(left - 8, y_coord, text=label, anchor="e", fill="#475569", font=("Consolas", 8))
            canvas.create_line(left, y_coord, right, y_coord, fill="#e5e7eb")

        state_y = {3: 58, 2: 74, 1: 90, 0: 106}
        state_color = {3: "#2563eb", 2: "#16a34a", 1: "#dc2626", 0: "#94a3b8"}
        for state, label in ((3, "睡"), (2, "静"), (1, "动"), (0, "离")):
            y_coord = state_y[state]
            canvas.create_text(left - 8, y_coord, text=label, anchor="e", fill="#475569", font=("Microsoft YaHei UI", 8))
            canvas.create_line(left, y_coord, right, y_coord, fill="#f1f5f9")

        breath_top = 124
        breath_bottom = 170
        canvas.create_text(left - 8, breath_top, text="30", anchor="e", fill="#475569", font=("Consolas", 8))
        canvas.create_text(left - 8, breath_bottom, text="BR 0", anchor="e", fill="#475569", font=("Consolas", 8))
        canvas.create_line(left, breath_top, right, breath_top, fill="#f1f5f9")
        canvas.create_line(left, breath_bottom, right, breath_bottom, fill="#e5e7eb")

        end_time = self.sleep_history[-1][0] if self.sleep_history else self._wall_time()
        start_time = end_time - self._sleep_history_window_s
        history = [item for item in self.sleep_history if item[0] >= start_time]
        for index in range(5):
            ratio = index / 4
            timestamp = start_time + ratio * self._sleep_history_window_s
            x_position = left + ratio * (right - left)
            anchor = "sw" if index == 0 else ("se" if index == 4 else "s")
            canvas.create_line(x_position, 14, x_position, breath_bottom, fill="#f8fafc")
            canvas.create_text(
                x_position,
                height - 5,
                text=datetime.fromtimestamp(timestamp).strftime("%H:%M"),
                anchor=anchor,
                fill="#64748b",
                font=("Consolas", 8),
            )

        if len(history) < 2:
            canvas.create_text(
                (left + right) / 2,
                (state_y[0] + breath_top) / 2,
                text="等待趋势数据",
                fill="#94a3b8",
                font=("Microsoft YaHei UI", 10),
            )
            return

        def x_coord(timestamp: float) -> float:
            ratio = (timestamp - start_time) / self._sleep_history_window_s
            return left + max(0.0, min(1.0, ratio)) * (right - left)

        for previous, current in zip(history, history[1:]):
            previous_x = x_coord(previous[0])
            current_x = x_coord(current[0])
            for index, (_label, y_coord, color) in enumerate(rail_rows, start=1):
                if current[index]:
                    canvas.create_line(previous_x, y_coord, current_x, y_coord, fill=color, width=3)

            previous_state = previous[4] if previous[4] in state_y else 0
            current_state = current[4] if current[4] in state_y else 0
            canvas.create_line(
                previous_x,
                state_y[previous_state],
                current_x,
                state_y[current_state],
                fill=state_color[current_state],
                width=2,
            )

            previous_rate = previous[5]
            current_rate = current[5]
            if previous_rate > 0 and current_rate > 0:
                previous_br_y = breath_bottom - min(previous_rate, 30) / 30 * (breath_bottom - breath_top)
                current_br_y = breath_bottom - min(current_rate, 30) / 30 * (breath_bottom - breath_top)
                canvas.create_line(
                    previous_x,
                    previous_br_y,
                    current_x,
                    current_br_y,
                    fill="#0891b2",
                    width=2,
                )

    def _draw_points(self) -> None:
        if not hasattr(self, "canvas"):
            return
        canvas = self.canvas
        canvas.delete("all")
        width = max(canvas.winfo_width(), 1)
        height = max(canvas.winfo_height(), 1)
        margin = 44
        cx = width / 2
        cy = height - margin
        scale = min((width - 2 * margin) / 240, (height - 2 * margin) / 260)
        mode = self.display_mode_var.get()
        now = self._monotonic()
        self._trim_scene_points(now)

        canvas.create_rectangle(0, 0, width, height, fill="#ffffff", outline="")
        canvas.create_text(
            margin,
            18,
            text="俯视图: X横向 / Y距离",
            anchor="w",
            fill="#334155",
            font=("Microsoft YaHei UI", 9, "bold"),
        )
        canvas.create_line(margin, cy, width - margin, cy, fill="#cbd5e1", arrow="last")
        canvas.create_line(cx, cy, cx, margin, fill="#cbd5e1", arrow="last")
        canvas.create_text(cx, margin - 16, text="前方 +Y", anchor="center", fill="#334155", font=("Microsoft YaHei UI", 9))
        canvas.create_text(margin, cy + 16, text="左 -X", anchor="w", fill="#64748b", font=("Microsoft YaHei UI", 9))
        canvas.create_text(width - margin, cy + 16, text="右 +X", anchor="e", fill="#64748b", font=("Microsoft YaHei UI", 9))
        canvas.create_rectangle(cx - 20, cy - 8, cx + 20, cy + 8, fill="#111827", outline="")
        canvas.create_polygon(cx - 10, cy - 8, cx + 10, cy - 8, cx, cy - 20, fill="#111827", outline="")
        canvas.create_text(cx, cy + 24, text="雷达", anchor="center", fill="#111827", font=("Microsoft YaHei UI", 9, "bold"))
        if mode == DISPLAY_MODE_SCENE:
            canvas.create_oval(width - margin - 78, 14, width - margin - 68, 24, fill=SCENE_POINT_FILL, outline="")
            canvas.create_text(width - margin - 62, 19, text="背景点", anchor="w", fill="#475569", font=("Microsoft YaHei UI", 8))
        elif mode == DISPLAY_MODE_MIXED:
            canvas.create_oval(width - margin - 78, 14, width - margin - 68, 24, fill=SCENE_POINT_FILL, outline="")
            canvas.create_text(width - margin - 62, 19, text="背景点", anchor="w", fill="#475569", font=("Microsoft YaHei UI", 8))
            canvas.create_oval(width - margin - 78, 32, width - margin - 68, 42, fill=MOTION_POINT_FILL, outline="")
            canvas.create_text(width - margin - 62, 37, text="运动点", anchor="w", fill="#475569", font=("Microsoft YaHei UI", 8))
        else:
            canvas.create_oval(width - margin - 78, 14, width - margin - 68, 24, fill=MOTION_POINT_FILL, outline="")
            canvas.create_text(width - margin - 62, 19, text="运动点", anchor="w", fill="#475569", font=("Microsoft YaHei UI", 8))
        for y_cm in (50, 100, 150, 200):
            y = cy - y_cm * scale
            canvas.create_line(margin, y, width - margin, y, fill="#edf2f7")
            canvas.create_text(margin + 4, y - 8, text=f"{y_cm}cm", anchor="w", fill="#64748b", font=("Consolas", 8))

        if mode in (DISPLAY_MODE_SCENE, DISPLAY_MODE_MIXED):
            for _timestamp, point in self.scene_points:
                self._draw_xy_point(canvas, point, cx, cy, scale, SCENE_POINT_FILL, 2)

        motion_points = [point for point in self.latest_points if point.kind == "motion"]
        show_motion = mode in (DISPLAY_MODE_MOTION, DISPLAY_MODE_MIXED)
        if show_motion:
            for point in motion_points[-300:]:
                self._draw_xy_point(canvas, point, cx, cy, scale, MOTION_POINT_FILL, 4)

        scene_visible = mode in (DISPLAY_MODE_SCENE, DISPLAY_MODE_MIXED) and bool(self.scene_points)
        motion_visible = show_motion and bool(motion_points)
        if scene_visible or motion_visible:
            return

        waiting = "等待背景点" if mode == DISPLAY_MODE_SCENE else "等待运动点"
        canvas.create_text(cx, height / 2, text=waiting, fill="#64748b", font=("Microsoft YaHei UI", 12))

    def _draw_xy_point(
        self,
        canvas: tk.Canvas,
        point: Point,
        cx: float,
        cy: float,
        scale: float,
        fill: str,
        radius: int,
    ) -> None:
        if point.x_cm is None or point.y_cm is None:
            return
        x = cx + point.x_cm * scale
        y = cy - point.y_cm * scale
        canvas.create_oval(x - radius, y - radius, x + radius, y + radius, fill=fill, outline="")

    def _handle_pointcloud_packet(self, packet) -> None:
        if not hasattr(self, "canvas"):
            return
        now = self._monotonic()
        if packet.points:
            self._remember_scene_points(packet.points, now)
            self.latest_points = packet.points
            self._last_pointcloud_at = now
            self._draw_points()
        elif self.latest_points:
            if now - self._last_pointcloud_at > self._pointcloud_hold_s:
                self.latest_points = []
                self._draw_points()
            return
        else:
            self._draw_points()
            return

        self._log_spi(f"数据HIF 0xC3 frame={packet.frame_index} points={len(packet.points)}")

    def _remember_scene_points(self, points: list[Point], timestamp: float) -> None:
        for point in points:
            if point.kind == "motion":
                continue
            if point.x_cm is None or point.y_cm is None:
                continue
            self.scene_points.append((timestamp, point))
        self._trim_scene_points(timestamp)

    def _trim_scene_points(self, now: float) -> None:
        cutoff = now - self._scene_hold_s
        while self.scene_points and self.scene_points[0][0] < cutoff:
            self.scene_points.popleft()

    def _expire_pointcloud_if_needed(self) -> None:
        if not hasattr(self, "canvas"):
            return
        self._trim_scene_points(self._monotonic())
        if not self.latest_points:
            return
        if self._monotonic() - self._last_pointcloud_at <= self._pointcloud_hold_s:
            return
        self.latest_points = []
        self._draw_points()

    def _log_control(self, line: str) -> None:
        self._write_session_log("CONTROL", line)
        self._append_log(self.control_log_lines, self.control_log_text, line)

    def _handle_heart_raw_line(self, line: str) -> None:
        self._heart_raw_sample_count += 1
        self._write_session_log("HRRAW", line)
        sample = self.heart_rate_estimator.add_line(line, self._monotonic())
        fields = {}
        for item in line.split()[1:]:
            if "=" not in item:
                continue
            key, value = item.split("=", 1)
            fields[key] = value
        valid_text = "有效" if fields.get("V") == "1" else "无效"
        sample_rate = fields.get("FS", "5")
        bin_count = fields.get("N", "1")
        self.status_vars["heart_raw"].set(
            f"{valid_text} | 约{sample_rate}Hz | 已存{self._heart_raw_sample_count}帧 | "
            f"bin {fields.get('BIN', '--')} x{bin_count} / ant {fields.get('ANT', '--')}"
        )
        if sample is None:
            return
        estimate = self.heart_rate_estimator.maybe_estimate(self._latest_breath_rate, sample.timestamp)
        if estimate is None:
            return
        if sample.in_bed:
            self._record_heart_estimate(estimate.bpm, self._wall_time())
        if estimate.bpm is None:
            self.status_vars["heart_rate"].set(estimate.reason)
        else:
            if "确认中" in estimate.reason:
                prefix = "保持"
            elif estimate.harmonic_conflict:
                prefix = "保持" if "保持" in estimate.reason or "确认中" in estimate.reason else "谐波冲突候选"
            else:
                prefix = "候选" if estimate.quality == "低" else "估计"
            bin_text = "/".join(str(value) for value in estimate.target_bins)
            self.status_vars["heart_rate"].set(
                f"{prefix} {estimate.bpm} 次/分 · {estimate.quality} | "
                f"bin {bin_text} / {estimate.antenna_support}路"
            )
        self._write_session_log(
            "HR_EST",
            f"HR={estimate.bpm or 0} Q={estimate.quality} REASON={estimate.reason} "
            f"BIN={estimate.target_bin if estimate.target_bin is not None else -1} "
            f"BINS={','.join(str(value) for value in estimate.target_bins) or '-'} "
            f"SHARE={estimate.bin_share:.2f} ANT={estimate.antenna_support} "
            f"DUR={estimate.duration_s:.1f} BR={estimate.breath_rate} "
            f"H3={estimate.harmonic_bpm} HCON={1 if estimate.harmonic_conflict else 0} "
            f"HORD={estimate.harmonic_order}",
        )

    def _log_spi(self, line: str) -> None:
        self._write_session_log("SPI", line)
        if hasattr(self, "spi_log_text"):
            self._append_log(self.spi_log_lines, self.spi_log_text, line)
        else:
            self.spi_log_lines.append(line)

    def _write_session_log(self, channel: str, line: str) -> None:
        if self._log_file is None:
            return
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        try:
            self._log_file.write(f"[{timestamp}] {channel} {line}\n")
        except OSError:
            self._log_file = None

    def _append_log(self, lines: deque[str], text: tk.Text, line: str) -> None:
        lines.append(line)
        text.configure(state="normal")
        text.delete("1.0", "end")
        text.insert("end", "\n".join(lines))
        text.insert("end", "\n")
        text.see("end")
        text.configure(state="disabled")

    def destroy(self) -> None:
        self.control_serial.disconnect()
        self.data_serial.disconnect()
        self.spi_service.disconnect()
        if self._log_file is not None:
            try:
                self._log_file.write(f"==== closed {datetime.now():%Y-%m-%d %H:%M:%S} ====\n")
                self._log_file.close()
            except OSError:
                pass
            self._log_file = None
        super().destroy()


def main() -> None:
    app = HostApp()
    app.mainloop()


if __name__ == "__main__":
    main()
