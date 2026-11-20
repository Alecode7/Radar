"""Read the latest saved L1/Q1 file offline, and export inside this project."""
from pathlib import Path
import sys
from inspect_capture import main as inspect_main
ROOT=Path(__file__).resolve().parents[1]
def main():
    candidates=list((ROOT.parent/'w803_radar_link_l1/board-tests').rglob('*.bin'))
    if not candidates:print('尚未找到已保存的链路BIN记录。');return 1
    latest=max(candidates,key=lambda p:p.stat().st_mtime_ns)
    sys.argv=['inspect_capture.py',str(latest),'--out',str(ROOT/'inspection-results')]
    print('仅分析已保存文件：',latest)
    return inspect_main()
if __name__=='__main__':sys.exit(main())
