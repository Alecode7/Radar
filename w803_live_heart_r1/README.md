# W803 心率 R1 · V1.0.0

2026-11-19，工程正式整理版。默认采用 ROTATE16 双精度频率递推；保留 DIRECT 回退。独立于 C1—C5，旧工程、旧镜像和实测证据保留。

推荐烧录文件：`D:\MRS6130\w803_live_heart_r1\release\W803_HEART_R1_V1.0.0_ROTATE16_20261119.fls`。

启动标识：`W803_HEART_R1_V1.0.0_ROTATE16_20261119`。本版完成编译、镜像校验和离线回归；递推单模式连续实板验收与同期参考准确率仍待验证。

## 使用方法

1. 打开桌面已有的“W803图形烧录”，点击“选择固件…”，选择上面的完整 `.fls` 文件，按之前已成功的 PA0/黄G 下载步骤烧录，等 `flash device complete`。
2. 烧录后将 PA0 与地完全断开并绝缘。保持白F接地，按已有流程复位 W803；串口监视使用实际 W803 端口，当前为 COM9，115200、8N1。
3. 开启 **Toggle File Logging**，确认 `BOOT` 标识是 R1、`DEFAULT_MODE=ROTATE16`，并看到 `READY ... ID=R1`。释放白F，让雷达开机，等雷达启动信息。
4. 发送一次 `link start`。默认已是递推和摘要日志，不必再发 `link spectrum rotate16`、`link trace off` 或 `link compare`。
5. 到原检测区域坐稳约10分钟，中途无需操作电脑。结束后发送 `link stop`，等几秒保存日志。

本版只更新 W803。RS6130 沿用当前 V27，已有板内 UART 连线和 PC 的 RX02/TX02 接线沿用。黄G是 W803 复位线；PA0是新引出的下载选择线，两者不能混同。

VS Code 可直接打开本目录或 `W803_HEART_R1.code-workspace`。菜单“终端 → 运行生成任务”执行“R1 编译并校验固件”；“终端 → 运行任务”可选择“R1 打开图形烧录（可选固件）”。桌面原烧录入口同样可以手动选择本版 `.fls`。

日志继续保存至 `D:\MRS6130\w803_gui_project\logs`。重新编译所得文件在 `build\w803_link.fls`；命名发布文件是本次冻结副本，后续重新编译不会自动覆盖发布副本。

## 固定的计算方案

- `double` 精度、真实源时间戳，原145个频点。
- 沿频率方向递推正余弦，每16个频点以原 `sin/cos` 公式重锚；三个天线共享旋转因子。
- 保持原窗口、预处理、窗函数、选峰、谐波判断、确认次数、保持和平滑规则；没有加入单精度、共享角度约简或局部 epsilon。
- 原75秒历史要求、416样本接收环形历史、一致快照、单个可合并待算请求继续使用。
- 接收/控制/心率/日志任务优先级分别为4/3/2/1；心率任务只投递结果，由日志任务输出。
- 正常每5秒源时间产生一次更新请求。是否接受新心率，仍由质量与确认规则决定。预算保留120秒作为失控保护，不作为实时性能目标。

递推与 DIRECT 的频谱浮点结果并不逐位相同，已有数据在边界处出现确认和接受时刻差异。本版采用用户选定的递推行为；离线“零差异”指与冻结递推版本相比，不能解释成与 DIRECT 或原 Python 完全等价。历史证据见 `reference\递推数值差异说明.md`，其中 C2 默认 DIRECT 的描述属于当时状态。

## 本版工程修改

| 位置 | 作用 |
| --- | --- |
| `Algorithm\heart_rate_shared_impl.h` | 直接/递推频谱实现；与已测 C4 文件完全相同 |
| `Algorithm\heart_rate_core.c/.h` | 原判定与工作区；与已测 C4 文件完全相同 |
| `App\snapshot_heart.c` | 接收、快照、计算核心和工作区初始化统一默认 MODE1，避免启动后回到 DIRECT |
| `main\src\main.c` | 单模式运行；移除自动对照调度、临时核心分配和影子结果；保留必要性能计数 |
| `main\CMakeLists.txt` | 默认 `HR_DEFAULT_FREQ_ROTATION=1`，不编入自动对照源码 |
| `Config\board_config.h` | 独立版本标识，保持原硬件定义和5秒状态周期 |
| `.vscode`、`tools` | 图形任务、官方 SDK 构建入口和带文件选择的烧录入口 |

## 常用命令

| 命令 | 作用 |
| --- | --- |
| `link start` | 清理旧轮状态，向雷达发送 stop / report points 0 / start，开始新轮 |
| `link stop` | 停止并隔离旧窗口和结果 |
| `link status` | 请求当前通信和心率状态 |
| `link trace on` | 调试时转发完整源文本，会增加输出量 |
| `link trace off` | 返回默认摘要转发 |
| `link spectrum direct` | 切换 DIRECT 并重新积累数学历史 |
| `link spectrum rotate16` | 切回递推并重新积累数学历史 |

