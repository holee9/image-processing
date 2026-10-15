# GUI-C-212b — Codex #96 보류 4건 (212 오라클) (#249)

## 결론

| # | 지적 | 처리 | 확인 |
|---|---|---|---|
| 1 높음 | 오라클이 `xpe_preprocess_shutdown` 으로 운영 교정을 지움 | 오라클을 **별도 프로세스**(앱 exe 자신, `--run-preprocess-oracle <dll>`)에서 실행. 결과는 표준 출력 한 줄 JSON, 제한 시간 60초, 초과하면 자식 프로세스 트리 강제 종료 | E2E: 실제 exe 로 돌리는 동안 80만 회 넘는 교정이 전후 바이트 동일, 같은 프로세스 대조군은 빨강 |
| 2 보통 | gain 단계가 효과 없이도 통과 | 합성 flat 을 화소마다 다르게(2000 + (i%8)·100) 바꾸고, gain 출력을 `입력 ÷ (flat ÷ 평균(flat))` 의 독립 산술값과 허용오차 1e-3 으로 비교, `gainEffect > 0` 도 요구 | 입력 복사로 바꾸면 빨강 |
| 3 보통 | 임시 폴더 삭제 실패를 숨김 | 경로 계산까지 try 안으로, 삭제 실패는 결과의 `TempCleanupWarning` 에 담김, 시작할 때 **자기 이름 모양**(`xpe_oracle_` + 32 hex) 폴더 중 1시간 넘은 것만 회수 | 삭제 실패를 다시 삼키면 빨강, 나이·이름 조건 제거도 각각 빨강 |
| 4 낮음 | `OutputIdenticalToInput` 은 0.5 허용오차 판정 | 이름을 `OutputWithinHalfUnitOfInput` 로, 표시 문구를 "within 0.5 of input" 으로 | 컴파일 + 레거시 E2E 9/9 |

최종 빌드에서: IntegrationTests 735/735 통과, 레거시 E2E 9/9 통과(준비도 화면 UIA 시험 포함), 오라클 시험 12/12.

## 1. 별도 프로세스 — 선택과 이유

**배포물에 무엇이 더 자연스러운가**: 같은 exe 를 인자로 부른다. 보조 exe 를 따로 두면 배포물·서명·경로 탐색이 하나 늘고, 앱 exe 는 이미 `--probe-native-readiness`, `--run-preprocess-fixture-e2e` 같은 헤드리스 모드를 갖고 있다(`App.xaml.cs`). 새 모드는 같은 형태다.

동작:
- `XpePreprocessOracleProcess.Run(dll)` 이 `Environment.ProcessPath` 를 `--run-preprocess-oracle <dll>` 로 시작하고 표준 출력을 읽는다. `XpePreprocessReadinessProbe` 가 이 진입점을 부른다.
- 자식은 `XpePreprocessSyntheticOracle.Run` 을 돌려 결과를 JSON **한 줄**로 쓰고 종료한다. 예외도 실패 결과 한 줄이 된다(부모가 추측하지 않는다).
- 제한 시간(60초)을 넘기면 `Kill(entireProcessTree: true)` 후 종료를 확인하고, 확인되면 "was killed", 못 하면 "could not be confirmed killed" 로 보고한다.
- 결과 줄이 없거나 JSON 이 아니면 **실패**("Oracle process gave no result", 종료 코드와 stderr 포함). 통과로 읽히는 경로가 없다.
- 작업 객체(job object)는 쓰지 않았다. 자식이 손자 프로세스를 만들지 않아(오라클은 DLL 호출뿐) 트리 종료로 충분하다고 판단했다. 이 판단은 시험으로 확인하지 못했다(아래 한계).

### 시험

- **E2E `LegacyOracleIsolationScenarios`** (실제 `ImageProcTest.exe` 필요, 파일 신선도 가드 포함):
  이 시험 프로세스가 "운영자"가 되어 모듈을 init 하고 offset 교정을 생성·적재한다(균일 dark 300 — 오라클 자신의 합성 dark 100 과 달라서 합성 맵이 모듈에 보이면 출력이 달라진다). 기대 출력은 `raw − 300` 의 산술값이다. 오라클을 실제 exe 로 돌리는 동안 **다른 스레드가 같은 모듈을 계속 호출**하고, 전·중·후 출력을 기대값과 바이트 비교한다. 결과: 816,551회 동시 교정 중 편차 0, 전후 동일.
