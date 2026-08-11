# MRS6130 Sleep Radar Algorithms

> MRS6130-P1812 毫米波睡眠监测核心算法：人员/在床检测、呼吸率估计、睡眠状态、翻身统计与多天线 IQ 心率候选分析。

本仓库只保留算法，不包含厂商完整 SDK、CDK 工程、烧录镜像、GUI、串口服务、SPI 驱动或测试日志。代码来自实际 MRS6130-P1812 睡眠雷达研发链路，并整理为便于阅读、测试和移植的最小结构。

生命体征和睡眠结果仅用于工程研发与趋势参考，不作为医疗诊断结果。

![算法总体流程](docs/algorithm_overview.svg)

## 核心算法

### 1. 板端睡眠检测

`firmware/sleep_detect.c`接收运动点云和微动点云，完成：

- motion点负责人员进入和明显活动检测；
- micro点负责入床后的静止保活和呼吸信号构造；
- 床区坐标、连续帧计数和迟滞机制抑制空床误报与静止掉人；
- IQ相位展开与自相关周期估计得到呼吸率候选；
- 根据在床、活动、安静时间和呼吸连续性输出睡眠状态；
- 统计翻身、入离床、睡眠段、中断、最长安静和呼吸覆盖率。

![睡眠状态机](docs/sleep_state_machine.svg)

默认床区和关键时序基于5 Hz处理节拍：

| 参数 | 默认值 | 含义 |
|---|---:|---|
| 床区X | -50～50 cm | 雷达左右范围 |
| 床区Y | 50～180 cm | 雷达前方距离 |
| 床区Z | -70～70 cm | 高度范围 |
| 入床确认 | 8帧 / 1.6秒 | 抑制瞬时运动误触发 |
| 离床确认 | 24帧 / 4.8秒 | 保留短时信号丢失容错 |
| 安静确认 | 50帧 / 10秒 | 进入在床安静状态 |
| 疑似睡眠 | 1500帧 / 5分钟 | 同时需要稳定呼吸支持 |

### 2. PC端心率候选估计

`host_algorithm/heart_rate.py`解析`HRRAW`三天线复数IQ，并执行：

- 目标距离BIN稳定性检查与相邻BIN分段融合；
- 三天线相位展开、去趋势和高通处理；
- 48～120次/分钟频带扫描；
- 多天线候选一致性融合；
- 呼吸三倍频和高阶谐波冲突识别；
- 候选连续确认、变化限速、旧结果保持与过期清除。

![心率处理链路](docs/heart_rate_pipeline.svg)

该算法同时兼容5 Hz单BIN安全采样和20 Hz多BIN实验数据。当前实机使用5 Hz方案，重点追求长时间运行稳定性。

### 3. 会话统计

`host_algorithm/session_metrics.py`提供：

- 心率均值、范围和有效覆盖率；
- 十分钟呼吸率、心率、翻身和体动汇总；
- 0～100工程体动指数；
- 长时间无有效呼吸/心率和低覆盖率提醒。

## 目录结构

```text
firmware/
├─ radar_types.h       最小点云数据类型
├─ sleep_detect.h      板端算法接口和结果结构
└─ sleep_detect.c      存在、呼吸、睡眠和翻身状态机

host_algorithm/
├─ heart_rate.py       多BIN、多天线心率候选算法
├─ session_metrics.py  会话统计和监测提醒
└─ sleep_protocol.py   SLEEP文本协议最小解析器

tests/                 Python算法测试
docs/                  算法解释图
```

## 运行测试

Python部分只依赖标准库：

```powershell
$env:PYTHONDONTWRITEBYTECODE='1'
python -m unittest discover -s tests -v
```

## 集成板端算法

独立仓库中的`radar_types.h`仅用于说明最小依赖。集成回MRS6130 SDK时，可将`sleep_detect.h`中的引用恢复为厂商`mmw_type.h`，并在点云回调中调用：

```c
sleep_detect_update(motion_points, motion_count, micro_points, micro_count);
sleep_detect_update_breath_phase(valid, target_bin, phase_mrad, amplitude);
const SleepDetectResult *result = sleep_detect_get_result();
```

## 已知限制

- 疑似睡眠是工程规则，不区分所有清醒静卧场景。
- 心率候选容易受呼吸谐波、侧躺、动作和目标BIN切换影响。
- 参数来自当前床位、安装姿态和5 Hz采样条件，迁移场景后必须重新标定。
- 仓库未包含厂商SDK；完整固件编译需使用对应版本的MRS6130开发环境。
