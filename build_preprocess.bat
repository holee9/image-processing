@echo off
REM Build script for XPE preprocess module in xpe-pre worktree
REM Requires Visual Studio 2022 with C++ development tools

set MSBUILD_PATH=D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe
set PROJECT_ROOT=D:\workspace-github\xpe-pre

echo Building XPE preprocess module...
echo.

echo Source files in modules/preprocess/src:
dir /b "%PROJECT_ROOT%\modules\preprocess\src\*.cpp"

echo.
echo For full build with tests, use CMake:
echo   mkdir build
echo   cd build
echo   cmake .. -DBUILD_TESTS=ON -DBUILD_PREPROCESS=ON
echo   cmake --build . --config Release

echo.
echo Or run tests directly:
echo   cd build
echo   ctest --output-on-failure

pause
