# GUI-C-224 — #249 SPEC-XPE-GUI-IT 미구현 요구의 다음 묶음 계획 (보고서만, 코드 변경 없음)

트리: `dev/gui` `9368f8cd`(origin/main `8de2a6c0` 의 문서·시험과 대조). 읽은 것: `spec.md`(main), `progress.md`, RTM-GUI-001, GUI-C-207·208·209·210 보고서와 증거, 현재 시험 소스. 실행한 것: REQ-004 변이 실험(아래), 통합 시험 전체(GUI-C-219c 때 839 통과·0 실패·0 건너뜀).

## 0. 결론 먼저

1. **"미구현 17개"는 지금 8개다.** SPEC §4.6 의 19/14/3 은 GUI-C-207 시점(`efbc74f8`)의 수치이고, 그 뒤 GUI-C-208(M1·M2)·209(M1·M2·M3)·210 이 14개 중 9개를 닫았다. 지금 읽은 판정은 **구현됨 28 · 부분 4 · 없음 4**(합 36). 한계를 가진 "구현됨"을 빼면 엄격한 구현됨은 23이다(§1 표의 "구현됨(한계)" 5건). SPEC §4.6·RTM·`progress.md` 는 아직 19/14/3 이라 **문서가 시험보다 뒤처져 있다**(§3 의 묶음 B).
2. **8개 중 모듈 함수가 필요한 것은 0개다.** 전부 gui 시험 쪽에서 끝난다. 결정이 필요한 것은 4~5개(008·050·063·064·065)이고 나머지(020·031·041)는 결정 없이 gui 만으로 된다.
3. **새로 드러난 것: REQ-041 은 지금 시험이 하나도 없다.** GUI-C-208 D5 가 항상 참이던 시험(`WhenDllAbsent_FixtureReportsUnavailable_NotCrash`)을 지우고 `ArchitectureGuard_…`(REQ-042 의 반쪽)로 바꿨는데, 그 시험이 REQ-041 의 유일한 시험이었다. 지금 REQ-041 을 인용하는 것은 남은 문서 주석 한 줄뿐이다(`DllLoadSmokeTests.cs:69`, 이제 다른 시험 위에 붙어 있음).
4. 207 이 찾은 "항상 통과하는 시험" 7건(D1~D7)은 **모두 고쳐졌거나 정정됐다**(§4).

## 1. 미구현 17개 — 지금 상태

범례: 207 = GUI-C-207 시점 판정. 지금 = 이번에 시험 소스를 읽어 다시 매긴 판정(§ 판정 기준은 207 과 같다: 시험이 요구 문구를 독립된 기대값으로 단언해야 "구현됨"). 필요한 것: **gui** = gui 레인(`clients/` 시험)만, **결정** = 사용자/리더 결정 필요, **모듈** = 모듈 함수 필요. 크기: 작음 / 중간 / 큼.

