# GUI-C-211 — 낡은 진단 오라클이 앱 준비도에 무엇을 보이는가 (#249)

증거(이 폴더): `legacy_app_native_uia.txt`(진단·평가·교정 탭을 UIA 로 읽은 전체 덤프), `legacy_app_run_selected_uia.txt`(교정 탭에서 SWU-1.1 행을 선택하고 Run Selected 를 UIA Invoke 로 누른 결과), `uia_probe_script.ps1.txt` · `uia_run_selected_script.ps1.txt`(그 두 실행의 스크립트). 코드 변경 없음.

## 먼저 — 결론 (우선 보고 대상: 거짓 "준비됨"인가?)

**아니다. 지금은 거짓 "준비됨"이 아니라 거짓 "준비 안 됨"(false negative)이다.** 앱을 실제로 띄워 화면에서 읽었다: 오라클이 실패하면 `xpe_preprocess` 가 R3 에 못 오르고 R2("Version and export checklist ready")에 머물며 `ProcessingEnabled=false`, 교정 탭의 전처리 3단계가 **막힘**으로 보이고, 안내 문구가 "fix synthetic oracle gaps"(고칠 것이 오라클 자신인데)를 가리킨다. 운영자에게 "통과로 오판"되는 경로는 이번 실측에서 보지 못했다. 단, **과거**에는 거짓 "준비됨"이었을 수 있다(아래 "언제부터").

## 1. 화면에서 확인한 것 (레거시 `clients/ImageProcTest` 앱, Native)

실행 조건: `dotnet build clients/ImageProcTest -c Debug`(오류 0·경고 0) 직후의 `ImageProcTest.exe` 를 `XPE_NATIVE_DIR` = IntegrationTests 출력 폴더(`xpe_common.dll` 등 22개 DLL)로 띄움. 이 앱 자신의 출력 폴더에는 xpe_*.dll 이 없다(앱 폴더에서 못 찾으면 이 환경 변수를 본다). 조작은 UIA 패턴(탭 `SelectionItem.Select`, 행 선택, 버튼 `Invoke`)만 썼다 — 키보드·마우스 입력 없음. 앱 창을 닫기 전에 읽은 값이다.

| 화면 | 읽은 값 (UIA, 그대로) | 운영자에게 |
|---|---|---|
| 상태줄 | `Status: Native common backend ready. Image-processing modules remain gated.` | 일반 문구(오라클 언급 없음) |
| 진단 탭 `PreprocessHealthText` | `Preprocess health: Export checklist ready (0.1.0); smoke=Synthetic oracle fail; params=6/6 ok; …` | **"smoke=Synthetic oracle fail"** 보임 |
| 진단 탭 `PreprocessSmokeText` | `Preprocess smoke: Synthetic oracle fail; pass=False; latency=0.837ms` | 실패 보임 |
| 진단 탭 준비도 표 | `xpe_preprocess`: Level `R2`, Status `Version and export checklist ready`, Evidence `…synthetic=Synthetic oracle fail…`, NextAction `Next: fix synthetic oracle gaps, then run fixture E2E before enabling GUI execution.` | R2, 아래 요약의 꺼진 모듈 목록에 포함 |
| 진단 탭 `ModuleReadinessSummaryText` | `Executable modules=0; levels R0=6, R2=2; Off modules=xpe_display:R0, xpe_preprocess:R2, …` | **"Executable modules=0"** — 전처리는 꺼진 모듈로 셈 |
| 교정 탭 알고리즘 사슬 | 단계 3개 모두 `[xpe_preprocess; native-preview; R2 Version and export checklist ready]`, 그리고 `AlgorithmDependencyFinding { Severity = Hard, RuleId = NATIVE-NOT-READY, Message = (1) Offset correction is selected but the native preprocess adapter is not ready. …}` ×3 (Offset·Gain·Defect) | **Hard 지적 3건** |
| 교정 탭 `AlgorithmValidationResultText` | `Algorithm validation: 33 SWUs across 7 modules; runnable=1.` | 33개 중 1개만 실행 가능 |
| 교정 탭: SWU-1.1(OffsetCorrector) 행 선택 → `RunSelectedAlgorithmButton` Invoke | `Calibration validation: blocked for SWU-1.1; Select the acquired calibration folder … apply offset correction. Module gate: Next: fix synthetic oracle gaps, then run fixture E2E before enabling GUI execution.` | **"blocked"** + 오라클을 고치라는 안내 |
| 평가 탭 `StageModesInfoText` | `Preprocess=ready; Post basic=blocked. …` 그리고 `OffsetEnabledCheckBox`/`GainEnabledCheckBox`/`DefectEnabledCheckBox` **enabled=True**, `ApplyNativePreviewButton`(Run Selected) enabled=False | **교정 탭과 모순**("ready") |

