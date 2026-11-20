# 源码获取、构建与验证边界

本次提交保存P1/V29当前源码及R1/V28基线源码，不附厂商完整SDK、烧录镜像、工具链、真实串口记录和历史DLL。

## W803

需自行取得WM IoT SDK（本机基线v2.4-rc，目录`w803_sdk/wm_iot_sdk`）、CKV2ElfMinilib V3.10.32工具链、CMake、Ninja和SDK所需Python依赖。`w803_protocol_p1/CMakeLists.txt`支持`SDK_PATH`，默认从相邻的`w803_sdk/wm_iot_sdk`加载SDK。

`tools/ide_build.py`和`tools/wm.py.cmd`是当前Windows本机包装脚本，仍含`D:/C-Sky`、`D:/MRS6130/tools/w803_env`及Visual Studio 18路径。其他电脑需按实际安装位置适配后使用；它们不能在缺少SDK时独立生成固件。`启动协议联调工具.vbs`也依赖原本机Python路径，跨电脑请直接用Python运行Host脚本。

本机已完成目标构建，镜像身份/CRC/源码哈希与链接设备表审计见`build-verification.json`；镜像不随源码发布。`source-lock.json`及发布报告中的源码哈希按原字节计算；版本目录的Git属性保留原换行，避免检出改变冻结源文件哈希。报告里的绝对路径、时间和二进制哈希是原本机的历史证据。

## RS6130

需自行取得RS6x_7x mmWave SDK V2.0.6、CDK与XTGccElfNewlib V3.1.0。把SDK内容准备到各雷达项目的` sdk/ `目录后，再覆盖本仓库对应的定制文件，不能反过来用原SDK覆盖本仓库改动。根目录早期SDK快照不是V29的完整替代。

`build_radar.ps1`和`build-safe.mk`含本机工具路径。当前安全构建沿用冻结的`Obj/src_hal_board.o`、`Obj/utils_version.o`，这两个对象及库/打包资源没有发布。要复现当前镜像需要原受控SDK与这些冻结对象；全新SDK自行重建对象属于新的构建，须重新核对板级初始化、版本/校准布局及实板行为，不能宣称与冻结镜像相同。

项目保留逐个归档打包中间文件的方法。默认厂商清理流程可能删除目录，勿用它替换安全包装。

## 可独立运行的协议软件检查（Windows）

需安装Visual Studio C++构建工具。本机脚本使用`C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat`；安装路径不同先适配脚本。以下检查仅使用内存UART、时钟、Flash和RTOS替身，不打开COM、不烧录：

```powershell
cmd /c w803_protocol_p1\tools\build_protocol_native.cmd
cmd /c w803_protocol_p1\tools\build_runtime_native.cmd
python -B -m unittest discover -s w803_protocol_p1/tests -p 'test_*.py' -v
```

包含协议33、主板C客户端12、存储5、运行桥10、Python客户端4，共64项。存储检查包括80种中断写位置和640种位损坏场景，仍不代替真实Flash断电测试。

远程已有算法精简版可独立验证：

```powershell
python -B -m unittest discover -s tests -v
```

## 需要私有测试材料的历史回放

P1 `tests/check_native.py`、`check_snapshot.py`、`check_release.py`及雷达`tests/check_stream.py`还引用本地`w803_gui_project/logs`、`w803_iq_ingest/fixtures`、历史C1/C2结果和原生DLL。自研离线帮助脚本保留在`w803_iq_ingest/tools`，但人体数据和历史DLL未上传。这些回放不是上述64项独立检查的依赖，缺少材料时不能把原129项报告宣称为本次克隆的实测结果。

## 当前待验收

两芯片需配套P1/V29固件；P1对旧V28不提供完整控制。现场还需确认六针UART、实际启停和安装读回、显式保存后断电读回、独立重启与迟到包隔离、事件补查、通信并行心率耗时及长时稳定性。当前没有新一轮同步参考心率，覆盖率和输出连续性不等于准确率。
