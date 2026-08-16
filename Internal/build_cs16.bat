@echo off
echo Building CS 1.6 GoldSrc Extractor...
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
cl /EHsc /std:c++17 /O2 cs16_extractor.cpp /Fe:cs16_extractor.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] cs16_extractor.exe built
) else (
    echo [FAILED] Build failed - check Visual Studio installation
)
endlocal
pause
