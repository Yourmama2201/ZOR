@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if not exist "cheat.dll" (
    echo [ERROR] cheat.dll not found in this folder. Copy your built DLL here first.
    exit /b 1
)

rc /fo loader.res source.rc >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

cl /nologo /W3 /EHsc /std:c++17 /Fe:injector.exe loader.cpp loader.res user32.lib >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

del loader.obj loader.res >nul 2>&1
echo [OK] injector.exe built. Usage:
echo      injector.exe --gen 30   -> generate a 30-day license
echo      injector.exe            -> wait for game, inject cheat.dll
endlocal
