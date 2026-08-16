@echo off
setlocal enabledelayedexpansion
echo Building ZORMenu (Client DLL + Loader)...

set "MSB=%~1"
if "%MSB%"=="" (
    for %%M in (
        "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
        "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
    ) do if exist %%M set "MSB=%%~M"
)
if "%MSB%"=="" (echo [ERROR] MSBuild.exe not found & exit /b 1)

echo [1/2] ZORClient.dll...
"%MSB%" "%~dp0ZORClient.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build /m /v:m /nologo
if errorlevel 1 (echo [FAILED] ZORClient.dll & exit /b 1)
for %%F in ("%~dp0x64\Release\ZORClient.dll") do echo [SUCCESS] ZORClient.dll - %%~zF bytes

echo.
echo [2/2] ZORLoader.exe...
"%MSB%" "%~dp0ZORLoader\ZORLoader\ZORLoader.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Build /m /v:m /nologo
if errorlevel 1 (echo [FAILED] ZORLoader.exe & exit /b 1)
for %%F in ("%~dp0ZORLoader\ZORLoader\x64\Release\ZORLoader.exe") do echo [SUCCESS] ZORLoader.exe - %%~zF bytes

echo.
echo ALL BUILDS COMPLETE
endlocal
pause