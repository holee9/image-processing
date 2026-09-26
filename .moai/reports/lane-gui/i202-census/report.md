# `#202` 1번 — 공용 모듈 적재 전수 조사

> 걸린 곳과 **안 걸린 곳의 이유**를 둘 다 적는다. 안 걸리는 이유가 근본 고침의 근거다.
> 이 조사는 **측정과 읽기만** 했고 코드는 바꾸지 않았다.

## 1. 결론

`clients/`·`gui/` 에서 네이티브 모듈을 직접 적재하는 곳은 **6곳(4파일)** 이고, 그중
**위험한 것은 1곳**이다 — `GUI-C-135` 에서 이미 고쳤다. 나머지는 **안 걸리는데, 이유가
하나로 모인다**: 모듈 전역 상태를 **두 번째 핸들로 읽지 않는다.**

함정은 **둘이 함께** 있어야 생긴다.

1. 모듈 **전역 상태**를 쓰고 읽는다(알림 큐, `g_calib`)
2. 한 프로세스에서 그 모듈이 **두 가지로 해결**된다

## 2. 전수 — 6곳

| # | 자리 | 적재 방식 | 만지는 전역 | 위험 | 이유 |
|---|---|---|---|---|---|
| 1 | `Fixtures/NativeLibraryFixture.cs:65` | `xpe_common`, **경로** (env → 시험 출력 → …) | 없음 — **함수를 부르지 않는다** | 아니오 | 적재 가능성만 확인하는 핸들이다. **그러나 순서를 정하는 당사자다**(§4) |
| 2 | `Functional/GsdfCalibrationInputTests.cs:114` | `xpe_display`, 경로 | 없음 | 아니오 | `xpe_gsdf_calibrate` 만 부르고 LUT 은 **호출자 소유 버퍼**다. 모든 데이터가 매개변수로 오간다 |
| 3 | `P1AReady/NonlinearityStageWiringTests.cs:48` | `xpe_preprocess`, 경로 | `g_calib`(init/shutdown) | 아니오 | 단언이 **화소 버퍼의 바이트 동일성**이고 버퍼는 매개변수다. 파일 주석이 *"알림이 뜨는 것이 아니라 단계가 도는 것을 단언한다"* 고 적어 둔다 |
| 4 | `P1AReady/NonlinearityStageWiringTests.cs:112` | 같음 | 같음 | 아니오 | 같음 |
| 5 | `P1AReady/GainPolyClampAlertTests.cs:220` | `xpe_preprocess`, 경로 | **알림 큐(쓰기)** + `g_calib` | **그렇다** | 미는 쪽 |
| 6 | `P1AReady/GainPolyClampAlertTests.cs:451` | `xpe_common`, **이름**(어셈블리 기준) | **알림 큐(읽기)** | **그렇다** | 읽는 쪽. 5 와 **다른 인스턴스**로 해결됐다 → `GUI-C-135` 에서 고침 |

`DllImport` 로 이름 해결되는 곳(`clients/ImageProcTest/**` 의 `DllName` 상수 8개)은 모두
**어셈블리 옆**으로 해결되므로 서로 같은 인스턴스다 — 섞이는 것은 **경로 적재와 이름
적재가 만나는 자리**뿐이다.

## 3. 구조적 원인 — **복사가 비대칭이다**

```
clients/ImageProcTest.IntegrationTests.csproj  Target CopyXpeDllsForTests
  → 시험 출력으로 복사하는 것: xpe_common.dll, spdlog.dll, fmt.dll     (그것뿐)

실제 시험 출력 디렉터리의 xpe_*.dll:   xpe_common.dll                  (하나뿐)
XpePreprocessNative.TryFindDll() 이 주는 것: build/ci-common/bin/xpe_preprocess.dll
```

**`xpe_common` 은 두 곳에 있고 `xpe_preprocess` 는 한 곳에만 있다.**

```
98bfc688  build/ci-common/bin/xpe_common.dll                        ← xpe_preprocess 의 이웃
98bfc688  clients/.../bin/Debug/net8.0/xpe_common.dll               ← 어셈블리 옆
```

**md5 가 같은 것이 함정이다.** 파일이 동일하다는 것은 **인스턴스가 하나라는 뜻이 아니다** —
Windows 는 경로별로 모듈을 따로 적재하고, 각 인스턴스가 자기 `g_alerts` 를 갖는다.

## 4. 왜 전체 실행에서는 초록이었나 — **순서를 정하는 당사자는 #1 이다**

