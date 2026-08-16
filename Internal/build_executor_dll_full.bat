@echo off
echo Building Full Executor DLL...
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
cl /EHsc /std:c++17 /O2 /LD executor_dll_full.cpp /Fe:executor_full.dll user32.lib
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] executor_full.dll built
) else (
    echo [FAILED] Build failed
)
endlocal
pause
