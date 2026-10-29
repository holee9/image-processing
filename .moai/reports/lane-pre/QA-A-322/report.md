[2026-10-29 13:54 KST] [xpe-leader 대행 QA-A-322]

# QA-A-322 — main 0162bd31 CI dotnet-tests 실패 2건 해소 (Refs #278)

> 하네스가 작업자의 보고서 쓰기를 막아, 작업자가 돌려준 본문을 리더가 옮기고 §리더 확인을 덧붙였다.

## 결과
- 두 실패 로컬 재현, 원인 file:line 확인, 수정. 커밋 872897e6 (push 안 함).
- 수정 후 0162bd31 트리 + 수정 2파일 전체: 실패 0, 통과 859, 건너뜀 140, 전체 999.
- 수정 전 같은 환경: 실패 2, 통과 857, 건너뜀 140, 전체 999 (CI run 40810077627 과 같은 두 시험).

## 측정 환경
- xpe-pre 작업 트리에 무관한 미커밋 변경(gui·clients 11개)이 있어 git archive 로 커밋 트리를 임시 폴더에 풀어 시험.
- 71b87d63 트리에서는 NativeModuleSignatureParityTests 세 번째 실패(XPE_BPM_V2_FEATURE_COUNT is not a number) — main 이 ModuleSignatureParity.cs 를 이미 고쳤으므로 기준 트리를 0162bd31 로 함. 관련 5개 파일은 두 커밋 사이 차이 없음.
- 네이티브 DLL 없음(로컬 build/ci-common/bin 에 2026-10-04 xpe_common.dll 만) → 건너뜀 140(CI 1).

## 원인 1 — NativeCallWrapperGuardTests
- RealXpeBackend.cs:76(수정 전) Initialize 의 xpe_version() 이 InvokeNative 밖 → #110 알림 비우기 누락. a3a573a8 에서 추가.
- 면제 3개와 같은 종류 사유 없음(인스턴스 메서드, 같은 메서드의 GetDisplayVersion 이 이미 InvokeNative 사용).
- 수정: GetCommonVersion() => InvokeNative(...) 추가, Initialize 는 그 값 사용. 값 계산식 동일.

## 원인 2 — NonlinearityLutCallSiteTests
- GuiPreprocessRunner.cs:152 마지막 인자 config = RunPipelineOut 매개변수(:130-131), 유일한 호출부 :104-106 이 Serialize(new { ..., bypassNonlinearity = true, ... }) 로 생성. a3a573a8 에서 변경.
- 성질 유지, 표현만 바뀜. 설정이 운영자 선택으로 실행마다 달라지므로 상수로 되돌릴 수 없음 → 시험 수정.
- 새 시험: 호출 인자 → 매개변수 위치 → 호출부 1개 단언 → Serialize(new{...}) → bypassNonlinearity = true. 수정 전보다 엄격(실전달값 추적, 값 true 확인, 호출부 수 단언).
- 반증: bypassNonlinearity=false 로 바꾸면 실패(실패 1, 통과 0), 원복 후 cmp 일치.

## 수정 뒤 확인
- 대상 두 클래스 4개 통과, 출력 "config handed to the pipeline: config <- config = {..., bypassNonlinearity = true, ...}".
- 전체: 통과! 실패 0, 통과 859, 건너뜀 140, 전체 999.
- GUI 앱 빌드 exit 0, 오류 0, 경고 22(RealXpeBackend.cs 언급 0줄).
- 원문: test-output.txt.

## GUI 동작 영향
- 버전 표시: 값 동일, 호출 뒤 알림 비우기가 한 번 더 돎(GetDisplayVersion 과 같은 방식).
- 캘리브레이션 워크플로: GuiPreprocessRunner.cs 미변경, 영향 없음.

## 확인된 사실
- CI 실패 2건 같은 메시지로 재현. 원인 줄은 a3a573a8(git show). 실전달 설정에 bypassNonlinearity = true. 수정 후 실패 0, 앱 빌드 오류 0.

## 추정
- 관련 5개 파일이 71b87d63 와 0162bd31 에서 같으므로 다음 main 병합 결과도 같음. CI 로 확인 필요.

## 미확인
- 네이티브 DLL 포함 전체 실행(CI 에서만 확인).
- GUI 실제 실행으로 버전 표시·알림 목록 확인.
- dev/preprocess 단독 트리의 NativeModuleSignatureParityTests 실패는 main 병합 후 사라질 것으로 보이나 미확인.
- PipelineConfigJson 상수(:124)는 더 이상 쓰이지 않음(주석 "앱이 넘기는 설정", E2E 주석 NonlinearityBypassedScenarios.cs:22 참조) — 삭제는 리더 판정.

## 지운 임시 폴더 · 남긴 증거
- 지움: 스크래치 qa322-head(export 트리·빌드 산출물), qa322-*.log 6개.
- 남김: report.md, test-output.txt(로그 6개 sha256 포함).
- 이 작업이 남긴 프로세스 없음. VBCSCompiler PID 20036(13:50 시작)은 작업 전부터 있던 것.

## 리더 확인 (2026-10-29 13:56)
- `git show 872897e6` 로 diff 를 직접 읽었다. RealXpeBackend.cs 는 값 계산식이 그대로이고 InvokeNative 로만 감쌌다. 시험 변경은 단언을 약하게 만들지 않았다: 호출 인자를 선언·유일 호출부·직렬화 객체까지 따라가고 `bypassNonlinearity = true` 값까지 단언한다.
- 인덱스 61개 유지 확인.
- 판정: 받아들인다. main 병합 후 CI 로 dotnet-tests 를 확인한다(dev/preprocess CI 는 ci.yml:716-718 조건으로 dotnet-tests 를 돌리지 않는다).
- PipelineConfigJson 미사용 상수 정리는 이번 범위에서 하지 않는다(동작 무관, 별도 판단).