`NativeLibraryFixture` 가 `xpe_common` 을 **경로**로 적재하는데, 그 경로의 우선순위가
`XPE_NATIVE_DIR` → **시험 출력 디렉터리** → … 다. 전체 실행에서 이 픽스처가 먼저 돌면
**어셈블리 옆 사본**이 프로세스에 먼저 들어오고, 그 뒤 `xpe_preprocess` 를 적재하면
Windows 가 **이름으로 이미 적재된 그것**에 묶는다. 그래서 읽는 쪽과 같아진다.

**단독 실행에서는 그 픽스처가 안 돌아** `xpe_preprocess` 가 자기 이웃을 적재하고, 읽는
쪽이 어셈블리 옆 사본을 새로 적재한다 → 큐가 둘.

### 여기에 뒤집기가 하나 더 있다 — **§4c 에서 측정했다**

`NativeLibraryFixture` 의 우선순위 1번이 `XPE_NATIVE_DIR` 다. 그 변수를 `ci-common/bin`
으로 두면 픽스처가 이웃 사본을 먼저 적재하므로 승자가 반대로 뒤집힌다 — **추론이 아니라
실측이다(§4c).** IntegrationTests 에 `XPE_NATIVE_DIR` 을 설정하면 곤란했던 기록
(`GUI-C-128`)과 같은 계열이고, 그때도 `NativeSearchPolicyTests` 4건이 빨개졌다.

## 4a. `NativeSearchPolicyTests` 4건이 실제로 단언하는 것 — **A 와 부딪히지 않는다**

읽었다. 4건은 **파일 존재가 아니라 locator 의 후보 목록**을 단언한다.

```
NativeSearchPolicyTests.cs:43  ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered(locator)
  Assert.All(candidates, c => Assert.False(LooksLikeFallback(c)))
  LooksLikeFallback(c) := c 에 "/build/" 가 들어 있다            (:71-72)
  Assert.NotEmpty(candidates)                                    ← 좁은 검색도 뭔가는 낸다
:61  WithDeveloperSearch_BuildDirectoriesReturn(locator)
  XPE_NATIVE_DEV_SEARCH=1 이면 fallback 이 다시 후보에 든다
```

locator 4개(`NativeModuleLibraryLocator`, `XpeEnhanceBasic…`, `XpePreprocess…`,
`XpeCommon…`)의 후보 순서는 이렇다:

```
XpeCommonLibraryLocator.cs:34   AppContext.BaseDirectory              ← 앱(= 시험 출력) 디렉터리
                        :39     XPE_NATIVE_DIR
                        :48-74  (#129: opt-in) build/… 여러 곳
```

**즉 정책이 금지하는 것은 `/build/` 경로이고, 앱 디렉터리는 허용된 1순위다.**

그러므로 **A 는 `#129` 와 충돌하지 않는다 — 오히려 정책을 확장한다.** 지금 함정을 만드는
것은 `XpePreprocessNative.TryFindDll()` 이 `build/ci-common/bin/xpe_preprocess.dll` 을
주는 것이고, 그것은 **locator 가 아니라 시험 헬퍼**라 4건의 단언 범위 밖이다. `/build/` 로
손을 뻗는 행위자가 아직 하나 남아 있다는 뜻이다.

## 4b. (D) 복사를 없애기 — **탈락. 복사에는 이유가 있다**

`csproj` 가 이유를 적어 두었다.

```
DLL staging: … spdlog.dll and fmt.dll are required because the native build links them as
shared libraries (BUILD_SHARED_LIBS=ON); copying xpe_common.dll alone produced a
loadable-looking file that failed at P/Invoke with 0x8007007E (#98).
```

그리고 locator 의 1순위가 앱 디렉터리이므로, **복사가 곧 "환경 변수 없이도 기본 검색이
찾을 수 있게" 만드는 장치다.** 복사를 없애면 기본 구성에서 후보가 하나도 실재하지 않아
`DllImport` 기반 시험이 전부 `XPE_NATIVE_DIR` 이나 dev search 를 요구한다.

**D 는 탈락이다** — lead 가 붙인 조건("복사가 왜 들어왔는지 먼저 확인, 이유가 있으면 탈락")에
그대로 걸린다.

## 4c. `XPE_NATIVE_DIR` 뒤집기 — **측정했다. 뒤집힌다**

추론이었던 것을 쟀다. `GUI-C-135` 의 한 줄을 **되돌린 상태**로 전체 실행을 두 팔 돌렸다.

