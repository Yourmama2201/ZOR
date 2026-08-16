@echo off
setlocal enabledelayedexpansion

echo ========================================
echo CS 1.6 GoldSrc Data Extractor - Automated
echo ========================================
echo.

REM Check if driver file exists
set "DRIVER_PATH=%~dp0..\Driver\zordriver.sys"
if not exist "%DRIVER_PATH%" (
    echo [ERROR] Driver not found: %DRIVER_PATH%
    echo Please build the driver first.
    pause
    exit /b 1
)

REM Check if extractor exists
set "EXTRACTOR_PATH=%~dp0cs16_extractor.exe"
if not exist "%EXTRACTOR_PATH%" (
    echo [ERROR] Extractor not found: %EXTRACTOR_PATH%
    echo Please build the extractor first.
    pause
    exit /b 1
)

echo [1/4] Loading kernel driver...
sc create ZorDriver type= kernel binPath= "%DRIVER_PATH%" start= demand >nul 2>&1
if errorlevel 1 (
    echo Driver already exists or failed to create
) else (
    echo Driver service created
)

sc start ZorDriver >nul 2>&1
if errorlevel 1 (
    echo Driver already running or failed to start
) else (
    echo Driver started successfully
)

echo.
echo [2/4] Waiting for CS 1.6 to be running...
echo Please start CS 1.6 and join a server.
echo.

timeout /t 3 >nul

REM Check if CS 1.6 is running (try common process names)
set cs_running=0
tasklist /FI "IMAGENAME eq hl.exe" 2>NUL | find /I /N "hl.exe">NUL
if not errorlevel 1 set cs_running=1

tasklist /FI "IMAGENAME eq cstrike.exe" 2>NUL | find /I /N "cstrike.exe">NUL
if not errorlevel 1 set cs_running=1

tasklist /FI "IMAGENAME eq cs16.exe" 2>NUL | find /I /N "cs16.exe">NUL
if not errorlevel 1 set cs_running=1

if %cs_running% equ 0 (
    echo [WARNING] CS 1.6 not detected running.
    echo The extractor will attempt to find it anyway.
    echo.
)

echo [3/4] Running CS 1.6 GoldSrc extractor...
echo.

cs16_extractor.exe

echo.
echo [4/4] Extraction complete!
echo.

REM Check if header file was generated
if exist "cs16_offsets.hpp" (
    echo [SUCCESS] Generated: cs16_offsets.hpp
    echo.
    type cs16_offsets.hpp
) else (
    echo [INFO] No header file generated - patterns may need updating
)

echo.
echo ========================================
echo CS 1.6 extraction complete!
echo ========================================
echo.
echo What would you like to add next?
echo.
echo Options:
echo 1. Add more GoldSrc patterns
echo 2. Add ESP rendering
echo 3. Add aimbot functionality  
echo 4. Add bunnyhop
echo 5. Add no recoil
echo 6. Switch back to MW2022
echo 7. Other (tell me what you want)
echo.

set /p choice="Enter your choice (1-7): "

echo.
echo You selected: !choice!
echo.
echo Tell me what you want to add next and I'll implement it.
echo.
pause
