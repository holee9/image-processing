# GUI-C-18 — #56 "BP-10 저하 모드 교차 레인 통합 검증" 체크리스트 실측 (읽기 전용 감사)

- 카드: GUI-C-18 · Refs #56 #128 · **코드 변경 없음**
- 워크트리: `D:/workspace-github/xpe-gui` / `dev/gui` / HEAD `3b5c7fd` (main 병합)
- **결론: 5항 중 3항 충족, 1항 충족이나 공허, 1항 게이트 없음.** 카드 후보 3건 (§4)

---

## 0. 먼저 — 카드 전제 정정 2건

### 0-1. 체크리스트는 4항이 아니라 **5항**이다

`gh issue view 56` 원문:

```
- [ ] Pre-A + Post-B Lane 결과를 main으로 병합
- [ ] BP-10 전체 파이프라인 stress 테스트 실행 (기존 테스트 드라이버 활용)
- [ ] benchmark-regression.yml CI에서 BP-10 cross-lane 통과 확인
- [ ] 성능 예산 검증: Phase 1 파이프라인 < 3000ms (3072×3072)
- [ ] Peak memory <= 190MB 검증        ← 카드에 누락
```

다섯 번째(메모리 예산)가 카드 요약에서 빠져 있었다. **그 항목이 유일하게 CI 로그로 통과가
관측되는 항목**이라 누락이 결과를 바꾼다(§2-5).

### 0-2. "BP-10" 은 두 계열이 아니라 하나다 — 다만 층이 다르다

착수 전 두 계열이 우연히 번호만 겹치는지 확인했다. 겹치는 것이 아니다:

- `benchmark-regression.yml:1` 이름이 `XPE Benchmark Regression (BP-06~10)`, 3행 주석이
  `BP-10: Degraded-mode stress - cross-lane integration test`
- 즉 **BP-06~09 = 벤치마크 동결, BP-10 = 저하 모드 교차 레인**
- `test_post_degraded_mode.cpp` 의 `BP-06`~`BP-10` 은 그 **BP-10 안의 모듈별 시나리오**
  라벨이다(gsvg / enhance_advanced / enhance_basic / dicom / display)

C-14~17 이 검증한 것은 후자(모듈별 저하)이고, #56 의 BP-10 은 그것을 CI 에서 묶는 층이다.
**같은 대상의 다른 층**이라 C-14~17 이 #56 을 자동으로 채우지는 않는다.

## 1. 체크리스트 5항 대조

| # | 항목 | 무엇이 충족하는가 | 판정 |
|---|---|---|---|
| 1 | Pre-A + Post-B 를 main 병합 | leader 가 이번 세션에 완료 (`origin/main` = `3b5c7fd`) | **충족** |
| 2 | BP-10 전체 파이프라인 stress 테스트 실행 | 드라이버는 `tools/ci/Test-DegradedMode.ps1`, `benchmark-regression.yml:112` 가 시나리오 6개로 호출 | **충족하나 공허** (§2-2) |
| 3 | `benchmark-regression.yml` CI 에서 BP-10 통과 | 워크플로가 실제로 돌고 최근 4회 `success` | **충족하나 공허** (§2-3) |
| 4 | 성능 예산 Phase 1 < 3000ms (3072²) | 테스트는 있다 — `FullPipelineE2E.PostProcess_3072x3072_Within3000ms`. **그러나 어느 게이트도 돌리지 않는다** | **게이트 없음** (§2-4) |
| 5 | Peak memory ≤ 190MB | `FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB` — **CI 에서 실행·통과 관측** | **충족** (§2-5) |

## 2. 항목별 실측

### 2-2 / 2-3. BP-10 은 "준비도" 를 묻지 않는다 — C-14 가 지적한 결함이 CI 층에도 있다

`benchmark-regression.yml` 의 BP-10 잡은 시나리오 6개(`missing_gsvg`,
`missing_enhance_advanced`, `missing_display`, `missing_dicom`, `missing_…`, `all_optional_absent`)
를 `expected_readiness: "R0"` 과 함께 `Test-DegradedMode.ps1` 에 넘긴다. 그런데 그 스크립트의
**합격 판정은 준비도가 아니다**:

```powershell
$passedCount = ([regex]::Matches($stdout, '\[\s+OK\s+\]\s+DegradedMode\.')).Count   # :185
$passed = ($exitCode -eq 0 -and $passedCount -gt 0)                                  # :188
```

