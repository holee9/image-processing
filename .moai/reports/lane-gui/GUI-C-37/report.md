# GUI-C-37 — 전처리 성공 경로 실행 관측 완료 (#141)

- 카드: GUI-C-37 · Refs #141 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `afe8425`(기계) · `d9de6fa`(배선 수정 + 자동화 실행) — 미푸시
- 네이티브·생성기: **CI run 34528935791** 아티팩트 3종
- **결과: E2E Mock 9/9 · Native 9/9(건너뜀 0) · 통합 0/171/1/172 · slnx 0/0 · 자동화 Native `PreprocessRan=true`**

---

## 1. 로컬 네이티브 빌드는 하지 않았다

카드는 "완료 전이면 로컬 ci-preprocess 빌드로 대체 가능" 을 제시했다. **쓰지 않았다.**

이 레인의 상시 제약이다 — `CLAUDE.local.md`: *"네이티브 DLL 을 빌드해 재실행하지 말 것 — CI 몫 (#98)"*. `xpe_calib_fixture_gen` 을 만들려면 preprocess 모듈을 구성·빌드해야 하고, 그것이 정확히 그 조항이 막는 행위다. 제약은 사용자 프로젝트 파일에서 오며 카드가 해제할 수 있는 것이 아니다.

대신 **생성기가 CI 아티팩트로 staging 되면 그대로 동작하는** 경로를 만들었다(§2). run 34528935791 은 이 작업 시점에 `in_progress` 였다.

## 2. 구현

| 파일 | 역할 |
|---|---|
| `Services/AutomationArgs.cs` | `--automation-calib <dir>` 추가 |
| `App.xaml.cs` / `MainWindow.xaml.cs` | 그 경로를 격리 설정의 offset/gain/defect 3곳에 반영 |
| `Fixtures/ApplicationFixture.cs` | Native 실행 시 생성기를 `--out <temp> --width 1024 --height 1024 --seed 0` 으로 실행, 산출 파일 목록을 기록 |
| `Scenarios/Workflows/WorkflowScenarios.cs` | W-02 를 **성공 라인 전용**으로 좁히고 뷰포트 상태값 추가 |
| `Models/PreprocessRunResult.cs` / `RealXpeBackend` / `MainWindowViewModel` | 결과 픽셀 → 미리보기 → `ProcessedImage` |
| `Models/GuiAutomationReport.cs` | `PreprocessRan` / `PreprocessStages` |

세 가지가 이 커밋의 요점이다.

**(a) 새 스위치는 파서에 등록해야 한다.** C-28 이 알 수 없는 `--automation-*` 를 거부하게 만들었으므로, 앱에만 추가하고 파서를 두면 **앱이 자기 명령줄을 거부한다.** 회귀 1건으로 고정했다.

**(b) W-02 는 이제 성공 라인만 받는다.** C-36 은 성공/거부가 공유하는 부분 문자열을 단언했고, 그 보고서 §7 이 스스로 위험으로 적었다 — 캘리브레이션이 생겨도 **스테이지가 한 번도 돌지 않은 채 계속 통과**할 수 있었다. 이제 `offset -> gain -> defect` 를 요구하고 `skipped` 를 불허한다.

**(c) 캘리브레이션이 없으면 실패가 아니라 SKIP 이다.** 거부 라인을 받아들이면 **"재지 않은 것" 이 "재 보니 괜찮음" 으로 보고된다.** 스킵 사유에 생성기 부재를 그대로 적는다.

## 3. 성공 경로를 실행했다 — 그 과정에서 배선 결함 2개를 잡았다

생성기가 아티팩트에 있었다(leader 통보). 스테이징 후 단독 실행부터 확인했다:

```
xpe_calib_fixture_gen.exe --out <dir> --width 1024 --height 1024 --seed 0
exit=0 · offset.xcal(4 194 456) · gain.xcal(4 194 456) · defect.xcal(1 048 728) · manifest.json
```

그런데 **첫 Native 실행은 여전히 거부 라인이었다.** 스킵은 0이었으므로 세트는 만들어졌는데 앱이 못 찾은 것이다. 추적한 결과 두 곳이 끊겨 있었다.

**(1) `--automation-calib` 가 격리 분기 안에만 있었다.** 그 분기는 `--automation-report` 가 함께 있을 때만 도는데 E2E 픽스처는 일부러 report 를 주지 않는다. **파일이 있는데도 "calibration file(s) not found"** 를 보고한 이유다. **C-31 의 `--automation-backend` 와 같은 형태의 결함**이며, 같은 자리에서 두 번째로 재발했다.

**(2) 자동화 시나리오가 로드 전에 전처리를 눌렀다.** 진단을 기록하게 하니 `not attempted (menu enabled=True, frame loaded=False)` 가 찍혔다 — 메뉴는 활성인데 프레임이 없어 조기 반환한 것이다. 표시 파이프라인 뒤로 옮겼다.

**"눌렀다" 를 "실행됐다" 로 읽지 않은 것**이 (2)를 잡은 방법이다. 리포트가 `false` 인 이유를 값으로 남기게 하자 원인이 한 번에 드러났다.

