# GUI-C-08 — CI dotnet-tests 51건 실패: 의존 DLL 미복사 + 픽스처 가용성 판정 정정

- 카드: GUI-C-08 · Refs #98 (#107 후속) · Class B
- 워크트리: `D:/workspace-github/xpe-gui` / 브랜치 `dev/gui` / 커밋 `03acde0` (미푸시)
- 선행: `git merge origin/main` → `92861fa` (fast-forward, `.claude/` 변경 0건)
- **상태: 완료. 단, CI 는 녹색이 되지 않는다 — §5 참조**

---

## 0단계 — 재현. 인과 사슬은 맞다

카드가 적어 준 사슬을 그대로 확인했다. 다만 **첫 시도는 CI 수치와 달랐고, 그 차이를 맞추지 않고
원인을 규명했다.**

| 스테이징 | 실패 | 통과 | 건너뜀 |
|---|---:|---:|---:|
| 카드 지시대로 3개 (`xpe_common` `spdlog` `fmt`) | 48 | 33 | 9 |
| **CI 와 동일** (두 아티팩트 합집합) | **51** | **39** | **0** |

차이의 원인: **`xpe_preprocess.dll` 이 로컬 `build/ci-common/bin/` 에 없다.** 카드 배경 1번은
그 폴더에 두 DLL 이 다 있다고 적었지만, 실측하면 `xpe_common.*` 만 있고 `xpe_preprocess.dll` 은
`build/ci-preprocess/bin/` 에 있다. 그래서 로컬에서는 P1A 9건이 스킵됐다(건너뜀 9).

`ci.yml:182-191` 을 읽으니 CI 는 **두 아티팩트를 같은 폴더(`build/ci-common/bin/`)로 내려받는다.**
그 합집합(`xpe_common` `xpe_preprocess` `spdlog` `fmt` + gtest/gmock 4개)을 그대로 재현하니
**51 / 39 / 0 — CI 와 정확히 일치**했다. 카드의 3개 복사는 근사치였고, 사슬 자체는 옳다.

로드 실패 메시지도 확인: `Unable to load DLL 'xpe_common.dll' or one of its dependencies`.
수정 전 축1에서 통과하던 3건은 P/Invoke 를 하지 않는 테스트였다
(`DllSearchPathSafetyTests` 2건 — 경로 문자열만 검사, `LeakEnduranceTests.AfterTests_…` 1건).

로그: `step0-repro.log`(3개), `step0-repro-detailed.log`, `step0-repro-ciparity.log`(CI 동일)

## 1단계 — 픽스처: "있다" 가 아니라 "로드된다" 를 판정

`NativeLibraryFixture`:

- `IsAvailable` 을 `NativeLibrary.Load(path)` 성공 여부로 결정. 파일 존재 + x64 PE 검사는
  **필요조건으로 유지**하되 그것만으로 true 를 주지 않는다
- 로드한 핸들을 픽스처가 보관하고 `Resolver` 가 그대로 반환 — 판정과 실제 P/Invoke 로드가
  **한 결정**이 된다. 종전 `Resolver` 는 매번 다시 탐색·로드해서 판정과 어긋날 수 있었다
- 로드 실패 시 `SkipReason` 에 로더 오류를 담는다 (HRESULT 포함)

기존 계약은 보존했다: `ResolvedPath` 는 성공 시 경로, 아키텍처 불일치 시
`Architecture mismatch: … is not x64` — C 유형 3건이 이 문자열에 의존한다.

## 2단계 — 복사 타깃에 의존 DLL 추가

`CopyXpeDllsForTests` 에 `spdlog.dll` / `fmt.dll` 을 **이름으로 명시**해 추가했다
(`ci-common/bin`, `ci-common/bin/Debug`, `default/bin`, `default/bin/Debug` 4경로 × 2파일).
`*.dll` 와일드카드는 gtest/gmock 4개까지 끌고 오므로 쓰지 않았다 — 카드 지시 그대로.

**축2(`xpe_preprocess.dll`)는 csproj 변경이 필요 없다.** `XpePreprocessNative.TryFindDll()` 은
출력 폴더가 아니라 `build/ci-common/bin/` 등을 직접 탐색해 **절대 경로로** 로드하므로, 의존
DLL 이 같은 폴더에 있으면 그대로 로드된다. 3단계 실측이 이를 확인한다.

## 3단계 — 검증 (4개 실행)

| # | 스테이징 | 실패 | 통과 | 건너뜀 | 로그 |
|---|---|---:|---:|---:|---|
| a | CI 동일 (합집합) | **6** | 84 | 0 | `step3a-full-staging.log` |
| b | `spdlog.dll` 만 제거 | 3 | 36 | **51** | `step3b-no-spdlog.log` |
| c | DLL 전부 제거 | **0** | 30 | **60** | `step3c-no-dlls.log` |
| d | 카드의 3개만 | 3 | 78 | 9 | `step3d-three-dlls.log` |

