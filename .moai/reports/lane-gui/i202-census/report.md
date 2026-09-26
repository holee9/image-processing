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

### 여기에 잠재된 뒤집기가 하나 더 있다

`NativeLibraryFixture` 의 우선순위 1번이 `XPE_NATIVE_DIR` 다. **그 변수를 `ci-common/bin`
으로 두면 픽스처가 이웃 사본을 먼저 적재**하므로, 어느 인스턴스가 이기는지가 **반대로
뒤집힌다.** IntegrationTests 에 `XPE_NATIVE_DIR` 을 설정하면 곤란했던 기록(`GUI-C-128`)과
같은 계열이다 — 그때는 `NativeSearchPolicyTests` 4건이 빨개졌다.

## 5. 근본 고침의 지렛대 — 판정하지 않고 선택지만

| 방향 | 무엇을 하는가 | 확인해야 할 것 |
|---|---|---|
| A. **한 디렉터리로 통일** | `xpe_preprocess` 등 나머지 네이티브 모듈도 시험 출력으로 복사하고 **전부 이름으로** 적재 | `NativeSearchPolicyTests` 4건이 *"기본 검색이 빌드 디렉터리를 제시하지 않는다"* 를 단언한다 — 사본을 늘리면 그 단언이 흔들릴 수 있다. **재야 한다** |
| B. **이웃으로 통일** | 공용 모듈을 항상 *"그것을 쓸 모듈의 이웃"* 으로 경로 적재 | `DllImport` 기반 시험은 어셈블리 옆 사본이 필요하다 — 그쪽은 여전히 두 인스턴스 |
| C. **먼저 묶기** (`GUI-C-135` 가 한 것) | 쓰는 모듈보다 **읽는 모듈을 먼저** 적재 | 파일마다 넣어야 한다. **근본 고침이 아니라 우회**다 |

**A 가 유일하게 "인스턴스가 하나" 를 만든다.** 다만 위 확인 없이 하면
`NativeSearchPolicyTests` 를 깨뜨릴 수 있고, 그 시험은 **의도적으로** 빌드 디렉터리가
검색에 안 잡히는 것을 지키고 있다. 두 요구가 충돌하는지는 **재 봐야 안다.**

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
- **선택지 A 를 실제로 시도하지 않았다.** `NativeSearchPolicyTests` 와 충돌하는지는
  **추론**이고, 재지 않았다.
- **`XPE_NATIVE_DIR` 로 뒤집히는 것을 실측하지 않았다.** 우선순위 코드를 읽고 추론했다.
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