### 관측 결과

| 실행 | 결과 |
|---|---|
| E2E Native | **9/9, 건너뜀 0** — W-02 가 `offset -> gain -> defect` 성공 라인을 관측 |
| 자동화 Native | `PreprocessRan=true`, `"Preprocess: offset -> gain -> defect on 1024x1024 (Abdomen)."` |
| 자동화 Mock | `PreprocessRan=false`, `"not attempted (menu enabled=False, frame loaded=True)"` — 이유까지 기록 |

**이로써 C-36·C-37 이 미검증으로 남겼던 것들이 닫혔다**: 세 스테이지가 `XPE_OK` 를 돌려주고, `XpeImageMetadataNative`(64바이트 배열 + `Pack=8`) 정렬이 맞으며(틀렸다면 스테이지가 거부하거나 죽었다), 결과가 뷰포트 경로를 지난다.

### 반증 (`step5-falsify.log`)

생성 직후 `defect.xcal` **하나만** 지웠다:

```
실패!  - 실패: 1, 통과: 0, 전체: 1
String:    "Preprocessing skipped: calibration file(s"…
Not found: "offset -> gain -> defect"
```

세트가 온전할 때만 성공 라인이 나온다. 원복 후 재확인.

## 4. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9 (1 m 3 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                       (Mock)
통과!  - 실패: 0, 통과: 9, 건너뜀: 0, 전체: 9

ImageProcTest.exe --automation-backend Native --automation-calib <set> …
Passed=True · PreprocessRan=True · "Preprocess: offset -> gain -> defect on 1024x1024 (Abdomen)."

ImageProcTest.exe --automation-backend Mock …
Passed=True · PreprocessRan=False · "not attempted (menu enabled=False, frame loaded=True)"

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 171, 건너뜀: 1, 전체: 172
```

Baseline 귀속: 통합 172건은 커밋 `afe8425` 시점과 동일(이번 커밋은 gui 자동화 배선). E2E Native 는 **8통과+1스킵 → 9통과 0스킵**으로 바뀌었다 — 스킵의 사유였던 생성기 부재가 해소됐기 때문이며, **단언은 그대로 성공 라인 전용**이다.

네이티브 교체 확인(스테이징): `xpe_common.dll` md5 `c59bb975… → 23fd55d6…`, `xpe_preprocess.dll` `2ddc8be9…`, 생성기 `e1ba0ef0…`.

## 5. 미검증 (Gaps)

- **결과 픽셀의 정확성은 확인하지 않았다.** 스테이지가 `XPE_OK` 를 돌려주고 뷰포트 경로를 지난다는 것까지다 — 보정값이 옳은지는 이 층의 질문이 아니다(모듈 테스트 소관).
- **뷰포트가 시각적으로 바뀌는 것을 사람 눈으로 보지 않았다.** E2E 는 `ViewportShell` 의 사각형이 비어 있지 않음까지 본다.
- **`--automation-calib` 를 세 디렉터리에 같은 값으로 넣는다.** offset/gain/defect 가 서로 다른 위치에 있는 구성은 확인하지 않았다.
- **`manifest.json` 을 읽지 않는다.** 생성기가 함께 쓰는 그 파일의 내용(SHA 등)을 검증에 쓰지 않았다.
- 1024×1024 한 크기, seed 0 한 조합만 돌렸다.
- CI 반영 결과 미확인 — leader 가 Native 잡에 생성기 단계와 "스킵 0" 단언을 넣는다.

## 6. 잔여 위험 (Residual risk)

- **같은 배선 결함이 두 번 났다**(C-31 `--automation-backend`, 이번 `--automation-calib`). `CreateSettings` 의 자동화/비자동화 두 갈래가 원인이고, 새 `--automation-*` 스위치를 더할 때마다 같은 함정이 있다. 두 갈래를 하나로 합치거나 가드를 두는 편이 낫다 — 이번 카드 범위 밖이라 하지 않았다.
- **성공 라인 문자열이 계약이 됐다.** `offset -> gain -> defect` 형식을 바꾸면 W-02 가 깨진다. 대신 그 문자열이 "세 스테이지가 순서대로 돌았다" 를 실제로 증명한다.
- 픽스처가 스위트마다 XCal 세트를 새로 만든다(약 9 MB, temp). 지우지 않으므로 누적된다.
- Native E2E 가 1분을 넘겼다(1 m 3 s). 게이트(3 min)에는 여유가 있지만 스모크 게이트(30 s)와는 다른 규모다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run download 34528935791 -n xpe-ci-common-binaries -n xpe-ci-preprocess-binaries -n xpe-ci-post-binaries -D <tmp>
cp -f <tmp>/*/*.dll build/ci-common/bin/ ; cp -f <tmp>/xpe-ci-preprocess-binaries/xpe_calib_fixture_gen.exe build/ci-common/bin/
./build/ci-common/bin/xpe_calib_fixture_gen.exe --out <dir> --width 1024 --height 1024 --seed 0

export PATH="/c/Program Files/dotnet:$PATH"
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
ImageProcTest.exe --automation-backend Native --automation-calib <set> …
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