| 팔 | 클램프 시험 | 정책 4건 |
|---|---|---|
| `XPE_NATIVE_DIR` 없음 | `ALL` **통과**, 부분 실패 | 통과 |
| `XPE_NATIVE_DIR=build/ci-common/bin` | **둘 다 실패** | **4건 실패** |

**환경 변수가 승자를 바꾼다.** 없을 때는 픽스처가 우선순위 2번(앱 디렉터리) 사본을 먼저
적재해 읽는 쪽과 맞고, 있을 때는 우선순위 1번으로 `ci-common/bin` 사본을 먼저 적재해
`xpe_preprocess` 가 그것에 묶이므로 읽는 쪽(어셈블리 옆)과 갈린다.

### 그리고 이것이 `GUI-C-135` 수정의 한계를 드러냈다

수정을 **복원한 뒤** 같은 환경 변수로 다시 쟀다.

```
수정 복원 + XPE_NATIVE_DIR 있음 : 실패 6 — 정책 4건 + 클램프 2건
수정 복원 + 환경 변수 없음      : 통과 265, 건너뜀 1, 실패 0
```

**내 수정은 환경 변수가 걸린 경우를 고치지 못한다.** 이유는 기계적이다 — 그 경우 픽스처가
`ci-common/bin` 사본을 **먼저** 적재하므로, 내 "먼저 묶기" 한 줄이 적재하는 어셈블리 옆
사본은 **두 번째 인스턴스**가 되고 `xpe_preprocess` 는 첫 번째(이름이 이미 있는 것)에
묶인다.

`GUI-C-135` 보고서는 이 한계를 적지 않았다 — **그때 환경 변수 팔을 재지 않았기 때문이다.**
회귀는 아니다(그 구성은 정책 4건이 이미 빨강인, 지원되지 않는 구성이다). 다만 **수정의
범위가 보고서보다 좁다.**

## 5. 근본 고침의 지렛대 — 판정하지 않고 선택지만

| 방향 | 무엇을 하는가 | 확인해야 할 것 |
|---|---|---|
| A. **한 디렉터리로 통일** | `xpe_preprocess` 등 나머지 네이티브 모듈도 시험 출력으로 복사하고 **전부 이름으로** 적재 | **확인 끝: 충돌하지 않는다**(§4a). 4건은 후보 목록의 `/build/` 만 금지하고 앱 디렉터리는 1순위다 |
| B. **이웃으로 통일** | 공용 모듈을 항상 *"그것을 쓸 모듈의 이웃"* 으로 경로 적재 | `DllImport` 기반 시험은 어셈블리 옆 사본이 필요하다 — 그쪽은 여전히 두 인스턴스 |
| C. **먼저 묶기** (`GUI-C-135` 가 한 것) | 쓰는 모듈보다 **읽는 모듈을 먼저** 적재 | 파일마다 넣어야 한다. **근본 고침이 아니라 우회**다 |

**A 가 유일하게 "인스턴스가 하나" 를 만들고, `#129` 와 충돌하지 않는다**(§4a — 4건은
후보 목록의 `/build/` 만 금지하며 앱 디렉터리는 허용된 1순위다). **D 는 탈락**이다(§4b —
복사에 `#98` 이라는 이유가 있고, 복사가 곧 기본 검색을 성립시키는 장치다). **C 는 우회이고,
환경 변수가 걸리면 듣지 않는다**(§4c 측정).

남는 판단은 A 의 범위다 — `TryFindDll()` 이 `/build/` 를 가리키는 것을 앱 디렉터리로 옮기면
`#129` 정책이 시험 헬퍼까지 확장되는데, 그것이 의도인지는 lead 몫이다.

## 6. 측정한 것

```
NativeLibrary.Load 전수 (clients/ + gui/, bin·obj 제외) : 6곳 / 4파일
알림 큐를 읽는 곳                                        : 1곳 (GainPolyClampAlertTests:451)
시험 출력 디렉터리의 xpe_*.dll                           : xpe_common.dll 하나
xpe_common.dll 사본 (작업 트리 전체)                     : 19개 — 런타임에 겹치는 것은 동일 내용 2벌
```

## 7. 미검증 (Gaps)

- ~~`gui/` 의 resolver 가 어느 디렉터리를 주는지 확인하지 않았다~~ → **읽었다. GUI 는 구조적으로
  안 걸린다** — §9.