**화면 사이에 불일치가 있다**: 진단·교정 탭은 오라클 결과에 따라 전처리를 "꺼짐/막힘"으로, 평가 탭은 "Preprocess=ready"로 보인다. 원인은 코드가 두 곳에서 서로 다른 값을 쓰기 때문이다(읽어서 확인): 평가 탭의 `IsNativePreviewReady()` 는 `lastPreprocessHealth.IsExportReady` 만 본다(`MainWindow.xaml.cs:2155`, 오라클 무관), 진단·교정 쪽은 `ModuleReadinessSnapshot.ProcessingEnabled`(= `IsSyntheticOracleReady`)를 본다(`ModuleReadinessService.cs:~112`, `AlgorithmValidationCatalogService.cs:86`). 평가 탭 `Run Selected` 의 비활성(enabled=False)은 이 실행에서 **이미지를 불러오지 않아서**이므로(`currentPreview is not null` 조건) 오라클 때문이라고 단정하지 않는다 — 이 버튼 경로가 오라클에 막히는지는 코드상 **막히지 않는다**(`ApplyNativePreviewButton_Click` 은 `IsNativePreviewReady()` 만 확인) 가 정확하고, 화면에서 이미지를 불러 실행해 보지는 않았다.
**귀속의 근거와 한계**: "오라클 때문에"는 (1) 화면의 안내 문구가 "fix synthetic oracle gaps"를 가리키고, (2) 해당 `CanRun` 이 `ProcessingEnabled` 에서 온다는 코드 읽기에서 온다. 오라클이 통과하도록 바꿔 같은 화면을 다시 읽는 **대조 실험은 하지 않았다**(코드 변경 범위 밖).

## 언제부터 낡았나 (git, 읽은 것)

- 헤더 `preprocess_api.h` 의 보정 함수는 `xpe_offset_correct(const XpeImageBuffer* input, XpeImageBuffer* output, const XpeImageMetadata* metadata)` 형태가 `dada6aff`(2026-04-16, Phase 1)에 이미 있었다.
- 오라클은 `77b42f3f`(2026-04-18)에 **`(ref image, ref map)`·`(ref image, ref defectMap, config)`** 로 불러 만들어졌다 — **처음부터 헤더와 맞지 않았다.** 오라클 파일의 커밋은 이 둘뿐(`77b42f3f`, `3f4f1c96` 2026-04-19)이고 그 뒤 손대지 않았다.
- `XPE_ERR_CALIB_NOT_LOADED`(교정을 안 적재하면 보정 함수가 거부)는 `f3ccb0ec`(2026-09-10)에 들어왔다. 그 전에는 교정 없이 호출해도 보정이 도는 시맨틱("identity/no-calibration"이라고 오라클 상세 문구가 말한다)이었다.
- **추론(실행해 확인하지 않음)**: 04-18~09-10 사이 오라클은 어긋난 인자로 불러도 우연히 통과했고(그 동안 앱은 R3 "Synthetic oracle ready"·`ProcessingEnabled=true` 를 보였을 것 — **그 기간엔 거짓 "준비됨"**), 09-10 에 DLL 이 교정 상태를 검사하면서 실패로 바뀌어 그 뒤 약 3주 동안 거짓 "준비 안 됨"이다. 옛 DLL 로 오라클을 돌려 보지 않았으므로 "우연히 통과"는 가설이다.
- `IntegrationTests` 에는 이 오라클을 부르는 시험이 **없다**(검색 0건) — 낡아도 어떤 게이트도 빨개지지 않은 이유.

## 2. 처분 선택지와 권고

