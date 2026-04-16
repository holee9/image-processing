@echo off
REM Build script for XPE preprocess module
REM Requires Visual Studio 2022 with C++ development tools

set MSBUILD_PATH="D:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
set SOLUTION_FILE="D:\workspace-github\image-processing\modules\preprocess\build_test\xpe.sln"

echo Building XPE preprocess module...
echo.

%MSBUILD_PATH% %SOLUTION_FILE% /p:Configuration=Release /m /v:normal

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build succeeded!
) else (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