| REQ | 한 줄 요약 | 207 | **지금** | 필요한 것 | 크기 | 근거(코드 위치·검색 범위) |
|---|---|---|---|---|---|---|
| 004 | BodyPart = ANSI 고정 64(`ByValTStr`, `SizeConst=64`, `CharSet.Ansi`)와 63자 왕복 | 부분 | **구현됨** | — | — | 헤더 `char bodyPart[64]` 와 미러의 선언을 `NativeSignatureParityTests`(208 M1)가 기계로 대조한다. **변이 실험**(throwaway 워크트리, 증거 `req004_mutation_experiment.txt`): `SizeConst 64→63`, `ByValTStr→LPStr`, `CharSet.Ansi→Unicode` 셋 모두 `AbiLayoutTests`·`EveryDeclaration_SaysWhatTheHeaderSays(mirror)` 가 빨강, 원본은 18/18 초록. 한계: 앱 쪽 사본은 변이하지 않았다 |
| 006 | 모든 오류 경로가 관리 예외 없이 `XpeErrorCode`(또는 문서화된 예외)로 나타남 | 부분 | **구현됨** | — | — | `ErrorMapping/NegativeInputPathTests.cs`(209 M2): xpe_common 의 서로 다른 거부 경로 **18개**, 행마다 정확한 코드·통제 행(같은 호출을 유효값으로)·네이티브 소스의 검사 문장 인용(소스에 그 문장이 있는지 시험). 반증 증거 `GUI-C-209/m2_falsification_arms.txt`. "20+" 은 한 검사를 두 번 세어야 채워져서 18 이 정직한 수(209) |
| 007 | Mock 백엔드를 쓰지 않음, 폴백은 실패 | 부분 | **구현됨(한계)** | — | — | `Safety/MockBlockingTests.cs`: 로드된 어셈블리에서 `MockXpeBackend`·`CompositeXpeBackend` 타입 부재, 참조 이름 검사, 그리고 208 D4 가 더한 **통제 시험**(`TheTypeScan_FindsATypeThatIsLoaded`, 스캔이 아무것도 못 보면 `Assert.Null` 이 공짜로 참이 되는 것을 막음). 208 이 "CompositeXpeBackend 가 어디에도 없다"는 옛 주석을 정정(`clients/ImageProcTest/Backends/CompositeXpeBackend.cs` 에 있음 — 시험은 실제 방어). 한계: 요구의 "Mock 폴백은 시험 실패로 본다"를 직접 단언하는 시험은 없다(통합 시험은 P/Invoke 미러를 직접 부르므로 폴백 경로가 없다) |
| 008 | 해석된 DLL 경로가 `<repo>/build/` 또는 시험 출력 폴더 아래, 그 밖은 실패 | 부분 | **부분** | **결정**(작음) + gui | 작음 | `Safety/DllSearchPathSafetyTests.cs:23-`: 시스템 폴더 두 곳을 **차단**하고 파일 존재만 본다(허용 목록이 아님). 디코이 시험은 208 D3 가 고쳐 로케이터를 디코이가 PATH 맨 앞에 있는 상태로 다시 돌린다. 결정이 필요한 이유: 로케이터 자신이 `XPE_NATIVE_DIR`(임의 폴더)·`modules/common/build_test`·`clients/ImageProcTest/bin` 을 후보로 가진다(`NativeLibraryFixture.TryLocateDll` 98~110행) — 요구 문구의 허용 목록과 로케이터의 후보가 다르다 |
| 009 | 열거된 오류 코드마다 `xpe_error_string` 이 비지 않은 문자열 | 부분 | **구현됨** | — | — | `Functional/ErrorCodeMappingTests.cs`: 열거 18개 행 전부, 208 D1 이 `NOT_IMPLEMENTED` 고정 건너뜀을 풀고 "폴백이 아님"을 폴백을 DLL 에서 읽어 비교. 반증 `GUI-C-208/m2_falsification_arms.txt`(폴백만 묻게 하면 18/18 빨강). 요구 문구는 비지 않음까지라 부록 A 문구 대조는 요구 밖 |
| 010 | 시험 뒤 고정 핸들 없음 | 부분 | **구현됨(한계)** | — | — | `Safety/LeakEnduranceTests.cs`: `PinnedObjects_AtThisPoint_AreNoMoreThanWhenTheFixtureWasCreated`(런타임의 고정 객체 수를 전체 수집 뒤 센다) + 계측 자체의 통제 시험(208 D2). 한계: 시작 시점 대비 상대 비교이고 xUnit 이 정한 시점에 돈다 |
| 020 | DLL 을 **5초 안에** 찾고 `xpe_version()` 이 semver | 부분 | **부분** | gui | 작음 | `Smoke/DllLoadSmokeTests.cs:26-40`: semver 만 단언. 5초는 어디에도 없다(`TryLocateDll` 호출에 시간 한도 없음 — `Smoke/`·`Fixtures/` 를 `Stopwatch|5000|TimeSpan` 로 검색해 이 요구와 관련된 것 0건) |
| 031 | `xpe_log_flush` 가 **초기화 전·후·종료 후** 어느 상태에서도 예외 없음 | 부분 | **부분** | gui | 작음 | `Lifecycle/LoggingHandlerTests.cs:25`(초기화 전)·`:101`(종료 후)·`ErrorMapping/NativeErrorTranslationTests.cs:78`(초기화 전). **초기화된 상태**에서의 flush 호출은 `xpe_log_flush` 를 부르는 곳 전부(위 3곳)에 없다 |
| 041 | DLL 이 어느 탐색 경로에도 없으면 부트스트랩이 **명확한 메시지의 `DllNotFoundException`** 을 올리고 탐색이 호스트를 죽이지 않음 | 부분 | **없음** | gui(+ 해석 한 줄) | 작음~중간 | 208 D5 가 유일한 시험을 지웠다(위 §0-3). 현재 `DllNotFoundException` 을 일으키는 시험 0건(`grep -rn DllNotFoundException` 으로 `clients/ImageProcTest.IntegrationTests` 의 `*.cs` 검색: 시험 4건은 `catch (… DllNotFoundException …)` 로 삼키기만 하고 일으키지 않음). 해석: 요구의 "부트스트랩이 올린다"와 픽스처의 설계("없으면 `IsAvailable=false` 로 건너뜀")가 다르다 — 시험은 "탐색 경로를 비운 로케이터는 (found=false, 찾은 곳을 말하는 메시지)를 돌려주고 존재하지 않는 DLL 이름의 P/Invoke 는 `DllNotFoundException` 에 그 이름이 있다" 두 쪽으로 나눌 수 있다 |
| 042 | x86↔x64 불일치 시 첫 P/Invoke 가 `BadImageFormatException`, 실패에 경로 포함 | 부분 | **구현됨(한계)** | — | — | `Smoke/ArchitectureMismatchTests.cs`(209 M1) 3건: 같은 x64 시스템 DLL 의 PE 헤더 Machine 필드만 바꾼 사본을 로더가 거부, 첫 `[DllImport]` 호출이 던짐, x86 xpe_common 이 있는 네이티브 폴더에서 부트스트랩이 경로를 말함. 반증 `GUI-C-209/m1_falsification_arms.txt`. 한계: 진짜 x86 빌드 DLL 로는 돌려 보지 못함(209 가 명시) |
| 043 | x64 가 아닐 때 **기록**하는 진단 시험, 다른 시험을 실패시키지 않음 | 부분 | **구현됨(한계)** | — | — | `Diagnostics/PlatformDetectionTests.cs:15`: 아키텍처를 시험 출력과 `platform-diagnostic.txt` 에 기록하고 파일을 읽어 확인(208 D7). 한계: ARM64 분기는 실행해 보지 못함 |
| 050 | AV/SEH 를 관측하지 않음, 관측되면 기록하고 그 시험을 실패시킴 | 부분 | **부분** | **결정**(작음) | — | 위 006 의 18행이 `Record.Exception` 으로 `SEHException` 을 잡아 실패시킨다. `AccessViolationException` 은 .NET 에서 잡을 수 없고 프로세스를 죽인다 — 시험 호스트가 죽으면 CI 가 빨개져 "실패"는 충족되나 "기록"은 없다. 문구대로의 시험(예외를 잡아 기록)은 원리적으로 불가 → 요구를 "호스트가 살아 있다 = 충족"으로 고칠지의 결정 |
| 052 | `MarshalDirectiveException`·`InvalidCastException`·`COMException` 이 번역 없이 새지 않음, 부정 시험마다 기대 코드 문서화 | 부분 | **구현됨(한계)** | — | — | 006 의 18행이 각각 **정확한 코드**를 문서화(209 M2, "이것 또는 저것" 금지). 한계: 그 예외들을 실제로 일으키는 입력은 없다(공통 모듈의 공개 함수에서 만들어지지 않음) |
| 061 | 합성 어댑터 체인 offset→gain→defect: 입력 SHA-256 보존·NaN/Inf 0·결정성 RMSE 0 | 부분 | **구현됨** | — | — | `P1AReady/PreprocessCorrectionChainSmokeTests.cs`(210): defect 단계가 표시된 화소만 고치고 나머지는 비트 동일(`:157`), 입력 버퍼 SHA-256 보존(`:189`)과 그 통제 시험(`:206`), NaN/Inf(`:133`), 결정성(`:111`). 반증 `GUI-C-210/falsification_arms.txt`(결함 맵을 안 적재하면 6건 중 4건 빨강) |
| 063 | 선택: `XPE_GUI_IT_ETW=1` 일 때 `EventSource` 이벤트 | 없음 | **없음** | **결정** | 중간 | `EventSource` 를 쓰는 코드 0건(`clients/`·`gui/` 의 `*.cs` 검색, SPEC §4.6 도 같은 결론). 소비자(이벤트를 읽는 도구)가 없다 |
| 064 | 선택: CI 에 SDK 9.0.x 가 있으면 `net9.0` 보조 통과 | 없음 | **없음** | **결정** + ci.yml(리더) | 중간 | 시험 프로젝트는 `net8.0` 단일 타깃, `ci.yml` 에 `9.0.x` 없음 |
| 065 | 선택: ARM64 에서 arm64 변종 xpe_common 을 찾아 기록 | 없음 | **없음** | **결정** | 작음(코드)·불가(실행) | ARM64 러너가 없어 실행해 볼 수 없다. 코드만 쓰면 한 번도 안 도는 시험이 된다 |

