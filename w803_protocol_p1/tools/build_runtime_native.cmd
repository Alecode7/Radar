@echo off
setlocal
cd /d "%~dp0.."
if not exist "native-output" mkdir "native-output"
if not exist "native-output\runtime-obj" mkdir "native-output\runtime-obj"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /fp:strict /std:c11 /LD /D_CRT_SECURE_NO_WARNINGS /DHR_DEFAULT_FREQ_ROTATION=1 /I"tests/stubs" /I"Config" /I"BSP" /I"App" /I"Drivers" /I"Algorithm" /Fo:native-output\runtime-obj\ /Fe:native-output\runtime.dll App/product_protocol.c BSP/product_board.c App/snapshot_heart.c Algorithm/heart_rate_core.c Drivers/radar_iq.c Drivers/rx_state.c tests/runtime_host.c tests/board_host.c /link /IMPLIB:native-output\runtime.lib
exit /b %errorlevel%
