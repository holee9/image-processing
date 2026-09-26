# GUI-C-05 — 90건 중 네이티브 부재 시 실질 무검증인 테스트 분류

- 카드: GUI-C-05. **상태: 완료 (합격 조건 3건 충족)**
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui`
- Refs: #98
- 변경한 코드: **없음** (분류 카드). `git status` clean.

---

## 0. 결론 — B 유형 **60건 / 90건 (66.7%)**

> **CI 를 붙이기 전까지 검증되지 않는 실제 범위 = 90건 중 60건.**

| 분류 | 정의 | 실행 케이스 | 비율 | 테스트 메서드 |
|------|------|-----------:|-----:|-------------:|
| **A. 네이티브 무관** | 네이티브 가용성 참조 자체가 없음 | **27** | 30.0% | 27 |
| **B. 네이티브 필수** | 첫 실행문이 가용성 가드 + 조기 `return` → DLL 부재 시 본문 전체 미실행 | **60** | 66.7% | 45 |
| **C. 부분 의존** | 가용성을 참조하되 조기 return 이 아니거나, 가드 앞에 실제 단언이 선행 | **3** | 3.3% | 3 |
| 합계 | | **90** | 100% | 75 |

메서드 75개 → 실행 케이스 90개 (`[Theory]` 2개의 `InlineData` 확장: `ErrorString_ForAllDefinedCodes` 11행, `LogSetLevel_ValidLevels` 6행 → +15).

---

## 1. 선결 정정 — 지시서와 내 이전 보고의 전제가 둘 다 틀렸다

카드 배경에는 `Fixtures/SkipHelper.ShouldSkip()` 이 조기 return 을 만들어 Pass 로 집계된다고 적혀 있다. 나 자신도 GUI-C-02·03·04 에서 같은 설명을 반복했다. **틀렸다.**

```bash
grep -rn 'ShouldSkip' clients/ImageProcTest.IntegrationTests --include=*.cs
  Fixtures/SkipHelper.cs:12:    /// Caller should return early: if (SkipHelper.ShouldSkip(condition)) return;
  Fixtures/SkipHelper.cs:14:    public static bool ShouldSkip(bool condition) => condition;
