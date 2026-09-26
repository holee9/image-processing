# GUI-C-17 — `xpe_common` 탐색 분리 + DllName 중복 드리프트 가드 (#129 마무리)

- 카드: GUI-C-17 · Refs #129 · 커밋 `228364d` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `0ed0926`
- **결과: 앱 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 125 / 건너뜀 1 / 전체 126**

---

## 1. 닫은 구멍

C-16 이 탐색 정책을 네 곳에 적용했지만 **회귀는 셋에만** 걸렸다. `xpe_common` 후보 목록이
`PInvokeWrapper.cs` 안에 있었고, 그 파일은 테스트가 컴파일할 수 없다 — `XpeCommonApi` 의 정적
생성자가 DllImport 리졸버를 등록하는데 한 어셈블리에 리졸버는 하나뿐이다(GUI-C-13 실측).

그래서 **다른 모든 모듈이 의존하는 바로 그 탐색**만 "기본 상태에서 형제 저장소 후보 없음" 이
미검증이었다. 코드가 옳아 보인다는 것과 그것을 관측했다는 것은 다르다(`gate.md` #9).

## 2. 분리 (카드 1항)

`Diagnostics/XpeCommonLibraryLocator.cs` 신설. `PInvokeWrapper.cs` 에서 옮긴 것:

- `GetDllCandidates()` — 후보 15경로 × (저장소 + `xpe-pre` 형제)
- `FindRepositoryRoot` / `GetRepositoryAndSiblingRoots` 사본

옮긴 뒤 그 파일에는 **P/Invoke 도, 리졸버도, 정적 생성자도 없다** — 순수 경로 계산이다.
`XpeCommonApi.ResolveNativeLibrary` 는 이제 `XpeCommonLibraryLocator.GetDllCandidates()` 를
**호출만** 한다(`PInvokeWrapper.cs:177`). 그 파일에서 경로 관련 코드 4042자가 사라졌다.

`XpeCommonApi.DllName` const 는 `[DllImport]` 특성이 쓰므로 그대로 뒀다. 그래서 이름이 두
곳에 적히게 됐고, 그것이 카드 3항이다.

## 3. 회귀 — 이제 네 로케이터 전부 (카드 2항)

| 로케이터 | 기본 상태 후보 없음 | `DEV_SEARCH=1` 이면 있음 |
|---|---|---|
| `NativeModuleLibraryLocator` | ✅ | ✅ |
| `XpeEnhanceBasicLibraryLocator` | ✅ | ✅ |
| `XpePreprocessLibraryLocator` | ✅ | ✅ |
| **`XpeCommonLibraryLocator`** | ✅ **신규** | ✅ **신규** |

기본 상태 케이스는 `Assert.NotEmpty(candidates)` 를 함께 단언한다 — 후보가 0개여도 "폴백
없음" 은 참이 되므로 그 상태로 통과하는 것을 막는다.

## 4. 이름 중복 가드 (카드 3항)

`Functional/DllNameParityTests.cs` 신설. C-12/13 패턴 그대로 **쌍을 데이터로** 두고 소스
텍스트에서 `const string DllName = "…"` 를 읽어 대조한다.

| 로케이터 | 래퍼 | 이름 |
|---|---|---|
| `Diagnostics/XpeCommonLibraryLocator.cs` | `PInvokeWrapper.cs` | `xpe_common.dll` |
| `Diagnostics/XpeEnhanceBasicLibraryLocator.cs` | `PInvokeWrappers/XpeEnhanceBasicWrapper.cs` | `xpe_enhance_basic.dll` |

- 파일당 `DllName` 선언이 **정확히 1개**인지도 단언한다. 선언이 옮겨지거나 늘어나면
  가드가 조용히 빗나가는 대신 "가드를 갱신하라" 는 메시지로 실패한다
- 파일을 못 찾으면 **스킵이 아니라 실패**시킨다 — 조용한 스킵은 중복을 무방비로 되돌린다
- 새 분리가 생기면 **행 하나** 추가

이 중복은 사고가 아니라 **분리의 대가**다. 래퍼 상수를 참조하면 `XpeCommonApi` 가 딸려와
분리 자체가 무효가 된다. "동기화 유지" 주석은 장치가 아니라는 것이 GUI-C-13 의 교훈이고,
이 테스트가 그 장치다.

## 5. 반증 — 두 가드를 한 번에 (카드 2·3항)

`XpeCommonLibraryLocator` 에서 게이트를 빼고 `DllName` 을 `xpe_commonX.dll` 로 어긋냈다.

```
Assert.Equal() Failure: Strings differ
XpeCommonLibraryLocator: default search offered a build/sibling candidate:
  D:\workspace-github\xpe-gui\build\local-vs2022-common\bin\xpe_commonX.dll
```

경로 회귀는 **그 로케이터를 지목**하고, 이름 대조는 별도로 실패한다 — 두 가드가 서로 다른
것을 본다는 확인이기도 하다. 로그: `step3-falsify.log`. 원복 후 재확인(`step4-final.log`).

### 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   125, 건너뜀:     1, 전체:   126
```

126 = C-16 의 122 + xpe_common 회귀 2 + 이름 대조 2. 건너뜀 1은 C-12 부터의 `-15` 문자열 건.

## 6. 미검증 (Gaps)

- **`XpeCommonApi` 의 리졸버가 실제로 새 후보 목록으로 DLL 을 로드하는지는 이 테스트가 보지
  않는다.** 후보 목록 자체는 회귀가 덮고, 리졸버가 그것을 호출한다는 것은 컴파일과 앱 빌드로만
  확인된다 — 그 파일은 여전히 링크 불가다. 다만 기존 스위트가 `xpe_common.dll` 을 실제로
  P/Invoke 하며 통과하므로(125건 중 상당수), **로드 경로가 살아 있다는 간접 증거는 있다**
- `Trace.WriteLine` 출력 경로는 C-16 과 동일하게 미검증
- 앱을 실행해 화면을 보지 않았다 — 빌드만 확인
- CI 실행 결과는 아직 없다
- 네 로케이터의 `FindRepositoryRoot` / `GetRepositoryAndSiblingRoots` 중복은 그대로다.
  이번에 xpe_common 사본이 **파일만 옮겼을 뿐 사본 수는 줄지 않았다** — 정책만 공유한다는
  C-16 조건 (1) 을 유지했다

## 7. 잔여 위험

- `DllName` 중복이 이제 **2쌍**이다. 가드가 있으므로 어긋나면 즉시 드러나지만, 세 번째 분리가
  생기면 목록에 행을 추가해야 한다 — 손 목록이라는 점은 `gate.md` #11 의 축소판이다.
  다만 대조 대상이 소스 전체가 아니라 쌍이라 새 쌍 추가가 리뷰에서 눈에 띈다
- `PInvokeWrapper.cs` 는 여전히 링크 불가다. 이 파일에 경로 판단이 다시 들어가면 같은 구멍이
  재발한다 — 지금은 리졸버만 남았으므로 그럴 여지가 작다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~NativeSearchPolicyTests|FullyQualifiedName~DllNameParityTests"
```
