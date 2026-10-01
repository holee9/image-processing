@echo off
REM QA-B-171C audit10 final: full builds and full ctest from the PRESETS (both carry /WX).
REM QA-B-171C audit 12 fixes: final source, both presets, full ctest. Exit codes without a pipe.
REM Cache-vs-preset values are printed too (a stale dir hid AI from ci-post once).
REM Exit codes without a pipe.
call "D:\workspace-github\xpe-post\.moai\reports\lane-post\QA-B-120\_env.bat"
cd /d D:\workspace-github\xpe-post

cmake --preset ci-post > build\g183-q-post-cfg.txt 2>&1
echo ===POST_CFG=%errorlevel%===
cmake --build --preset ci-post > build\g183-q-post-build.txt 2>&1
echo ===POST_BUILD=%errorlevel%===
ctest --test-dir build\ci-post --output-on-failure > build\g183-q-post-ctest.txt 2>&1
echo ===POST_CTEST=%errorlevel%===

cmake --preset ci-ai > build\g183-q-ai-cfg.txt 2>&1
echo ===AI_CFG=%errorlevel%===
cmake --build --preset ci-ai > build\g183-q-ai-build.txt 2>&1
echo ===AI_BUILD=%errorlevel%===
set XPE_AI_EXPECT_ONNX=1
ctest --test-dir build\ci-ai --output-on-failure > build\g183-q-ai-ctest.txt 2>&1
echo ===AI_CTEST=%errorlevel%===
set XPE_AI_EXPECT_ONNX=

tasklist /FI "IMAGENAME eq xpe_ai_worker.exe" 2>nul | find /c "xpe_ai_worker.exe" > build\g183-q-left.txt
findstr /B "BUILD_AI: XPE_AI_STUB_BUILD: XPE_AI_USE_ONNXRUNTIME:" build\ci-post\CMakeCache.txt build\ci-ai\CMakeCache.txt > build\g183-q-cache.txt 2>&1