建议切换前 `link stop`，选择模式后再 `link start`。所选模式在同一次上电期间跨 stop/start、雷达重启、离床恢复保留；W803重新上电/复位后固定回到递推默认。模式不写入Flash。

`link compare` / `link compare off` 仅回显正式版不提供对照，不触发额外计算。需要历史影子对照时使用保留的 C4 工程，不能把其耗时当本版单模式性能。

## 日志读法与本次验收

默认关闭完整原始转发，周期状态每5秒输出一次，另保留心率结果、雷达状态和命令回执。

| 字段 | 含义 |
| --- | --- |
| `MODE=1` | 递推；`MODE=0` 是 DIRECT |
| `STATUS=1 COMMIT=1` | 计算完成且结果在提交时仍属于有效采集轮次；不代表一定接受新心率 |
| `CORE_BPM` / `REASON` | 核心输出心率及原因；-1表示未给出有效值 |
| `CORE_FRESH=1` | 本次新接受；0可能是等待或保持旧值，要结合 BPM/REASON |
| `CONF_MS` / `AGE_MS` | 上次接受的源时间、至提交时接受值的年龄；4294967295表示无接受值 |
| `SRC_MS` | 本次快照截止的雷达源时间 |
| `COST_MS` / `KERNEL_MS` | 从数学计时起点到提交的耗时 / 内核经过时间；快照复制、输入补入另列为 COPY/INGEST |
| `DFT_MS` | 频谱阶段的经过时间 |
| `LAG_MS` | 快照最后一条输入到提交的延迟 |
| `ON_TIME` / `LIVE_AT_COMMIT` | 提交时是否满足5秒延迟要求、是否仍为当前采集轮次；不是永久有效标记 |
| `SKIPPED` | 过载时合并掉的待算更新数，不能当已完成估计 |
| `HIST_LOST` / `MRESET` | 所需近期历史丢失数 / 数学核心重建次数；新轮初次重建通常为1 |
| `NSAMP FREQ TRIG` | 实际完整频谱的样本、频点和三角函数调用计数 |

本轮先确认普通 `HEART MODE=1 COMMIT=1` 连续出现，不应有 `COMPARE` 影子输出。只用 `STATUS=1` 且 `TRIG>0` 的完整计算统计性能，另列300样本窗口；检查延迟、跳过、历史丢失、取消、通信错误、日志丢弃及堆/栈。

工程目标是通信并行时完整计算通常3秒以内、覆盖压力条件时不超过5秒，且无持续积压和过载清历史。C4此前仅一次同快照测得 DIRECT 6101ms、ROTATE16影子2132ms；本版尚无新的板端运行证据，不能以该值保证长测达标。

没有同期参考记录时只评价连续性、质量、恢复及实时性，不沿用其他轮的55—56作为真值。可记录本轮小米手环的读数和时间；其断续读数不能替代连续同步参考。

## 构建、测试和冻结资料

本机已有依赖：同级 `w803_sdk\wm_iot_sdk`、`tools\w803_env`，以及 `D:\C-Sky\CDKRepo\Toolchain\CKV2ElfMinilib\V3.10.32\R\bin`。源包不重复包含完整SDK和工具链。

PowerShell可重现构建：

```powershell
& 'D:\MRS6130\tools\w803_env\Scripts\python.exe' -B 'D:\MRS6130\w803_live_heart_r1\tools\ide_build.py'
```

离线测试：先运行 `tools\build_native.cmd`，再用相同Python运行 `tests\check_native.py` 和 `tests\check_release.py`。测试依赖同级冻结 C1/C2 DLL、`w803_iq_ingest\tools` 以及原实录，源包不重复包含这些历史输入。`check_release.py` 会同时运行 DIRECT 回退套件。

- 接收生命周期19项、DIRECT快照与过载模型17项、默认递推及模式生命周期/回放17项，共53项检查通过。
- 三组真实记录共4665帧、189次估计：默认递推的完整56字节输出与冻结 C2 递推 DLL 均相同；DIRECT回退各189次与原冻结对照相同。
- 目标固件编译通过，应用启用 `-Wall -Wextra -Werror`；FLS三段头/负载CRC、源哈希、版本字符串与已链接UART3均通过检查。
- SDK已有Python转义提示和CMake开发者提示，记录在构建日志；应用编译未出现 warning/error。
- `build-verification.json`、`native-verification.json`、`snapshot-verification.json`、`release-verification.json` 保存检查结果；`release-manifest.json` 保存发布哈希和范围。

没有自动打开串口、复位或烧录。后续实测结论应追加在本工程的 `board-tests`，不能回填成此次交付已经完成的验收。
