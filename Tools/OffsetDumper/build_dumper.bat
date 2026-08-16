@echo off
echo Building Offset Dumper...
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul
cl /EHsc /MT /std:c++20 /O2 /nologo dumper.cpp /link /MACHINE:x64 /OUT:nxs_dumper.exe /SUBSYSTEM:CONSOLE advapi32.lib shell32.lib user32.lib
if errorlevel 1 (echo [FAILED] & exit /b 1)
for %%F in (nxs_dumper.exe) do echo [SUCCESS] nxs_dumper.exe - %%~zF bytes
pause
