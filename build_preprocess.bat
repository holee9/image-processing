@echo off
REM Build script for XPE Preprocess module

set MSBUILD="D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"

echo Building xpe_preprocess...
cd build\build_test\modules\preprocess

%MSBUILD% xpe_preprocess.vcxproj /p:Configuration=Debug /nologo /v:m

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build xpe_preprocess
    exit /b 1
)

echo.
echo Building test_xpe_preprocess...
%MSBUILD% xpe_preprocess_tests.vcxproj /p:Configuration=Debug /nologo /v:m

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build test_xpe_preprocess
    exit /b 1
)

echo.
echo Build completed successfully!
cd ..\..\..\..