```

**호출 지점 0건.** `SkipHelper` 는 정의만 있고 아무도 쓰지 않는 죽은 코드다. 나는 이 함수의 존재와 주석만 보고 메커니즘을 단정했고, 호출부를 확인하지 않았다. 관측 없는 주장이었다.

실제 메커니즘은 각 테스트 본문에 직접 박힌 인라인 조기 return 이며, **가드 축이 두 개**다:

| 가드 | 대상 DLL | 형태 | 위치 |
|------|----------|------|------|
| `if (!_fixture.IsAvailable) return;` | `xpe_common.dll` | `NativeLibraryFixture` 주입 | 36건 (메서드) |
| `if (DllPath is null) return;` | **`xpe_preprocess.dll`** | 정적 필드 `DllPath` | 9건 (메서드) |

두 번째 축(`xpe_preprocess.dll`)은 GUI-C-02~04 어디에도 등장하지 않았다. `xpe_common.dll` 만 보고 있었기 때문이다. **네이티브 의존은 하나가 아니라 둘이다.**

---

## 2. 판정 규칙 (기계적, 추측 없음)

각 `[Fact]`/`[Theory]` 메서드 본문에서 XML 주석·중괄호·공백을 제거해 실행문 목록을 만든 뒤:

| 조건 | 분류 |
|------|------|
| 네이티브 가용성 식(`IsAvailable`, `DllPath is null`, `NativeLibrary.TryLoad`) 참조가 **전혀 없음** | **A** |
| 참조가 있고, **첫 실행문**이 그 조건의 `if (...) return;` | **B** |
| 참조는 있으나 조기 return 이 아니거나, 가드 앞에 실제 단언이 선행 | **C** |

`[Theory]` 는 `InlineData` 행 수만큼 케이스로 환산했다.

---

## 3. 전체 분류표 (75 메서드 / 90 케이스, 누락 0)

### A. 네이티브 무관 — 27건

| 파일:행 | 테스트 | 근거 |
|---------|--------|------|
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:14` | Compute_NullReadiness_ReturnsNotConnectedSnapshot | 가용성 참조 없음 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:28` | Compute_EmptyReadiness_ReturnsNotConnectedSnapshot | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:37` | Compute_ReadinessWithoutAiModule_ReturnsNotConnected | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:51` | Compute_AiModuleR0_ReturnsNotConnected | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:67` | Compute_AiModuleR1WithoutConnectAttempt_ReturnsAvailable | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:83` | Compute_AiModuleR2WithoutConnectRequest_ReturnsAvailable | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:100` | Compute_AiModuleR2UserRequestedConnect_ReturnsConnecting | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:116` | Compute_AiModuleR3Enabled_ReturnsConnected | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:132` | Compute_LastErrorMessage_OverridesToErrorState | 〃 |
| `AiBridgeUi/AiBridgeStatusComputerTests.cs:152` | Compute_NeverThrowsOnUnusualReadinessShapes | 〃 |
| `Diagnostics/PlatformDetectionTests.cs:15` | ProcessArchitecture_IsX64 | 〃 |
| `Diagnostics/PlatformDetectionTests.cs:22` | IntPtrSize_IsEight | 〃 |
| `Diagnostics/PlatformDetectionTests.cs:29` | DotNetVersion_Is8OrHigher | 〃 |
| `Diagnostics/PlatformDetectionTests.cs:38` | OperatingSystem_IsWindows | 〃 |
| `Functional/StructLayoutParityTests.cs:16` | XpeImageBuffer_RoundTrip_PreservesAllFields | 〃 |
| `Functional/StructLayoutParityTests.cs:52` | XpePixelFormat_EnumValues_MatchNativeAbi | 〃 |
| `Functional/StructLayoutParityTests.cs:60` | XpeErrorCode_EnumValues_MatchNativeAbi | 〃 |
| `Safety/MockBlockingTests.cs:20` | MockXpeBackend_TypeNotLoadedInTestAssemblies | 〃 |
| `Safety/MockBlockingTests.cs:34` | CompositeXpeBackend_TypeNotLoadedInTestAssemblies | 〃 |
| `Safety/MockBlockingTests.cs:48` | TestAssembly_HasNoMockBackendReference | 〃 |
| `Smoke/AbiLayoutTests.cs:17` | XpeImageBuffer_MarshalSize_Is40Bytes | 〃 |
| `Smoke/AbiLayoutTests.cs:25` | XpeImageMetadata_MarshalSize_Is96Bytes | 〃 |
| `Smoke/AbiLayoutTests.cs:37` | XpeImageBuffer_FieldOffsets_MatchNativeLayout | 〃 |
| `Smoke/AbiLayoutTests.cs:54` | XpeImageMetadata_BodyPart_63CharAscii_RoundTrips | 〃 |
| `Smoke/AbiLayoutTests.cs:80` | XpeImageMetadata_BodyPart_FieldOffset_IsZero | 〃 |
| `Smoke/AbiLayoutTests.cs:88` | IntPtrSize_IsEight_OnX64 | 〃 |
| `Smoke/DllLoadSmokeTests.cs:71` | ProcessArchitecture_IsRecordedForDiagnostics | 〃 |

> A 의 성격: ABI 구조체 레이아웃(Marshal.SizeOf/OffsetOf), enum 값 대조, 순수 C# 상태 계산기, 플랫폼 감지, Mock 차단 어셈블리 검사. **네이티브 바이너리 없이도 의미 있는 검증**이다. 특히 `AbiLayoutTests`/`StructLayoutParityTests` 는 관리 측 P/Invoke 선언이 네이티브 ABI 상수와 맞는지 보는 것이라, DLL 없이도 회귀를 잡는다.

### B. 네이티브 필수 — 60 케이스 / 45 메서드 ← **이 카드의 핵심 숫자**

#### B-1. `xpe_common.dll` 가드 (`if (!_fixture.IsAvailable) return;`) — 51 케이스 / 36 메서드

