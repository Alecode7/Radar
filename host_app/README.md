# MRS6130 睡眠雷达上位机

这是针对当前 `radar_analysis_sleep` 固件的 PC 调试上位机。日常测试主流程只使用控制串口，当前版本的主要能力是：

- 串口下拉选择，例如 `COM10 - USB-HiSpeed-SERIAL-A`
- 一键连接控制/日志口，提供开始测试、停止雷达和关闭上报等常用命令
- 自动保存完整串口日志，离床后自动导出会话CSV，也可手动导出
- 解析 `SLEEP P=... A=... BED=...` 状态行
- 显示人员存在、活动、在床、呼吸、睡眠状态及关键时间
- 显示呼吸率、心率候选值、会话均值、范围和有效输出覆盖率
- 每十分钟汇总睡眠状态、呼吸率、心率、睡眠中翻身和体动占比
- 离床后生成整段会话总结，并提示记录时长是否适合整晚分析
- 显示0～100体动指数，并提示长时间无有效值、低覆盖率和长时间离床

当前不会输出未经验证的“睡眠评分”。心率、呼吸率和睡眠状态均用于工程研发与趋势参考，不作为医疗诊断结果。

CH347 SPI/HIF 和点云解析代码仍保留用于底层排查，但已经从日常测试主界面中移除，普通呼吸、心率和睡眠测试不需要配置 SPI。

注意：官方 `RadarAnalysisTool` 如果选择“数据口通信方式 = SPI”，走的是 `CH347T.M1:USB2.0 To SPI&IIC` 设备，不是 COM 串口。当前日常测试只连接控制串口；下文的 SPI/HIF 内容仅用于保留的点云底层排查代码。

## 运行

推荐按你当前的 `uv` 项目运行：

```powershell
cd host_app
uv sync
uv run python main.py
```

也可以在仓库根目录用普通 Python 运行：

```powershell
python -m pip install -r host_app/requirements.txt
python -m host_app
```

如果当前 Python 没有安装 `pyserial`，程序仍能打开界面，但不能列出串口，会在日志里提示安装依赖。

## 一键连接和 SPI/HIF

官方工具能走 SPI，是因为它调用 CH347T 的 Windows SPI/IIC 驱动，打开 `CH347T.M1:USB2.0 To SPI&IIC`，并按 HIF SPI 流程轮询数据。本程序的 SPI 接收链路是：

```text
CH347DLLA64.DLL -> CH347StreamSPI4 -> HIF 0x0C HOST_READ poll -> SerialStreamDecoder -> HIF 0xC3 点云解析 -> 画布
```

界面里 `CH347 SPI index / clock` 的两个输入框分别是：

```text
index：CH347 设备索引，默认 0
clock_code：CH347 SPI 时钟代码，默认 3，接近官方工具 8 MHz 的配置
byte_order：CH347 字节序默认 1，与官方 RadarAnalysisTool 的 `iByteOrder = 1` 一致
```

使用 SPI 点云时：

```text
1. 连接控制/日志口，例如 COM11 @ 115200
2. 确认 CH347 index=0、clock=3
3. 点击“一键连接”
4. 点击“配置SPI上报”，或手动发送：
mmwc stop
mmwc comtype 1
mmwc report points 0
mmwc start
```

`mmwc report points 0` 表示连续上报；`mmwc report points 100` 只上报 100 帧，适合短时间验证，不适合作为实时点云显示的默认流程。

也可以先用命令行 probe 验证 SPI 原始帧：

```powershell
cd host_app
uv run python spi_probe.py --index 0 --clock-code 3 --loops 50
```

读取 CH347 驱动实际 SPI 配置：

```powershell
cd host_app
uv run python ch347_cfg_dump.py --index 0 --clock-code 3 --byte-order 1 --chip-select 0x80
```

注意：当前电脑上请求 `byte_order=1` 时，`CH347SPI_GetCfg` 可能读回 `byte_order=0`；请求 `byte_order=0` 时可能读回 `128`。这更像 CH347 DLL 内部状态位，不直接等价于命令行入参。按官方初始化顺序先配置 GPIO6 输入、再初始化 SPI 后，`--byte-order 1` 可以读到 `0xA5/0xC3` HIF 点云帧。

如果 probe 连续输出 `timeout`，说明当前没有从 CH347 SPI 读到 HIF 帧。优先确认官方工具已经关闭、板子拨码在 SPI/IIC 接到 Type-C 的位置、并且固件已经发送 `mmwc comtype 1`。

如果要把 GUI、点云解析和 SPI/HIF 底层分开排查，可以单独运行 HIF 验证脚本。它只发送安全的 HIF 读/状态类消息，不包含 reboot、flash、efuse、下载、保存校准等危险 Msg ID：

```powershell
cd host_app
uv run python hif_verify.py version spi-poll-delay host-active point-poll --index 0 --clock-code 3 --set-irq-input --timeout-ms 200
```

