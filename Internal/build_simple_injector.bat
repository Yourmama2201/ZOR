@echo off
echo Building Simple Injector (32-bit)...
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>&1
)
cl /EHsc /std:c++17 /O2 driver_injector.cpp /Fe:simple_injector.exe user32.lib
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] simple_injector.exe built
) else (
    echo [FAILED] Build failed
)
endlocal
pause
