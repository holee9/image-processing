[2026-10-21 20:25 KST]
# QA-A-277 — 시험 계약 patch 적용 후 실패 6건 실행 확인

Refs #278. 원 조사: gui GUI-C-254. 결과: **6건 모두 PASS, patch 로컬 커밋 `de8a37cc240c4cfc699d04ecb0025f1166abc785` (push 없음).**

## 1. 실행 기준 (먼저 읽을 것)

- 기준 트리 HEAD `d9d5d7dc` (dev/preprocess). 제품 코드는 CI 실패 head `b140f7f2`(run 39571675063)와 같은 내용이고, 그 뒤 커밋은 보고서·증거 문서뿐이다.
- **xpe-pre 작업 트리에는 커밋되지 않은 gui 제품 변경이 있다**(`EvidenceSnapshotStore.cs` 98줄 삭제, `EvidenceExportResult.cs` 삭제 등 8개 파일). 이 트리에서 빌드한 앱은 CI 가 돌린 제품이 아니다. 실제로 이 트리에서 먼저 돌린 U01 은 CI 와 다른 사유(`The export did not name its snapshot`)로 실패했다(`evidence/run-U01b.log`). 그 변경은 건드리지 않았다.
- 그래서 **실행은 `git archive HEAD gui clients` 로 푼 깨끗한 사본 `build/qa277/src`(git 무시 경로)에서 했다.** 사본에 같은 patch 를 적용해 시험했고, 작업 트리에는 patch 6개 파일만 적용해 커밋했다.
- 네이티브 DLL: CI run 39571675063 의 산출물 4개(common·preprocess·post·dicom)를 `gh run download` 로 받아 `build/qa277/dlls` 에 합쳤다(CI 의 병합 규칙과 같음, 기존 파일은 덮어쓰지 않음). 보정 세트는 CI 와 같이 release `testdata-calib-v1` 을 받아 SHA256SUMS 6건 OK 확인 후 `xpe_calib_fixture_gen --expiry-ms 4102444800000` 으로 만들었다(`build/qa277/calib-real`, exit 0). 제조사 DLL·장치 접근 없음.
- 환경 변수는 CI 와 같다: `XPE_E2E_BACKEND=Native`, `XPE_NATIVE_DIR`(+`_EXCLUSIVE=1`), `XPE_E2E_ALLOW_GLOBAL_INPUT=1`(CI 가 같은 값을 줌; 이 시험들은 UIA 패턴만 쓰며 전역 입력 API 는 호출하지 않음), `XPE_C231_*`.
- patch: `evidence/e2e.patch.gz.b64.txt`(gzip+base64, 복원 아래), 복원한 원본 sha256 앞 16자 `4f8e1473acafd35f`(카드와 일치). `git apply --check --whitespace=error` OK, 적용 OK.
- 카드는 "E2ETests 의 시험 6개 파일"이라 했으나 patch 의 6개 파일 중 하나(`SettingsProcessingConnectionTests.cs`)는 `clients/ImageProcTest.IntegrationTests/` 에 있다. patch 가 정한 대로 6개 모두 적용·커밋했다. 리더가 범위를 확인해 주기 바란다.

## 2. 반증 1건 (patch 적용 전)

깨끗한 HEAD 사본에서 patch 를 되돌린 상태로 U01(`U01_CalibrationStages_AreDisabledAndMarked`)만 실행.

- 명령: `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --filter "FullyQualifiedName~U01_CalibrationStages_AreDisabledAndMarked"` — exit 1
- 원문: `CalibOffsetModeAuto is enabled, but no processing reads the Offset mode (#182).` / 표준 출력 `U01 CalibOffsetModeAuto enabled=True checked=True`
- CI 원문(`gui-e2e-native.log` 1217~1222행)과 같은 메시지·같은 표준 출력이다 → 낡은 시험 기대값이라는 가설에 부합. (`evidence/clean-refute-u01.log`)

## 3. 6건 실행 (시험당 1개, 동시 1개, 실행 직후 남은 프로세스 0)

공통 명령: `dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --filter "FullyQualifiedName~<이름>" --logger "console;verbosity=detailed" --blame-hang-timeout 5m --blame-hang-dump-type none` (깨끗한 사본, patch 적용, 환경 변수는 §1). 최종 판정 기록은 `evidence/clean-bs-*.log`.