| | 내용 | 얻는 것 | 잃는 것·위험 |
|---|---|---|---|
| **(a) 오라클을 고친다** | 새 시그니처 `(input, output, metadata)` 로 부르고, 교정을 **적재**한 뒤 돈다: offset/gain 은 모듈의 `xpe_calib_generate_offset/gain` 으로 임시 폴더에 만들어 적재(210 의 xUnit 이 하는 그대로), defect 맵은 XCal 파일이 필요한데 **그것을 쓰는 생성기는 없다**(`xpe_bpm_generate` 는 메모리의 UINT8 마스크를 만든다 — 파일이 아님) → 210 에서 시험이 쓴 것과 같은 30줄짜리 XCal 쓰기가 앱 쪽에 또 필요 | 런타임 준비도가 "이 기계의 DLL 이 이 계약대로 실제 보정을 도는가"를 다시 답한다(앱의 원래 의도) | 같은 로직이 앱·시험 두 곳에 복제되어 다시 갈라질 수 있다 — **오라클을 부르는 시험을 IntegrationTests 에 함께 넣어야 한다**(그 파일은 이미 다른 진단 파일을 링크한다; 이 오라클 + 레코드 + 의존 파일만 링크하면 된다. 210 에서 임시 콘솔이 그 조합으로 컴파일됐다) |
| **(b) 오라클을 지우고 시험(210)에 맡긴다** | 앱의 준비도는 내보내기 점검 + 파라미터 범위(R2)에서 끝, R3 의 정의를 바꿔야 한다 | 코드 감소, 갈라짐 없음 | **앱 런타임이 그 기계의 DLL 을 한 번도 실행해 보지 않고 `ProcessingEnabled=true` 를 줘야 한다**(210 의 xUnit 은 CI 의 DLL 을 검사하지 운영자 기계의 DLL 이 아니다). R3 이 의미를 잃는다 |
| **(c) 데이터 없는 계약 스모크로 바꾼다** | `init` → 세 보정 함수를 교정 없이 새 시그니처로 호출해 **각각 정확히 `CALIB_NOT_LOADED`** 가 오는지(모듈이 문서화한 상태 모델) 확인 | 데이터 생성이 필요 없다. 시그니처가 어긋나면 `INVALID_INPUT`·크래시로 바로 드러난다(오늘 `defect` 가 `INVALID_INPUT` 을 낸 것이 정확히 그 신호였다). 앱 쪽 코드 최소 | NaN/Inf·결정성 같은 **수치 속성은 안 본다**(CI 의 210 이 본다). "R3 = 어댑터 체인 스모크 통과"라는 문구는 "계약 스모크"로 고쳐야 한다 |
| (d) 그대로 둔다 | — | — | 교정 탭이 계속 거짓으로 막힘 표시 + 평가 탭과 모순 + 옛 문구 "fix synthetic oracle gaps" 가 운영자를 오라클로 보냄 |

**권고: (a) 를 하되 반드시 시험과 함께**, 데이터 생성이 과하다고 판단되면 (c). 이유: 이 사건의 뿌리는 오라클이 낡았다는 것이 아니라 **낡아도 아무것도 빨개지지 않는 진단**이었다는 것이다(시험 0건). 어느 선택이든 오라클을 부르는 시험을 두는 것이 재발 방지책이고, (a) 는 앱 준비도가 원래 답하려던 질문("이 기계에서 보정이 실제로 도는가")을 살린다. (b) 는 "운영자 기계의 DLL 을 한 번도 안 돌리고 처리 켜짐"이 되어 권하지 않는다. 평가 탭이 `IsExportReady` 만 보는 불일치(위 화면 표)는 어느 선택이든 같이 정리해야 한다 — 한 값(`ProcessingEnabled`)을 모든 화면이 쓰게.

**공허한 판정 grep (항목 2 마지막)**: 오라클 자신의 **판정**은 공허하지 않다 — `passed = inputPreserved && nanInfCount == 0 && stages.All(Passed)` 이고 단계가 OK 가 아니면 실패라서 이번에 정확히 실패했다. 공허한 것은 **개별 필드**(NaN/Inf 0, RMSE 0, Min=Max=0)이며 `GuiE2eReportService.cs:392–395` 가 그것들을 `Synthetic passed: False` 바로 옆에 줄로 찍는다(판정은 같이 찍히므로 오도 정도는 낮지만 "NaN/Inf 0·RMSE 0"만 읽으면 건강해 보인다). 다른 진단의 통과 판정은 `PreprocessFixtureE2eService.cs:240`(`rawPreserved && anyStageExecuted && NaNInf==0`)·`Phase1bFixtureE2eService.cs:308–315`(단계별 `Stages.Any(Executed)` 포함)처럼 **"실제로 실행됐다"를 요구**해서 같은 결함 모양은 아니다. 다만 `NativePreviewMetrics.InputPreserved: changed == 0 && nanInf == 0`(`NativePreprocessPreviewService.cs:1076` 외 둘)는 이름과 식이 어긋나 보인다("입력 보존"인데 `changed` 는 출력 대 입력의 변경 화소 수) — 읽어서 의심만 했고 어떤 값을 세는지 추적하지 않았으니 **미확인 후보**로만 남긴다.

## 한계
- 레거시 앱 하나(`clients/ImageProcTest`)만 읽었다. 사용자 앱 `gui/ImageProcTest` 는 이 오라클을 쓰지 않는다(검색 확인) — 이 문제는 레거시 진단 앱에 한정된다.
- DLL 은 IntegrationTests 출력 폴더의 것(`xpe_preprocess.dll` 파일 날짜 2026-09-26)이다. 현재 소스로 새로 빌드한 DLL 이 아니다(이 카드는 네이티브를 빌드하지 않는다). 현재 소스와 달라도 결론(시그니처 불일치로 세 호출 거부)은 헤더 기준으로 같지만 "09-26 DLL" 임을 밝혀 둔다.
- 스크린샷을 한 장 찍었으나 저장소에는 두지 않았다(증거는 UIA 덤프 텍스트).
