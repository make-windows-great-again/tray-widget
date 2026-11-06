@echo off
"%~dp0YataNativeFramework.exe" --exit
timeout /t 1 /nobreak >nul
start "" "%~dp0YataNativeFramework.exe" --native-vpn