- **E2E 쪽을 세지 않았다.** 이 조사는 `clients/`·`gui/` 소스의 적재 지점이고, E2E 는 앱을
  띄우므로 프로세스가 다르다 — 그래도 같은 함정이 앱 안에서 가능한지는 위 항목과 같은 질문이다.
- **선택지 A 를 실제로 구현해 보지 않았다.** `NativeSearchPolicyTests` 와 충돌하지 않는다는
  것은 **4건의 단언을 읽어** 확인했지만(§4a), A 를 적용한 트리에서 전체 실행을 돌린 것은
  아니다 — 착수 승인 뒤의 일이다.
- ~~`XPE_NATIVE_DIR` 로 뒤집히는 것을 실측하지 않았다~~ → **쟀다. 뒤집힌다** (§4c).
- **`XPE_NATIVE_DIR` 이 걸린 구성 자체는 지원되지 않는다고 간주했다** — 정책 4건이 빨강이
  되므로. 그 구성을 지원해야 하는지는 판단하지 않았고, `GUI-C-135` 수정이 그 구성에서
  듣지 않는 것도 그 전제 아래 남겨 두었다(§4c).
- `xpe_display`·`gsvg` 등 다른 공용 모듈이 전역 상태를 갖는지 보지 않았다. 알림 큐는
  `xpe_common` 에 있고, 그것만 확인했다.

## 8. 잔여 위험

- `GUI-C-135` 의 고침은 **한 파일에만** 있다. 나중에 알림 큐를 읽는 시험이 새로 생기면
  같은 함정에 걸리고, **전체 실행만 돌리면 초록이라 드러나지 않는다.**
- 이 조사는 **지금 트리**의 상태다. 복사 대상이나 `TryFindDll` 우선순위가 바뀌면 결론이
  바뀐다.

## 9. GUI 는 왜 안 걸리는가 — **선택지 A 의 실물 증거**

GUI 는 `NativeLibrary.Load` 를 **0곳**에서 쓴다. 전부 `DllImport` + **resolver 하나**다.

```
gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs:50
    NativeLibrary.SetDllImportResolver(typeof(GuiNativeLibraryResolver).Assembly, Resolve);
:119  CandidatesFor(libraryName) — 모듈 이름별 후보 목록
주석  "(주입 디렉터리 → 앱 디렉터리 → 선택적 개발자 대체) 순서는 정의가 하나다"
```

**resolver 하나가 모든 모듈에 같은 순서를 적용하므로, `xpe_preprocess` 와 `xpe_common` 이
같은 디렉터리에서 온다.** 그래서 미는 쪽과 읽는 쪽이 같은 인스턴스다 — GUI 에서 알림이
화면까지 도달하는 것을 여러 번 관측했고(`GUI-C-129`·`GUI-C-132`), **이제 왜 맞는지도 읽었다.**

**이것이 §5 선택지 A 가 통한다는 실물 증거다** — "한 디렉터리로 통일" 을 GUI 는 이미 하고
있고, 걸리지 않는다. 통합 시험만 `xpe_common` 은 이름으로·`xpe_preprocess` 는 경로로
해결해 두 축이 갈렸다.

주석이 적어 둔 것 하나 더: GUI 의 resolver 는 `XPE_NATIVE_DIR` 을 **의도적으로 무시**한다
(`GUI-C-31` 측정 — 빈 `XPE_NATIVE_DIR` 로도 앱 폴더에서 적재해 통과했다). *"두 앱이 같은
환경 변수에 다르게 답하는 것"* 을 막으려던 것인데, 통합 시험 쪽은 그 변수를 우선순위 1번으로
읽으므로(§4) **같은 변수가 두 곳에서 다르게 작동하는 상태가 여전히 남아 있다.**

🗿 MoAI

---

# 10. A 를 적용했다 — 최소 범위, 측정으로 확인

## 10.1 무엇을 바꿨나 — 두 곳

| 파일 | 변경 |
|---|---|
| `ImageProcTest.IntegrationTests.csproj` | 스테이징에 **`xpe_preprocess.dll` 추가**(ci-common·default 의 Debug/비Debug 4후보) |
| `PInvoke/XpePreprocessNative.cs` `TryFindDll()` | **앱 디렉터리를 1순위로** 추가. `XPE_NATIVE_DIR`·`build/…` 후보는 fallback 으로 유지 |

이제 시험 출력에 `xpe_common.dll` 과 `xpe_preprocess.dll` 이 함께 있고, 두 모듈이 **같은
디렉터리**에서 해결된다 — GUI 가 resolver 하나로 하는 것과 같은 구성(§9)이다.

