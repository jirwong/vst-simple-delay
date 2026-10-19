@echo off
setlocal enabledelayedexpansion
rem Build the Simple Delay plugin.
rem
rem   build.bat          build the plugin (Release)
rem   build.bat test     build, then run both offline test suites
rem
rem Finds CMake on PATH, falling back to the copy bundled with the Visual Studio
rem Build Tools. Configures the build tree on first run. Does not clone JUCE --
rem if it is missing, the clone command is printed.

cd /d "%~dp0"

set "CONFIG=Release"
set "RUN_TESTS=0"
if /i "%~1"=="test" set "RUN_TESTS=1"
if not "%~1"=="" if /i not "%~1"=="test" (
    echo error: unknown option: %~1
    echo usage: build.bat ^[test^]
    goto :fail
)

rem --- locate cmake: PATH first, then the Build Tools copy ---
set "CMAKE="
for /f "delims=" %%i in ('where cmake 2^>nul') do if not defined CMAKE set "CMAKE=%%i"
if not defined CMAKE (
    set "BUNDLED=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if exist "!BUNDLED!" set "CMAKE=!BUNDLED!"
)
if not defined CMAKE (
    echo error: cmake not found.
    echo   Install it with:  winget install Kitware.CMake   ^(then open a new shell^)
    echo   or install the Visual Studio Build Tools with the C++ workload.
    goto :fail
)

if not exist "libs\JUCE\CMakeLists.txt" (
    echo error: libs\JUCE is missing. Clone it once with:
    echo.
    echo   git clone --depth 1 --branch 8.0.15 --recurse-submodules --shallow-submodules https://github.com/juce-framework/JUCE.git libs/JUCE
    goto :fail
)

if not exist "build\CMakeCache.txt" (
    echo ==^> configuring build\
    "%CMAKE%" -S . -B build -G "Visual Studio 17 2022" -A x64 || goto :fail
)

echo ==^> building ^(%CONFIG%^)
"%CMAKE%" --build build --config %CONFIG% --parallel || goto :fail

if "%RUN_TESTS%"=="1" (
    echo ==^> running offline tests
    build\Tests\DelayLineTests.exe || goto :fail
    build\Tests\ProcessorTests.exe || goto :fail
)

echo ==^> done
echo     VST3:       build\SimpleDelay_artefacts\%CONFIG%\VST3\Simple Delay.vst3
echo     Standalone: build\SimpleDelay_artefacts\%CONFIG%\Standalone\Simple Delay.exe
goto :end

:fail
set "EXITCODE=1"

:end
rem Pause only when double-clicked, so the window does not vanish before it is read.
rem %cmdcmdline% contains the script's full path only in that case; a shell
rem invocation ("build.bat test") does not match, so it never stalls automation.
rem findstr, not find: a git-bash PATH shadows find with GNU find.
echo %cmdcmdline% | findstr /i /c:"%~f0" >nul && pause
if defined EXITCODE exit /b %EXITCODE%
exit /b 0
