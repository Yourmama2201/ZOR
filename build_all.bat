@echo off
setlocal enabledelayedexpansion
echo ========================================
echo   BUILD ALL
echo ========================================
echo.

echo [1/2] Building Client DLL + Loader...
cd /d "%~dp0Client"
call build_client.bat
if %ERRORLEVEL% neq 0 (
    echo ERROR: Client build failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Build complete - no WDK available for driver
echo     Pre-built driver at: %~dp0Driver2.0\x64\Release\nxs_drv.sys
echo.
echo ========================================
echo  OUTPUT FILES:
echo    %~dp0Client\nxs_core.dll
echo    %~dp0Client\ZORLoader\ZORLoader\nxs_loader.exe (rename ZORLoader folder)
echo ========================================
echo.
pause
