@echo off
REM Build script for Gravel (Windows equivalent of Makefile)

set BUILD_DIR=build
set BINARY=%BUILD_DIR%\bin\Debug\Gravel.exe

if "%1"=="" goto all
if "%1"=="build" goto build
if "%1"=="run" goto run
if "%1"=="configure" goto configure
if "%1"=="clean" goto clean
if "%1"=="all" goto all
goto usage

:build
echo Building project...
cmake --build %BUILD_DIR%
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)
goto :eof

:run
if not exist %BINARY% (
    echo Binary not found. Building first...
    call :build
)
echo Running Gravel...
%BINARY%
goto :eof

:configure
echo Configuring project...
cmake -S . -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Release
goto :eof

:clean
echo Cleaning build...
cmake --build %BUILD_DIR% --target clean
goto :eof

:all
call :build
call :run
goto :eof

:usage
echo Usage: build.bat [target]
echo Targets:
echo   build      - Build the project
echo   run        - Run the project (builds if needed)
echo   configure  - Configure CMake
echo   clean      - Clean build files
echo   all        - Build and run (default)
goto :eof