세는 대상 `DegradedMode.*` 는 `modules/common/tests/test_post_degraded_mode.cpp` 의
자리표시자이고, 그 본문은 **파일이 없다는 것만 단언한다**:

```cpp
EXPECT_FALSE(std::filesystem::exists(dllPath))   // :56
::testing::Test::RecordProperty("expected_readiness", "R0");   // :59  ← 기록만, 질의 아님
```

`expected_readiness` 는 워크플로에서도 스크립트에서도 **비교되지 않는다**. 집계 잡
(`:149-160`)도 `$data.passed` 만 본다. 결국 이 게이트가 증명하는 것은
**"하네스가 DLL 을 지웠다"** 이지 "앱이 R0 으로 낮췄다" 가 아니다.

C-14 가 네이티브 자리표시자에 대해 내린 판독과 같은 결함이며, **CI 층에도 그대로 있다.**

> 로컬에서는 `XPE_DEGRADED_ABSENT_DLL` 미설정이라 항상 스킵이지만, CI 에서는 스크립트가
> `:130` 에서 그 변수를 설정하므로 **실행된다**. "항상 스킵" 은 로컬 한정이다 — C-14 배경의
> 서술을 이 지점에서 좁힌다.

**교차 레인 위험 1건**: Lane A 의 A-24 는 이 자리표시자 삭제가 예정돼 있다. 삭제하면
`passedCount` 가 0이 되어 **BP-10 잡이 실패한다.** 삭제와 드라이버 교체는 같은 카드에서
움직여야 한다.

### 2-4. 3000ms 예산 — 어느 게이트도 돌리지 않는다

테스트는 존재한다: `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:202`
`PostProcess_3072x3072_Within3000ms`, `EXPECT_LE(total_ms, 3000)`.

빌드도 된다: 루트 `CMakeLists.txt:161` 이 `BUILD_ENHANCE_BASIC/ADVANCED/GSVG/DISPLAY` 가 모두
켜질 때(= `ci-post` 프리셋) `tests/e2e_post_pipeline` 을 추가한다.

그러나 **두 게이트 모두 이 테스트를 제외한다**:

| 게이트 | 패턴 | 결과 |
|---|---|---|
| `ci.yml:200` | `-E "Performance\|Within[0-9]+ms\|PerformanceBudget\|LargeImagePerformance"` | `Within3000ms` 가 **제외 패턴에 걸림** |
| `benchmark-regression.yml:62` | `-R 'BenchmarkFreeze\|CollimationDetectTest\.BenchmarkFreeze_BP07…\|ExposureIndex\.BenchmarkFreeze_BP0[89]…'` | `FullPipelineE2E` 가 **포함 패턴에 없음** |

`ci.yml:193-197` 의 주석은 제외 사유를 "공유 러너에서는 기계를 재는 것" 이라 밝히며
**"타이밍 회귀는 벤치마크 워크플로의 게이트"** 라고 위임한다. 그런데 그 워크플로의 포함
패턴에 이 테스트가 없다. **위임처가 받지 않는 위임이다.**

CI 아티팩트로도 확인했다 — 최근 성공한 `ci.yml` 실행(`34451123734`)의
`xpe-ci-post-test-results` 안에 `Within3000ms` 는 **한 번도 나오지 않는다**(§2-5 의 검색과
동일한 아티팩트).

### 2-5. 메모리 190MB — 유일하게 통과가 관측되는 항목

`PeakMemory190MB` 는 제외 패턴 넷 중 어느 것에도 걸리지 않아 `ci.yml:200` 에서 실행된다.
추론이 아니라 CI 아티팩트에서 읽었다:

```
427/427 Testing: FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB
[ RUN      ] FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB
[ E2E ] Peak working set: 54 MB (budget: 190 MB)
[       OK ] FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB (319 ms)
Test Passed.
```

출처: `gh run download 34451123734 -n xpe-ci-post-test-results` → `LastTest.log`.
**54MB / 190MB, 여유 136MB.**

## 3. 드라이버 실측 (카드 2항)

| 드라이버 | 위치 | 성격 |
|---|---|---|
| 저하 모드 시나리오 | `tools/ci/Test-DegradedMode.ps1` (6448바이트) | DLL 을 스테이지에서 제거 → `test_xpe_common.exe` 의 `DegradedMode.*` 실행 → JSON 결과. **준비도 미질의**(§2-2) |
| 전체 파이프라인 E2E | `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp` | 3000ms / 200ms / 190MB 게이트 3종. `ci-post` 에서만 빌드 |
| `clients/**` E2E | `gui/ImageProcTest.E2E` 프로젝트 | 이번 감사 범위에서 워크플로 참조 0건 — 어느 CI 잡도 실행하지 않는다 |

