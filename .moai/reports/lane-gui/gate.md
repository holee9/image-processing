# Lane C — 반복 실패 유형 기록 (gate)

레인 공통 자산. lead 요청으로 GUI-C-06 에서 개설했다.
공통 형태: **"있다" 를 "작동한다" 로 읽는 것.** 검증 없이 겉모습으로 판단한 사례만 모은다.

| # | 카드 | 겉모습 | 실제 | 잡은 방법 | 낸 쪽 |
|---|---|---|---|---|---|
| 1 | GUI-C-06(조사) | `strings` 출력이 비어 있음 → "Skip 심볼 0건" | `strings` 미설치. 셸이 조용히 실패한 것 | `which strings` 확인 후 바이트 스캔 재실행 | Lane C (자가 정정) |
| 2 | GUI-C-05 | `SkipHelper` 가 존재함 → "스킵 메커니즘이 있다" | 호출부 0건. 실제는 인라인 조기 `return` | 호출부 grep | lead |
| 3 | GUI-C-05 후속 | 광범위 삭제 명령이 "정리" 로 보임 | 드라이런 480 항목 삭제 대상, `.moai/reports/`(증거 유일본)·`.moai/lanes/` 포함 | 실행 전 드라이런 | lead |
| 4 | GUI-C-06(구현) | `Xunit.Sdk.SkipException.ForSkip` 컴파일 통과 → "동적 스킵 지원됨" | 실행하면 Failed. 토큰 생산자(`xunit.assert`)만 있고 소비자(execution / VSTest 어댑터)가 없음 | 1건만 전환해 러너 출력 확인 + 어셈블리 바이트 스캔 | lead 판정 1 (Lane C 프로브가 근거 제공) |
| 5 | GUI-C-08 | 픽스처: DLL 파일이 있고 x64 PE 다 → "가용하다"(`IsAvailable=true`) | 의존 DLL(spdlog/fmt) 부재로 P/Invoke 에서 로드 실패. CI 51건 실패 | CI 로그 → 로컬 재현 → `NativeLibrary.Load` 로 판정 교체 | 원 코드 (레인 공통 유형) |
| 6 | GUI-C-08(재현) | 카드가 적은 재현 조건("`build/ci-common/bin` 에 두 DLL 다 있음")대로 하면 48/33/9 — CI 51/39/0 과 불일치 | 그 폴더에 `xpe_preprocess.dll` 이 없었다. CI 는 두 아티팩트를 같은 폴더로 내려받는다(`ci.yml:182-191`) | 숫자를 맞추지 않고 `ci.yml` 을 읽어 스테이징을 합집합으로 교정 → 51/39/0 일치 | leader 카드 배경 |
| 7 | GUI-C-09 | 카드 배경: "api-spec 에 `dataSize` 입력 계약이 생겼다(2026-09-10)" | `origin/main`(a3ae133)에 없었다. 절 자체는 존재하며 커밋 `379432f`(로컬 main)에 있고 **push 전**이었다 — 레인은 origin 만 볼 수 있다 | 착수 전 `grep` + `git log -- api-spec.md`. 단 결론은 "없다" 가 아니라 "origin 에 아직 없다" 로 좁혔어야 했다 | leader 카드 배경 (레인 진술 범위도 과했음) |
| 8 | GUI-C-09 | `DataSize` 를 대입하는 코드가 있다 → "정확히 채워진다" | 대입 존재는 정확성이 아니다. 값이 `w×h×bpp` 와 맞는지는 호출 사슬 끝(배열 할당부)까지 봐야 안다 | 생성 지점 8곳 → 호출부 18곳 → 배열 할당부까지 추적 | 원 코드 (검증 방식) |
| 9 | GUI-C-10 | "가드가 main 에 들어왔으니 테스트가 통과할 것" | 소스에 있는 것과 **내가 실행하는 바이너리에 있는 것**은 다르다. 로컬 최신 DLL 은 2026-08-18, 가드는 2026-09-10 | 반환코드 순서로 행동 증거(크기검사가 앞선데 NOT_INITIALIZED 가 나옴) + `-newermt` 로 가드 이후 빌드본 0건 확인 | 레인 착수 가정 |
| 10 | GUI-C-10 | 테스트를 추가했으니 CI 가 판정해 줄 것 | `ci.yml` 이 `xpe_enhance_basic.dll` 을 스테이징하지 않아 3건은 CI 에서도 **스킵**된다 — 로컬·CI 양쪽에서 관측 불가 | 착수 전이 아니라 보고 직전에 `ci.yml` 아티팩트 목록 재확인 | 레인 (보고로 해소) |

| 11 | GUI-C-12 | `ErrorString_ForAllDefinedCodes` 가 통과한다 → "정의된 코드를 다 확인했다" | 순회 대상이 손으로 적은 목록이라 **테스트가 자기 입력의 완전성을 못 본다**. enum 에 -11~-15 가 없는 채로 계속 통과했다 | 순회를 `Enum.GetValues` 로 바꾸고, 헤더를 직접 읽어 양방향 대조하는 케이스를 따로 추가 | 원 코드 (구조적 사각) |

| 12 | GUI-C-13 | 주석 `@MX:NOTE "kept in sync intentionally"` → "동기화되어 있다" | **의도의 선언이지 장치가 아니다.** 실제로 -11~-16 여섯 개가 빠져 있었다 | 주석을 믿지 않고 헤더와 직접 대조. 이제 테스트가 그 주석이 약속한 일을 한다 | 원 코드 |

