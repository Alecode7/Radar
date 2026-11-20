# RS6130 V28：采集与呼吸保留，心率交给 W803

2026-11-19。本版基于实际板上 V27 对应的冻结分支，独立放在 `D:\MRS6130\radar_acquisition_v28`，配套 W803 心率 R1 V1.0.0。原 SDK 工作目录仍是较早 V24，未将它当作 V27 直接修改。

**编译成功，已生成镜像；尚未烧录或实板验证。**

烧录文件：`D:\MRS6130\radar_acquisition_v28\release\RS6130_V28_IQ_STREAM_20261119.img`。

## 分工与修改

| 芯片 | 本次正式整理后的职责 |
| --- | --- |
| RS6130雷达 | 原5Hz射频采集、原三天线IQ选格、人体/在床/活动、呼吸与睡眠状态处理，发送SLEEP、HRRAW和带源时间的HRIN |
| W803主控 | R1双精度ROTATE16心率计算、原质量与确认规则、结果与性能日志 |
| 电脑 | 发送启动/停止、录制与离线分析 |

V27的 `v6_heart_device.c` 同时承担心率计算和HRIN输出。直接禁用整个模块会丢失W803输入，因此本版保留旧API，替换为只输出IQ的轻量适配器。

- `app/src/v6_heart_device.c`：不创建旧32项心率队列，不创建8192字节栈的心率任务，不申请14648字节频谱工作区；移除旧心率确认/计算/发布，只保留源时钟、代数、序号、呼吸上下文、采样发送和限频诊断。
- `app/src/main.c`：启动声明 `MODE=IQ_STREAM HEART_COMPUTE=0 OWNER=W803`，时钟失败不报READY；版本改为 `UART_RADAR_IQ_20261119_V28`。
- `build-safe.mk` 与CDK工程：不再链接心率核心。旧核心文件仍作为来源资料留在副本中，不能据其存在判断该计算仍在执行。
- `mmw_report.c`、`sleep_detect.c/.h`、`mmw_shell.c`、采样时钟、板级连线、天线校准加载与保存、射频和默认检测区域均与V27源码一致；正常采样与选格路径未改。

采集API保留旧 `v6_heart_*` 名称，以便已有回调与mmwc start/stop路径继续工作。实际功能由V28启动标识和链接证据确认。

## 数据帧兼容

示例帧来自本轮主机适配器检查，不是新增实板采集：

```text
HRIN V=27 N=1 GEN=4 F=1 MS=10000 BIN=13 FLAGS=3 BR=22 I0=-32768 Q0=32767 I1=0 Q1=-1 I2=1234 Q2=-2345 ENQ=0
```

- `V=27` 保留原数据格式编号；固件版本通过 `FW_VERIFY_VERSION=...V28` 区分。现有R1解析器只接受V25/26/27数据格式，故本次不改成HRIN V=28。
- `MS` 仍是雷达源时钟的毫秒值，`F`为雷达帧号，`N`为本次上电累计输出序号。每次开始/停止/复位与离床失效按原调用顺序更新GEN，防止旧历史混用。
- `BIN`、六路有符号16位IQ、有效/在床/活动FLAGS和呼吸参考BR由原V27调用方提供。有效且在床才调用此发送路径的条件保留。
- `ENQ=0`表示不再投入雷达本地的心率计算队列，不代表串口失败，也不代表W803入队失败。R1原解析器已能接收，且不以它作为信号质量门槛。
- 雷达不再发布旧HRDEV/HREST心率结果；心率看W803的 `HEART ORIGIN=W803 ID=R1 MODE=1`。

在床时正呼吸值继续按原SLEEP观察顺序保持，离床清除。默认状态诊断每5秒至多一条HRSTREAM，不改源采样节奏；W803 R1默认仍过滤完整原始转发。

## 烧录与操作

本版是雷达的 `.img`，使用此前成功烧录RS6130的工具与通道；桌面的W803图形烧录入口接受 `.fls`，不能用于此 `.img`。本次没有自动连接串口、复位、烧录或写校准。

两芯片的程序分别更新：

| 对象 | 文件 |
| --- | --- |
| RS6130 | `D:\MRS6130\radar_acquisition_v28\release\RS6130_V28_IQ_STREAM_20261119.img` |
| W803 | `D:\MRS6130\w803_live_heart_r1\release\W803_HEART_R1_V1.0.0_ROTATE16_20261119.fls` |

如果W803已经烧入R1，只需再更新雷达V28。接线沿用现有方案。正常运行时PA0断地绝缘；先保持白F接地启动W803，确认R1 READY，再释放白F启动雷达，开启文件日志并发送一次 `link start`。坐稳约10分钟，结束发 `link stop`。无需中途操作或发compare。

启动应见：

```text
HRFW_INIT RET=0 MODE=IQ_STREAM HEART_COMPUTE=0 OWNER=W803 FS=5 CORE_BYTES=0 WS_BYTES=0
SLEEP_FW_READY p1812 SAFEHR5 MICROIQ NOPOINTTX
FW_VERIFY_VERSION=UART_RADAR_IQ_20261119_V28
```

如需基线比较，可先用R1+原V27记录一轮，再改为R1+V28并尽量保持相同摆位。回退镜像保存在 `reference\RS6130_V27_rollback.img`，SHA256与原V27发布记录核对一致。

## 已完成的检查

- 目标编译与打包成功；镜像282448字节，生成时间2026-11-19 20:22:16（本机时间）。
- 最终ELF没有 `hr_core_*` 符号。采集适配器目标对象也没有队列创建、任务创建或堆分配依赖；旧对象即使留在构建目录，也不进入本次链接。
- SRAM映射260352/301056字节；原V27是294784字节，减少34432字节。映射包含系统堆预留，不能把映射差额当作实测运行空闲堆。取消工作区/任务/队列还会影响运行堆，实际值待板测。
- 12项主机检查通过，覆盖时钟失败、等待显式启动、起停、离床代数、正呼吸保持、时间回绕、IQ边界和诊断限频。
- 三组旧实录共4665帧，经真实V28适配器重新发送后，未修改的R1解析器全部接收；采样时间、帧号、距离格、FLAGS、BR及六个IQ值无差异。GEN/N按新流的生命周期重新产生，ENQ固定为0，不宣称这三个字段与旧计算队列记录相同。
- 原V27冻结SDK全部文件哈希复核不变；来源清单及本版完整性报告保存在 `baseline-v27-hashes.json`、`verification.json` 和 `release-manifest.json`。

以上是软件与编译检查。新板端吞吐、源时间间隔、心率质量及恢复尚待测试；不能把W803此前单快照2.132秒或这次4665帧兼容检查当作本版实板性能。

## 工程位置与构建

CDK工作区：`sdk\project\mmwave\radar_analysis_sleep\radar_sleep_llx\radar_sleep_llx.cdkws`。工程和源码已同步，本轮实际验证使用下面的构建脚本，未另外操作CDK界面。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 'D:\MRS6130\radar_acquisition_v28\build_radar.ps1'
```

脚本沿用本地P1812工具链，完整重建可重建源，保留原已冻结的 `Obj/src_hal_board.o` 与 `Obj/utils_version.o`，使用不删除文件的打包流程。源码包包含这两个基线对象、对象列表和SDK源码/库；不重复包含本机Python与工具链安装。

`package_safe.py` 将打包中要移除的单个中间文件逐一保存到 `packaging-preserved`，不执行批量删除或递归清理。正常SDK工作目录和原V27均未被覆盖。
