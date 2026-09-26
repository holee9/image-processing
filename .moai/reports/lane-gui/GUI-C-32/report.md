# GUI-C-32 — gui 가 `NativeSearchPolicy` 로케이터를 채택 (#129 #136)

- 카드: GUI-C-32 · Refs #129 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋: `3d97a97` (미푸시) · 선행 `git merge origin/main` → `e5d2e7a`
- **결과: Mock 5/5 · Native 5/5 · slnx 0/0 · 통합 0/170/1/171 (168 → +3)**

---

## 1. 구현

| 파일 | 변경 |
|---|---|
| `Services/Native/GuiNativeLibraryResolver.cs` (신규) | `SetDllImportResolver` 로 이 어셈블리의 `DllImport` 를 **공유 후보 목록**에 연결 |
| `ImageProcTest.csproj` | clients 의 `NativeSearchPolicy` / `XpeCommonLibraryLocator` / `NativeModuleLibraryLocator` / `NativeDependencyLoader` 를 Link |
| `App.xaml.cs` | `OnStartup` 에서 리졸버 설치 |
| `Services/XpeBackendFactory.cs` | 경로를 **로케이터로 해석**(§2) |
| `clients/…/XpeCommonLibraryLocator.cs` | XML 주석 `cref` → 코드 표기 (§4) |

**후보 순서를 복제하지 않았다.** 탐색 순서(주입 디렉터리 → 앱 폴더 → 옵트인 개발 폴백)는 clients 에 한 정의로 남고 gui 는 그것을 Link 로 컴파일한다. 순서를 gui 에 다시 쓰면 두 앱이 또 갈라진다.

리졸버 설치를 정적 생성자가 아니라 `OnStartup` 에 둔 이유: 첫 `DllImport` 보다 먼저여야 하고 **어셈블리당 한 번만** 등록할 수 있다(재등록은 `InvalidOperationException` — C-13 실측). 순서를 눈에 보이는 곳에 두는 편이 안전하다.

## 2. 리졸버만으로는 아무것도 바뀌지 않았다 — 팩토리가 앞을 막고 있었다

리졸버를 설치하고 첫 채택 검증을 돌렸더니 **여전히 실패**했다:

```
A) 앱 폴더 비움 + XPE_NATIVE_DIR=스테이징
   Native run: expected a semver …, got 'mode=Native | common=v0.0.0-mock …'
```

원인: `XpeBackendFactory.Create` 가 `Path.Combine(AppContext.BaseDirectory, "xpe_common.dll")` 를 `File.Exists` 로 검사해 **P/Invoke 가 일어나기도 전에** "네이티브 없음" 으로 판단하고 Mock 을 돌려주고 있었다. 리졸버는 호출될 기회조차 없었다 — 죽은 코드였다.

팩토리도 로케이터로 해석하게 고친 뒤 통과했다. **"리졸버를 설치했다" 를 "정책이 적용된다" 로 읽지 않는다** — 이 레인의 gate #2(정의 존재를 동작으로 오귀속)와 같은 형태다.

## 3. 카드 전제 정정 — 앱 폴더에 DLL 이 있으면 어떤 `XPE_NATIVE_DIR` 도 결과를 바꾸지 못한다

카드는 "빈 `XPE_NATIVE_DIR` + EXCLUSIVE → S-05 실패" 를 채택 증거로 제시했다. 그대로 돌리면 **통과한다.** 정책이 그렇게 설계됐기 때문이다:

```
XpeCommonLibraryLocator.GetDllCandidates()
  1) AppContext.BaseDirectory      ← 앱 폴더가 먼저
  2) XPE_NATIVE_DIR
  3) EXCLUSIVE 면 여기서 중단
```

`EXCLUSIVE` 는 "주입 디렉터리 **이후로** 더 가지 말라" 이지 "주입 디렉터리 **만** 보라" 가 아니다(C-16 설계). 앱 폴더에 DLL 이 있는 한 그것이 이긴다.

그래서 채택을 **두 조합**으로 쟀다(둘 다 앱 폴더의 DLL 을 치운 상태):

| 실험 | 조합 | 결과 | 의미 |
|---|---|---|---|
| A | `XPE_NATIVE_DIR` = 스테이징 | **Native 5/5 통과** | 앱 폴더 없이 **주입 디렉터리만으로** 로드 — C-31 에서는 실패했던 조합 |
| B | `XPE_NATIVE_DIR` = 빈 디렉터리 | **S-05 만 실패** | 갈 곳이 없으면 조용한 폴백을 잡는다 |

