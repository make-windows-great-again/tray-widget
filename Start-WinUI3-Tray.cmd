@echo off
setlocal
set "WINUI3_TRAY_EXE=%~dp0bin\WinUI3Tray\Release\YataWinUI3Tray.exe"
if not exist "%WINUI3_TRAY_EXE%" (
  echo Build first: pwsh -File tools\winui3-tray\Build.ps1 -Restore -SelfTest
  exit /b 1
)
start "" "%WINUI3_TRAY_EXE%" --show
