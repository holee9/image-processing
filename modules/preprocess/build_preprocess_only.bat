@echo off
REM Build script for xpe_preprocess module only
REM This avoids DCMTK dependency issues in the full project build

setlocal enabledelayedexpansion

REM Setup Visual Studio environment
call "\d\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"

set BUILD_DIR=%~dp0build
set SRC_DIR=%~dp0

echo Build directory: %BUILD_DIR%
echo Source directory: %SRC_DIR%

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure with CMake
echo Configuring with CMake...
cmake -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_CXX_STANDARD=17 ^
    -DBUILD_TESTING=ON ^
    "%SRC_DIR%"

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build with Ninja
echo Building with Ninja...
ninja

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo Build completed successfully!

REM Run tests
echo Running tests...
ctest --output-on-failure

endlocal
