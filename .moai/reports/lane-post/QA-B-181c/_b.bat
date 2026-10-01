@echo off
call "D:\workspace-github\xpe-post\.moai\reports\lane-post\QA-B-120\_env.bat"
cd /d D:\workspace-github\xpe-post
cmake --build --preset ci-post > build\g181c-build.txt 2>&1
echo ===BUILD=%errorlevel%===
