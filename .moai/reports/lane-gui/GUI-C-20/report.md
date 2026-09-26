# GUI-C-20 — 저하 모드 "선택 DLL 전부 부재" 케이스 (#56 잔여 b, #128)

- 카드: GUI-C-20 · Refs #56 #128 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 선행: `git merge origin/main` → `95f36a7` 병합 (변경 `docs/project/api-spec.md` 1건, `.claude` 변경 0건)
- **결과: 앱 재빌드 경고 0 / 오류 0 · dotnet 실패 0 / 통과 137 / 건너뜀 1 / 전체 138** (+2, 카드 예측과 일치)

---

## 1. 주장 (Claim)

1. `DegradedModeReadinessTests` 에 `all_optional_absent` 케이스가 생겼다 — 선택 DLL 5개를 전부 뺀 스테이징에서 5개 모두 미해석·`R0`, 필수 `xpe_common`/`xpe_preprocess` 는 해석, 예외 없음.
2. `ModuleReadinessReportingTests` 가 같은 스테이징에서 **등급 보고** 수준으로 같은 사실을 단언한다.
3. 반증 1회로 두 케이스가 실제로 발화함을 확인했다.

## 2. 증거 (Evidence)

### 착수 전 실측 — 필수 DLL 한쪽은 "해석됨" 단언이 공허하다

```
ls clients/ImageProcTest.IntegrationTests/bin/Debug/net8.0/*.dll
  … xpe_common.dll …          # xpe_preprocess.dll 은 없음
grep -n "CopyXpeDllsForTests" clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj
  74:  <Target Name="CopyXpeDllsForTests" BeforeTargets="Build">
```

`XpeCommonLibraryLocator.GetDllCandidates()` 는 `AppContext.BaseDirectory` 를 **주입 디렉터리보다 먼저** 본다(`:34`). 즉 빌드가 복사해 둔 사본 때문에 `xpe_common` 은 스테이징이 비어 있어도 해석된다 — "not null" 만 단언하면 이 레인이 반복해 잡아온 **정의 존재를 동작으로 오귀속**(gate #2)이 된다.

그래서 단언을 경로까지 좁혔다: 해석 경로가 **스테이징 또는 앱 출력 폴더** 둘 중 하나여야 한다. 빌드 디렉터리·형제 저장소로 새면 실패한다.

### 회귀 실행 (신규 2건 포함, `step2-new-cases.log`)

```
dotnet test … --filter "…DegradedModeReadinessTests|…ModuleReadinessReportingTests"
통과!  - 실패: 0, 통과: 11, 건너뜀: 0, 전체: 11
```

### 반증 (`step3-falsify.log`) — 스테이징에 `gsvg.dll` 을 남겨 둠

```
실패!  - 실패: 2, 통과: 9, 전체: 11

ModuleReadinessReportingTests.AllOptionalDllsAbsent_GradesEveryOptionalR0_AndNoRequiredOne [FAIL]
DegradedModeReadinessTests.AllOptionalDllsAbsent_RequiredFloorStillResolves [FAIL]
```

정확히 **신규 2건만** 실패하고 기존 9건은 영향이 없다. 원복 후 재확인함.

### 최종 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   137, 건너뜀:     1, 전체:   138
```

## 3. Baseline 귀속

- 이전 기준: GUI-C-19 최종 `0/135/1/136` (`GUI-C-19/step4-final.log`)
- 이번 실측: `0/137/1/138` — 증가 2 = 신규 케이스 2건. 카드가 예상한 +2 와 일치하며, 이번 실행에서 직접 관측한 값이다.
- 건너뜀 1은 C-12 부터의 `-15` 문자열 건으로 변동 없음.

## 4. 미검증 (Gaps)

- **플레이스홀더는 PE 가 아니다.** 스테이징 파일은 `MZ` 2바이트다. 로케이터가 `File.Exists` 로 판정하므로 충분하지만, **로드 가능성은 확인하지 않았다** — 이 케이스는 "탐색이 무엇을 찾는가" 까지이고 "찾은 것이 로드되는가" 는 범위 밖이다.
- **스냅샷 실물은 만들지 않았다.** `ModuleReadinessService` 를 통과시키면 프로브가 `XpeCommonApi` 를 건드려 DllImport 리졸버 이중 등록으로 던진다(C-13 실측). C-14~15 와 같은 경계를 유지해 등급 함수(`ModuleReadinessGrading`)·보고 함수(`ModuleReadinessReporting`) 수준에서 단언했다.
- `xpe_common` 의 해석은 스테이징이 아니라 앱 출력 폴더에서 났을 수 있다(§2). 두 뿌리 중 하나임까지가 이 테스트가 고정하는 범위다.
- WPF 앱을 실행해 실제 저하 화면을 보지 않았다 — 빌드만 확인.
- CI 실행 결과는 아직 없다.

## 5. 잔여 위험 (Residual risk)

- 옛 BP-10 CI 잡이 하던 일 중 **네이티브 측 단언**(`test_post_degraded_mode.cpp`)은 여전히 별개다. 이 스위트는 C# 결론만 덮는다.
- 필수 DLL 집합(`RequiredDlls`)을 손으로 적었다 — gate #11 과 같은 구조적 사각이다. 필수 목록이 늘면 이 테스트는 조용히 낡는다. 지금은 2개로 고정된 계약이라 두었고, 늘어날 때 열거 기반으로 바꿔야 한다.
- 환경변수 주입은 프로세스 전역이다. xUnit 병렬 실행에서 다른 케이스와 겹치면 간섭할 수 있다 — 현재 두 클래스 모두 컬렉션 분리를 하지 않았고, 이번 실행에서는 관측되지 않았다.

## 부록 — 사용한 명령

```bash
git fetch origin main && git merge origin/main            # → 95f36a7
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test … --filter "…DegradedModeReadinessTests|…ModuleReadinessReportingTests"   # 회귀·반증
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
