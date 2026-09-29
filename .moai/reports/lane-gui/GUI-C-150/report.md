# GUI-C-150 (#224) — §1 의 답: **세 갈래 중 어느 것도 아니다.** 운영자 앱이 아닌 두 번째 앱의 화면이다

상태: **§1 답변 완료. §2 착수하지 않았다**(카드 §5 지시).

핵심: 이 서비스가 그리는 표는 **CI 가 빌드하지도 실행하지도 않는 앱**의 화면에만 실린다.
그리고 카드 §4 의 전제(네이티브에 새 필드 셋이 생겼으니 바로 받아 쓸 수 있다)는 **지금 어느 브랜치에도 없다**(§4).

---

## 1. 호출처와 노출 경로 (카드 §1 의 산출)

### 1.1 호출처 — 3곳, 전부 같은 앱 안

| 위치 | 무엇 |
|---|---|
| `clients/ImageProcTest/Services/NativePreprocessPreviewService.cs:474` | **유일한 `Compute(...)` 호출.** 네이티브 프리뷰가 끝난 뒤 그 출력(`finalValues`)으로 계산 |
| 같은 파일 `:232` | `Empty("all correction stages bypassed")` |
| `clients/ImageProcTest/MainWindow.xaml.cs:1893` | `Empty("native calibration preview not run")` |

**"모듈 미연동 시절의 잔재" 는 아니다.** 이 서비스는 네이티브 보정(`xpe_offset_correct`·`xpe_gain_correct`·`xpe_defect_correct`)이
**실제로 돈 뒤** 그 결과에 대해 계산한다(`:466` `stages.Any(stage => stage.Executed)` 검사 뒤).

### 1.2 노출 경로 — 두 갈래

| 경로 | 자리 |
|---|---|
| 화면 | `clients/ImageProcTest/MainWindow.xaml.cs:1894-1896` → `MainWindow.xaml:859·878·897` 의 `DarkMetricsGrid`·`FlatMetricsGrid`·`DefectMetricsGrid` 세 `DataGrid` |
| 보고서 | `clients/ImageProcTest/Services/GuiE2eReportService.cs:590-597` `AppendDetectorMetrics` |

### 1.3 **그런데 그 화면은 운영자 앱이 아니다** — 이것이 §1 의 답이다

`ImageProcTest` 라는 이름의 WPF 앱이 **둘** 있다.

| | `clients/ImageProcTest` | `gui/ImageProcTest` |
|---|---|---|
| 이 서비스 | **있다** | **없다** (`gui/` 전체에 `DarkBias`·`DSNU` 문자열 0건) |
| CI 빌드 | **어떤 워크플로도 빌드하지 않는다** | `ci.yml:472`·`:542` `dotnet build gui/ImageProcTest/…` |
| CI 실행 | 없음 | `gui-automation`·`gui-e2e-native` 가 이 exe 를 띄운다 |
| E2E 대상 | 아님 | `ApplicationFixture.cs:111`·`:708` 이 **이 exe 를 찾는다** |
| 다른 프로젝트가 참조 | **0건** | `gui/ImageProcTest.E2E`·`ImageProcTest.SelfCheck` 가 `ProjectReference` |
| 마지막 커밋 | `0e6a60a` **2026-09-18** | `ab557c8` **2026-09-28** |

`ci.yml:451` 의 주석이 직접 적고 있다 — *"WPF automation run of **the operator app (gui/ImageProcTest)**"*.

**따라서 카드 §1 의 세 갈래 중 어느 것도 그대로는 맞지 않는다:**

| 카드의 갈래 | 왜 아닌가 |
|---|---|
| 모듈 미연동 시절의 **잔재** | 네이티브 보정이 돈 뒤 그 출력으로 계산한다 — 미연동 잔재가 아니다 |
| 표시용 **근사** | 운영자 앱에 표시되지 않는다. 근사 표기를 달 화면이 없다 |
| 독립 **교차검증** | 네이티브 지표와 대조하는 코드가 없다. `XpeCalibrationMetrics` 를 받는 P/Invoke 가 **clients 전체에 0건**이고, 프리뷰 서비스가 푸는 심볼은 보정 셋과 init/shutdown/expiry 뿐(`:162-167`) |

