# QA-B-19 — 기능 테스트 안의 시간 예산 단언 분리 (#120)

**레인**: Lane B (`dev/postprocess`)
**문제(카드 실측)**: `IntegrationTest.T603_FullPipelineIntegration` 이 `totalDuration < 500` 으로
실패(667)하면서 **같은 케이스의 기능 단언 15개까지 함께 잃는다.**

**결과**: 혼합 6건 분리, `ci-post` **407/407 무실패**.

---

## 1. 주장 (Claim)

1. Lane B 소유 테스트를 **블록 단위로 전수 스캔**했다 — grep 이 아니라 TEST 본문 파싱(§2).
2. 혼합 29건 중 **분리가 실제로 커버리지를 되찾는 6건**만 분리했다(§3). 나머지 23건의
   판정 근거를 남긴다(§4).
3. 기능 단언은 **한 줄도 바꾸지 않았다**. 시간 단언만 옮겼다.
4. gsvg 분리에서 원본 구성을 잘못 옮겼다가 **실측으로 정정**했다(§3.2).
5. 재실측: `ci-post` 407/407.
6. **카드가 전제한 "성능 제외 정규식" 은 이 워크트리에서 확인되지 않았다**(§4.1) — 근거로 쓰지 않았다.

---

## 2. 1단계 — 전수 스캔 (방법)

`grep` 만으로는 "한 케이스 안에 둘 다 있는지" 를 판정할 수 없어(파일 단위로만 잡힌다)
**TEST/TEST_F 본문을 중괄호 깊이로 잘라** 각 블록에서 세었다:

- 시간 단언: `(EXPECT|ASSERT)_(LT|LE|GT|GE|NEAR)` 인자에 `duration|elapsed|_ms|ms_|time` 포함
- 기능 단언: 그 외 모든 `EXPECT_*` / `ASSERT_*`

대상: `modules/{enhance_basic,enhance_advanced,ai,display,dicom,gsvg}/tests`,
`tests/ai_tests`, `tests/e2e_post_pipeline`.

결과 **29건**이 둘 다 갖고 있었다. 전체 목록: `_mixed_scan.txt`.

---

## 3. 2단계 — 분리한 6건

판정 기준: **이름이 시간 예산을 알리지 않는데 예산 단언을 품은 케이스.**
그런 케이스에서만 예산 실패가 "기능 검증 상실" 로 이어진다(카드가 측정한 T603 의 형태).

| # | 케이스 | 기능 단언 수 | 분리 후 |
|---|---|---|---|
| 1 | `IntegrationTest.T603_FullPipelineIntegration` | **15** | `T603b_FullPipeline_PerformanceBudget` |
| 2 | `GsvgAbiSmoke.Lifecycle3072_PassThroughIsByteEqual` | 5 | `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget` (1번째 arm) |
| 3 | `GsvgAbiSmoke.Lifecycle3072_VignetteAndGrid_OutputClampedAndSourceIntact` | 8 | 같은 케이스 (2번째 arm) |
| 4 | `GsvgDegradedMode.BP07_NullVignetteMap_IdentityOutput` | 6 | `GsvgDegradedMode.DegradedMode_PerformanceBudget` (arm 1) |
| 5 | `GsvgDegradedMode.BP08_AllOnesVignetteMap_OutputEqualsInput` | 6 | 같은 케이스 (arm 2) |
| 6 | `GsvgDegradedMode.BP09_GridDisabled_VignetteOnly` | 5 | 같은 케이스 (arm 3) |

기능 케이스에서는 측정 자체는 남기고(`elapsed` 계산) `(void)` 로 소비하며 주석으로
어디로 갔는지 적었다 — 측정 코드를 지우면 원래 무엇을 재고 있었는지가 사라진다.

### 3.1 파일당 1개로 묶은 이유

gsvg 5건을 5개 케이스로 쪼개면 동일한 준비 코드가 다섯 벌 생긴다. 대신 **파일당 하나**로
묶되 각 arm 이 원래 케이스가 재던 구성을 그대로 돌게 했다 — 타이밍 커버리지는 보존되고
중복은 줄어든다.

### 3.2 gsvg degraded 구성을 잘못 옮겼다가 정정했다 (기록)

처음에 세 arm 을 **설정 문자열**이 다른 것으로 썼다. 확인해 보니 `xpe_gsvg_init` 설정은
세 케이스가 **동일**(`vignette_correction:true, grid_suppression:false`)하고,
실제 차이는 **게인 맵**이었다:

| 원본 케이스 | 게인 맵 |
|---|---|
| `BP07_NullVignetteMap_IdentityOutput` | `nullptr` |
| `BP08_AllOnesVignetteMap_OutputEqualsInput` | 전 원소 `1.0f` |
| `BP09_GridDisabled_VignetteOnly` | 전 원소 `2.0f` |

없는 설정 차이를 만들어 넣을 뻔했다. **소스를 다시 읽어 게인 맵을 varying 축으로 고쳤다.**
분리 작업에서 "무엇이 서로 다른가" 를 잘못 잡으면 커버리지를 옮긴 게 아니라 잃는다.

---

## 4. 분리하지 않은 23건 — 판정 근거

`_mixed_after.txt` 기준. 22건은 **이름 자체가 시간 예산을 알린다**:

```
*_Performance*        : ContrastEnhance.Performance_3072x3072_Within50ms 등 11건
*Within##ms           : FullPipelineE2E.PostProcess_3072x3072_Within3000ms 등
BenchmarkFreeze*/Baseline : BP06/BP07/BP08/BP09 등 4건
*_PerformanceBudget   : T308, T508, T608 + 이번에 만든 3건
```

