# GUI-C-14 — 저하 모드 BP-06~10 을 실제 준비도 검증으로

- 카드: GUI-C-14 · Refs #128 · 커밋 `8c6b2cb` (미푸시)
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `73a2f0d`
- **결과: 실패 0 / 통과 112 / 건너뜀 1 / 전체 113** (기존 107 + 신규 6)
- **BP-08(enhance_basic)은 포함하지 못했다 — §5. 나머지 4건 + 대조군 + dicom 기본상태 커버**

---

## 1. 준비도 판정 경로 (카드 1항)

`ModuleReadinessService.Evaluate()`(`Services/ModuleReadinessService.cs:10`)가 모듈별 스냅샷을
만든다. R0 을 내는 조건은 모듈마다 다르지만, **DLL 을 찾지 못하는 경로는 모두 하나의
로케이터로 모인다.**

| BP | 모듈 | 스냅샷 생성 | 탐색 | R0 조건 |
|---|---|---|---|---|
| 06 | gsvg | `ModuleReadinessService.cs:265` `EvaluateGsvg` | `XpeGsvgReadinessProbe.cs:15` → `NativeModuleLibraryLocator.GetDllCandidates` | `health.IsVersionReady == false` (`:280`) |
| 07 | enhance_advanced | `ModuleReadinessService.cs:328` `EvaluateDllPresence` | `NativeModuleLibraryLocator.TryFindDll` (`:330`) | `found == null` (`:346`) |
| 08 | enhance_basic | `ModuleReadinessService.cs:168` `EvaluateEnhanceBasic` | **`XpeEnhanceBasicLibraryLocator.GetDllCandidates`** (별도) | `health.IsSmokeReady == false` |
| 09 | dicom | `ModuleReadinessService.cs:213` `EvaluateDicom` | `XpeDicomReadinessProbe.cs:27` → `NativeModuleLibraryLocator` | `health.IsSmokeReady == false` (`:91`) |
| 10 | display | `ModuleReadinessService.cs:53` `EvaluateDisplay` | `XpeDisplayVersionProbe.cs:23` → `NativeModuleLibraryLocator` | `display.IsSmokeReady == false` (`:65`) |

**핵심 관찰**: BP-08 만 별도 로케이터를 쓴다. 나머지 4개는
`NativeModuleLibraryLocator.GetDllCandidates`(`Diagnostics/NativeModuleLibraryLocator.cs:13`)가
유일한 탐색 입구이고, **DLL 부재는 거기서 결정된다.** 그 뒤 프로브가 버전·스모크를 더해
R1/R2/R3 을 올릴 뿐, 찾지 못하면 어느 경로든 R0 으로 떨어진다.

## 2. 주입 지점 (카드 3항)

`XPE_NATIVE_DIR` 주입은 **이미 있었다**(`NativeModuleLibraryLocator.cs:17`). 그러나 그것만으로는
부재를 만들 수 없다 — 뒤에 저장소 빌드 디렉터리 폴백 16경로 + 형제 저장소 탐색이 있어,
임시 디렉터리에서 뺀 DLL 을 `build/ci-common/bin` 등에서 다시 찾아낸다.

그래서 **앱 변경 1곳**을 넣었다. `GetDllCandidates` 에서 `XPE_NATIVE_DIR_EXCLUSIVE=1` 이면
주입 디렉터리에서 탐색을 멈춘다(`yield break`). 미설정이 기본이므로 **앱의 탐색 순서는 불변**
이다. 이 변경이 필요하다는 것은 논증이 아니라 §4 의 반증 실험으로 확인했다.

## 3. 케이스 6건

`Functional/DegradedModeReadinessTests.cs`.

| 케이스 | 관측 |
|---|---|
| BP-06 gsvg / 07 enhance_advanced / 09 dicom / 10 display (Theory 4건) | 뺀 모듈 미해결 + **나머지 모듈은 해결** + 예외 없음 |
| BP-09 보강 | 빈 디렉터리에서도 `xpe_dicom.dll` 미해결 — CI 기본 상태(dicom 은 coverage-dicom 에서만 빌드) |
| 대조군 | 아무것도 안 뺀 스테이징에서 4개 전부 해결 |

설계상 지킨 것:

- **원본 스테이징을 건드리지 않는다.** 실제 DLL 을 복사·삭제하는 대신 임시 디렉터리에
  자리표시자 2바이트(`MZ`)를 쓴다. 로케이터는 `File.Exists` 로 판정하므로 충분하고,
  `build/ci-common/bin` 은 읽지도 않는다
- **백그라운드 프로세스 없음.** 환경변수는 `IDisposable` 스코프로 설정·복원하고,
  임시 디렉터리는 `Dispose` 에서 지운다
- **대조군을 넣었다.** `TryFindDll` 이 항상 `null` 을 내는 버그가 생기면 저하 케이스 4건이
  전부 "통과" 한다. 대조군이 그것을 막는다
- **사전조건을 테스트가 스스로 단언한다.** 로케이터는 `AppContext.BaseDirectory` 를
  주입 디렉터리보다 **먼저** 본다. 테스트 출력 디렉터리에 모듈 DLL 이 있으면 실험이
  성립하지 않으므로, 스코프 진입 시 그 부재를 단언하고 아니면 사유와 함께 실패시킨다

## 4. 반증 실험 2회 (카드 5항)

### 4-1. 아무것도 제거하지 않으면 — 네 케이스 모두 실패

```
BP-09: xpe_dicom.dll was removed from …\xpe_degraded_7502a49… but still resolved to
       …\xpe_degraded_7502a49…\xpe_dicom.dll
(BP-06/07/10 동일)
```