합계(36개 전체): 구현됨 **28**(그중 한계를 가진 것 5: 007·010·042·043·052; 엄격히 23) · 부분 **4**(008·020·031·050) · 없음 **4**(041·063·064·065). 17개 중 이번에 닫힌 것: 004·006·007·009·010·042·043·052·061 = 9, 남은 것 8.

## 2. 지금 8개의 분류

- **gui 만, 결정 없음**: 020, 031, 041 (셋 다 작음~중간).
- **gui + 결정 한 줄**: 008(허용 목록을 어떻게 쓸지), 050(AV 를 어떻게 읽을지).
- **결정 필요(구현 여부 자체)**: 063·064·065 — 선택 요구이고 소비자·러너가 없다.
- **모듈 함수가 필요한 것: 없음.**
- **ci.yml(리더 소유)이 필요한 것**: 064(SDK 9 보조 통과) 하나.

## 3. 다음 구현 카드 후보 (gui 레인만, 결정 불필요)

**묶음 A — "DLL 부트스트랩·수명주기의 빈 곳"(REQ-020·031·041)**. 완료 조건: ① 020: `TryLocateDll`+첫 `xpe_version` 호출이 5초 안에 끝나고 semver 라는 시험이 있고, 한도를 0.001 s 로 바꾸면 빨강(반증); ② 031: 초기화된 상태에서 `xpe_log_flush` 가 예외 없이 끝나는 시험이 있고(초기화 전·후·종료 후 세 상태가 한 시험 클래스에 모임), 그 상태를 만드는 `xpe_init` 호출을 빼면 시험이 "초기화 전" 상태가 되어 구분되는지 통제로 확인; ③ 041: 탐색 경로를 비운 로케이터가 (found=false, 찾은 곳을 말하는 메시지)를 돌려주고, 없는 DLL 이름으로의 P/Invoke 가 그 이름을 담은 `DllNotFoundException` 을 올린다는 시험 — 반증: 메시지에서 이름을 빼면 빨강. 통합 시험 전체 초록 유지.

