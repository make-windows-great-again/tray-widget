@echo off
setlocal
set "FRAMEWORK_EXE=%~dp0bin\Release\YataNativeFramework.exe"
if not exist "%FRAMEWORK_EXE%" (
  echo Release executable not found. Build the project first.
  pause
  exit /b 1
)
"%FRAMEWORK_EXE%" --diagnose
if errorlevel 1 (
  echo.
  echo Launch this file from File Explorer on your normal Windows desktop.
  echo An isolated desktop cannot connect to the Explorer taskbar.
  pause
  exit /b 3
)
start "" "%FRAMEWORK_EXE%" --controls --show
