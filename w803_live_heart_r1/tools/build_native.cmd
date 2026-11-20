@echo off
setlocal
cd /d "%~dp0.."
if not exist "native-output" mkdir "native-output"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /fp:strict /std:c11 /LD /DRXL_EXPORTS /DRADAR_IQ_EXPORTS /DHR_TEST_EXPORTS /DHR_SPECTRUM_TEST_API /DSH_EXPORTS /DHR_DEFAULT_FREQ_ROTATION=1 /I"Drivers" /I"App" /I"Algorithm" /Fo:native-output\ /Fe:native-output\rxstate.dll Drivers\radar_iq.c Drivers\rx_state.c App\snapshot_heart.c Algorithm\heart_rate_core.c /link /IMPLIB:native-output\rxstate.lib
exit /b %errorlevel%
