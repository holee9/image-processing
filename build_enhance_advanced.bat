@echo off
REM Build script for XPE Enhance Advanced module

set MSBUILD="D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"

echo Building xpe_enhance_advanced...
cd build\build_test\modules\enhance_advanced

%MSBUILD% xpe_enhance_advanced.vcxproj /p:Configuration=Debug /nologo /v:m

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build xpe_enhance_advanced
    exit /b 1
)

echo.
echo Building test_xpe_enhance_advanced...
%MSBUILD% xpe_enhance_advanced_tests.vcxproj /p:Configuration=Debug /nologo /v:m

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to build test_xpe_enhance_advanced
    exit /b 1
)

echo.
echo Build completed successfully!
cd ..\..\..\..
