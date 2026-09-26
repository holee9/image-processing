# GUI-C-16 — 로케이터 폴백 opt-in (#129) + 로드 경로 기록 + BP-08 대역 해소

- 카드: GUI-C-16 · Refs #129 #128 · 커밋 `fcb6b86`(4b) + `0ed0926`(본체) · 미푸시
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui`
- **결과: 앱 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 121 / 건너뜀 1 / 전체 122**
- A안 판정대로 탐색 구현 **4곳 전부** + 공유 정책 파일

---

## 1. 현재 탐색 순서 (카드 1항) — 소유자 4개

| # | 파일 | 대상 DLL | 형제 저장소 |
|---|---|---|---|
| 1 | `Diagnostics/NativeModuleLibraryLocator.cs:13` | gsvg · dicom · display · enhance_advanced · ai | 호출자 지정 |
| 2 | `Diagnostics/XpeEnhanceBasicLibraryLocator.cs:18` | `xpe_enhance_basic.dll` | `image-processing`, `xpe-post` |
| 3 | `Diagnostics/XpePreprocessLibraryLocator.cs:17` | `xpe_preprocess.dll` | `image-processing`, `xpe-pre` |
| 4 | `PInvokeWrapper.cs:200` | **`xpe_common.dll`** (DllImport 리졸버) | `xpe-pre` |

**변경 전**: 실행 디렉터리 → `XPE_NATIVE_DIR` → 빌드 디렉터리(6~16) → 형제 저장소
**변경 후**: 실행 디렉터리 → `XPE_NATIVE_DIR` → *(여기서 끝)*, 폴백은 `XPE_NATIVE_DEV_SEARCH=1` 일 때만

## 2. 공유 정책 (카드 2항)

`Diagnostics/NativeSearchPolicy.cs` 신설 — 환경변수 3개의 **판정만** 담는다.

| 변수 | 뜻 |
|---|---|
| `XPE_NATIVE_DIR` | 실행 디렉터리 다음에 볼 디렉터리 |
| `XPE_NATIVE_DIR_EXCLUSIVE=1` | 그 디렉터리에서 탐색 중단 (테스트 격리, C-14) |
| `XPE_NATIVE_DEV_SEARCH=1` | 빌드 디렉터리·형제 저장소 폴백 재개방 |

**후보 목록은 네 곳이 각자 유지한다**(경로가 모듈마다 다르다) — leader 조건 (1) 그대로.
네 곳 모두 동일한 두 줄을 부른다:

```csharp
if (NativeSearchPolicy.StopAtInjectedDirectory) yield break;   // 주입 디렉터리 뒤
if (!NativeSearchPolicy.DeveloperSearchEnabled) yield break;   // 폴백 앞
```

## 3. 로드 경로 기록 (카드 3항)

- `ModuleReadinessSnapshot` 에 `ResolvedDllPath` 추가(기본 `""`)
- `ModuleReadinessService.Evaluate()` 가 프로브의 경로를 담고, 끝에
  `native modules: xpe_display=C:\…\xpe_display.dll; gsvg=<not resolved>; …` **한 줄**을
  `Trace.WriteLine` 으로 남긴다
- **절대 경로일 때만 기록한다.** 프로브는 미해결 시 `DllPath` 에 *파일명*을 넣는 관례가 있어,
  그것을 그대로 담으면 "찾았다" 로 오독된다(`ResolvedPathOrEmpty`)

포맷 함수(`ModuleReadinessReporting`)는 **스냅샷 파일 옆**에 뒀다. 서비스에 두면 테스트가
링크할 수 없다(프로브 → `XpeCommonApi` → 리졸버 이중 등록, GUI-C-13 실측) — 처음에 서비스에
넣었다가 그 이유로 옮겼다.

## 4. BP-08 대역 해소 (카드 4항)

`XpeEnhanceBasicLibraryLocator.DllName` 이 `XpeEnhanceBasicWrapper.DllName` 을 참조하던 한 줄을
리터럴로 바꾸고 `using ImageProcTest.PInvokeWrappers;` 를 제거했다. 이제 그 파일이 무의존이라
테스트가 링크할 수 있고, **BP-08 이 프로덕션 로케이터로 탐색한다**(C-15 의 대역 해소).

`AlgorithmValidationCatalogService.cs:74` 의 `"R0"` → `ModuleReadinessGrading.NotReady` (커밋 `fcb6b86`).

## 5. 회귀 (카드 5항)

| 케이스 | 결과 |
|---|---|
| 로케이터 3곳 × "기본 상태에서 빌드/형제 후보 없음" | 통과 |
| 로케이터 3곳 × "`XPE_NATIVE_DEV_SEARCH=1` 이면 있음" | 통과 |
| 로드 경로 기록 2건(`ResolvedPathOrEmpty`, `DescribeResolvedModules`) | 통과 |
| BP-08 포함 저하 모드 5건 + 대조군 + dicom | 통과 (프로덕션 로케이터 경유) |

기본 상태 케이스는 `Assert.NotEmpty(candidates)` 를 함께 단언한다 — 후보가 0개여도 "폴백 없음"은
참이 되므로, 그 상태로 통과하는 것을 막는다.

### 반증 (카드 5항)

`XpePreprocessLibraryLocator` 의 게이트만 제거하고 실행:

```
XpePreprocessLibraryLocator: default search offered a build/sibling candidate:
  D:\workspace-github\xpe-gui\build\local-vs2022-common\bin\xpe_preprocess.dll
