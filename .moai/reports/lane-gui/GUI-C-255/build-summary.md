[2026-10-23 16:47 KST] [xpe-gui] 빌드 완료 — GUI-C-255 0단계 (Refs #278)

- main 병합: `git merge main`, 종료 0, 커밋 `aca11dfc`, 충돌 없음.
- `tools/ci/Invoke-LocalVsCommonBuild.ps1 -BuildDir build/gui-c255-native`: VS 환경 초기화 및 구성 후 빌드 종료 1. vcpkg 후처리가 PATH의 `powershell.exe`를 찾지 못함. 명령·원문은 report.md 및 run-20261023-162704/native-retry.log에 기록.
- PowerShell 실행 경로를 작업 프로세스 PATH에 보완한 뒤 `native-retry.ps1`에서 MSVC 환경 재초기화 후 `cmake --build build/gui-c255-native --target xpe_preprocess xpe_display --parallel 4`: 종료 0. CMake 구성 `RelWithDebInfo`.
- `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c RelWithDebInfo -p:Platform=x64`: 종료 0, 경고 22개/오류 0개.
- 전체 시험 스위트 미실행. 단계별 GUI 관측 및 저장물 비교는 별도 기록.

| 산출물 | SHA-256 |
|---|---|
| build/gui-c255-native/bin/xpe_preprocess.dll | 650c46773bbbb35e65e22c2d94fbac5bfc1b15fc8f1c0102b8e741454f2f1584 |
| build/gui-c255-native/bin/xpe_display.dll | 3b14d471ecaefab3ebf38197eca0d7dbc3f779e6d574363f942a3c2c75b741e0 |
| build/gui-c255-native/bin/xpe_common.dll | c06a82a73c631b7ef86ca05d8ac8be2abacdcfa20bc2d91f5952b9a34868946c |
| gui/ImageProcTest/bin/x64/RelWithDebInfo/net8.0-windows/ImageProcTest.exe | 0b02ff282685107a47a41422fecfa682b52952eff6c3f53cb74b2398cf449c1a |

시각은 작성 직전 `date.exe -Iseconds`: `2026-10-23T16:47:21+09:00`.
