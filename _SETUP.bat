@echo off
cd /d "%~dp0"
echo ============================================
echo  1. Enabling Test Signing Mode
echo ============================================
bcdedit /set testsigning on
if %errorlevel% neq 0 (
    echo [ERROR] Failed to enable test signing. Run this batch file AS ADMINISTRATOR.
    pause
    exit /b 1
)
echo [OK] Test signing enabled

echo.
echo ============================================
echo  2. REBOOT REQUIRED
echo ============================================
echo  Press any key to reboot now, or close this
echo  window to reboot manually later.
echo ============================================
echo.
echo  After reboot:
echo   1. Run nxs_loader.exe as Administrator
echo   2. Driver will load automatically
echo   3. Run nxs_dumper.exe to test the chain
echo ============================================
pause
shutdown /r /t 0
