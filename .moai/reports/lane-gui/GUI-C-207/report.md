# GUI-C-207 — SPEC-XPE-GUI-IT 작업표·요구 실태 대조 (보고서, 코드 변경 없음)

트리: `dev/gui` `efbc74f8` · 증거(이 폴더): `req_census_out.txt`, `work_row_census_out.txt`, `verified_claims_out.txt`, `rtm_and_ci_claims_check.txt`, `experiment_not_implemented_row_unskipped.txt`, 수정안 `spec_and_progress_correction_draft.txt`·`rtm_correction_draft.txt`. 센 스크립트의 파서에는 대조군을 두었다(각 출력 첫 줄).

판정 기준(카드 그대로): **구현됨** = 시험이 그 요구 문구를 독립된 기대값으로 단언한다. 컨트롤을 가져오기만 하는 시험, 자기 비교, 여러 결과 중 아무거나 허용하는 단언, 이름만 약속하는 시험은 **부분**. **없음** = 그 요구를 단언하는 시험이 없다. 상태가 시험이 아닌 빌드 속성인 요구는 따로 적는다.

## 0. 결론 먼저

1. **이 SPEC 에는 작업표가 없다.** `.moai/specs/SPEC-XPE-GUI-IT/` 에는 `spec.md`·`progress.md`·`research.md` 뿐이고, AC-15 가 "구비"라고 한 `plan.md`·`acceptance.md`·`tasks.md` 는 한 번도 만들어지지 않았다. 그래서 행은 SPEC 이 스스로 둔 네 가지 표로 잡았다: 요구 36개(§4), 수용 기준 15개(§10), 심볼 15개(§5.1), `progress.md` 의 완료 표.
2. **정의된 요구는 36개인데 문서마다 다른 수를 말한다.** `progress.md`·RTM 은 "53 REQ", RTM 표는 39행, 실제 `spec.md` 제목으로 정의된 것은 **36개**다. RTM 의 **REQ-032·033·034 는 SPEC 에 정의가 없다**(AC-4 의 `020~034` 범위도 같은 유령 셋을 끌어온다). 대조군: 같은 파서가 정의된 029 는 잡고 없는 999 는 못 잡았다.
3. **36개 중 구현됨 19(그중 1은 빌드 속성) · 부분 14 · 없음 3.** 없음은 REQ-063·064·065(ETW·.NET 9·ARM64)이고 어떤 행에도 안 걸린다(AC·§5·§11·진행표 어디에도 없음).
4. **이름 census 와 행위 census 가 다르다.** 시험 소스가 REQ ID 를 인용한 것은 32개(미인용 001·063·064·065)인데, 인용된 32개 중 **14개는 문구의 일부만 단언**한다. 반대로 SPEC·RTM 이 적은 시험 클래스 이름 16개 중 실제로 있는 것은 **2개**(`AbiLayoutTests`·`LeakEnduranceTests`)뿐인데, 요구는 다른 이름의 클래스로 시험되고 있다(`requirement-matrix.json` 이 그 실제 이름을 가진다). 즉 문서의 시험 이름 열은 거의 전부 틀렸고, 시험 자체는 대부분 있다.
5. **이 SPEC 의 유일한 건너뜀 하나가 이제는 통과하는 케이스를 가리고 있다.** `ErrorCodeMappingTests` 의 `NOT_IMPLEMENTED` 행은 "네이티브에 매핑이 아직 없다(#126/QA-A-23)"며 고정 건너뜀이지만, `xpe_common.cpp:412` 가 이미 `Function not implemented in this version` 을 돌려준다. 건너뜀을 풀고 돌리면 **통과**한다(`experiment_not_implemented_row_unskipped.txt`, 파일은 바이트 동일 복원).
6. **SPEC 의 [HARD] 규칙 하나("실제 `XpeCommonApi` 의 `[DllImport]` 를 직접 호출, P/Invoke shim 재작성 금지")가 구조적으로 어겨져 있고 그것을 지키는 장치가 반쪽이다.** 시험 프로젝트는 `PInvoke/XpeCommonNative.cs` 라는 **거울**(DllImport 16개)을 쓴다(GUI-C-13 이 한 어셈블리에 DllImport 해석기 둘을 못 둔다고 결정). DLL 이름(`DllNameParityTests`)과 오류 코드 열거(`ErrorCodeHeaderParityTests`)는 소스로 대조하지만, **함수 시그니처·심볼 집합은 대조하지 않는다** — 거울의 `extern` 은 16개, 앱 래퍼(`PInvokeWrapper.cs`)는 15개로 거울에만 `xpe_alert_push` 가 있고, 나머지 15개는 **이름 집합이 같다는 것까지만** 확인했다(매개변수·마샬링 속성이 같은지는 어떤 시험도, 이 보고서도 보지 않았다).
7. **RTM 의 ✓ 는 시험이 아니라 문서의 시험 이름을 가리킨다.** RTM §4·§5 가 근거로 삼은 E2E ID 중 `W-04`·`W-06`(두 번)은 **코드에 없고**, `A-01~A-03` 은 접근성 시험이 아니라 지금은 다른 것(About·자동화 보고서)이며, "CI pseudo-localization"·"Category=Smoke CI 타이밍"·`MOAI_XPE_BACKEND_MODE` 는 **코드·CI 어디에도 없다**(문서에만). `.resx` 도 0개다.
8. **새 이슈가 필요하다**(이 SPEC 을 추적하는 열린 이슈가 없다 — `gh issue list --search SPEC-XPE-GUI-IT` 는 닫힌 #115·#111 만 돌려줬고 둘은 이 대조와 무관). 결함 후보는 §5, 수정안은 `.txt` 초안 둘.

## 1. 요구 36개 — 행 대조 (`verified_claims_out.txt`: 인용한 시험 이름 전부를 시험 메서드로 grep, 없는 것 0, 가짜 이름 대조군은 "없음"으로 나옴)

표 상태는 `progress.md` 가 "All AC done, all 53 REQ mapped"라 적은 것 외에 요구별 상태 열이 없다(**모두 "완료"로 읽힌다**). "시험이 단언하지 않는 문구"는 REQ 문구를 줄 단위로 읽어 시험 본문과 맞춘 것이다.

| REQ | 실제 | 근거(시험, 클래스) | 시험이 단언하지 않는 문구 |
|---|---|---|---|
| 001 xUnit·net8.0·x64·Nullable | **구현됨(시험 아님)** | csproj: `net8.0`·`x64`·`Nullable enable`·xunit 2.9.3·Test.Sdk 17.11.0 | 시험 인용 0건. 빌드가 곧 증거 |
| 002 크기 40/96 | **구현됨** | `XpeImageBuffer_MarshalSize_Is40Bytes`·`XpeImageMetadata_MarshalSize_Is96Bytes` (AbiLayoutTests) | — |
| 003 필드 오프셋 | **구현됨** | `XpeImageBuffer_FieldOffsets_MatchNativeLayout` | — |
| 004 BodyPart ANSI 64 | **부분** | `…63CharAscii_RoundTrips`, `…FieldOffset_IsZero` | `[MarshalAs(ByValTStr, SizeConst=64)]`·`CharSet.Ansi` 를 읽지 않는다. 왕복은 마샬러가 **자기가 쓴 것**을 읽는 것(네이티브 쪽·비ASCII·넘침 입력 없음) |
| 005 IntPtr 수명 | **구현됨** | `XpeVersion_CalledTwice_ReturnsSamePointer`, `ErrorString_CalledTwice_ReturnsSamePointer` | "해제하지 않는다"(부재) |
| 006 오류 경로에 관리 예외 없음 | **부분** | `Configure_VeryLongMalformedJson_…`, `GetPendingAlert_TinyBuffer_…`, `AllocImage_HugeDimensions_…` (NativeErrorTranslationTests) | "모든 오류 경로 시험": 시나리오 **3개**. `AccessViolation`·`SEH` 는 `Record.Exception` 으로 잡히지 않고 프로세스가 죽는 것으로만 나타난다 |
| 007 Mock 배제 | **부분** | `MockXpeBackend_TypeNotLoaded…`, `CompositeXpeBackend_TypeNotLoaded…`, `TestAssembly_HasNoMockBackendReference` | `CompositeXpeBackend` 는 **어디에도 없는 타입**이라 항상 통과(SPEC §4 실측 주석이 이미 적음). 참조 시험은 어셈블리 **이름**에 "Mock" 이 든 참조만 본다 — 소스로 링크된 타입은 안 보인다. "Mock 폴백을 실패로 취급"은 단언하는 시험 없음 |
| 008 해석된 DLL 경로 | **부분** | `ResolvedDllPath_IsUnderBuildTreeOrTestOutput`, `DllImportResolver_WinsOverSystemPath_…` | 요구는 `<repo>/build/` 또는 시험 출력 폴더만 허용(**허용 목록**). 시험은 `system32`·`syswow64` 만 배제(차단 목록)하고 파일 존재를 본다. 디코이 시험은 PATH 를 **해석이 끝난 뒤** 바꾸므로 단언 대상에 영향을 줄 수 없다 |
| 009 오류 코드 열거 대응 | **부분** | `ErrorString_ForAllDefinedCodes_…`(열거 18개 행), `…ForUnknownCode_…` | 문자열이 비지 않음·"Unknown error"가 아님만 단언 — api-spec 부록 A 의 **문구**는 대조하지 않는다. `NOT_IMPLEMENTED` 행은 낡은 고정 건너뜀(§0-5). 열거는 18개(SPEC 은 11개) |
| 010 핸들 누수 없음 | **부분** | `AfterTests_NoOutstandingPinnedHandles` | 이름이 약속하는 핀 핸들 검사는 없다. 단언은 **관리 힙 < 200 MiB**(절대값, 델타 아님). SPEC 이 정한 검증도 "GetTotalMemory 상식 점검"이라 약하다. 트레이트는 `Safety`(SPEC 은 `Lifecycle`) |
| 020 라이브러리 적재 | **부분** | `XpeVersion_WhenDllLoaded_ReturnsSemverString` | "5초 안에 찾는다"는 재지 않는다 |
| 021 초기화 성공 | **구현됨** | `Init_Success_AlertCountIsNonNegative` | — |
| 022 설정 JSON | **구현됨** | `Configure_ValidJson_ReturnsOk`, `Configure_MalformedJson_ReturnsConfigInvalid` | — (`{not json` 이 `CONFIG_INVALID`) |
| 023 할당/해제 | **구현됨** | `AllocImage_ValidDimensions_…`(512 바이트), `FreeImage_AfterAlloc_ReturnsOkAndZeroesData` | — |
| 024 잘못된 치수 | **구현됨** | `AllocImage_ZeroDimensions_ReturnsInvalidInput` | — |
| 025 복사 | **구현됨** | `CopyImage_MatchingDimensions_ReturnsOk`, `CopyImage_MismatchedDimensions_ReturnsError`(요구 문구가 두 코드 중 하나를 허용) | 복사된 화소는 비교 안 함(문구에 없음) |
| 026 파라미터 범위 | **구현됨** | `GetParamRange_ChestWindowCenter_ReturnsValidRange` | 값 자체는 대조 안 함(문구는 `min ≤ dflt ≤ max` 만) |
| 027 알림 큐 | **구현됨** | `AlertQueue_Empty_CountIsZeroAndFetchReturnsInvalidInput` | — |
| 028 알림 비우기 멱등 | **구현됨** | `ClearAlerts_CalledRepeatedly_DoesNotThrow` | — |
| 029 로그 수준 | **구현됨** | `LogSetLevel_ValidLevels_ReturnsOk`(0~5), `…NegativeOne…`, `…Six…` | — |
| 030 로그 파일 | **구현됨** | `LogSetFile_WritableTempPath_ReturnsOk`, `…NonExistentDirectory_ReturnsIoFailed` | 실제 기록은 안 봄(문구에 없음) |
| 031 flush 무예외 | **부분** | `LogFlush_PreInit_…`, `LogFlush_PostShutdown_…`, `LogFlush_BeforeInit_…` | "초기화된 뒤" 상태가 없다(요구: 초기화 전·후·종료 후) |
| 040 초기화 전 가드 | **구현됨** | `GetParamRange_BeforeInit_ReturnsNotInitialized` | — (AC-5 의 `UninitializedGuardTests` 클래스는 없고 시험은 NativeErrorTranslationTests 안에 있다) |
| 041 DLL 부재 | **부분** | `WhenDllAbsent_FixtureReportsUnavailable_NotCrash` | 요구는 `DllNotFoundException` + 분명한 메시지. 단언은 `IsAvailable \|\| SkipReason 비지 않음`으로 **어떤 상태에서도 참** |
| 042 아키텍처 불일치 | **부분(사실상 없음)** | `ArchitectureMismatch_SurfacesResolvedPathInMessage`, `ResolvedDll_IsX64Architecture` | `BadImageFormatException` 을 일으키는 시험이 없다(x86 DLL 을 주지 않음). 메시지 시험은 불일치 모드일 때만 무언가를 단언하고, `ResolvedDll_…` 는 DLL 이 없으면 조용히 반환(건너뜀이 아님) |
| 043 플랫폼 진단 | **부분** | `ProcessArchitecture_IsRecordedForDiagnostics`, `PlatformDetectionTests.ProcessArchitecture_IsX64` | 어느 것도 아키텍처를 **기록**하지 않는다(출력·추적 없음). `…IsX64` 는 ARM64 에서 **실패**한다 — 요구는 "다른 시험을 실패시키지 않고 기록" |
| 050 AV 전파 없음 | **부분** | 006 의 3개 + `Version_/ErrorString_/LogFlush_BeforeInit_DoesNotCrash` | AC-9·진행표의 "20개 넘는 부정 시나리오"는 **3 + 3**. AV/SEH 는 프로세스 내에서 관찰 불가 |
| 051 1000회 누수 | **구현됨** | `InitShutdown_1000Cycles_NoLeak` (WS < 20 MiB, GC < 5 MiB) | 주의: init 결과를 `OK` 또는 `NOT_INITIALIZED` 로 느슨히 받는다. "90초 안에" 는 재지 않는다 |
| 052 마샬링 예외 번역 | **부분** | 006 의 2개 | `MarshalDirective`·`InvalidCast`·`COM` 예외를 일으키는 시험이 없다 |
| 053 버전 스큐 | **구현됨** | `XpeVersion_MajorMatchesPinnedVersion`(고정 파일 `expected-versions.json`) | — |
| 060 preprocess 수명주기 | **구현됨** | `PreprocessVersion_WhenDllStaged_…`, `PreprocessInitShutdown_…`, `PreprocessDll_WhenStaged_HasAllRequiredExports` | 버전은 "비지 않음"만. 건너뜀 문구가 요구와 다르다 |
| 061 합성 어댑터 체인 | **부분** | `CorrectionChain_RunTwice_DeterministicRmseIsZero`, `…Output_HasNoNanOrInf`, `…WithoutCalibration_ReturnsCalibNotLoaded` | "입력 SHA-256 보존": 단언 **0건**(P1AReady 에서 SHA256 grep 0). 체인은 offset→gain 뿐(defect 단계 없음) |
| 062 보정 로더 계약 | **구현됨** | `CalibLoadOffset/Gain/DefectMap_NonExistentPath_ReturnsIoFailed` | — |
| 063 ETW | **없음** | — | `EventSource`·`XPE_GUI_IT_ETW` 를 쓰는 코드 0 |
| 064 .NET 9 | **없음** | — | 단일 `net8.0` 타깃, `ci.yml` 에 `9.0.x` 없음 |
| 065 ARM64 | **없음** | — | 코드 0 |

합계: 구현됨 **19**(001 포함, 시험이 아닌 것 1) · 부분 **14** · 없음 **3** = 36.

## 2. 수용 기준 15개 (진행표는 전부 "✓ PASS/READY")

| AC | 표 | 실제 | 이유 |
|---|---|---|---|
| 1 프로젝트 빌드 | ✓ | **구현됨** | 빌드·CI 가 곧 증거 |
| 2 ABI 크기 | ✓ | **부분** | REQ-004 가 부분 |
| 3 DLL 해석 | ✓ | **부분** | REQ-008·041·042 가 부분·부분·사실상 없음 |
| 4 "18개 심볼" | ✓ | **구현됨(수치 틀림)** | §5.1 표·§8 은 **15개**. 15개 모두 시험이 호출한다(`work_row_census_out.txt`). `xpe_shutdown`·`xpe_clear_alerts`·`xpe_log_flush` 는 반환값이 `void` 라 단언할 반환이 없고, 효과는 REQ-040·028 시험이 간접으로 본다. 범위 `020~034` 는 유령 032~034 를 끌어온다 |
| 5 초기화 전 가드 | ✓ | **구현됨(이름 다름)** | `UninitializedGuardTests` 클래스는 없다 |
| 6 "11개 오류 코드" | ✓ | **부분** | 열거는 **18개**. 알 수 없는 코드는 "비지 않음"만 단언 |
| 7 1000회 | ✓ | **구현됨** | "90초 안에" 미단언 |
| 8 Mock 배제 | ✓ | **부분** | REQ-007 |
| 9 부정 입력 | ✓ | **부분** | "20+ 시나리오" 거짓(3+3) |
| 10 알림 큐 | ✓ | **구현됨** | — |
| 11 로그 | ✓ | **부분** | REQ-031 |
| 12 성능 게이트 | ✓ | **없음** | `Category=Smoke` 필터를 도는 CI 단계가 없다(`.github` 0건), 시험별 5초 한도도 없음. 전체 IntegrationTests 가 이 기계에서 10초라 지금은 만족할 가능성이 크지만 **재지 않는다** |
| 13 선택 시험 | ✓ READY | **구현됨** | CI 는 xpe_preprocess 를 올려 9건이 실제로 돈다 — 진행표의 "SKIP (awaiting xpe_preprocess.dll)" 은 낡음 |
| 14 IEC 관련 매핑 | ✓ READY | **부분** | `requirement-matrix.json` 은 있고 33건을 실제 클래스에 매핑(전부 파일 존재). 063~065 누락, 그 json 과 RTM 의 시험 이름이 서로 다르다 |
| 15 DoD | ✓ READY | **부분/없음** | `plan.md`·`acceptance.md`·`tasks.md` 없음(3/5), README "How to run integration tests" 없음(`clients/README.md` 는 §"Native DLL search" 하나), MX 태그 2개(`@MX:ANCHOR` NativeLibraryFixture, `@MX:NOTE` 거울) — "각 `[DllImport]` 에 `@MX:NOTE`" 는 아님 |

AC 는 16개가 아니라 15개다(HISTORY v1.1.0 "All 16 AC", 진행표 "AC-16 ✓").

## 3. 어떤 작업(행)에도 안 걸린 요구 — 이름 census 와 행위 census

| 축 | 센 것 | 결과 |
|---|---|---|
| 정의 | `spec.md` 제목 `REQ-GUI-IT-nnn` | **36** |
| 문서가 말하는 수 | `progress.md`·RTM §1 "53"; RTM 표 39행 | RTM 39 − 정의 36 = **유령 3**(032·033·034) |
| 행(이름) | AC 제목이 부르는 REQ | 30/36 — **AC 에 없음: 005·010·043·063·064·065** |
| 행(이름) | AC·§5.2·§5.3·§11·진행표 어느 곳이든 | **063·064·065 는 어느 행에도 안 걸림** |
| 시험 소스가 REQ ID 를 인용 | 이름 census | 32/36 — 미인용 001·063·064·065 |
| 시험이 문구를 단언 | 행위 census(§1) | 구현됨 19 · **부분 14** · 없음 3 |
| 계획된 시험 클래스 이름이 존재 | §5·§11 의 16개 | **2/16** |

두 축의 차이가 발견이다: 이름으로 센 "인용 안 된 REQ 4개"는 행위로 다시 세면 **세 배 넘는 문제**가 된다(없음 3 + 부분 14). 그 14개 중 상당수가 같은 모양이다 — 부정 입력 시나리오 수 부풀림(006·050·052), 조건부로 아무것도 안 하는 단언(041·042), 문구의 한 갈래 누락(031·020·061), 이름이 약속한 것을 안 하는 시험(010).

## 4. RTM-GUI-001 ✓ 중 시험이 없는 것 (`rtm_and_ci_claims_check.txt`)

- §1·§2: "53 REQ", "총 39 / 구현 39 / 검증 37 / 95%" — 정의 36, 구현됨 19·부분 14·없음 3. 이벤트 구동 "15개(020~034)" 도 12개(020~031)다.
- §3 시험 클래스 열: REQ 36행 중 클래스 이름이 실제로 있는 것은 **`AbiLayoutTests`(002·003·004) 와 `LeakEnduranceTests`(010·051) 뿐**이고, 메서드 이름이 실제로 있는 것은 **0행**(`SizeOfXpeImageBuffer`, `FieldOffsets`, `StaticPointer`…). REQ-032~034 행은 "existing test class"로 얼버무려 있다.
- §4: `W-04`(비동기 P/Invoke) 와 `W-06`(×2: RealXpeBackend 표시, GSDF 토글) 은 E2E 에 **없다**(§5 HAZ-GUI-004 줄은 W-06 이 없었음을 스스로 인정하면서 §4.4 는 그대로 둠). `S-02`·`S-04`·`S-05`·`W-07` 은 있다 — 단 S-04 는 항목이 켜져 있음만 본다(GUI-C-203·204).
- §4.2 "E2E A-01"(AutomationProperties 필수, AutomationId 영문): 지금 `A01` 은 About 시험이고, **모든 대화형 XAML 요소를 세는 접근성 시험은 없다**. HAZ-GUI-009 의 "E2E A-01~A-03 Accessibility suite" 도 같다.
- §4.3 "ENV `MOAI_XPE_BACKEND_MODE=Mock`": 코드·CI 0건(문서 3곳뿐). 실제 변수는 `XPE_E2E_BACKEND`. "Smoke suite < 30s, xUnit filter `Category=Smoke`, CI timing": 그 필터를 쓰는 곳이 없다.
- §4.6·HAZ-GUI-010: `.resx` 0개, CI pseudo-localization 0건 — "ko-KR/en-US `Strings.resx`" 는 구현이 없다(검증 "Manual").
- §6: "5.2.6 모든 REQ-GUI-IT-* 에 시험 ✓" — 063~065 에 없다. "5.7 78/78 xUnit pass (2026-04-18)" — 지금 IntegrationTests 는 650 통과/1 건너뜀이고 대부분 다른 SPEC 의 시험이다.
- §7.1 "Optional tests 060~063: xpe_preprocess.dll 미스테이징으로 skip" — 지금 CI 가 올려 060~062 는 돈다.

## 5. 빌드·실행에서 빠진 시험 — CI 대조

| 시험 프로젝트 | CI 잡 | 필터 | 비고 |
|---|---|---|---|
| `clients/ImageProcTest.IntegrationTests` | `dotnet-tests` (`ci.yml:636`, **main 또는 lane=gui 일 때만**) | **없음** | `Verify native binaries present` 가 xpe_common·preprocess·enhance_basic·display·dicom·dcmdata·openjp2 를 요구해, GUI-IT 의 `SkippableFact` 는 DLL 부재로 조용히 건너뛰지 않는다. 건너뜀이 실패인 게이트는 BaselineDicom·EnhanceBasic·BaselineDisplay 세 클래스에만 있다 |
| `clients/ImageProcTest.E2ETests` | `gui-automation`(Mock)·`gui-e2e-native` | **없음** | Native 잡에만 unexplained-skip 게이트 |
| `gui/ImageProcTest.SelfCheck`·`.E2E` | `gui-shell-runners` | `dotnet run` | — |

- csproj 제외(`Compile Remove`) 0, 고정 `Skip =` 0. **건너뛴 시험은 한 건**(`NOT_IMPLEMENTED` 행)이고 그것을 지켜보는 게이트는 없다.
- `ci.yml` 의 주석 "SkipHelper has zero callers" 는 낡았다: 지금 16개 파일이 부른다(`ci.yml` 은 리더 소유).
- AC-12 의 `Category=Smoke` < 30초·시험별 5초 상한: 그 상한을 거는 단계가 없다.

## 6. 결함 후보 (고치지 않음; 중요도는 제 판단)

| # | 후보 | 근거 | 중요도 |
|---|---|---|---|
| D1 | `NOT_IMPLEMENTED` 행의 낡은 고정 건너뜀 | `xpe_common.cpp:412` 매핑 존재; 건너뜀 해제 시 통과(실험) | 중 |
| D2 | `AfterTests_NoOutstandingPinnedHandles` 가 핸들을 안 본다 | 단언은 `GetTotalMemory < 200 MiB` | 중 |
| D3 | 디코이 시험이 무효 | PATH 를 해석 **뒤**에 바꾼다 | 중 |
| D4 | `CompositeXpeBackend…NotLoaded` 가 항상 통과 | 타입이 어디에도 없음(SPEC 도 앎) | 낮 |
| D5 | `WhenDllAbsent_…` 항진 | `A \|\| !B`: 어떤 상태에서도 참 | 낮 |
| D6 | `ResolvedDll_IsX64Architecture` 가 DLL 없을 때 조용히 반환 | 건너뜀(Skip)이 아니라 통과로 셈 | 낮 |
| D7 | `PlatformDetectionTests.ProcessArchitecture_IsX64` 가 REQ-043 과 모순 | ARM64 에서 실패 | 낮 |
| D8 | 거울 P/Invoke 선언의 **시그니처 대조 없음**(SPEC [HARD] "shim 금지"의 우회) | 거울 `extern` 16 vs 앱 래퍼 15, 거울에만 `xpe_alert_push`; 대조는 DLL 이름·오류 열거뿐 | **높** |
| D9 | REQ-042 의 `BadImageFormatException` 시험 없음 | x86 DLL 을 쓰는 시험 0 | 중 |
| D10 | REQ-061: 입력 SHA-256 보존·defect 단계 없음 | `SHA256` grep 0 | 중 |
| D11 | REQ-006·050·052 의 부정 입력 시나리오가 3개 | AC-9 "20+" | 중 |
| D12 | REQ-063·064·065 구현 0, 어느 행에도 없음 | — | 결정 필요(구현할 것인가, 철회할 것인가) |
| D13 | `StructLayoutParityTests` 가 열거 18개 중 3개만 고정 | `OK`·`-1`·`-10` | 낮(헤더 대조가 따로 있음) |
| D14 | AC-12 상한을 재는 곳이 없다 | `Category=Smoke` 0 | 낮 |

## 미검증·한계

1. 시험 본문은 위 17개 파일(GUI-IT 의 원래 클래스)을 **읽어** 판정했다. 이번에 실행해 본 것은 전체 IntegrationTests(650 통과·1 건너뜀)와 `NOT_IMPLEMENTED` 행의 해제 실험뿐이다.
2. REQ 문구 중 일부는 SPEC 의 줄이 잘려 읽혔다(`REQ-003` 의 `XpeImageMetadata` 필드 목록, `REQ-009` 의 메시지 조건). 문구 전체를 읽은 것은 REQ-021~025·053·060~065 등이다.
3. "구현됨"으로 센 것도 CI 에서 돌았는지는 실행 로그로 확인하지 않았다(`dotnet-tests` 잡 정의를 읽음). 특히 xpe_common 이 `NOT_IMPLEMENTED` 를 매핑한 빌드가 CI 의 빌드인지는 소스 줄만 확인했다.
4. RTM·SPEC 의 SRS/SDD 열(문서 ID)은 대조하지 않았다.
5. 접근성·지역화 시험이 **전혀 없는지**는 `AutomationProperties`·`.resx`·`pseudo` 검색으로만 봤다.
6. 판정 14건의 "부분"은 REQ 문구를 엄격히 읽은 것이며 요구 문구 자체를 약하게 고치는 쪽이 맞는 경우가 있다(REQ-010·043). 어느 쪽으로 고칠지는 리더 결정이다.