- **같은 시험의 반증을 시험으로 남김** (`Control_…InTheSameProcess_ErasesTheOperatorsCalibration…`): 오라클을 이 프로세스 안에서 돌리면 121,633회 편차(첫 편차 `NOT_INITIALIZED`)이고 끝난 뒤에도 `NOT_INITIALIZED`. 오라클이 같은 프로세스에서 모듈을 init/shutdown 하므로 운영 교정이 사라진다는 지적이 **실측으로 확인**됐다.
- **IntegrationTests**: 자식의 타임아웃·강제 종료(실제 `PING.EXE -n 60`), "결과 줄 없음 = 실패", 없는 exe = 미실행, JSON 왕복(NaN 필드 포함), 워커가 정확히 한 줄만 쓰는지.
- **소스 텍스트 점검 1건**(그렇다고 이름 붙임): 앱 소스 중 `XpePreprocessSyntheticOracle.Run(` 을 부르는 파일이 호스트 하나뿐이고 준비도 탐침이 호스트를 부른다. 프로세스 밖에서 관찰할 방법이 없는 연결(어느 진입점을 부르는가)을 지키는 용도이고, 행위 증명은 위 E2E 가 한다.

### 평가 미리보기의 자체 `shutdown/init` — 같은 결함인가

`NativePreprocessPreviewService.Run` 은 시작과 끝에서 `xpe_preprocess_shutdown` 을 부르고(`NativePreprocessPreviewService.cs:217`, `:246`), 끝에서 DLL 을 해제한다(`:251`). 214 이후 교정도 적재한다. **같은 결함이 아니라고 판단한다.** 근거(실행이 아니라 소스 검색):

- 레거시 앱 안에서 `xpe_calib_load_*` / `xpe_preprocess_init` / `shutdown` 을 부르는 곳은 이 서비스와 오라클뿐이다(`grep` 결과: `XpePreprocessReadinessProbe` 의 이름 목록, `XpePreprocessSyntheticOracle`, `NativePreprocessPreviewService`). 즉 이 프로세스에는 "운영자가 적재한 교정"이 서비스·오라클 자신의 것 외에 없다. 사용자 앱(`gui/ImageProcTest`)의 `GuiPreprocessRunner` 는 **다른 프로세스**라 모듈 상태가 따로다.
- 미리보기는 끝날 때 shutdown 하고 DLL 을 해제하므로 자기 교정을 남기지 않는다. 오라클 결함의 핵심(운영 상태가 오라클 때문에 사라짐)은 오라클이 자식으로 빠진 지금 이 프로세스에서 재현될 경로가 없다.

그래서 이 서비스는 건드리지 않았다. 다만 이것은 **"이 앱에 다른 적재 경로가 없다"는 사실에 기댄 판단**이다. 앱에 운영자 교정을 적재하는 기능이 생기면(예: 교정 탭이 모듈에 적재) 미리보기의 shutdown/init 은 같은 결함이 되므로, 그때 미리보기도 같은 방식(자식 프로세스) 또는 상태 보존 없는 적재로 바꿔야 한다. 이 의존을 리더가 알고 있어야 해서 적어 둔다.

## 2. gain 단계

기존 합성 flat 은 균일(2000)이라 gain 맵이 전부 1.0 이었다 — gain 이 UINT16→FLOAT32 변환만 해도 "출력이 0 이 아님" 조건을 통과했다. 이제 flat 이 화소마다 달라 gain 은 실제 값을 바꾸고, 기대 출력은 오라클 안에서 flat 으로부터 산술로 계산한다(모듈의 정의: 출력 = 입력 ÷ gain, gain = flat ÷ 평균). 모듈이 이 정의와 1e-3 이내로 일치해서 통과한다(214 의 사슬 시험이 같은 정의를 독립적으로 확인한 것과 같다). 기존 시험의 "gain 은 효과 예외" 분기는 삭제해 세 단계 모두 `MaxAbsError > 0` 을 요구한다.

