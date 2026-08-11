# MRS6130-P1812 睡眠雷达

本仓库保存 MRS6130-P1812 睡眠监测项目的定制核心代码，包括板端固件算法和 Python 调试上位机。厂商完整 SDK、CDK 安装包、驱动、文档、编译中间文件、烧录镜像和测试日志不进入版本控制。

## 当前能力

- 人员存在、活动和在床状态检测
- 床区运动点与微动点分析
- 呼吸有效性、呼吸率和呼吸状态
- 在床活动、安静和疑似睡眠状态
- 翻身、睡眠中断、入离床和会话统计
- 三天线微动 IQ 原始数据输出
- 上位机心率候选估计、趋势统计、提醒和 CSV 导出

上述生命体征和睡眠结果用于工程研发与趋势参考，不作为医疗诊断结果。

## 目录

```text
RS6x_7x_mmWave_sdk_V2.0.6/Software_Kit/02_SDK/psdf_sdk/
├─ project/mmwave/radar_analysis_sleep/   P1812 睡眠固件工程
└─ subsys/mmw/mmw_cmd_sleep/              点云、IQ 与串口上报定制

host_app/                                  Python 上位机、协议解析和测试
```

固件文件保持厂商 SDK 中的原始相对路径。克隆代码后，需要准备同版本的 `RS6x_7x_mmWave_sdk_V2.0.6`，再将仓库内受控文件放到对应路径，或直接在相同目录结构中使用。

## 固件环境

- 芯片/模组：MRS6130-P1812
- SDK：RS6x_7x mmWave SDK V2.0.6
- CDK：V2.24.11
- 当前雷达帧周期：200 ms（5 Hz）
- 当前心率原始数据方案：`SAFEHR5 MICROIQ`

CDK 工作区：

```text
RS6x_7x_mmWave_sdk_V2.0.6/Software_Kit/02_SDK/psdf_sdk/project/mmwave/
radar_analysis_sleep/radar_sleep_llx/radar_sleep_llx.cdkws
```

## 上位机运行

```powershell
python -m pip install -r host_app/requirements.txt
python -m host_app
```

上位机通过控制串口接收固件输出的 `SLEEP` 和 `HRRAW` 文本。CH347 SPI/HIF 代码保留用于底层点云链路排查。

## 上位机测试

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
python -m unittest discover -s host_app -v
```

## 当前已知问题

- 长时间运行时点云上报回调偶尔中断，仍需定位固件回调、内存或通信链路状态。
- 心率容易受到呼吸谐波、姿态变化和目标距离单元切换影响。
- 疑似睡眠采用工程规则判断，清醒状态下长时间静卧也可能满足条件。