**묶음 B — "문서를 시험에 맞춤"(코드 없음, 문서만)**. 완료 조건: SPEC §4.6 표·`progress.md`·RTM-GUI-001 의 요구별 상태가 이 보고서 §1 과 같다(구현됨 28/부분 4/없음 4, 근거 시험 이름은 `grep` 으로 존재 확인), 그리고 SPEC 이 말하는 시험 클래스 이름 16개 중 실재하는 이름으로 바꾼다. gui 레인은 문서를 직접 고치지 않고 `.txt` 수정안으로 낸다(이 저장소의 규칙). **A 보다 먼저 하면 안 된다**: A 가 끝나면 수치가 한 번 더 바뀐다(구현됨 31 · 부분 2 · 없음 3) — 순서는 A → B.

**묶음 C — 결정이 나온 뒤: 008 허용 목록 + 050 문구**. 완료 조건: 008: 로케이터의 후보와 같은 허용 목록(결정된 문구)으로 `ResolvedDllPath` 를 단언하고, 디코이/시스템 폴더/후보 밖 경로 세 입력에서 빨강; 050: 결정된 문구에 맞는 시험(예: "호스트가 모든 부정 시험을 마쳤다 + `SEHException` 이 잡히면 실패")과 요구 문구 수정안. 결정 전에는 착수하지 않는다.

(선택 요구 063~065 는 후보에서 뺐다: 소비자 없는 EventSource, CI SDK, ARM64 러너 모두 이 레인 밖이거나 실행 불가다. 리더가 "철회/보류"로 결정해 SPEC 에 적는 편이 추적 비용이 가장 낮다.)

## 4. 207 이 찾은 "항상 통과하는 시험" 다시 확인 (현재 소스를 읽음)

