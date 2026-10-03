# GUI-C-208 — 거울 P/Invoke 시그니처 대조(M1)와 빈 시험 바로잡기(M2) (#249)

증거(이 폴더): `m1_falsification_arms.txt`, `m2_falsification_arms.txt`, `composite_backend_search.txt`. 커밋: M1 `37468bf7`, M2 는 이 보고서와 같은 커밋 이후(`보고` 메시지의 SHA).

## 0. 먼저 — GUI-C-207 의 D4 는 틀렸고, SPEC 의 실측 주석도 틀렸다

207 보고서는 "`CompositeXpeBackend` 는 어디에도 없는 타입이라 그 시험은 항상 통과한다"고 적었다. **검색 없이 SPEC §4 의 2026-09-17 실측 주석(`7a2de35c`)을 옮긴 것**이다. 이번에 검색했다(`composite_backend_search.txt`, 같은 검색이 `MockXpeBackend` 는 23개 파일에서 찾는 대조군 포함):

- `CompositeXpeBackend` 는 **있다**: `clients/ImageProcTest/Backends/CompositeXpeBackend.cs`(`internal sealed class`), 레거시 clients 앱의 `App.xaml.cs`·`MainWindow.xaml.cs`·`Phase1bFixtureE2eService.cs`·`PreprocessFixtureE2eService.cs` 가 쓴다(그 앱은 Real 실패 시 Mock 으로 **조용히 폴백**하는 합성 백엔드다).
- 따라서 `CompositeXpeBackend_TypeNotLoadedInTestAssemblies` 는 **의미 있는 지킴이**이고, REQ-GUI-IT-007 의 Composite 절반도 살아 있다. SPEC §4 의 실측 주석(`7a2de35c`)과 수용 기준 AC-8 에 대한 "존재하지 않는 타입을 단언한다"는 서술은 **고쳐야 한다**(리더 소유 문서). 207 의 REQ-007 행(`부분`)은 다른 이유(참조 시험이 어셈블리 *이름*만 본다, "Mock 폴백을 실패로 취급" 미단언)로는 여전히 맞지만 "항상 통과" 문장은 틀렸다.
- 이 카드에서의 처리: 시험을 **지우지 않았다**(처음 지웠다가 이 검색으로 되돌렸다). 대신 스캔이 정말 무언가를 보는지 붙잡는 **대조 시험**을 추가했다(`TheTypeScan_FindsATypeThatIsLoaded`): `SafeGetTypes` 는 `ReflectionTypeLoadException` 을 삼키고 빈 목록을 돌려주므로 아무것도 못 본 스캔도 `Assert.Null` 을 통과시킨다.

## 1. M1 — 거울 P/Invoke 시그니처 대조 (`NativeSignatureParityTests`)

**무엇을 대조하나.** 네이티브 공개 헤더 3개(`xpe_common_api.h`·`xpe_error.h`·`xpe_memory.h`)의 `XPE_API` 선언 **16개**를, xpe_common 을 부르는 **C# 사본 셋**과 소스로 대조한다: 시험 거울(`XpeCommonNative.cs`), 앱 래퍼(`PInvokeWrapper.cs`), gui 앱의 사본(`XpeDisplayInterop.cs` 의 `xpe_common.dll` 선언 5개). 항목: 함수 이름, 인자 수, 각 인자(값/포인터, 정수 폭·부호, 구조체, 문자열 `LPStr`/`StringBuilder`, `ref`/`out`/`in` 수식어와 const 여부), 반환 형식, 호출 규약(Cdecl), 문자셋(Ansi); 그리고 헤더의 구조체 둘(`XpeImageBuffer`·`XpeImageMetadata`: 필드 순서·형식·이름·`Pack = 8`·`char[64]` → `ByValTStr SizeConst=64`)과 `XpePixelFormat` 열거(값 집합).

**형식은 대응표로 읽는다.** 네이티브 형식 표(`NativeKinds`)와 C# 형식 표(`CSharpKinds`)에 **없는 형식은 실패**한다(조용히 건너뛰지 않음) — 통제 시험이 가짜 네이티브 형식 `FooHandle`·`BarKind` 와 C# `Int128` 로 확인한다. 한쪽에만 있는 선언은 **허용 목록**(이유 포함)으로만 통과하고, 목록의 낡은 항목도 실패한다: 앱 래퍼의 `xpe_alert_push`(생산자 쪽이라 앱은 안 쓰고 시험만 씀), gui 사본은 "부분 사본"(`Complete = false`), gui 의 `XpePixelFormatNative` 는 UInt8 이 없는 부분 열거(gui 는 UInt16·Float32 만 넘김).

