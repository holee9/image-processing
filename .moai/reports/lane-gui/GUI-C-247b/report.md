[2026-10-15 21:26 KST] GUI-C-247b 보고서 — W16 이 CI 에서 개발 PC 경로를 요구해 실패하던 문제 수정 (Refs #251)

## 1. 원인과 수정

PR #271 CI(`gui-e2e-native`)에서 W16 이 `the calibration folder does not exist: D:\workspace-github\xpe-data\calib-cyan-v1` 로 실패했다. W16 은 `XPE_C231_NATIVE_DIR` 가 있으면 발견되는데, 보정 폴더와 raw 경로는 개발 PC 절대경로 기본값이어서 실프레임 세트가 없는 러너에서 실패했다. 결함은 시나리오 쪽이다.

`QuickStartWalkthroughScenarios.cs` 의 `W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings`:

- 하드코딩 기본값 둘(`XPE_C247_CALIB`, `XPE_C247_RAW`)을 없앴다.
- `[EnvGatedFact]` 를 `[SkippableFact]` 로 바꿨다. `XPE_C231_NATIVE_DIR` 미설정 또는 두 변수 중 하나라도 미설정이면 `Skip.If` 로 사유를 적고 건너뛴다(발견에서 사라지지 않고 건너뜀으로 보인다). 사유 문구: `XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository`.
- 설정됐는데 폴더·파일이 없으면 기존처럼 `Assert.True` 로 실패한다.
- 주석의 "default …" 문구를 실제 동작에 맞췄다.
- 사유 문구는 카드의 한글 문구 대신 영어·ASCII 로 적었다(기존 파일의 다른 단언 메시지와 같은 언어, 콘솔 인코딩 문제 회피).

## 2. 검증 (한 번에 하나, 모두 `--no-build`, 명령: `dotnet test … --filter "FullyQualifiedName~W16_ARealFrame"`)

공통: `XPE_C231_NATIVE_DIR=D:\workspace-github\xpe-data\gui-247-dlls`(개인 폴더, 공유 폴더 미사용).

| 실행 | 변수 | 결과 | 출력 | 파일 |
|---|---|---|---|---|
| 1 | `XPE_C247_*` 미설정 | **건너뜀** (1.9 s) | `W16_… [SKIP]` / `XPE_C247_CALIB/XPE_C247_RAW not set - the real-frame set is not in the repository` / 총 1, 건너뜀 1 | `run1_unset.txt` |
| 2 | `XPE_C247_CALIB=…\calib-cyan-v3`, `XPE_C247_RAW=` cyan_test Foot 55kV | **통과** (13 s, 회귀 없음) | `preprocess: Applied — … (offset -> gain -> defect …) EI = 3143.86`, ALERT 2건(WARN 노출 편차 DI=11, INFO) | `run2_cyan_v3.txt` |
| 3 | `XPE_C247_CALIB=…\no-such-calib-typo` (반증) | **실패** (3 ms) | `the calibration folder does not exist: D:\workspace-github\xpe-data\no-such-calib-typo` | `run3_missing_path.txt` |

- 가용 메모리 전→후(MB): 실행 1 6054→6074, 실행 2 5781→5870, 실행 3 6066→6212. 각 실행 뒤 남은 ImageProcTest/testhost/xpe_ai_worker 0.
- 실행 2 의 결과는 GUI-C-247 보고서 7절(v3, EI 3143.86, 통과)과 같다.

## 3. 한계

- CI 러너에서 실제로 건너뜀이 되는지는 이 세션에서 관측하지 않았다. 변수 미설정 상태의 로컬 실행(실행 1)으로 대신 확인했다. 푸시 후 `gui-e2e-native` 의 W16 이 건너뜀으로 바뀌는지는 CI 에서 읽어야 한다.
- 이 변경 이후 `XPE_C231_NATIVE_DIR` 가 없는 환경에서도 W16 이 발견되어 건너뜀으로 집계된다(이전에는 발견되지 않았다). 의도된 변화다(카드: 조용히 미발견으로 만들지 않는다).
- Native 전체·IntegrationTests 전체는 돌리지 않았다(CI 몫).
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 하드코딩 기본 경로 제거, 두 변수 모두 설정 시에만 실행 | GUI-C-247b (1절) |
| 2. 미설정 시 사유 적고 건너뜀, 설정됐는데 없으면 실패 | GUI-C-247b (1·2절, 실행 1·3) |
| 3. 주석 문구 정리 | GUI-C-247b (1절) |
| 검증 3회 | GUI-C-247b (2절) |
