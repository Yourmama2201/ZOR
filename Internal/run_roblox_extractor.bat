@echo off
setlocal enabledelayedexpansion

echo ========================================
echo Roblox Data Extractor - Automated
echo ========================================
echo.

REM Check if driver file exists
if not exist "..\Driver\zordriver.sys" (
    echo [ERROR] Driver not found: ..\Driver\zordriver.sys
    echo Please build the driver first.
    pause
    exit /b 1
)

REM Check if extractor exists
if not exist "roblox_extractor.exe" (
    echo [ERROR] Extractor not found: roblox_extractor.exe
    echo Please build the extractor first.
    pause
    exit /b 1
)

echo [1/4] Loading kernel driver...
sc create ZorDriver type= kernel binPath= "%CD%\..\Driver\zordriver.sys" start= demand >nul 2>&1
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
echo [2/4] Waiting for Roblox to be running...
echo Please start Roblox and join a game.
echo.

timeout /t 3 >nul

REM Check if Roblox is running
tasklist /FI "IMAGENAME eq RobloxPlayerBeta.exe" 2>NUL | find /I /N "RobloxPlayerBeta.exe">NUL
if "%ERRORLEVEL%" neq 0 (
    echo [WARNING] Roblox not detected running.
    echo The extractor will attempt to find it anyway.
    echo.
)

echo [3/4] Running Roblox extractor...
echo.

roblox_extractor.exe

echo.
echo [4/4] Extraction complete!
echo.

REM Check if header file was generated
if exist "roblox_offsets.hpp" (
    echo [SUCCESS] Generated: roblox_offsets.hpp
    echo.
    type roblox_offsets.hpp
) else (
    echo [INFO] No header file generated - patterns may need updating
)

echo.
echo ========================================
echo Roblox extraction complete!
echo ========================================
echo.
echo What would you like to add next?
echo.
echo Options:
echo 1. Add more Roblox patterns
echo 2. Add ESP rendering
echo 3. Add aimbot functionality  
echo 4. Add Lua script execution
echo 5. Add teleportation
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