**첫 실행의 발견 하나:** 시험 거울의 `XpePixelFormat` 에 헤더의 `XPE_PIXEL_UINT8 = 2` 가 **빠져 있었다**(앱 래퍼엔 있음). 거울에 `UInt8 = 2` 를 채웠고, `StructLayoutParityTests` 가 이제 3개 멤버를 센다. 함수 선언은 세 사본 모두 헤더와 **일치**했다.

**반증(`m1_falsification_arms.txt`)**: 10팔, 8팔 빨강 — 거울의 `xpe_configure` 문자열을 `LPWStr` 로, `xpe_clear_alerts` 를 `StdCall` 로, UInt8 을 3 으로, `xpe_get_param_range` 의 `out float` 를 `out double` 로(알 수 없는 형식 실패); 앱 래퍼의 `xpe_alloc_image` 인자 `uint` → `int`, `xpe_log_flush` 개명(헤더에만/사본에만 있음 둘 다); gui 사본의 `out int severity` → `out uint`, 구조체 필드 `UIntPtr` → `ulong`. **나머지 둘**(거울의 `BitsStored` 를 `int` 로, `xpe_get_pending_alert` 의 `index` 를 `uint` 로)은 **시험 코드가 컴파일되지 않아** 팔이 못 돈다 — 같은 종류(인자 정수 부호)는 통제 시험(`Control_AChangedParameterType_IsReported` 가 거울의 `xpe_log_set_level(int)` → `uint` 를 메모리에서 바꿔 빨강을 확인)과 앱 래퍼·gui 팔이 대신한다. 전부 바이트 동일 복원.

**한계**: 헤더 파서는 이 헤더 셋의 모양(한 줄 `XPE_API 반환 이름(인자);`)에 맞춘 단순 정규식 + 표다 — 함수 포인터·배열 인자·매크로 반환형이 생기면 "읽을 수 없음" 실패로 알려준다. `[In]`/`[Out]` 속성·`SetLastError`·`MarshalAs` 의 세부(`SizeParamIndex` 등)는 보지 않는다. xpe_common 외 DLL(preprocess·enhance·display·dicom·ai) 선언은 이번 범위 밖이다.

## 2. M2 — 빈 시험 (D1~D7, D13). 결과: IntegrationTests **662 통과 / 0 건너뜀**(전: 650 통과 / 1 건너뜀), 7회 연속 동일

