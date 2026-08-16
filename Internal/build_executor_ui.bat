@echo off
echo Building Executor UI...
setlocal
cd /d "%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
if errorlevel 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
)
cl /EHsc /std:c++17 /O2 executor_ui.cpp imgui.cpp imgui_widgets.cpp imgui_draw.cpp imgui_tables.cpp imgui_impl_dx9.cpp imgui_impl_win32.cpp /Fe:executor_ui.exe user32.lib d3d9.lib
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] executor_ui.exe built
) else (
    echo [FAILED] Build failed
)
endlocal
pause