단언이 **실제로 부재에 걸려 있다**는 뜻이다. 로그: `step2-falsify-noremoval.log`

### 4-2. 주입 씸을 끄면 — 검색이 저장소 밖까지 뻗는다

```
BP-10: … still resolved to D:\workspace-github\xpe-gui\build\ci-common\bin\xpe_display.dll
BP-09: … still resolved to D:\workspace-github\image-processing\build\build_test\bin\Debug\xpe_dicom.dll
```

폴백이 **형제 저장소까지** 도달한다(`GetRepositoryAndSiblingRoots`). 앱 변경이 없으면 이 카드의
요구를 관측할 방법이 없다는 것이 실측으로 확인됐다. 로그: `step3-falsify-noseam.log`

두 실험 모두 원복 후 재확인(`step4-final.log`).

## 5. 남은 범위 — BP-08(enhance_basic)을 포함하지 못한 이유

`EvaluateEnhanceBasic` 은 `XpeEnhanceBasicLibraryLocator` 를 쓰고, 그 파일은
`XpeEnhanceBasicWrapper` 를 참조하며 그 래퍼의 델리게이트 서명이 `XpeCommonApi` 타입을 쓴다.
따라서 링크하면 `PInvokeWrapper.cs` 가 함께 들어오고, `XpeCommonApi` 정적 생성자가
`SetDllImportResolver` 를 호출한다. `NativeLibraryFixture` 도 **같은 어셈블리에** 등록하므로
`InvalidOperationException: A resolver is already set for the assembly` 가 난다 —
**GUI-C-13 에서 프로브로 실측한 그 장벽이다.**

같은 이유로 이 카드는 **로케이터 층에서 멈춘다.** 스냅샷의 `"R0"` 문자열 자체를 단언하려면
프로브를 링크해야 하고, gsvg 를 뺀 세 프로브가 `XpeCommonApi` 에 닿는다
(`XpeGsvgReadinessProbe` 0건 / `XpeDicomReadinessProbe` 2건 / `XpeDisplayVersionProbe` 5건 /
`XpeEnhanceBasicReadinessProbe` 6건).

**즉 이 카드가 검증한 것은 "R0 판정의 입력"이지 "R0 문자열"이 아니다.** 둘 사이는 각 모듈
3~5줄의 분기(§1 표의 R0 조건 열)이며, 그 분기까지 덮으려면 다음 중 하나가 필요하다 —
**leader 판정 사안**:

| 안 | 내용 | 비용 |
|---|---|---|
| A | `EvaluateDllPresence` 등 판정 분기를 WPF·XpeCommonApi 무의존 파일로 추출하고 서비스가 위임 | 앱 파일 2개 변경(추출 + 호출부). 이 카드의 "한 지점" 을 넘음 |
| B | `NativeLibraryFixture` 가 기존 리졸버를 허용하도록 완화 | 테스트 측 변경이지만 두 리졸버 중 하나만 유효해져 결합이 미묘해짐. 권하지 않는다 |
| C | 현행 유지 | R0 문자열은 미검증으로 남고, 입력(탐색)만 회귀 보호 |

## 6. 미검증 (Gaps)

- **스냅샷 `"R0"` 문자열과 "다른 모듈 정상 등급" 을 스냅샷 수준에서 단언하지 않았다** (§5).
  로케이터 수준에서 "뺀 것만 미해결, 나머지는 해결" 은 관측했다
- **BP-08 미포함** (§5)
- **자리표시자는 실제 DLL 이 아니다.** 로케이터가 `File.Exists` 로만 판정하므로 이 층에서는
  동등하지만, 프로브가 실제로 로드·버전확인하는 단계는 이 테스트가 보지 않는다
- 앱(WPF)을 실행해 저하 모드 화면을 확인하지 않았다 — 이 레인 범위 밖
- CI 실행 결과는 아직 없다. 로컬은 CI 아티팩트 바이너리 스테이징 상태에서 돌렸다
- 네이티브 자리표시자(BP-06~10) 삭제는 카드대로 손대지 않았다 — Lane A(A-24)

## 7. 잔여 위험 / 보고 사항

- **`XPE_NATIVE_DIR_EXCLUSIVE` 는 프로덕션 코드에 들어간 테스트용 씸이다.** 미설정이 기본이라
  앱 동작은 불변이지만, 환경변수 하나로 탐색 범위가 좁아진다는 사실 자체가 표면이다.
  운영 환경에서 실수로 설정되면 모듈이 전부 R0 으로 보인다
- **로케이터가 형제 저장소를 탐색한다**(`image-processing`, `xpe-post`, `xpe-pre`).
  §4-2 에서 실제로 `D:/workspace-github/image-processing/build/build_test/bin/Debug` 의 DLL 을
  집어왔다. 개발 편의 기능이지만, **어떤 저장소의 어떤 빌드가 로드됐는지 불확실해지는 경로**다.
  이 카드 범위 밖이라 보고만 한다
- 환경변수는 프로세스 전역이다. `xunit.runner.json` 이 `parallelizeTestCollections: false` 라
  현재는 안전하지만, 병렬 실행을 켜면 이 클래스가 다른 테스트의 탐색 경로를 흔든다

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # already up to date (73a2f0d)
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --filter "FullyQualifiedName~DegradedModeReadinessTests"   # 반증 실험 2회
grep -rn "XPE_NATIVE_DIR" --include=*.cs clients gui
grep -n "private static ModuleReadinessSnapshot Evaluate" clients/ImageProcTest/Services/ModuleReadinessService.cs
```