| # | 고친 것 | 반증 (`m2_falsification_arms.txt`) |
|---|---|---|
| D1 | `NOT_IMPLEMENTED` 행의 낡은 고정 건너뜀을 풀었다(네이티브가 이미 매핑 — 207 에서 해제 실험으로 통과 확인). "fallback 이 아님"의 기준을 리터럴 `"Unknown error"` 대신 **DLL 이 미매핑 코드(-999)에 돌려주는 문구**로 바꿨다 | 시험이 각 코드 대신 -999 를 물으면 **18행 전부 빨강**(`NOT_IMPLEMENTED` 행 포함) |
| D2 | `AfterTests_NoOutstandingPinnedHandles`(힙 < 200 MiB)를 런타임의 **고정 객체 수**(`GCMemoryInfo.PinnedObjectsCount`, 전체 압축 수집 뒤)를 **픽스처 생성 시점의 수와 비교**하는 `PinnedObjects_AtThisPoint_AreNoMoreThanWhenTheFixtureWasCreated` 로 바꿨다. 1000회 시험은 자기 루프 전후의 고정 객체 수를 직접 비교한다. 이 도구가 고정 핸들을 **세는지**는 통제 시험(`TheInstrument_CountsAPinnedHandle_…`: 핸들을 잡으면 늘고 풀면 돌아옴)이 붙잡는다 | 1000회 루프 안에서 핸들 하나를 풀지 않으면 빨강; 점검 직전 하나를 흘리면 빨강 |
| D3 | 디코이 시험은 해석이 **끝난 뒤** PATH 를 바꿨다. 이제 PATH 맨 앞에 디코이를 두고 **로케이터를 다시 돌려**(`TryLocateDll` 을 `internal static` 으로) 결과가 디코이가 아니고 픽스처가 해석한 파일과 같은지 단언한다. 추가로 `TheLoadedModule_IsTheResolvedFile`: 프로세스에 실제로 올라온 `xpe_common.dll` 모듈 경로가 해석된 파일이고 디코이는 없다 | 로케이터가 PATH 를 먼저 보게 바꾸면 빨강 |
| D4 | **지우지 않았다**(§0). 스캔의 대조 시험 추가 | 스캔이 **있는** 타입을 찾도록 바꾸면 Mock 시험 빨강; 스캔이 아무것도 못 보게 하면 대조 시험 빨강 |
| D5 | 항진 `IsAvailable \|\| SkipReason 비지 않음` 을 지우고, 그 자리에 **아키텍처 가드의 실제 동작**을 시험: 시험이 만든 PE 헤더로 AMD64 는 받고 i386·ARM64·MZ 두 바이트·MZ 아님·없는 파일은 **거절하며 던지지 않는다**(`VerifyX64Pe` 를 `internal static` 으로) | 가드가 받는 machine 을 i386 으로 바꾸면 빨강 |
| D6 | `ResolvedDll_IsX64Architecture` 는 DLL 이 없으면 `return`(통과로 셈) 했다 → `[SkippableFact]` + 픽스처의 사유로 **Skip** | DLL 을 못 찾는 조건에서: 새 시험 **건너뜀**, 커밋된 옛 시험 **통과**(= 결함의 직접 재현) |
| D7 | REQ-043 은 "다른 시험을 실패시키지 않고 아키텍처를 **기록**"하라는 요구다. `ProcessArchitecture_IsX64`(ARM64 에서 **실패**)와 DllLoadSmoke 의 `…IsRecordedForDiagnostics`(X64 또는 Arm64 만 받고 아무것도 기록 안 함)를 지우고, `PlatformDetectionTests.ProcessArchitecture_IsRecordedForDiagnostics` 가 아키텍처·OS·프레임워크를 시험 출력과 `platform-diagnostic.txt` 에 **기록**하고(옛 기록이 만족시키지 못하게 먼저 지움) 읽어 확인한다. 비-x64 이면 사유를 출력하고 실패하지 않는다 | 기록을 쓰지 않게 하면 빨강 |
| D13 | `XpeErrorCode_EnumValues_MatchNativeAbi` 가 18개 중 3개만 고정하던 것을 **18개 전부(이름·값)와 개수**로 고정. `XpePixelFormat` 3개 멤버(UInt8 포함)도 고정 | 거울의 `DICOM_INVALID` 값을 -23 으로 바꾸면 빨강 |

범위 밖(#249 에서 추적, 손대지 않음): D9(x86 DLL 의 `BadImageFormatException`)·D10(입력 SHA-256 보존)·D11(부정 입력 20+)·D12(REQ-063~065 — 리더가 "선택 요구·미구현"으로 둠)·D14(AC-12 측정)와 D8 의 **나머지 DLL**. REQ-008 의 허용 목록(차단 목록 → 허용 목록) 전환도 이번 범위가 아니다.

## 미검증·한계

1. 고정 객체 수 비교(`PinnedObjects_AtThisPoint…`)는 이 기계에서 7회 연속 통과했으나 **CI 러너에서 런타임이 소량의 일시적 핀을 더 세어 흔들릴 수 있는지는 모른다**. 흔들리면 이 시험은 `≤ 픽스처 시작 시점` 대신 여유를 두는 쪽으로 고쳐야 한다(통제 시험이 "핸들이 수를 올린다"는 것은 붙잡는다).
2. D7 의 비-x64 갈래는 **ARM64 에서 실행해 보지 못했다**(분기를 읽고 작성). 기록 자체는 x64 에서 동작한다.
3. "Mock·Native 두 구성"은 이 시험 프로젝트에는 백엔드 구성이 없다. 대신 기본 DLL 과 `XPE_NATIVE_DIR` 를 다른 모듈 폴더로 고정한 실행 둘 다 662/0 이고, DLL 을 못 찾는 조건은 D6 팔로 확인했다.
4. 새 파서(M1)는 CI 에서 `dotnet-tests` 잡이 도는 것으로만 확인된다(푸시 전). 이 시험들은 네이티브 DLL 이 없어도 도는 부분(M1 전체, D5)과 DLL 이 필요한 부분(D2·D3·D6)을 섞는다.