## 10.2 범위를 `xpe_preprocess` 로 **한정한 이유** — 측정된 제약

optional 군(`gsvg`·`xpe_display`·`xpe_dicom`·`xpe_enhance_advanced`)까지 복사하면
**`NativeSearchPolicyTests.AllOptionalDllsAbsent_GradesEveryOptionalR0_AndNoRequiredOne`
(`:148`)이 깨진다.** 그 시험은 `XPE_NATIVE_DIR` 을 빈 임시 디렉터리에 고정하고
(`XPE_NATIVE_DIR_EXCLUSIVE=1`) optional 모듈이 **R0(못 찾음)** 로 등급되는 것을 단언한다.

```
XpeCommonLibraryLocator.cs:34   yield return AppContext.BaseDirectory        ← 먼저 나온다
                        :41-45  if (StopAtInjectedDirectory) yield break;    ← EXCLUSIVE 는 그 다음
NativeModuleLibraryLocator.cs:15,23-25  같은 순서
```

**앱 디렉터리가 EXCLUSIVE 검사보다 먼저 나오므로**, 앱 디렉터리에 사본이 있으면 EXCLUSIVE
로도 가려지지 않는다. `xpe_preprocess` 는 그 시험의 **required 군**이라 단언이
`NotEqual(NotReady)` 이고, 앱 디렉터리에서 찾혀도 만족한다 — 그래서 안전하다.

**즉 A 는 "모든 모듈" 이 아니라 "required 모듈" 까지만 안전하다.** 이것이 4건을 읽어서는
안 보이고 5번째 시험을 읽어야 나오는 제약이다.

## 10.3 측정 — A 가 우회 없이도, 환경 변수에서도 듣는다

`GUI-C-135` 의 우회를 **되돌린 상태**로 쟀다.

```
A 적용 + 우회 없음 + 환경 변수 없음
  EveryPixel 단독 : 통과 1
  SomePixels 단독 : 통과 1
A 적용 + 우회 없음 + XPE_NATIVE_DIR=build/ci-common/bin (전체)
  실패 4 — 정책 4건뿐. 클램프 2건은 통과
```

**C 가 못 고치던 환경 변수 팔을 A 는 고친다**(§4c 와 대조). 남은 정책 4건은 A 와 무관하다 —
`XPE_NATIVE_DIR` 을 `/build/` 아래로 가리키면 **후보 자체가 `/build/` 경로**가 되므로
`ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered` 가 그것을 잡는 것이 정상이다.

## 10.4 우회를 지웠다

A 가 구조로 해결하므로 `GUI-C-135` 의 "먼저 묶기" 한 줄은 **죽은 코드**가 됐다. 지우고
그 자리에 **왜 아무것도 필요 없는지**를 적었다 — 아무 일도 하지 않는 방어 코드를 남기면
다음 사람이 그것을 필요한 것으로 읽는다.

```
grep -c "REVERTED202B|ORDER MATTERS" → 0
```

## 10.5 최종 상태

```
BUILD_EXIT=0
IntegrationTests 전체 (환경 변수 없음) : 통과 265, 건너뜀 1, 실패 0
EveryPixel 단독                        : 통과 1
SomePixels 단독                        : 통과 1
시험 출력의 xpe_*.dll                  : xpe_common.dll, xpe_preprocess.dll
```

## 10.6 이 변경의 미검증

- **E2E 를 돌리지 않았다.** 바꾼 것은 IntegrationTests 의 스테이징과 그 프로젝트의 헬퍼
  뿐이고 E2E 는 앱을 띄우므로 경로가 다르지만, **재지는 않았다.**
- **CI 에서 확인하지 않았다.** CI 는 `build/ci-common/bin` 을 만들어 두므로 복사가 성립할
  것으로 보지만, 그 디렉터리가 없는 구성에서는 복사가 조용히 건너뛰어지고(`Condition="Exists"`)
  `TryFindDll()` 이 fallback 으로 내려간다 — 그 경로를 **재지 않았다.**
- **optional 모듈까지 넓히면 깨진다는 것은 `AllOptionalDllsAbsent_…` 를 읽어 추론했다.**
  실제로 복사해 깨뜨려 보지는 않았다(깨뜨리는 것이 목적이 아니므로).
- `xpe_display`·`gsvg` 를 쓰는 시험이 같은 함정에 걸릴 수 있는지는 §7 대로 열려 있다 —
  그 모듈들이 전역 상태를 갖는지 보지 않았다.