`tests/CMakeLists.txt` 는 **없다**(#113 에서 폐지, 루트 주석 `CMakeLists.txt:152-156`).
`tests/e2e_post_pipeline` 만 루트가 직접 추가한다.

## 4. 카드 후보 (카드 3항)

| 후보 | 내용 | 소유 | 크기 |
|---|---|---|---|
| **A** | BP-10 드라이버가 **준비도를 실제로 질의**하도록: `expected_readiness` 를 기록이 아니라 비교로. 지금은 파일 삭제만 증명한다 | **Lane C**(판정 로직이 C# `ModuleReadinessGrading`) 또는 main(스크립트) — §5 참조 | 한 커밋 |
| **B** | 3000ms 예산에 게이트 붙이기: `benchmark-regression.yml` 포함 패턴에 `FullPipelineE2E.*Within3000ms` 추가. 러너 변동성 대응(반복·중앙값)이 필요할 수 있다 | **main** (워크플로 소유) | 한 커밋 |
| **C** | A-24(자리표시자 삭제)와 BP-10 드라이버 교체를 **한 카드로 묶기**. 따로 움직이면 삭제 시점에 BP-10 잡이 실패한다 | **main** 조정 (Lane A 실행 + A 후속) | 조정 |

**A 의 소유 판단**: 준비도 R0 판정은 C# 에 있고(C-15 의 `ModuleReadinessGrading`), 그것을
CI 에서 호출하려면 준비도를 출력하는 실행 파일이 필요하다. `gui/ImageProcTest.SelfCheck` 가
그 자리에 가장 가깝지만 현재는 픽스처 검증만 한다(GUI-C-15 조사). 즉 A 는
**"준비도를 찍는 콘솔 진입점" 을 만드는 일**을 포함하며, 그 크기가 한 커밋을 넘길 수 있다 —
착수 전 판정 권고.

## 5. 미검증 (Gaps)

- **`Test-DegradedMode.ps1` 을 실행하지 않았다.** 판독은 소스와 CI 로그 기준이다.
  스크립트가 실제로 어떤 시나리오에서 몇 건을 세는지는 CI 실행 로그를 열지 않았다
  (BP-10 잡의 아티팩트 `bp10-aggregate-report` 미확인)
- **`benchmark-regression.yml` 의 BP-10 잡이 최근 실행에서 실제로 성공했는지 잡 단위로
  확인하지 않았다.** 워크플로 전체의 `conclusion=success` 4회만 봤다 — 잡 단위 판독은 하지 않았다
- `gui/ImageProcTest.E2E` 가 워크플로에서 참조되지 않는다는 것은 **grep 결과**다.
  다른 이름으로 호출될 가능성은 ctest 호출 9곳을 전수로 훑어 배제했으나, 워크플로 밖
  (예약 작업 등)은 보지 않았다
- 성능 예산 3000ms 가 **로컬에서 통과하는지도 모른다** — 이 레인은 네이티브를 빌드하지 않는다
- #56 의 "Done When" 중 `dev-plan.md` 표시는 확인하지 않았다(main 소유 문서)

## 6. 잔여 위험

- **A-24 와 BP-10 의 결합이 가장 큰 위험이다.** 자리표시자는 지금 CI 게이트의 유일한 판정
  근거이고(공허하더라도), 삭제하면 게이트가 빨개진다. "공허한 게이트를 지우면 게이트가
  없어진다" 는 상태로 넘어가지 않도록 A 와 C 를 함께 다뤄야 한다
- 3000ms 게이트를 그냥 켜면 공유 러너 변동성 때문에 깜빡일 수 있다 — `ci.yml` 주석이
  1~16ms 차이로 3건이 실패한 이력(`34417874089`)을 남겼다. B 는 반복·중앙값 같은 완화가
  전제다

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/GitHub CLI:$PATH"
gh issue view 56 --json title,state,body
gh run list --workflow=benchmark-regression.yml --limit 5
gh run download 34451123734 -n xpe-ci-post-test-results   # → LastTest.log / CTestCostData.txt
grep -rn "ctest" .github/workflows/
grep -n "ExpectedReadiness\|passedCount\|XPE_DEGRADED_ABSENT_DLL" tools/ci/Test-DegradedMode.ps1
sed -n '150,165p' CMakeLists.txt
```