**실제 성질**: 이것은 **앱 자체가 갈라진 잔재**다. 지표 구현이 낡은 것이 아니라, **그것을 담은 앱이 운영자 앱 자리에서 내려왔다.**
`clients/ImageProcTest` 는 오늘도 **경고 0 / 오류 0 으로 빌드된다**(실측) — 죽어 있지 않고, 개발자가 손으로 띄우는 도구로 살아 있다.
`clients/README.md` 도 이 앱을 *"WPF app and its integration tests"* 로 소개한다.

### 1.4 그래서 무엇을 물어야 하나 — 리더 판단

넷을 고치기 전에 **이 앱이 무엇으로 남는지**가 먼저다. 제가 정할 수 있는 것이 아니다.

| 만약 | 그러면 §2 는 |
|---|---|
| 이 앱을 **계속 쓴다**(개발자 도구) | 넷은 진짜 결함이다 — 사람이 그 표를 보고 판단하므로. 고친다 |
| 이 앱을 **접는다** | 고칠 이유가 없다. 걷는 것이 맞다 |
| 이 지표를 **운영자 앱으로 옮긴다** | 옮기면서 고친다 — 다만 그것은 이 카드보다 큰 일이다 |

## 2. 네이티브 호출로 대체하는 것은 **간단하지 않다** (참고)

카드 §1 의 첫 갈래("네이티브 호출로 바꾸면 넷이 한 번에 없어진다")가 선택되더라도 그대로는 안 된다.

**첫째, 형이 안 맞는다.** `xpe_verify_offset` 의 `corrected_image` 는 **UINT16** 인데
(`preprocess_api.h:880-890` 주석 *"corrected_image Offset-corrected image (UINT16)"*),
이 서비스가 쥔 것은 `ReadOnlySpan<float>` 다. 공교롭게도 결함 (2)가 결함인 이유와 **같은 축**이다.

**둘째, 12행 중 6행은 네이티브에 대응이 없다.**

| GUI 행 | 네이티브 대응 |
|---|---|
| `DarkBias` | `dark_bias` |
| `DSNU_ADU` | **없음**(네이티브 `dsnu` 는 %) — §4 |
| `PRNU_CV` | `prnu_after` |
| `DefectResidualADU` | `correction_error`(근사) |
| `DarkReduction_dB`·`ClampRate`·`FlatResidualPct`·`LineArtifactScore`·`DefectRecall`·`DefectFPR`·`GoodPixelDeltaP99` | **없음** |

`ClampRate`·`LineArtifactScore`(프로파일 FFT)·`DefectRecall`/`FPR`(오라클 BPM 대조)는 네이티브 API 가 아예 계산하지 않는다.
**"호출로 바꾼다" 는 절반에만 적용된다.**

## 3. 결함 넷 — 읽기만 했다. §2 는 착수하지 않았다

카드 §5 가 *"§1 답 전에 §2 착수"* 를 금지했다. 코드를 읽어 확인한 것만 적는다 — **실행 관측은 하지 않았다.**

| # | 카드의 기술 | 코드에서 확인 | 상태 |
|---|---|---|---|
| (1) 어두운 ROI 미선택 | `:57` `Stats(output)` 전 화면 | 맞다. ROI 선택 코드 없음 | 미착수 |
| (2) `Math.Abs` 누락 | `:63` `correctedStats.Mean <= 5.0` | 맞다. `output` 은 `ReadOnlySpan<float>`(`:27`·`:54`) | **아래 참고** |
| (3) `or` 미표현 | `:63`·`:65` 가 독립 행 | 맞다 | 미착수 |
| (4) `20 ADU` 출처 없음 | `:64` | 값은 확인. 출처 검색은 pre 가 한 것을 **인용**한 것이고 제가 다시 뜨지 않았다 | 미착수 |