| 파일:행 | 테스트 | 가드 위치 |
|---------|--------|-----------|
| `ErrorMapping/NativeErrorTranslationTests.cs:31` | GetParamRange_BeforeInit_ReturnsNotInitialized | L33 |
| `ErrorMapping/NativeErrorTranslationTests.cs:42` | Version_BeforeInit_DoesNotCrash | L44 |
| `ErrorMapping/NativeErrorTranslationTests.cs:58` | ErrorString_BeforeInit_DoesNotCrash | L60 |
| `ErrorMapping/NativeErrorTranslationTests.cs:73` | LogFlush_BeforeInit_DoesNotCrash | L75 |
| `ErrorMapping/NativeErrorTranslationTests.cs:86` | Configure_VeryLongMalformedJson_DoesNotThrowManagedException | L88 |
| `ErrorMapping/NativeErrorTranslationTests.cs:103` | GetPendingAlert_TinyBuffer_ReturnsErrorCodeNotException | L105 |
| `ErrorMapping/NativeErrorTranslationTests.cs:123` | AllocImage_HugeDimensions_ReturnsErrorNotException | L125 |
| `Functional/ErrorCodeMappingTests.cs:36` | ErrorString_ForAllDefinedCodes_IsNonNullAndNonEmpty **[Theory ×11]** | L38 |
| `Functional/ErrorCodeMappingTests.cs:50` | ErrorString_ForUnknownCode_ReturnsFallbackNonNull | L52 |
| `Functional/ErrorCodeMappingTests.cs:65` | ErrorString_CalledTwice_ReturnsSamePointer | L67 |
| `Functional/ImageBufferLifecycleTests.cs:38` | AllocImage_ValidDimensions_ReturnsOkAndNonZeroData | L40 |
| `Functional/ImageBufferLifecycleTests.cs:57` | FreeImage_AfterAlloc_ReturnsOkAndZeroesData | L59 |
| `Functional/ImageBufferLifecycleTests.cs:74` | AllocImage_ZeroDimensions_ReturnsInvalidInput | L76 |
| `Functional/ImageBufferLifecycleTests.cs:90` | CopyImage_MatchingDimensions_ReturnsOk | L92 |
| `Functional/ImageBufferLifecycleTests.cs:117` | CopyImage_MismatchedDimensions_ReturnsError | L119 |
| `Functional/ImageBufferLifecycleTests.cs:148` | Init_Success_AlertCountIsNonNegative | L150 |
| `Functional/MetadataMarshallingTests.cs:28` | Configure_ValidJson_ReturnsOk | L30 |
| `Functional/MetadataMarshallingTests.cs:40` | Configure_MalformedJson_ReturnsConfigInvalid | L42 |
| `Functional/MetadataMarshallingTests.cs:52` | GetParamRange_ChestWindowCenter_ReturnsValidRange | L54 |
| `Functional/MetadataMarshallingTests.cs:66` | GetParamRange_UnknownBodyPart_ReturnsErrorCode | L68 |
| `Lifecycle/AlertCallbackTests.cs:30` | AlertQueue_Empty_CountIsZeroAndFetchReturnsInvalidInput | L32 |
| `Lifecycle/AlertCallbackTests.cs:47` | ClearAlerts_CalledRepeatedly_DoesNotThrow | L49 |
| `Lifecycle/LoggingHandlerTests.cs:25` | LogFlush_PreInit_DoesNotThrow | L27 |
| `Lifecycle/LoggingHandlerTests.cs:42` | LogSetLevel_ValidLevels_ReturnsOk **[Theory ×6]** | L44 |
| `Lifecycle/LoggingHandlerTests.cs:53` | LogSetLevel_NegativeOne_ReturnsInvalidInput | L55 |
| `Lifecycle/LoggingHandlerTests.cs:64` | LogSetLevel_Six_ReturnsInvalidInput | L66 |
| `Lifecycle/LoggingHandlerTests.cs:75` | LogSetFile_WritableTempPath_ReturnsOk | L77 |
| `Lifecycle/LoggingHandlerTests.cs:89` | LogSetFile_NonExistentDirectory_ReturnsIoFailed | L91 |
| `Lifecycle/LoggingHandlerTests.cs:101` | LogFlush_PostShutdown_DoesNotThrow | L103 |
| `Safety/DllSearchPathSafetyTests.cs:27` | ResolvedDllPath_IsUnderBuildTreeOrTestOutput | L29 |
| `Safety/DllSearchPathSafetyTests.cs:49` | DllImportResolver_WinsOverSystemPath_WhenEnvVarOrAppContextHasDll | L51 |
| `Safety/LeakEnduranceTests.cs:36` | InitShutdown_1000Cycles_NoLeak | L38 |
| `Safety/LeakEnduranceTests.cs:84` | AfterTests_NoOutstandingPinnedHandles | L86 |
| `Smoke/DllLoadSmokeTests.cs:30` | XpeVersion_WhenDllLoaded_ReturnsSemverString | L32 |
| `Smoke/DllLoadSmokeTests.cs:99` | XpeVersion_CalledTwice_ReturnsSamePointer | L101 |
| `Smoke/VersionPinTests.cs:28` | XpeVersion_MajorMatchesPinnedVersion | L30 |