이들의 "기능 단언" 은 대부분 `ASSERT_EQ(rc, XPE_OK)` 같은 **전제조건**이지 독립적인
기능 검증이 아니다. 분리해도 되찾을 커버리지가 없다.

### 4.1 카드의 전제 하나를 확인하지 못했다

카드는 "성능 제외 정규식에 안 걸린다" 를 근거로 든다. **이 워크트리의
`.github/workflows/` 에는 `coverage-post` 잡도, 성능 제외 정규식도 없다**
(`ctest` 호출 5곳 전부 `-E`/`--exclude-regex` 없음). main 쪽에 있을 수 있으나
확인할 수 없었다.

그래서 정규식을 근거로 쓰지 않고 **"예산 실패가 기능 검증을 함께 죽이는가"** 를
판정 기준으로 삼았다. 두 기준은 대체로 같은 집합을 고르지만, 근거로 삼은 것은 후자다.

### 4.2 `IntegrationTest.T602_DiagnosticLogging` — 성격이 다르다

유일하게 이름이 예산을 알리지 않으면서 목록에 남은 건이다. 그러나 단언이

```cpp
EXPECT_GT(duration, 0);  // Some time elapsed
```

**하한**이지 예산이 아니다. 느린 머신에서 실패하지 않으므로 이 카드가 겨냥한 문제
(성능 때문에 기능 커버리지 상실)에 해당하지 않는다.

다만 **반대 방향의 취약성**은 있다: 연산이 1ms 미만으로 끝나면 `duration == 0` 이 되어
실패한다. 빠른 머신에서 플래키할 수 있다 — 별개 사안으로 보고만 한다.

---

## 5. 재실측

| 항목 | 결과 | 로그 |
|---|---|---|
| 대상 케이스 실행 | gsvg 8/8, enhance_advanced T603 2/2 | `_run.log` |
| `cmake --preset ci-post` | `CFG_EXIT=0` | `_verify.log` |
| `ctest` 전체 (ci-post) | **407 passed / 0 failed**, `ALL_EXIT=0` | `_verify.log` |
| 분리 후 재스캔 | 6건 전부 목록에서 사라짐 | `_mixed_after.txt` |

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` 전체 | 404 (`QA-B-18/gate.md` §6) | **407** | +3 = 신규 예산 케이스 3개 |
| 혼합 케이스(기능명 + 예산) | 6 (`_mixed_scan.txt`) | **0** | 목적 달성 |
| 혼합 케이스(예산명 포함 전체) | 29 | 23 | -6, 나머지는 §4 사유 |
| 기능 단언 내용 | — | **무변경** | 시간 단언만 이동 |

## 7. 미검증 (Gaps)

- **Debug 구성에서 재현/확인하지 않았다.** 카드의 실측(667ms 실패)은 CI Debug 인데
  로컬은 RelWithDebInfo 다. 분리가 Debug 에서 의도대로 동작하는지는 leader 의 CI 재측정 몫이다.
- **커버리지 회복량을 측정하지 않았다.** "T603 의 기능 단언 15개를 되찾는다" 는 구조적
  논증이지 커버리지 수치로 확인한 것이 아니다.
- **성능 제외 정규식 미확인**(§4.1). 분리한 이름(`*_PerformanceBudget`)이 실제로 그 정규식에
  걸리는지 확인할 수 없었다 — 이름 규칙은 카드 지시를 따랐다.
- **Lane A 소유(common/preprocess) 테스트는 스캔 대상에서 제외했다.** 같은 유형이 있을 수 있다.
- **스캔 정규식의 재현율 미검증.** `duration|elapsed|_ms|ms_|time` 로 잡았으므로,
  다른 변수명(`tick`, `cost` 등)을 쓴 예산 단언은 놓쳤을 수 있다.
- **`T602_DiagnosticLogging` 의 하한 플래키성을 실측하지 않았다**(§4.2) — 코드 판독만.

## 8. 잔여 위험 (Residual risk)

- 분리한 예산 케이스는 **파이프라인을 한 번 더 돈다**. `ci-post` 실행 시간이 그만큼
  늘어난다(T603b 39ms, gsvg abi 51ms 관측). 크지 않지만 공짜는 아니다.
- gsvg 예산 케이스를 파일당 1개로 묶었으므로(§3.1), **한 arm 이 실패하면 그 뒤 arm 은
  실행되지 않는다**(`ASSERT_EQ` 사용 지점). 원래 3개 케이스였다면 독립적으로 보고됐을
  정보가 줄어든다 — 중복 감소와 맞바꾼 부분이다.
- §4.2 의 `EXPECT_GT(duration, 0)` 은 그대로 남아 있다. 빠른 머신에서 실패하면
  `T602_DiagnosticLogging` 의 기능 단언 2개도 함께 잃는다 — 이 카드가 고친 것과 **같은
  형태의 문제이며 방향만 반대**다.

---

## 9. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 분리 목록 | PASS | §3 표(6건) + §4 미분리 23건 사유 |
| diff | PASS | `test_integration.cpp`, `test_gsvg_abi_smoke.cpp`, `test_gsvg_degraded.cpp` |
| 재실측 | PASS | §5 — 407/407, 재스캔으로 6건 소거 확인 |
| footer `Refs #120` | PASS | 커밋 메시지 |