**(2)에 대한 보강 단서 하나** — 같은 파일이 이미 **음수가 가능하다는 전제로 쓰여 있다**:
`CountClamped`(`:265-277`)가 `values[i] <= 0.5f` 를 클램프로 센다. 0 이하를 세는 코드가 있다는 것은
**이 배열에 0 이하가 들어올 수 있다고 저자가 보았다**는 뜻이다. 그래도 이것은 **코드 읽기**이고,
카드가 요구한 **실행 관측(음수 평균 입력으로 통과 보이기)은 §2 의 일이라 하지 않았다.**

## 4. **카드 §4 의 전제를 정정한다** — 새 필드 셋은 어느 브랜치에도 없다

카드 §4:

> `#223` 에서 `XpeCalibrationMetrics` 에 셋이 붙었습니다: `dark_reduction_db`, `dsnu_adu`, `measured_mask`.
> §1 이 "호출로 바꾼다" 로 답하면 **그 값을 바로 받아 쓸 수 있습니다.**

**찾지 못했다.** `git grep` 으로 `origin/main`·`origin/dev/preprocess`·`origin/dev/postprocess` 의 `modules/` 를 뒤졌다:

| 식별자 | `origin/main` | `origin/dev/preprocess` | `origin/dev/postprocess` |
|---|---|---|---|
| `dsnu_adu` | **0** | **0** | **0** |
| `measured_mask` | **0** | **0** | **0** |
| `dark_reduction_db` | `xpe_verify_metrics.cpp:370`·`:387` — **함수 안 지역 변수**, 구조체 필드 아님 | — | — |

`origin/main` 의 `preprocess_api.h:884-885` 는 여전히 `dark_bias` / `dsnu`(%) 둘뿐이다.

**따라서 "바로 받아 쓸 수 있다" 는 지금 성립하지 않는다.** 제 워크트리가 낡아서가 아니다 —
원격 브랜치를 직접 조회했다(제 HEAD 는 `origin/main` 보다 **176 커밋 뒤**이고, 그래서 로컬만 보지 않았다).

`#223` 이 아직 안 올라왔거나, 필드명이 다르거나, 제가 찾은 범위 밖일 수 있다. **검색 범위를 적어 두니 리더가 판별해 주십시오.**

## 5. 카드 §3 확인 — `DSNU_ADU` 행은 건드리지 않았다

`:64` 가 `correctedStats.StandardDeviation` 을 ADU 로 낸다. 카드가 정본 `Protocol.md:204` 정의 그대로라고 했고, **손대지 않았다.**
(정본 파일은 이 워크트리 밖이라 제가 직접 대조하지는 않았다 — 카드 기술을 **인용**한다.)

## 6. 바꾼 것

**없다.** 이 카드는 §1 답변이 산출이고, 카드 §5 가 §2 선행을 금지했다.

## 7. 미검증 / 잔여 위험

- **결함 넷의 실행 관측을 하지 않았다** — 특히 (2)의 음수 평균 통과. §2 의 일이다
- **(4)의 "출처 없음" 은 pre 의 검색을 인용**한 것이고 제가 `SRS-CALIB-001`·`Protocol.md` 를 다시 뜨지 않았다
- **정본 `Protocol.md` 를 직접 읽지 않았다** — 이 워크트리에 없다. `:203`·`:204` 인용은 카드에서 가져온 것이다
- `clients/ImageProcTest` 가 **사람 손으로 실제 쓰이는지**는 코드·CI 로는 알 수 없다. 빌드된다는 것과 쓰인다는 것은 다르다 — **리더·사용자만 아는 사실**이다
- 제 HEAD 는 `origin/main` 보다 **176 커밋 뒤**다(`0 0` 아님). `clients/` 쪽 최신 상태가 main 에서 달라졌을 수 있다. `git merge main` 은 리더 결정이라 하지 않았다

---

Refs #224
