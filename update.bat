@echo off
:: ZOR Auto-Updater - Double-click to check for updates
:: Runs the PowerShell update script
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File "%~dp0update.ps1" %*
pause
