[2026-10-24 10:51 KST] [xpe-gui]

수신·착수: GUI-HOOK-1 / Refs #279 카드 및 20261024-1056-gui 지시 전문 확인.
대상 두 C# 파일 공백만 수정하며 정상 gate/훅으로 검증한다. 기존 tracked dirty5파일 보존. 훅우회·이력재작성·검증비활성화·설정변경·push 없음.

실행 도구 PATH에는 moai가 없어 기존 Machine/User PATH를 현재 작업 프로세스에만 읽어 명령을 찾는다. 설치나 영구 설정 변경은 하지 않는다.

## 재현·공백 수정·검증

- 기존 설치 `C:/Users/drake/AppData/Local/Programs/moai/moai.exe` 및 `C:/Program Files/dotnet/dotnet.exe` 사용. CLAUDE_PROJECT_DIR은 xpe-gui로 설정했다. SKIP_MOAI_PRECOMMIT가 있으면 실행을 중단하도록 했다. 훅 설정은 변경하지 않았다.
- gate-before.log: rc1 dotnet format WHITESPACE80건 재현. 실제로 NativeDependencyLoader7건, NativePreprocessPreviewService34건, NativePresentationExportService39건이었다. 카드의 두 파일 외 세 번째 파일도 처음부터 오류가 있었다.
- 두 파일만 dotnet format whitespace --include로 수정 후 gate-after-two-files.log에서 세 번째 파일39건만 남았다. 카드3항(소유 범위 안 최소 수정)에 따라 GUI 소유 세 번째 파일도 공백만 수정했다. 다른 서비스 로직/파일 변경은 없다.
- gate-after.log: rc0. dotnet format --verify-no-changes 및 dotnet test 실행 단계 통과. typecheck 기본 명령 미설정과 sg 미설치로 스캔 미실행 경고는 gate 자체 기존 동작이며, 검증을 끄거나 설치/설정 변경하지 않았다. 전체 제품 시험 통과나 ast-grep 스캔 통과로 확대하지 않는다.
- build.log: ImageProcTest.csproj Release --no-restore 컴파일1회 rc0, 경고0/오류0. 별도 시험/GUI 실행 없음(명시된 gate 내부 dotnet test만 수행).
- 세 파일 git diff -w: rc0/표준출력0바이트. 별도 검사에서도 모든 공백 문자를 제거한 전후 바이트 동일. 의미/문자열/주석 변경 없음.
- git diff --stat: NativeDependencyLoader14줄, NativePreprocessPreviewService46줄, NativePresentationExportService78줄; 총3파일69삽입/69삭제(공백만). 기존 tracked dirty5파일은 stage하지 않는다.

정상 pre-commit에서 moai gate가 실제 발견되는 PATH로 새 커밋을 시도한다. 실패 시 우회 없이 오류 원문/스테이징 상태를 보고한다. commit.log는 커밋 실행 중 생성되므로 최초 commit에는 포함되지 않으며 결과 SHA는 후속 회신으로 전달한다. push/다른 레인 적용 없음.
