@echo off
setlocal enabledelayedexpansion
echo Building Client DLL...

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 (echo [ERROR] No Visual Studio found & exit /b 1)

echo [1/2] nxs_core.dll...
cl /EHsc /MT /std:c++17 /D "_UNICODE" /D "UNICODE" /O2 /nologo ^
   /LD ^
   /I "." /I "ImGui" /I "ImGui\backends" /I "Features" /I "Features\Misc" ^
   /I "Features\Aimbot" /I "Features\AntiAim" /I "Features\ESP" ^
   /I "Features\Exploits" /I "Features\Radar" /I "Features\Triggerbot" /I "Features\Visuals" ^
   main.cpp ^
   ImGui\imgui.cpp ^
   ImGui\imgui_draw.cpp ^
   ImGui\imgui_widgets.cpp ^
   ImGui\imgui_tables.cpp ^
   ImGui\backends\imgui_impl_dx11.cpp ^
   ImGui\backends\imgui_impl_win32.cpp ^
   /link /MACHINE:x64 /OUT:nxs_core.dll ^
   d3d11.lib dxgi.lib dwmapi.lib winmm.lib user32.lib advapi32.lib
if errorlevel 1 (echo [FAILED] nxs_core.dll & exit /b 1)
for %%F in (nxs_core.dll) do echo [SUCCESS] nxs_core.dll - %%~zF bytes

echo.
echo [2/2] nxs_loader.exe...
cd ZORLoader\ZORLoader
if errorlevel 1 (echo [FAILED] Cannot find loader directory & exit /b 1)
cl /EHsc /MT /std:c++20 /O2 /nologo ZORLoader.cpp /link /MACHINE:x64 /OUT:nxs_loader.exe user32.lib advapi32.lib wintrust.lib
if errorlevel 1 (echo [FAILED] nxs_loader.exe & exit /b 1)
for %%F in (nxs_loader.exe) do echo [SUCCESS] nxs_loader.exe - %%~zF bytes

echo.
echo ALL BUILDS COMPLETE
pause
