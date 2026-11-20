@echo off
setlocal
cd /d "%~dp0.."
if not exist "native-output" mkdir "native-output"
if not exist "native-output\protocol-obj" mkdir "native-output\protocol-obj"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /std:c11 /LD /I"tests/stubs" /I"Config" /I"BSP" /I"App" /I"protocol/reference" /I"../radar_protocol_v29/sdk/project/mmwave/radar_analysis_sleep/app/inc" /Fo:native-output\protocol-obj\ /Fe:native-output\protocol.dll App/product_protocol.c BSP/product_board.c protocol/reference/mcu_rsp.c tests/radar_control_host.c tests/board_host.c /link /IMPLIB:native-output\protocol.lib
exit /b %errorlevel%
