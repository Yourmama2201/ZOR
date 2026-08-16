@echo off
setlocal enabledelayedexpansion
echo Building Driver...

call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 (echo [ERROR] No Visual Studio found & exit /b 1)

if "%WDKContentRoot%"=="" set "WDKContentRoot=C:\Program Files (x86)\Windows Kits\10"

set WDK_VERSION=
for /f %%i in ('dir "%WDKContentRoot%\Include" /B /O-N') do (
    if exist "%WDKContentRoot%\Include\%%i\km\ntifs.h" (
        set WDK_VERSION=%%i
        goto :found_version
    )
)
:found_version

if "%WDK_VERSION%"=="" (
    echo [ERROR] No WDK with km headers found
    pause
    exit /b 1
)

echo Using WDK version: %WDK_VERSION%

cl /LD /GS /Zl /GR- /EHa- /Oi /Gy /Os /MT /nologo ^
   /kernel /guard:cf /Zp8 /Gz ^
   /D "_UNICODE" /D "UNICODE" /D "DBG=0" /D "NDEBUG" ^
   /D "WINVER=0x0A00" /D "NTDDI_VERSION=0x0A00000B" ^
   /D "_AMD64_" /D "AMD64" /D "DRIVER" ^
   /I "%WDKContentRoot%\Include\%WDK_VERSION%\km" ^
   /I "%WDKContentRoot%\Include\%WDK_VERSION%\shared" ^
   /I "%WDKContentRoot%\Include\%WDK_VERSION%\um" ^
   zordriver.c ^
   /link /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /MACHINE:x64 ^
   /guard:cf /NODEFAULTLIB /BASE:0x100000000 /OPT:REF /OPT:ICF ^
   /LIBPATH:"%WDKContentRoot%\Lib\%WDK_VERSION%\km\x64" ^
   /LIBPATH:"%WDKContentRoot%\Lib\%WDK_VERSION%\um\x64" ^
   ntoskrnl.lib hal.lib wdmsec.lib BufferOverflowFastFailK.lib ^
   /OUT:nxs_drv.sys
if exist nxs_drv.sys (echo [SUCCESS] Driver built) else echo [FAILED]
pause