| 207 항목 | 지금 | 근거 |
|---|---|---|
| D1 `NOT_IMPLEMENTED` 낡은 고정 건너뜀 | **고쳐짐** | `ErrorCodeMappingTests` — 건너뜀 해제, 폴백을 DLL 에서 읽어 비교. 반증 18/18 빨강(`GUI-C-208/m2_falsification_arms.txt`) |
| D2 `AfterTests_NoOutstandingPinnedHandles` 가 핸들을 안 봄 | **고쳐짐** | 런타임 고정 객체 수 + 계측 통제 시험(`LeakEnduranceTests`) |
| D3 디코이 시험이 PATH 를 해석 뒤에 바꿈 | **고쳐짐** | 로케이터를 디코이가 앞선 PATH 로 다시 돌림(`DllSearchPathSafetyTests`) |
| D4 `CompositeXpeBackend…NotLoaded` 가 항상 통과 | **정정됨** | 타입이 `clients/ImageProcTest/Backends/` 에 있다(208 이 옛 주석을 틀렸다고 정정). 시험은 실제 방어이고 스캔의 통제 시험이 추가됨 |
| D5 `WhenDllAbsent_…` 항진 | **교체됨 — 그러나 REQ-041 이 시험 없이 남음** | `ArchitectureGuard_RejectsABinaryThatIsNotX64…` 로 바뀌었고 그것은 REQ-042 의 시험이다. §0-3 |
| D6 `ResolvedDll_IsX64Architecture` 가 DLL 없을 때 조용히 통과 | **고쳐짐** | `DllLoadSmokeTests` — 건너뜀(Skip) 으로 바뀜 |
| D7 `ProcessArchitecture_IsX64` 가 REQ-043 과 모순 | **고쳐짐** | `PlatformDetectionTests.ProcessArchitecture_IsRecordedForDiagnostics`(기록 후 파일을 읽어 확인) |
| (D13 오류 코드 열거 중 3개만 고정) | **고쳐짐** | `StructLayoutParityTests`: 18개 전부 값 대조 |

고정 건너뜀·항상 참인 단언 재검색: `Skip = "` 0건(`clients/ImageProcTest.IntegrationTests` `*.cs`), `Assert.True(true` 0건. 이 검색은 문자열 모양만 본다 — **새로 항상 참인 시험이 없다는 증명은 아니다**.

"구현됨" 중 남은 작은 약점(207 의 주석에서 아직 해소되지 않은 것을 코드로 다시 보지는 않았다): 051 의 초기화 결과를 `OK` 또는 `NOT_INITIALIZED` 로 느슨히 받음, 025 가 두 코드 중 하나를 허용(요구 문구가 그렇다), 060 의 건너뜀 문구가 요구와 다름. AC-12 시간 한도는 209 M3 이 ci.yml 의 단계(`Assert the SPEC time gates (AC-12)`)로 넣었다(main 에 있음).

## 5. 모듈 함수·결정이 필요한 것 — 리더용 한 줄

- **008(결정)**: "허용 폴더" 를 로케이터의 후보(`XPE_NATIVE_DIR`·`build/**`·`modules/common/build_test`·시험 출력·`clients/ImageProcTest/bin`)와 맞출지, 요구 문구(`build/` 와 시험 출력만)대로 로케이터 후보를 줄일지.
- **050(결정)**: AV 는 .NET 에서 잡히지 않아 "기록"을 단언할 수 없다. "호스트 생존 + SEHException 은 잡혀 실패" 로 요구 문구를 바꿔도 되는지.
- **063·064·065(결정)**: 구현 / 철회 / 보류. 064 는 ci.yml 에 SDK 9 보조 통과를 넣는 일(리더), 065 는 ARM64 러너가 없어 실행 불가.
- **모듈 함수가 필요한 요구: 없음. 사용자에게 물을 것: 위 결정 중 요구 철회에 해당하는 063·065 정도.**

## 6. 한계 / 미검증

- 시험은 소스를 읽어 판정했고 실행한 것은 REQ-004 변이 실험과 통합 시험 전체뿐이다. "구현됨" 28 중 004 외에는 이전 카드(208·209·210)의 반증 증거를 인용했고 이번에 다시 변이하지 않았다.
- 207 의 `구현됨` 19(001·002·003·005·021~030·040·051·053·060·062) 는 이번에 다시 읽지 않았다(001 은 빌드 속성, 나머지는 207 판정 인용). 그 사이 해당 시험 파일이 바뀐 곳(208 M2 의 `ErrorCodeMappingTests` 등)만 확인했다.
- SPEC 문구 일부가 줄 잘림으로 읽힌 곳은 207 과 같다.
- 카드가 말한 "RTM-GUI·GUI-IT 진행 문서는 리더가 고쳤다(`50ca6bee`·`edf69c1c`)"는 main 의 `spec.md` §4.6(19/14/3)을 읽었다. `edf69c1c` 는 이 저장소에서 확인하지 않았다.
- 다음 카드의 "완료 조건"은 이번에 시험해 보지 않은 설계다.
