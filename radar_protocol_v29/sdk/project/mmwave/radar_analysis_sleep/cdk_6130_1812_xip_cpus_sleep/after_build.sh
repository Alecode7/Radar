#!/bin/bash
set -e
build_dir="$(cd "$(dirname "$0")" && pwd)"
task_dir="$(cd "$build_dir/../../../../.." && pwd)"
export SDK_ROOT_DIR="$task_dir/sdk"
export CURRENT_PRJ_PATH="$build_dir"
toolchain='D:/C-Sky/CDKRepo/Toolchain/XTGccElfNewlib/V3.1.0/R/bin'
"$toolchain/riscv64-unknown-elf-nm.exe" -S "$build_dir/Obj/psdf.elf" > "$build_dir/Obj/psdf.symbols"
'D:/MRS6130/RS6x_7x_mmWave_sdk_V2.0.6/Software_Kit/02_SDK/psdf_sdk/tools/python/WPy64-31321b1/python/python.exe' -B "$task_dir/package_safe.py"