| 시험 | exit | 결과 | 시간 |
|---|---:|---|---|
| U01_CalibrationStages_AreUsableExactlyWhenConnected (patch 가 U01 의 이름을 바꿈) | 0 | 통과 | 41 s |
| W13a_TheDefaultChain_PreprocessesAnOpenedFrame_WithoutRunPreprocessing | 0 | 통과 | 9 s |
| W13b_WithoutCalibrationData_ThePreprocessIsSkippedWithItsReason_NotSilentlyAndNotAsFailed | 0 | 통과 | 9 s |
| L07_AfterTheBackendIsReplaced_TheLanesSayTheyHoldNoAppliedImage | 0 | 통과 | 11 s |
| A01_About_ShowsTheBackendVersionTheStatusBarShows | 0 | 통과 | 2 s |
| NonlinearityBypassedScenarios.TheShippedPath_RunsNoNonlinearityStage_… | 0 | 통과 | 1 m 34 s |

각 실행 모두 `총 테스트 수: 1`, 건너뜀 0.

### 실행 중 있었던 일 (숨기지 않고 적는다)

- W13a·W13b 는 첫 두 번(작업 트리, 깨끗한 사본) **실패**했다: `the dialog (or a message box it raised) is still open after BM_CLICK on its confirm button`(`evidence/clean-W13a_*.log`, `clean-W13b_*.log`). CI 의 실패 사유(`Assert.Contains() Failure`)와 다르다. 원인은 내가 `XPE_C231_*` 경로를 `D:/...` 슬래시 형태로 준 것이었다(Open 대화 상자가 파일을 열지 못해 메시지 상자를 띄움). CI 와 같은 `D:\...` 역슬래시 형태로 다시 주자 같은 시험 두 건이 통과했다(`clean-bs-W13a_*.log`, `clean-bs-W13b_*.log`). 시험 코드·제품 코드는 그 사이에 바뀌지 않았다. 슬래시 형태가 왜 실패하는지(시험 쪽 경로 처리인지)는 원인까지 확인하지 않았다 — 환경 변수 형식에 대한 관측일 뿐이다.
- 나머지 4건을 역슬래시 형태로 재실행하던 중, 한 번은 경로 변수 계산 실수로 `XPE_*` 경로가 비어(`\build\...`) U01·L07·A01 이 통과, NonlinearityBypassed 는 결과 줄이 없었다. 이 실행은 증거로 쓰지 않고 올바른 경로로 4건을 다시 돌렸다(§3 표의 값). 폐기한 로그는 같은 이름 `clean-bs-*.log` 에 덮어써졌다.

## 4. 시험 기대값 변경 범위

patch 가 바꾼 것은 시험 6개 파일뿐이다(206 줄 추가, 50 줄 삭제). 이 세션은 기대값을 직접 고치거나 느슨하게 하지 않았다. 변경 내용의 타당성 판정은 이 카드의 범위가 아니다(실행 확인만).

## 5. 커밋과 상태

- 시험 patch 커밋: `de8a37cc240c4cfc699d04ecb0025f1166abc785` — 명시적 pathspec 6개, `Refs #278`, `QA-A-277`, `GUI-C-254` 표기. push 없음.
- 기존 dirty 파일(gui 제품 8개 등)은 스테이징·수정하지 않았다.
- 증거: `evidence/e2e.patch.gz.b64.txt` 와 이 보고서만 추적된다. `.log` 는 `.moai/reports/**` 무시 규칙 때문에 디스크에만 있다.
- **남은 프로세스 0**: 모든 실행 직후와 보고서 작성 직전에 `tasklist | grep -ciE 'ImageProcTest|testhost|xpe_ai_worker|dotnet'` = 0.

## 6. 미검증

- 전체 스위트(`Category!=Measurement`)는 돌리지 않았다. 6건 각각만 확인했다. 다른 시험이 이 patch 로 영향을 받는지는 CI 몫이다.
- 슬래시 경로에서 W13 이 실패한 구체 원인.
- 작업 트리의 미커밋 gui 제품 변경이 의도된 것인지(별도 확인 필요, 이 카드에서는 손대지 않음).

## 7. 증거 형식 정정 (2026-10-21 20:3x, CI 39591918725 text-lint)

patch 원본 `e2e.patch.txt` 에 줄 끝 공백 17건이 있어 text-lint 가 실패했다(제품 실패 아님). QA-A-275 와 같은 방식으로 patch 바이트는 그대로 두고 저장 형식만 gzip+base64 텍스트로 바꿨다. lint 규칙은 약화하지 않았고 patch 를 다듬지 않았다.
- 복원: `base64 -d e2e.patch.gz.b64.txt | gzip -dc > e2e.patch.txt`
- 왕복 검증(python gzip mtime=0): 원본 27731 바이트, sha256 `4f8e1473acafd35f15784d019f30183efacef641a45a17c11ebe8ec77e7c1423` = 복원본과 동일. 커밋 `de8a37cc` 에 들어간 시험 6개 파일은 바뀌지 않았다.