```

**그 로케이터만** 지목하며 실패한다 — 정책이 파일별로 실제 작동한다는 증거다.
로그: `step4-falsify-gate.log`. 원복 후 재확인(`step5-final.log`).

### 실측 (verbatim — leader 요청)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   121, 건너뜀:     1, 전체:   122
```

122 = C-15 의 114 + 정책 6 + 기록 2. 건너뜀 1은 C-12 부터의 `-15` 문자열 건.

## 6. 문서 (카드 6항)

`clients/README.md` 신설. Lane C 소유 경로(`clients/**`)에 문서가 하나도 없어 새로 만들었다 —
루트 `README.md` / `CLAUDE.md` 는 레인 경계 밖이다. 기본 탐색 축소, 로컬 개발 시 필요한
`XPE_NATIVE_DEV_SEARCH=1` / `XPE_NATIVE_DIR`, 그리고 로드 경로가 어디 기록되는지를 담았다.

## 7. 미검증 (Gaps)

- **`xpe_common`(#4) 의 후보 목록에는 회귀 케이스가 없다.** 그 목록은 `PInvokeWrapper.cs` 안에
  있고 그 파일은 테스트가 링크할 수 없다(정적 생성자가 리졸버를 등록 → 이중 등록 예외).
  정책 호출은 다른 셋과 **동일한 두 줄**이고 앱 빌드가 통과하지만, **"기본 상태에서 형제
  저장소 후보 없음" 을 xpe_common 에 대해 직접 관측하지는 못했다.** 해소하려면 그 후보
  목록을 무의존 파일로 분리해야 한다 — 이 카드 범위 밖
- `Trace.WriteLine` 이 실제로 어디에 찍히는지(WPF 실행 시 리스너 구성)는 확인하지 않았다.
  포맷 함수는 테스트가 검증하지만 **출력 경로는 미검증**이다
- 앱을 실행해 준비도 화면/로그를 보지 않았다 — 빌드만 확인
- `XpeEnhanceBasicWrapper.DllName` 과 로케이터의 리터럴이 **같은 값을 두 곳에 적는다.**
  드리프트 가드는 없다(C-12/13 의 드리프트 테스트 패턴을 적용하지 않았다) — 보고만 한다
- CI 실행 결과는 아직 없다

## 8. 잔여 위험

- **개발자 워크플로가 바뀐다.** 저장소를 빌드해 두면 앱이 알아서 찾던 동작이 사라졌다.
  `clients/README.md` 에 적었지만, 기존 사용자는 앱이 갑자기 모든 모듈을 R0 으로 보고하는
  것으로 겪는다. 첫 실행에서 그 원인을 알려면 `native modules: …` 로그를 봐야 한다
- 네 로케이터가 `FindRepositoryRoot` / `GetRepositoryAndSiblingRoots` 를 여전히 각자 복사해
  갖고 있다. 정책만 공유했고 중복 자체는 남았다 — 조건 (1) 대로다
- 환경변수는 프로세스 전역이다. `parallelizeTestCollections: false` 가 전제

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~NativeSearchPolicyTests"   # 반증
grep -rn "GetDllCandidates" --include=*.cs clients/ImageProcTest
```
