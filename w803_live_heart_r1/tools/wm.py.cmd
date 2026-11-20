@echo off
setlocal
if /I "%~1"=="build" goto build
if /I "%~1"=="guiflash" goto flash
"%~dp0..\..\tools\w803_env\Scripts\python.exe" -B -X utf8 "%~dp0..\..\w803_sdk\wm_iot_sdk\tools\wm\wm.py" %*
exit /b %errorlevel%
:build
"%~dp0..\..\tools\w803_env\Scripts\python.exe" -B -X utf8 "%~dp0ide_build.py"
exit /b %errorlevel%
:flash
"%~dp0..\..\tools\w803_env\Scripts\pythonw.exe" -B -X utf8 "%~dp0open_flash.py"
exit /b %errorlevel%
