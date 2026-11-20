@echo off
setlocal
cd /d "%~dp0.."
if not exist "native-output" mkdir "native-output"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /std:c11 /LD /DRADAR_IQ_EXPORTS /I"tests/stubs" /I"sdk/project/mmwave/radar_analysis_sleep/app/inc" /I"../w803_protocol_p1/Drivers" /Fo:native-output\ /Fe:native-output\stream.dll tests/host_stream.c sdk/project/mmwave/radar_analysis_sleep/app/src/v6_heart_device.c ../w803_protocol_p1/Drivers/radar_iq.c /link /IMPLIB:native-output\stream.lib
exit /b %errorlevel%
