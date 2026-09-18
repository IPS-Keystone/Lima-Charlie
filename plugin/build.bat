@echo off
rem Builds the Lima Charlie TeamSpeak 3 plugin (build\limacharlie_win64.dll) and offline tests (build\lc_tests.exe), runs the tests,
rem then packages build\LimaCharlie_<version>.ts3_plugin with the radio sounds.
setlocal
pushd "%~dp0"

where cl >nul 2>nul
if not errorlevel 1 goto :havecl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :novs
rem "call" stops cmd stripping the quotes around a path with spaces
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH goto :novs
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul
where cl >nul 2>nul
if errorlevel 1 goto :novs
:havecl

if not exist build mkdir build
if not exist build\tests mkdir build\tests

set COMMON=/nologo /O2 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN
set OURS=%COMMON% /W4 /std:c11 /Isrc /Ithird_party\ts3sdk /Ithird_party\cjson
set LIBS=shell32.lib ole32.lib uuid.lib winmm.lib
set SOURCES=src\lc_core.c src\lc_peers.c src\lc_game_state.c src\lc_profile_files.c src\lc_log.c src\lc_audio.c src\lc_direct_voice.c src\lc_radio.c src\lc_transmissions.c src\lc_sounds.c
set TEST_SOURCES=src\lc_direct_voice.c src\lc_peers.c src\lc_game_state.c src\lc_log.c src\lc_radio.c src\lc_transmissions.c

rem Third-party code at a lower warning level so our own warnings stay visible
cl %COMMON% /W1 /c third_party\cjson\cJSON.c /Fobuild\cJSON.obj
if errorlevel 1 goto :fail

cl %OURS% /LD src\lc_plugin.c %SOURCES% build\cJSON.obj /Fobuild\ /Fe:build\limacharlie_win64.dll /link %LIBS%
if errorlevel 1 goto :fail

cl %OURS% test\lc_tests.c %TEST_SOURCES% build\cJSON.obj /Fobuild\tests\ /Fe:build\lc_tests.exe
if errorlevel 1 goto :fail

echo.
build\lc_tests.exe
if errorlevel 1 goto :fail

rem ---- Package: package.ini, plugins\limacharlie_win64.dll, plugins\limacharlie\sounds\<set>\*.wav ----
set "LC_VERSION="
for /f "tokens=3" %%v in ('findstr /c:"define LC_PLUGIN_VERSION" src\lc_version.h') do set "LC_VERSION=%%~v"
if not defined LC_VERSION goto :fail
set "PACKAGE=build\LimaCharlie_%LC_VERSION%.ts3_plugin"

if exist build\package rmdir /s /q build\package
mkdir build\package\plugins\limacharlie\sounds
(
echo Name = Lima Charlie
echo Type = Plugin
echo Author = Lima Charlie
echo Version = %LC_VERSION%
echo Platforms = win64
echo Description = "Arma Reforger radio and proximity voice integration."
) > build\package\package.ini
copy /y build\limacharlie_win64.dll build\package\plugins\ >nul
xcopy /e /i /q /y sounds build\package\plugins\limacharlie\sounds >nul
if errorlevel 1 goto :fail

rem Laid out like TFAR's package (directory entries, file attributes): TeamSpeak's installer extracted empty
rem files from the plain zip .NET produced
python package.py build\package "%PACKAGE%"
if errorlevel 1 goto :fail

echo.
echo Built build\limacharlie_win64.dll and %PACKAGE%
popd
exit /b 0

:novs
echo MSVC not found. Install Visual Studio 2022 Build Tools with the "Desktop development with C++" workload.
popd
exit /b 1

:fail
echo Build failed.
popd
exit /b 1
