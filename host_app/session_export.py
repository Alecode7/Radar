from __future__ import annotations

import csv
from io import StringIO
from pathlib import Path
from typing import Iterable, Mapping


SESSION_CSV_FIELDS = (
    "time",
    "elapsed_s",
    "P",
    "A",
    "BED",
    "SS",
    "BV",
    "BR",
    "HR",
    "HR_age_s",
    "MI",
    "motion_score",
    "TE",
    "TC",
    "STC",
    "MT",
    "MB",
    "McT",
    "McB",
    "Act",
    "BVR",
    "RVR",
    "alerts",
)


def render_session_csv(records: Iterable[Mapping[str, object]]) -> str:
    output = StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=SESSION_CSV_FIELDS)
    writer.writeheader()
    for record in records:
        writer.writerow({field: record.get(field, "") for field in SESSION_CSV_FIELDS})
    return output.getvalue()


def write_session_csv(
    path: Path, records: Iterable[Mapping[str, object]]
) -> Path:
    rows = list(records)
    if not rows:
        raise ValueError("没有可导出的会话数据")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render_session_csv(rows), encoding="utf-8-sig", newline="")
    return path
