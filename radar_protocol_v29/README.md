# RS6130 V29：双芯片协议配套雷达固件

2026-11-20，基于昨日已实板验证的V28。心率仍由W803上的R1 ROTATE16核心负责，本版不恢复雷达重复心率计算。

烧录文件：`release\RS6130_V29_PROTOCOL_20261120.img`。配套W803 `D:\MRS6130\w803_protocol_p1\release\W803_PROTOCOL_P1_20261120.fls`；必须两侧一起更新。目标编译与软件测试通过不表示P1/V29已经上板。

本版修改：

1. 增加 `mmwc pctl` 事务入口，提供实际状态、幂等启停、安装补偿应用和读回。高32位控制器前缀由状态查询绑定，低32位单调序号；重复不重新执行，旧请求或同编号不同参数拒绝。启动再次核对期望安装值和源时钟，避免配置未确认就开始。
2. 在原呼吸稳定估计合格时附加序号和确认时间，输出 `BRC`；旧BR读回/保持不续新时间，不修改原稳定、滤波或输出阈值。
3. 启用已有异步TX环形缓冲与唯一发送任务，HRRAW整行编码，HRIN/BRC/PCTL整行入队，Shell及SDK printf经同一出口。拒绝计数经HRSTREAM报告；不将回执字符插入IQ行。
4. 5Hz射频、IQ读取/选格、原呼吸和在床数值路径保持；旧心率核心仍未链接。安装改变仅经已有 `sleep_detect_set_mount/disable_mount`，不写RF/天线校准。

版本：`UART_RADAR_PROTOCOL_20261120_V29`。内部PCTL回执包含operation、action、返回值、实际RUN、MOUNT/HEIGHT/PITCH和CLOCK_OK。源时钟无效时不批准开始。W803可用匹配nonce的状态回执发现已在运行的雷达，无需把一次开机版本行当成唯一恢复条件。

控制action：0停止、1开始并核对安装、2查询、3应用安装。1/3附加补偿开关、高度0—300cm、俯角0—80°；安装应用要求已停止并排空数据事件，启用要求原vertical模式。状态与停止不更改配置。原Shell调试入口保留，量产接入走W803产品协议，不直接把该内部文本当主板RSP。

构建仍使用 `build_radar.ps1`，源工程在 `sdk\project\mmwave\radar_analysis_sleep`，CDK工作区同目录 `radar_sleep_llx\radar_sleep_llx.cdkws`。保留V28已冻结的板级与版本对象，打包逐个归档中间文件，不批量删除；原V28 SDK/镜像不覆盖。`reference\RS6130_V28_rollback.img`为回退副本。

软件检查包括真实V29发送适配器与P1原解析器4665帧字段往返、控制事务、W803实际运行桥的文本回执/安装预处理等。模型不代替UART电气、真实RTOS并行、Flash断电或人体准确率验证。完整交付说明、协议、测试清单见 `D:\MRS6130\协议交付_20261120`。
