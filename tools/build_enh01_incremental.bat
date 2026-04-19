@echo off
REM SPRINT-P1B-ENH-01 incremental build (configure already done)
setlocal enableextensions enabledelayedexpansion

set VS_ROOT=D:\Program Files\Microsoft Visual Studio\2022\Professional
set VCVARS="%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat"

set BUILD_DIR=D:\workspace-github\xpe-post\build\enh01_release

call %VCVARS% >nul || exit /b 1

pushd "%BUILD_DIR%"
cmake --build . --target xpe_enhance_basic xpe_enhance_basic_tests -j 8
set RC=%ERRORLEVEL%
popd

exit /b %RC%
