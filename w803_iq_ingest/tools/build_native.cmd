@echo off
setlocal
cd /d "%~dp0.."
if not exist "native-output" mkdir "native-output"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /std:c11 /LD /DRADAR_IQ_EXPORTS /DLINK_EXPORTS /I"Drivers" /I"reference" /Fo:native-output\ /Fe:native-output\radar_iq.dll Drivers\radar_iq.c reference\link_stream.c /link /IMPLIB:native-output\radar_iq.lib
if errorlevel 1 exit /b 1
cl /nologo /W4 /WX /O2 /fp:strict /std:c11 /LD /DHR_TEST_EXPORTS /I"reference\Algorithm" /Fo:native-output\core.obj /Fe:native-output\shared_core.dll reference\Algorithm\heart_rate_core.c /link /IMPLIB:native-output\shared_core.lib
if errorlevel 1 exit /b 1
"..\tools\w803_env\Scripts\python.exe" -B -X utf8 tools\stamp_native.py
exit /b %errorlevel%
