@echo off
REM Test compile single source file
REM Requires Visual Studio 2022 with C++ development tools

set CL_FLAGS=/std:c++17 /EHsc /I"D:\workspace-github\image-processing\modules\preprocess\include" /I"D:\workspace-github\image-processing\modules\common\include" /c

echo Testing compilation of fixed source files...
echo.

echo [1/4] Testing xpe_preprocess.cpp...
cl %CL_FLAGS% "D:\workspace-github\image-processing\modules\preprocess\src\xpe_preprocess.cpp" /Fo"test_preprocess.obj" 2>&1 | findstr /C:"error" /C:"succeeded"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile xpe_preprocess.cpp
    exit /b 1
)
echo OK: xpe_preprocess.cpp compiled successfully
echo.

echo [2/4] Testing xpe_offset.cpp...
cl %CL_FLAGS% "D:\workspace-github\image-processing\modules\preprocess\src\xpe_offset.cpp" /Fo"test_offset.obj" 2>&1 | findstr /C:"error" /C:"succeeded"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile xpe_offset.cpp
    exit /b 1
)
echo OK: xpe_offset.cpp compiled successfully
echo.

echo [3/4] Testing xpe_gain.cpp...
cl %CL_FLAGS% "D:\workspace-github\image-processing\modules\preprocess\src\xpe_gain.cpp" /Fo"test_gain.obj" 2>&1 | findstr /C:"error" /C:"succeeded"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile xpe_gain.cpp
    exit /b 1
)
echo OK: xpe_gain.cpp compiled successfully
echo.

echo [4/4] Testing xpe_defect.cpp...
cl %CL_FLAGS% "D:\workspace-github\image-processing\modules\preprocess\src\xpe_defect.cpp" /Fo"test_defect.obj" 2>&1 | findstr /C:"error" /C:"succeeded"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to compile xpe_defect.cpp
    exit /b 1
)
echo OK: xpe_defect.cpp compiled successfully
echo.

echo All tests passed! Cleaning up...
del *.obj 2>nul
echo Done.