#### B-2. `xpe_preprocess.dll` 가드 (`if (DllPath is null) return;`) — 9 케이스 / 9 메서드

| 파일:행 | 테스트 | 가드 위치 |
|---------|--------|-----------|
| `P1AReady/CalibrationCheckExpirySmokeTests.cs:22` | CalibLoadOffset_NonExistentPath_ReturnsIoFailed | L24 |
| `P1AReady/CalibrationCheckExpirySmokeTests.cs:31` | CalibLoadGain_NonExistentPath_ReturnsIoFailed | L33 |
| `P1AReady/CalibrationCheckExpirySmokeTests.cs:40` | CalibLoadDefectMap_NonExistentPath_ReturnsIoFailed | L42 |
| `P1AReady/PreprocessCorrectionChainSmokeTests.cs:16` | CorrectionChain_WithoutCalibration_PreservesSyntheticInput | L18 |
| `P1AReady/PreprocessCorrectionChainSmokeTests.cs:37` | CorrectionChain_RunTwice_DeterministicRmseIsZero | L39 |
| `P1AReady/PreprocessCorrectionChainSmokeTests.cs:65` | CorrectionChain_Output_HasNoNanOrInf | L67 |
| `P1AReady/PreprocessHandshakeTests.cs:22` | PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString | L24 |
| `P1AReady/PreprocessHandshakeTests.cs:49` | PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds | L51 |
| `P1AReady/PreprocessHandshakeTests.cs:76` | PreprocessDll_WhenStaged_HasAllRequiredExports | L78 |

> B-2 는 **2차 가드**도 가진다: `if (!NativeLibrary.TryLoad(DllPath, out var handle)) return;`. `DllPath` 가 있어도 로드 실패 시 다시 조기 return 한다. 즉 `xpe_common.dll` 만 스테이징해도 이 9건은 여전히 무검증으로 남는다.

### C. 부분 의존 — 3건

| 파일:행 | 테스트 | 근거 |
|---------|--------|------|
| `Smoke/DllLoadSmokeTests.cs:49` | ResolvedDll_IsX64Architecture | 가드(L57) **앞에** 실제 단언이 선행 — `ResolvedPath` 가 "Architecture mismatch" 를 포함하면 `Assert.Fail`. 아키텍처 불일치 검출은 DLL 부재와 무관하게 동작 |
| `Smoke/DllLoadSmokeTests.cs:87` | WhenDllAbsent_FixtureReportsUnavailable_NotCrash | 조기 return 없음. `IsAvailable \|\| SkipReason 비어있지 않음` 을 무조건 단언 — DLL 부재 시에도 "픽스처가 부재를 결정론적으로 보고했는가" 를 실제로 검증 |
| `Smoke/DllLoadSmokeTests.cs:116` | ArchitectureMismatch_SurfacesResolvedPathInMessage | 조기 return 없음. `ResolvedPath` 비어있지 않음을 무조건 단언한 뒤, mismatch/available 분기에서만 추가 단언 |

---

## 4. 판독 — 이 숫자가 의미하는 것