A 가 채택의 증거이고, B 가 그 단언이 공허하지 않다는 증거다. **A 없이 B 만 보면 "원래 실패하는 것" 과 구분되지 않는다.**

## 4. 회귀 3건 + 반증

`GuiNativeSearchAdoptionTests` (소스 대조 — gui 타입은 WPF 에 묶여 이 어셈블리에서 로드할 수 없다):

| 케이스 | 확인 |
|---|---|
| `GuiResolver_UsesTheSharedLocators` | `SetDllImportResolver` + 두 로케이터의 `GetDllCandidates` 사용 |
| `AppStartup_InstallsTheResolver` | `Install()` 호출 존재 — 없으면 리졸버는 죽은 코드다 |
| `BackendFactory_ResolvesThroughTheLocators_NotTheAppDirectoryAlone` | 팩토리가 `TryFindDll` 경유, 앱 폴더 경로는 폴백으로만(§2 의 결함을 고정) |

```
통과!  - 실패: 0, 통과: 3, 건너뜀: 0, 전체: 3
```

**반증**: 팩토리를 옛 방식(`Path.Combine(AppContext.BaseDirectory, …)`)으로 되돌리니 `BackendFactory_…` **1건만** 실패했다. 원복 후 재확인.

부수: 링크된 `XpeCommonLibraryLocator` 의 XML 주석이 `<see cref="XpeCommonApi"/>` 를 쓰는데 gui 쪽 컴파일에서는 그 타입이 없어 `CS1574` 경고가 났다. **문서 주석만** 코드 표기로 바꿔 0 경고를 회복했다(`NoWarn` 추가는 하지 않았다 — 다른 문서 오류까지 가린다).

## 5. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/…                                  (Mock)
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …                 (Native)
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패:     0, 통과:   170, 건너뜀:     1, 전체:   171
```

Baseline 귀속: C-31 최종 `0/167/1/168` → 증가 3 = 신규 회귀 3건. 스모크는 5건 불변.

## 6. 미검증 (Gaps)

- **리졸버가 실제로 호출되는 것을 런타임으로 확인하지 않았다.** 채택 증거는 "앱 폴더 없이 주입 디렉터리에서 로드된다"(§3 A)는 **결과**이고, 그 경로가 리졸버를 탔는지 로더 기본 동작이었는지는 구분하지 않았다. `ResolvedPath()` 를 남겨 뒀지만 어디서도 읽지 않는다.
- **`xpe_display.dll` 의 후보는 범용 로케이터를 쓴다.** clients 에는 display 전용 로케이터가 없어 `NativeModuleLibraryLocator` 에 이름을 넘겼다 — 동작은 확인했지만 clients 와 **같은 함수**는 아니다.
- **`XPE_NATIVE_DEV_SEARCH` 경로는 확인하지 않았다.** 개발 폴백(형제 저장소)이 gui 에서도 같은 조건에서 열리는지는 관측하지 않았다.
- 리졸버가 후보를 모두 실패했을 때 `IntPtr.Zero` 로 기본 로더에 넘기는 경로는 코드로만 있고 실행하지 않았다.
- CI 실행 결과는 아직 없다.

## 7. 잔여 위험 (Residual risk)

- **앱 폴더가 여전히 1순위다.** CI 의 Native 잡은 아티팩트를 gui 출력 폴더에 스테이징하므로(leader, `e5d2e7a`) `XPE_NATIVE_DIR` 없이도 통과한다 — 즉 CI 는 이 카드가 만든 경로를 실제로는 쓰지 않는다. 정책상 옳지만, **채택 여부를 CI 가 검증하지는 않는다.**
- 링크 소스가 4개 늘어 gui 가 clients 파일 5개를 컴파일한다. 원본은 하나라 드리프트는 없지만 clients 쪽 리팩터링이 gui 빌드를 깨뜨릴 수 있다 — 이번에도 XML 주석 하나가 그랬다.
- 회귀는 **소스 대조**라 모양만 본다. `Install()` 호출이 도달 불가능한 분기로 옮겨가도 통과한다.

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → e5d2e7a
export PATH="/c/Program Files/dotnet:$PATH"
# 채택/반증 (앱 폴더의 DLL 을 치운 상태)
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…   # A
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<빈 디렉터리>            dotnet test …E2ETests…   # B
dotnet test …IntegrationTests… --filter "FullyQualifiedName~GuiNativeSearchAdoptionTests"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
