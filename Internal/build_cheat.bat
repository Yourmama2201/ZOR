@echo off
echo Building MW2022 Cheat...
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
cl /EHsc /std:c++17 /O2 mw2022_cheat.cpp /Fe:mw2022_cheat.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] mw2022_cheat.exe built
) else (
    echo [FAILED] Build failed - check Visual Studio installation
)
endlocal
pause