| 13 | GUI-C-14 | 주입 지점(`XPE_NATIVE_DIR`)이 있다 → "그것으로 부재를 만들 수 있다" | 뒤에 빌드 디렉터리 16경로 + **형제 저장소** 폴백이 있어 뺀 DLL 을 다시 찾아낸다 | 씸을 끈 채 돌려 검색이 `D:/workspace-github/image-processing/...` 까지 뻗는 것을 관측 | 원 코드 (탐색 설계) |

| 14 | GUI-C-19 | 반증 실험이 **통과**했다 → "가드가 튼튼하다" | 변형이 무력했을 뿐이다. `!match.Success` 를 `false` 로 바꿔도 `TryParse("")` 가 여전히 실패해 같은 경로를 탔다 | 변형이 **실제로 동작을 바꾸는지** 먼저 확인한다. 통과한 반증은 결과가 아니라 **실험 실패**다 | 레인 (자가 정정) |

**#14 는 이 목록의 메타 항목이다** — 반증 실험 자체가 #1~#13 과 같은 형태로 실패할 수 있다.
"관측처럼 보이는 비관측" 은 검증 도구에도 적용된다.

**#11·#12·#13 의 교정은 반증 실험으로 확인했다** — `DICOM_INVALID` 를 일부러 지우니 드리프트 테스트가
누락 코드 이름을 지목하며 실패했다. 가드를 추가했으면 **그 가드가 실제로 발화하는지** 본다.

## 유형 #9 의 해소책 — CI 아티팩트를 내려받는다 (GUI-C-11)

"소스에 있는 것 ≠ 내가 실행하는 바이너리에 있는 것"(#9)은 세 카드에 걸쳐 검증을 막았다.
GUI-C-11 에서 해소했다. **네이티브를 빌드하지 않고도 CI 와 같은 바이너리를 실행할 수 있다.**

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh run list --branch main --limit 5                     # 최신 "XPE CI Pipeline" 실행 id
gh run download <run-id> -n xpe-ci-common-binaries     -D <tmp>
gh run download <run-id> -n xpe-ci-preprocess-binaries -D <tmp>
gh run download <run-id> -n xpe-ci-post-binaries       -D <tmp>
cp <tmp>/*/*.dll build/ci-common/bin/
```

빌드 잡(common-build / preprocess-tests / post-build)이 끝나면 dotnet-tests 가 아직
진행 중이어도 아티팩트는 받을 수 있다. **네이티브 빌드 금지 조항을 위반하지 않는다** —
산출물 복사다(GUI-C-08 0단계와 같은 근거).

## 유형별 대응

- **도구 부재가 관측값 0 으로 위장** (#1) — 명령을 신뢰하기 전에 그 명령이 실제로 실행됐는지 확인한다. 빈 출력은 "없음" 이 아니라 "모름" 이다
- **정의 존재를 동작으로 오귀속** (#2, #4) — 심볼·타입·파일이 있다는 것은 배선됐다는 뜻이 아니다. 호출부 또는 런타임 출력을 본다. 컴파일 통과는 집계·동작 보장이 아니다
- **명령 사정거리가 의도보다 넓음** (#3) — 파괴적 명령은 드라이런으로 대상 목록을 먼저 본다
- **부분 시야에서 부재를 단정** (#7) — 레인이 보는 것은 `origin/main` 뿐이다. 거기서 못 찾은
  것을 "없다" 로 적으면 상대의 미푸시 작업을 부정하게 된다. **"내가 볼 수 있는 X 에 없다"**
  까지가 관측이고, 그 너머는 상대에게 물을 일이다. 관측 자체는 옳아도 진술 범위가 넘칠 수 있다

## 절차상 효과 (관측)

#4 는 이전 세션이 스스로 "미검증" 으로 표시해 둔 항목이었고, lead 가 그것을 **첫 검증 항목**으로
배치해서 1건 전환 시점에 잡혔다. 60건 전환 후에 발견됐다면 원복 비용이 컸다.
**미검증 표시 → 우선 검증 배치** 는 실제로 비용을 막은 절차다.

## GUI-C-06 제외 기록

C 유형 3건은 전환 대상에서 제외했고, 카드 완료 시점에도 그대로 유지됐다.
**제외 사유: 가드 이전에 이미 단언이 실행된다** — 스킵으로 바꾸면 수행된 검증이 보고에서
사라진다 (판정 3).

| 파일:행 | 테스트 |
|---|---|
| `Smoke/DllLoadSmokeTests.cs:49` | `ResolvedDll_IsX64Architecture` — 가드 앞 `Assert.Fail` 경로 |
| `Smoke/DllLoadSmokeTests.cs:87` | `WhenDllAbsent_FixtureReportsUnavailable_NotCrash` — 조기 return 없음 |
| `Smoke/DllLoadSmokeTests.cs:116` | `ArchitectureMismatch_SurfacesResolvedPathInMessage` — 조기 return 없음 |

최종 실측에서 이 3건은 A 27건과 함께 통과 30에 남았다(실패 0 / 통과 30 / 건너뜀 60 / 전체 90).
전환했다면 건너뜀은 63이 되고 수행되던 검증이 보고에서 사라졌을 것이다.
