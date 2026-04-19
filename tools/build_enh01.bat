@echo off
REM SPRINT-P1B-ENH-01 build script
REM Configures and builds xpe_enhance_basic + its tests in Release mode.
setlocal enableextensions enabledelayedexpansion

set VS_ROOT=D:\Program Files\Microsoft Visual Studio\2022\Professional
set VCVARS="%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"
set VCPKG_TOOLCHAIN=%VS_ROOT%\VC\vcpkg\scripts\buildsystems\vcpkg.cmake

set REPO_ROOT=D:\workspace-github\xpe-post
set BUILD_DIR=%REPO_ROOT%\build\enh01_release

echo === Setting up MSVC environment ===
call %VCVARS% || exit /b 1

echo === Configuring CMake (Release, enhance_basic only) ===
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

cmake -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DBUILD_SHARED_LIBS=ON ^
    -DBUILD_TESTS=ON ^
    -DBUILD_GSVG=OFF ^
    -DBUILD_PREPROCESS=OFF ^
    -DBUILD_ENHANCE_BASIC=ON ^
    -DBUILD_ENHANCE_ADVANCED=OFF ^
    -DBUILD_AI=OFF ^
    -DBUILD_DISPLAY=OFF ^
    -DBUILD_DICOM=OFF ^
    -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
    -DVCPKG_MANIFEST_DIR="%REPO_ROOT%\third_party" ^
    "%REPO_ROOT%" || (popd & exit /b 1)

echo === Building xpe_enhance_basic + tests ===
cmake --build . --target xpe_enhance_basic xpe_enhance_basic_tests -j 8 || (popd & exit /b 1)

popd
echo === Done. Binaries in %BUILD_DIR%\bin ===
endlocal
