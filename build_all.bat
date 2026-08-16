@echo off
setlocal enabledelayedexpansion
echo ========================================
echo   BUILD ALL - ZORMenu
echo ========================================
echo.

echo [1/3] Building Client DLL + Loader...
cd /d "%~dp0Client"
call build_client.bat
if %ERRORLEVEL% neq 0 (
    echo ERROR: Client build failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Building Kernel Driver...
cd /d "%~dp0Driver"
call build_driver.bat
if %ERRORLEVEL% neq 0 (
    echo WARNING: Driver build failed (pre-built sys still usable)
)

echo.
echo [3/3] Building Offset Grabber...
cd /d "%~dp0Tools\OffsetDumper"
call build_dumper.bat 2>nul
if %ERRORLEVEL% neq 0 echo WARNING: Grabber build failed (use existing graboffsets.exe)

echo.
echo ========================================
echo  OUTPUT FILES:
echo    %~dp0Client\x64\Release\ZORClient.dll
echo    %~dp0Client\ZORLoader\ZORLoader\x64\Release\ZORLoader.exe
echo    %~dp0Driver\nxs_drv.sys
echo    %~dp0Tools\OffsetDumper\graboffsets.exe
echo ========================================
echo.
pause