- **90건 중 60건(66.7%)이 현재 공회전이다.** 실행되지만 첫 줄에서 반환하고, xUnit 은 이를 Skip 이 아니라 Pass 로 집계한다. 건너뜀 카운트가 0인 이유다.
- 무검증 범위는 `xpe_common.dll` 하나로 닫히지 않는다. 51건은 `xpe_common.dll`, **9건은 `xpe_preprocess.dll`** 을 별도로 요구한다. `common-build` 아티팩트만 인계하면 B-2 9건은 여전히 통과-무검증으로 남는다.
- 실제로 검증되고 있는 것은 A 27건 + C 3건 = **30건(33.3%)** 이며, 그 대부분은 ABI 레이아웃·enum 대조·순수 로직이다. 네이티브 런타임 동작(alloc/free/init/shutdown/로깅/에러코드)은 **한 건도 검증되지 않고 있다.**

### CI(#98) 에 대한 함의

| 스테이징 대상 | 해소되는 B | 남는 B |
|---------------|-----------:|-------:|
| 없음 (현재) | 0 | 60 |
| `xpe_common.dll` 만 | 51 | **9** |
| `xpe_common.dll` + `xpe_preprocess.dll` | 60 | 0 |

`build/ci-common` 인계만으로는 60건 중 51건만 해소된다. 나머지 9건까지 살리려면 P1A(preprocess) 산출물도 스테이징하거나, 최소한 그 9건이 여전히 무검증임을 CI 가 명시해야 한다.

---

## 5. 미검증 (Gaps)

- **네이티브 DLL 을 빌드해 재실행하지 않았다** (카드가 금지). 따라서 "DLL 을 스테이징하면 60건이 실제로 검증된다"는 것은 **분류에서 도출한 추론이지 관측이 아니다.** 위 표의 "해소되는 B" 열은 예측값이다.
- 분류는 **정적 판독**이다. 런타임에 실제로 조기 return 했는지 실행 추적으로 확인하지 않았다(카드가 코드 수정을 금지하므로 계측 불가).
- B 로 분류된 테스트가 DLL 존재 시 실제로 통과할지는 확인하지 않았다. 무검증에서 벗어난다는 것과 통과한다는 것은 다르다.
- `[Theory]` 케이스 환산은 `InlineData` 행 수를 셌다. `MemberData`/`ClassData` 는 이 저장소에 없음을 확인했으나, `--list-tests` 의 90건과 환산값 90이 일치하는 것으로 교차 확인했다.
- C 3건은 "일부 경로만 네이티브를 탄다"에 해당하나, 각 경로의 커버리지 비율은 산정하지 않았다.

## 6. 잔여 위험 / 후속 제안

- **`SkipHelper` 는 죽은 코드다.** 호출 0건이면서 "이것이 스킵 메커니즘"이라는 오해를 만들었다 — 나와 lead 가 각각 이 함수만 보고 메커니즘을 단정했다. 삭제하거나, 실제로 `[Fact(Skip=...)]` 집계로 전환하는 별도 카드를 권고한다. 후자를 택하면 건너뜀 카운트가 0이 아니게 되어 대시보드에서 무검증이 즉시 보인다. **이 카드에서는 지시대로 손대지 않았다.**
- `NativeLibraryFixture.SkipReason` 프로퍼티는 `[Fact(Skip=...)]` 에 쓰라고 문서화되어 있으나 실제로는 어디에도 연결되지 않았다. 위와 같은 사안이다.
- 무검증 66.7% 는 CI 부재와 곱해져 위험이 커진다 — 지금은 아무도 이 60건을 실행하지 않고, 실행해도 통과한다.

---

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj \
  -c Debug --no-build --list-tests          # → 90건 목록 (list-tests.log)
grep -rn 'ShouldSkip' clients/ImageProcTest.IntegrationTests --include=*.cs   # → 정의 2행뿐, 호출 0건
grep -rnE 'IsAvailable|DllPath is null|NativeLibrary.TryLoad' clients/ImageProcTest.IntegrationTests --include=*.cs
# 분류: [Fact]/[Theory] 탐지 → 중괄호 매칭으로 본문 추출 → 주석·공백 제거 후
#       첫 실행문이 네이티브 가용성 조기 return 인지 판정 (스크립트 실행, 코드 무수정)
```

로그: `D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-05/list-tests.log`