`hif_verify.py` 和 `spi_probe.py` 默认使用 `--byte-order 1`，并在 HIF 头 6 字节无效时按官方工具思路继续扫描 `1024` 字节寻找 `0xA5` 帧头。可以用 `--scan-chunk-size 0` 关闭这一步扫描。

源码已确认的 SPI/HIF 点云格式：

```text
点云 Msg ID：0xC3
固件定义：subsys/hif/inc/hif_msg.h -> HIF_MSG_ID_OBJECTS 0xC3
发送位置：subsys/mmw/mmw_cmd_sleep/src/mmw_report.c -> mmw_msg_send_obj_data(), msg.msg_id = 0xC3
点云 TL：subsys/mmw/mmw_cmd_sleep/inc/mmw_report.h -> MMW_TL_TYPE_POINTS = 4
SPI 物理帧：subsys/hif/src/hif_com.c 中 SPI 发送 hdr_len = HIF_HEAD_LEN + 1，check_len = HIF_CHKEC_LEN + 1
```

因此 PC 端 SPI 读取层会先把 `6字节HIF头 + 1字节SPI填充 + payload + 4字节checksum + 1字节SPI填充` 还原成普通 HIF 帧，再交给点云解析器。当前若诊断仍然显示 `A5=0`，问题不在 `0xC3` 点云 ID 或 TL 解析，而是在更底层的 CH347/SPI/HIF 物理读帧或官方工具初始化差异。

常见判断：

```text
version 有 msg=0x00 且 status=0：SPI/HIF 命令 ACK 链路是通的
spi-poll-delay 有 msg=0x0A 且 status=0：HIF 参数读取链路是通的
point-poll 有 msg=0xC3：已经读到点云 HIF 帧
全程 A5=0 且 last_rx 全 00：CH347 SPI 没有读到 HIF 物理帧，优先查官方工具是否占用、CH347 index/CS/IRQ/板级配置
全程 A5=0 且 last_rx 类似 `01 00 00 00 00 00`：CH347 已经能读到稳定电平，但从设备还没有输出合法 HIF 帧，优先查官方工具额外初始化流程或固件 HIF/SPI 启动条件
```

## 连接选择

当前界面默认按官方工具一致的 SPI 点云链路使用：

```text
控制/日志：COM 口，默认 115200
HIF点云：CH347 SPI，默认 index=0、clock=3
```

左侧只保留一个“一键连接”按钮。点击后会同时打开控制/日志 COM 口和 CH347 SPI/HIF，不再提供单独的 UART 数据口按钮，避免把 SPI 点云误接到普通 COM 口。

如果后续确实需要验证 UART HIF，底层 `SerialService` 仍然保留，但当前界面不再把它作为主流程入口。

界面里的“配置SPI上报”会按下面顺序发送，只负责把固件输出通道切到 SPI：

```text
mmwc stop
mmwc comtype 1
mmwc report points 0
mmwc start
```

这条流程要配合“一键连接”使用。原因是固件里 `mmwc start` 会根据当前 `gui_data_report` 初始化点云处理。若先 `start` 再 `report points`，设备已经处于 running 状态时可能返回 `ERR:Running, please stop sensor first!`。
其中 `report points 0` 是连续上报；如果使用 `report points 100`，固件只会输出 100 帧，几秒后点云会停止。

停止时发送：

```text
mmwc stop
```

关闭上报：

```text
mmwc report disable
```

## 数据说明

当前固件会周期打印文本状态行，例如：

```text
SLEEP P=1 A=0 BED=1 BV=0 BR=0 MT=0 MB=0 McT=172 McB=0 Act=0 PP=0 PA=1 BP=0 BA=1 BrP=0 BrA=320
```

字段含义：

| 字段 | 含义 |
|---|---|
| `P` | 是否有人 |
| `A` | 是否活动 |
| `BED` | 是否在床区 |
| `BV` | 呼吸是否有效 |
| `BR` | 当前稳定呼吸率；无有效估计时为 `0` |
| `MT` / `MB` | 运动点总数 / 床区运动点数 |
| `McT` / `McB` | 微动点总数 / 床区微动点数 |
| `Act` | 活动运动点数 |
| `PP` / `PA` | 人员存在 / 消失连续帧计数 |
| `BP` / `BA` | 在床 / 离床连续帧计数 |
| `BrP` / `BrA` | 呼吸有效 / 无效连续帧计数 |
| `Bsig` | 当前帧呼吸代理信号 |
| `Bq` | 呼吸时间窗口质量 |
| `Bper` | 当前估计到的呼吸周期（帧） |
| `Brc` | 当前候选呼吸率 |
| `Brr` | 候选呼吸率连续/相近命中次数 |
| `Brs` | 当前稳定呼吸率 |

## 会话统计

上位机在每次从离床进入在床状态时重置本次统计：

