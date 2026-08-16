@echo off
setlocal
echo Building UI Test Harness EXE...

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 (echo [ERROR] No Visual Studio found & exit /b 1)

cl /EHa /MT /std:c++17 /D "_UNICODE" /D "UNICODE" /O2 /nologo ^
   /I "." /I "ImGui" /I "ImGui\backends" /I "Features" /I "Features\Misc" ^
   /I "Features\Aimbot" /I "Features\AntiAim" /I "Features\ESP" ^
   /I "Features\Exploits" /I "Features\Radar" /I "Features\Triggerbot" /I "Features\Visuals" ^
   test_harness.cpp ^
   ImGui\imgui.cpp ^
   ImGui\imgui_draw.cpp ^
   ImGui\imgui_widgets.cpp ^
   ImGui\imgui_tables.cpp ^
   ImGui\backends\imgui_impl_dx11.cpp ^
   ImGui\backends\imgui_impl_win32.cpp ^
   /link /MACHINE:x64 /SUBSYSTEM:WINDOWS /OUT:nxs_ui_test.exe ^
   d3d11.lib dxgi.lib dwmapi.lib winmm.lib user32.lib advapi32.lib
if errorlevel 1 (echo [FAILED] nxs_ui_test.exe & exit /b 1)
for %%F in (nxs_ui_test.exe) do echo [SUCCESS] nxs_ui_test.exe - %%~zF bytes

echo.
echo BUILD COMPLETE
pause