[2026-10-14 18:32 KST] GUI-C-243 보고서 — 고정 모드(XPE_NATIVE_DIR_EXCLUSIVE=1)의 의존 DLL 선적재·늦게 로드되는 경로 (Refs #251, #267, Codex #195 지적 1)

기준 487a63ff 위의 새 커밋. 푸시는 리더가 한다.

## 1. 무엇을 바꿨나

| 항목 | 변경 |
|---|---|
| 팩토리 점검 (`RealXpeBackend.WhyUnusable`) | 고정 모드에서 주 DLL 을 LoadLibraryEx(DLL 폴더 + System32)로 적재. 실패 사유(`loadError`)를 Mock 사유에 포함 |
| 의존 DLL 선적재 (`NativeDependencyLoader`) | 고정 모드에서 같은 제한 적재(`OracleModuleConfinement.TryLoad`). 일반 모드는 기존 `NativeLibrary.TryLoad` |
| 모듈 감사 (`NativeModuleAudit`) | `Outsiders`, `ThrowIfOutsidePinnedFolder(feature)`, `AiWorkerPathBeside`, `DescribeAndCheckAiWorker` 추가. 경고만 하고 계속하던 경로를 기능 단위 실패로 |
| DICOM (`GuiDicomNative`) | `xpe_dicom_open` 과 `xpe_dicom_write` 직후 감사. 고정 폴더 밖 모듈이면 실패 |
| AI (`GuiAiRunner`) | `xpe_ai_init` 호출 전에 감사하고, 기대하는 `xpe_ai_worker.exe` 경로를 진단 줄에 기록 |
| 시험 | 아래 3절 |

## 2. 실측: 어떤 DLL 이 무엇을 가져오나 (`dll_imports.txt`)

`xpe_common.dll` 은 `spdlog.dll` 을 가져온다(`fmt.dll` 이 아니다). 첫 번째 시험은 fmt.dll 을 지웠는데도 Native 로 시작했다.
PE 임포트 표를 읽고 나서 시험이 지우는 DLL 을 spdlog 로 바로잡았다. `fmt.dll` 은 이름이 이미 적재돼 있으면 경로와 무관하게 쓰이기 때문에 제거해도 시험이 판별력이 없었다.

## 3. 시험과 반증 (`falsify.txt`)

시험:
- E2E `PinnedFolderMissingARuntimeDll_AWorkingCopyBesideTheExe_IsNotTaken_MockWithTheReason`: 고정 폴더에서 spdlog 를 빼고, 앱 폴더에는 동작하는 spdlog 를 둔다. 기대: Mock + 사유, 앱 폴더 사본은 쓰지 않음.
- 통합 `PinnedNativeSearchGuardTests` 3건: DICOM 감사 위치, AI 감사 순서(probe → 모듈 감사 → worker 확인 → init), 적재 방식 3곳.
- SelfCheck 시나리오 11 확장: 데코이와 AI worker 경로.

반증 5팔(원본 수정 → 재빌드 → 실행 → 바이트 동일 복원 → 재빌드):

| 팔 | 되돌린 것 | 빨강이 된 시험 |
|---|---|---|
| A | 팩토리 점검을 기본 검색으로 | 통합 가드 + E2E 데코이 시험 (동작으로 확인) |
| B | 의존 DLL 선적재를 기본 검색으로 | 통합 가드만 (E2E 는 초록) |
| C | 감사가 경고만 하고 계속 | SelfCheck 시나리오 11 |
| D | DICOM 직후 감사 제거 | 통합 가드만 |
| E | AI 시작 전 감사 제거 | 통합 가드만 |

복원 후 전부 초록.

## 4. 회귀 확인

| 실행 | 결과 |
|---|---|
| Native E2E 전체 (실데이터 환경 변수, 전역 입력 미설정, `native_e2e_full.txt`) | 337건 중 통과 304 / 실패 15 / 건너뜀 18, 70분. W12 뒤에서 멈추지 않고 끝까지 감 |
| 실패 15건 | 전부 신선도 가드(`ImageProcTest.dll is older than …`): 레거시 앱·gui E2E 러너를 다시 짓지 않은 탓. 제 소스 변경의 결과가 아니다 |
| 재빌드 후 해당 5개 클래스 재실행 (`native_e2e_rerun_failed.txt`) | 37건 전부 통과 (실패 0 / 건너뜀 0) |
| IntegrationTests 전체 (`integration_full.txt`) | 첫 실행 966 통과 / 2 실패 / 2 건너뜀. 실패 중 `AiDiagnosticsTests` 는 진단 줄에 `{workerDescription}` 이 붙은 새 형식을 시험이 몰라서였다 → 시험 기대값 갱신. 최종: 967 통과 / 1 실패 / 2 건너뜀 |
| 남은 실패 1건 | `QuickStartCitationTests.TheScriptsAndCommandsTheQuickStartGives_…`: `tools/Get-CiNativeDlls.ps1` 이 이 브랜치에 없다(PR #266 의 파일). 통합 브랜치에서는 해소됨 |
| SelfCheck 전체 (`selfcheck_full.txt`) | 30 시나리오 통과, exit 0 |

가용 메모리: 실행 전 5908 MB, 후 6408 MB. 남은 ImageProcTest / testhost / xpe_ai_worker 프로세스 없음.

## 5. 한계 (입증하지 못한 것)

- DICOM 내보내기와 AI 시작의 실패 처리는 실제 DICOM·모델 없이 실행되지 않았다. 호출 위치는 소스 모양 가드로만 고정돼 있고(반증 D, E), 동작 시험이 아니다.
- 의존 DLL 선적재의 제한 적재(반증 B)도 텍스트 가드만 잡는다. E2E 로 구분 가능한 입력(선적재 경로만 다르게 동작하는 DLL 구성)을 만들지 못했다.
- 감사는 호출 시점에 이미 로드된 모듈만 본다. AI worker 프로세스의 생성 시점 관측은 하지 않고, 기대 경로를 기록·확인할 뿐이다.
- 전체 E2E 는 한 번에 초록으로 돈 것이 아니라 첫 실행(304/15/18) + 재빌드 후 재실행(37 통과)의 합이다. 첫 실행의 실패 원인이 신선도 가드였음은 오류 메시지로 확인했다.
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거, 설정 백업 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 선적재와 주 DLL 의 제한 적재 통일, 누락 시 사유 있는 Mock | GUI-C-243 |
| 2. 데코이 반증 시험 (되돌리면 빨강) | GUI-C-243 (반증 A) |
| 3. 늦게 로드되는 경로 재감사, worker 경로 기록, 경고만으로 성공 금지 | GUI-C-243 (D, E, C) |
| 4. 정상 경로 회귀 없음 (Native E2E 전체 1회) | GUI-C-243 (4절) |