```text
心率统计：整段会话的有效输出数/估计次数、覆盖率、均值和范围
十分钟综合：最近十分钟的状态、呼吸/心率均值、睡眠中翻身次数和体动占比
会话提示：少于4小时提示数据不足，4～12小时提示可用于整晚分析，超过12小时提示核对离床判断
会话总结：离床后把时长、状态占比、睡眠过程、呼吸和心率统计写入日志
体动指数：根据固件 MI 四级体动和 Act 活动点数映射到0～100，仅作本项目工程比较
```

十分钟翻身统计使用固件的睡眠期翻身计数 `STC`，不会把睡前和醒后的一般动作重复计作睡眠翻身。

监测提醒采用以下保护条件，避免正常建窗阶段频繁误报：

```text
入床3分钟后仍无心率：提示心率尚无有效输出
已有心率后连续60秒无新值：提示心率信号中断
BR=0且呼吸信号连续60秒无效：提示呼吸无有效值
入床5分钟后心率覆盖率低于40%或呼吸率覆盖率低于50%：提示覆盖率偏低
已有在床会话且离床超过30分钟：提示长时间离床
```

CSV保存在桌面 `MRS6130睡眠雷达日志` 文件夹，文件名为 `mrs6130_session_日期_时间.csv`。主要字段包括时间、`P/A/BED/SS`、`BV/BR/HR`、心率数据年龄、`MI`、体动指数、翻身、点数、呼吸覆盖率和提醒状态。CSV使用UTF-8 BOM编码，可直接用Excel打开。

## 保留的点云调试代码

以下内容只用于开发阶段排查SPI/HIF和点云，当前日常测试主界面不显示点云。

当前画布是 XY 俯视图，不是 3D 视图：

```text
雷达在画布底部
Y 轴向上：离雷达越远，点越靠上
X 轴横向：雷达左侧是 -X，右侧是 +X
Z 高度当前不画
```

点云图右上角有显示模式：

| 模式 | 显示内容 | 适合用途 |
|---|---|---|
| 运动点 | 只画当前蓝色运动点 | 判断人是否在动，画面最干净 |
| 场景图 | 累积 micro/static 背景点，低亮显示，不画当前运动点 | 看不动区域、床/墙/家具等环境反射 |
| 混合图 | 背景点低亮显示 + 当前运动点高亮 | 更接近官方工具/激光雷达风格的场景感 |

背景点来自点云包里的 micro/static 点，会在上位机里保留一段时间并逐渐被新数据替换。它不是完整 3D 建图，只是把毫米波反射点投影到 XY 平面做可视化。

因此画面不能按摄像头遮挡关系理解。毫米波雷达显示的是反射点，不是照片里的可见轮廓；背景墙、床、椅子、人体后方物体、多径反射或旁瓣都可能产生点。当前上位机把点云投影到 XY 平面，Z 高度和遮挡关系会被压平，所以有些点看起来像在人的“后面”。判断人是否在动，应优先看中间蓝色运动点，以及右侧 `SLEEP A/MT/McT/Act` 等状态，而不是把每个点当作人体轮廓。

## 点云排查

如果点云画布一直显示“等待 HIF 点云数据”，先确认：

- 是否已经点击“一键连接”，顶部状态里同时显示控制口和 `CH347 SPI[...]`；
- 官方工具、`spi_probe.py`、`hif_verify.py` 是否已经关闭，避免占用 CH347；
- `mmwc report points` 是否已经发送；
- 当前板型和拨码是否匹配 CH347 SPI 链路。

界面右侧有 HIF 诊断值：

| 字段 | 判断方法 |
|---|---|
| HIF字节 | 一直为 `0`：当前 HIF 链路没有收到任何字节，优先用 `spi_probe.py` 看是否 timeout |
| SPI轮询 | 仅 CH347 SPI 连接时有效。`poll` 是 HOST_READ 轮询次数，`timeout` 是没等到 HIF 帧次数，`raw` 是 CH347 原始回读字节数，`A5` 是原始回读里出现的 HIF 帧头数量，`FF/00` 表示回读全 `FF` 或全 `00` 的次数 |
| A5帧头 | 大于 `0`：HIF 链路里出现物理帧头；一直为 `0` 说明还没有读到合法 HIF 帧 |
| HIF包/点 | 第一个数增长说明解析到 HIF 包；第二个数增长说明解析到点云点 |
| 最后Msg | `0xC3 点云` 才是点云包；其他 Msg 说明 HIF 链路有数据但不是点云上报 |
| 解析状态 | `校验失败` 增长通常是波特率/协议格式不匹配；`等待 x/y` 表示收到了半包 |

右侧日志分成两个黑框：

- `控制日志`：显示 `发→ ...`、`控制收← ...`、命令成功/失败、`SLEEP ...` 等控制口文本。
- `SPI日志`：显示 `SPI状态 ...`、`HIF RX ...`、`数据HIF 0xC3 ...` 等 SPI/HIF 摘要。

SPI/HIF 原始二进制不直接打印到日志框。原始二进制容易出现乱码，排查时优先看右侧 `HIF字节`、`A5帧头`、`HIF包/点` 和 `解析状态`。
