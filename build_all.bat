@echo off
setlocal enabledelayedexpansion
echo ========================================
echo   BUILD ALL - ZORMenu
echo ========================================
echo.

echo [1/3] Building Client DLL + Loader...
cd /d "%~dp0Client"
call build_client.bat
if %ERRORLEVEL% neq 0 (
    echo ERROR: Client build failed!
    pause
    exit /b 1
)

echo.
echo [2/3] Building Kernel Driver...
cd /d "%~dp0Driver"
call build_driver.bat
if %ERRORLEVEL% neq 0 (
    echo WARNING: Driver build failed (pre-built sys still usable)
)

echo.
echo [3/3] Building Offset Grabber...
cd /d "%~dp0Tools\OffsetDumper"
call build_dumper.bat 2>nul
if %ERRORLEVEL% neq 0 echo WARNING: Grabber build failed (use existing graboffsets.exe)

echo.
echo [3b] Building kdmapper (BYOVD mapper)...
if exist "%~dp0Tools\kdmapper\kdmapper.exe" (
    echo [SKIP] kdmapper.exe already built
) else (
    if exist "%~dp0Tools\kdmapper\kdmapper.sln" (
        for %%M in (
            "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
            "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
        ) do if exist %%M set "MSB=%%~M"
        if defined MSB (
            "%MSB%" "%~dp0Tools\kdmapper\kdmapper.sln" /p:Configuration=Release /p:Platform=x64 /t:Build /m /v:m /nologo
            if exist "%~dp0Tools\kdmapper\x64\Release\kdmapper_Release.exe" (
                copy /y "%~dp0Tools\kdmapper\x64\Release\kdmapper_Release.exe" "%~dp0Tools\kdmapper\kdmapper.exe" >nul
                del "%~dp0Tools\kdmapper\x64\Release\kdmapper_Release.pdb" 2>nul
                echo [SUCCESS] kdmapper.exe built
            ) else (
                echo [FAILED] kdmapper build failed
            )
        ) else (
            echo [FAILED] MSBuild not found
        )
    ) else (
        echo [WARNING] kdmapper source missing - loader will use SCM fallback
    )
)

echo.
echo ========================================
echo  OUTPUT FILES:
echo    %~dp0Client\x64\Release\ZORClient.dll
echo    %~dp0Client\ZORLoader\ZORLoader\x64\Release\ZORLoader.exe
echo    %~dp0Driver\nxs_drv.sys
echo    %~dp0Tools\OffsetDumper\graboffsets.exe
echo    %~dp0Tools\kdmapper\kdmapper.exe
echo ========================================
echo.
pause