- **a**: 수정 전 51 실패 → 6. 축1·축2 60건이 전부 **실행**됐고 54건이 통과한다.
  카드가 기대한 "60건 통과" 는 54건만 달성 — 나머지 6은 §5
- **b**: 축1 51건이 실패가 아니라 **스킵**으로 집계된다. 스킵 사유에 로더 오류가 실제로 담긴다
  (`step3b-no-spdlog-detailed.log`: `failed to load (missing dependency …) … 0x8007007E` 102회).
  **카드 기대는 "실패 0 / 건너뜀 60" 이었으나 실측은 3 / 51 이다** — `xpe_preprocess.dll` 은
  `spdlog.dll` 없이도 로드된다(의존하지 않는다). 남은 실패 3건은 로드와 무관한 §5 건이다
- **c**: GUI-C-06 회귀 없음. 0 / 30 / 60 그대로
- **d**: 축2 9건이 스킵되는 것 외에 a 와 같은 성격

## 4단계 — GUI-C-06 미검증 항목 2개가 여기서 해소됐다

1. **"축2의 2차 가드(`TryLoad` 실패) 경로 미관측"** — b 에서 축1이 로드 실패로 스킵되며
   그 경로가 실제로 실행됐다. 사유 문자열도 보고에 남는 것을 확인했다
2. **"DLL 이 있을 때 60건이 통과하는가"** — 답은 **아니오, 54/60 이다.** a 가 그 답이다

## 5. 남은 실패 6건 — 이 카드 범위 밖

로드가 정상인 상태(a)에서도 실패하는 6건. **처음으로 보이게 된 실제 동작 불일치**이며,
단언 변경 금지 조항과 소유 경계상 이 카드에서 손대지 않았다.

| 테스트 | 오류 |
|---|---|
| `P1AReady.PreprocessCorrectionChainSmokeTests` 3건 | `Assert.Equal() Failure: Values differ` |
| `Lifecycle.LoggingHandlerTests.LogSetFile_WritableTempPath_ReturnsOk` | `IOException: The process cannot access the file …` |
| `Functional.MetadataMarshallingTests.Configure_MalformedJson_ReturnsConfigInvalid` | `Assert.Equal() Failure` |
| `ErrorMapping.NativeErrorTranslationTests.Configure_VeryLongMalformedJson_…` | `Assert.Null() Failure` |

**따라서 이 커밋으로 CI 가 녹색이 되지는 않는다.** 51 → (예상) 6 으로 줄고, 그 6은 별도 판정
사안이다. 후속 카드가 필요하면 leader 가 낸다.

## 6. 미검증 (Gaps)

- **사용한 DLL 은 로컬에 있던 2026-08-18 산출물이다.** 현재 main 을 빌드한 CI 바이너리와
  같다는 보장이 없다. 6건 실패가 최신 바이너리에서도 재현되는지는 **미검증**이며,
  CI 재실행이 판정한다 (네이티브 빌드 금지 조항 때문에 여기서 확인할 수 없다)
- 수정 후 CI 실제 실행 결과는 아직 없다. 위 예상 6은 로컬 관측이지 CI 관측이 아니다
- `xpe_preprocess.dll` 이 `spdlog.dll` 에 의존하지 않는다는 것은 b 의 **동작 관측**이다.
  import table 을 직접 읽어 확인하지는 않았다
- Release 구성 미빌드, VS IDE 미확인, WPF 앱 미실행
- `build/ci-common/bin/` 은 gitignore 대상이라 커밋되지 않는다. 검증 종료 후 CI 합집합
  8개 DLL 상태로 복원해 두었다

## 7. 잔여 위험

- 복사 대상을 이름으로 고정했으므로 네이티브가 새 공유 라이브러리에 의존하기 시작하면
  같은 사고가 재발한다. 다만 이번 수정으로 그때는 **실패가 아니라 스킵 + 사유**로 드러난다 —
  조용한 실패가 시끄러운 스킵이 됐다는 것이 이 카드의 실질 성과다
- `Resolver` 가 핸들을 재사용하므로 픽스처 생성 이후 DLL 이 교체돼도 반영되지 않는다.
  테스트 세션 수명 안에서는 오히려 결정론적이라 의도한 동작이다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
git merge origin/main
cp "D:/workspace-github/image-processing/build/ci-common/bin/"*.dll     build/ci-common/bin/
cp "D:/workspace-github/image-processing/build/ci-preprocess/bin/"*.dll build/ci-common/bin/
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
dotnet test … --no-build --logger "console;verbosity=detailed"
```

로그: `step0-repro.log`, `step0-repro-detailed.log`, `step0-repro-ciparity.log`,
`step3a-full-staging.log`, `step3b-no-spdlog.log`, `step3b-no-spdlog-detailed.log`,
`step3c-no-dlls.log`, `step3d-three-dlls.log`