## 3. 임시 폴더

`XpePreprocessSyntheticOracle.Run(dll, OracleOptions?)` 의 `OracleOptions` 는 시험용 이음새다(임시 루트, 폴더 생성 직후 훅, 회수 나이). 삭제 실패 시험은 그 훅에서 폴더 안 파일을 `FileShare.None` 으로 잡아 두고 결과의 경고와 폴더의 실제 잔존을 둘 다 확인한다(경고가 참인지). 대조 시험(잡지 않은 정상 실행)은 경고가 없고 폴더가 비어 있음을 확인한다.

회수는 이름이 `^xpe_oracle_[0-9a-f]{32}$` 이고 마지막 수정이 1시간보다 오래된 폴더만 지운다. 시험은 (오래된 자기 폴더, 젊은 자기 폴더, 오래된 남의 폴더, 이름만 비슷한 오래된 폴더) 네 개를 두고 첫째만 사라지는 것을 단언한다.

## 4. 이름

`NativePreviewMetrics.OutputIdenticalToInput` → `OutputWithinHalfUnitOfInput`. 같은 레코드를 쓰는 `NativeEnhanceBasicPreviewService`·`NativePresentationExportService` 도 같은 0.5 기준이라(`abs > 0.5`) 함께 바꿨다. 화면 문구(`MainWindow.xaml.cs`)는 `outputWithinHalfUnitOfInput(within 0.5 of input)=`, E2E 보고서(`GuiE2eReportService.cs`)는 `Output within 0.5 of input`. 옛 이름을 문자열로 기대하는 시험은 없었다(`grep`).

## 반증 (`falsification_arms.txt`)

| 시도 | 결과 |
|---|---|
| 1 호스트가 오라클을 이 프로세스 안에서 실행 | 격리 E2E 빨강(대조군은 원래 초록) |
| 1b 준비도 탐침이 프로세스 안 오라클을 다시 부름 | 소스 텍스트 점검 빨강 |
| 1c 제한 시간이 자식을 죽이지 않음 | 타임아웃 시험 빨강 |
| 2 gain 출력을 입력 복사로 | 5건 빨강(호출 시험 포함) |
| 2c 합성 flat 을 다시 균일하게 | 5건 빨강 |
| 3 삭제 실패를 다시 삼킴 | 삭제 실패 시험 빨강 |
| 3b 나이 조건 제거 | 2건 빨강 |
| 3c 이름 모양 조건 제거 | 회수 시험 빨강 |
| **2b gain 판정을 옛 조건("출력이 0 이 아님")으로 약화** | **빨강 0건 (12/12 통과)** |

**2b 는 잡히지 않았다.** 이 시도는 모듈이 아니라 오라클의 판정식을 약하게 만드는데, 실제 모듈의 gain 은 맞게 동작하므로 약한 판정도 통과한다. 판정식 자체의 민감도를 시험하려면 gain 을 일부러 틀리게 만드는 모듈(가짜 DLL)이 필요하고, 이번에는 만들지 않았다. 그래서 "gain 판정식이 약해지면 잡힌다"고는 주장하지 않는다 — 입력 복사(시도 2)·균일 flat(2c)로 **효과 없는 gain 을 잡는다**는 것까지만 확인됐다.

## 한계 / 이관

- 자식이 손자 프로세스를 만들 때의 정리(작업 객체)는 시험하지 않았다. 현재 오라클은 만들지 않는다.
- E2E 격리 시험은 실제 exe 와 `XPE_NATIVE_DIR` 이 필요하고, 레거시 exe 가 소스보다 오래되면 실패하며 이유를 말한다(신선도 가드). CI 의 레거시 앱 빌드 단계는 리더가 병합하는 ci.yml 패치에 있다(212).
- 평가 미리보기 판단은 위처럼 "이 앱에 다른 적재 경로가 없다"는 소스 검색에 의존한다.
- `clients/ImageProcTest.E2ETests` 가 오라클·호스트·결과 파일 3개를 더 링크한다.
