# QA-A-26 검증 보고서 — SigmaClip 표준편차 정의 (#97)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-26 (`.moai/lanes/pre/inbox/QA-A-26.md`)
- 정본: `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md` §9.8.2.1 (7060행)

## 1. 주장 (Claim)

**결론부터: #97 의 수치 결함은 이미 고쳐져 있었고, 이 카드에서 새로 고칠 것은 없었다. 대신 같은 §9.8.2.1 의 다른 조항 하나가 미구현임을 실측으로 확인했다.**

1. **현황 실측.** `MultiOffsetTest.SigmaClip_CustomSigma` 는 삭제·완화·미등록이 아니다. `modules/preprocess/CMakeLists.txt:173` 에 등록돼 있고 **통과한다**(`a26-green.log:683`, 테스트 #340). 5개 SigmaClip 케이스 전부 통과.
2. **#97 의 109 는 재현되지 않는다.** 커밋 `7fd3436`(2026-08-18, 이슈 등록 당일)이 `stddev_of` 를 표본(Bessel, `/N-1`)에서 **모집단**(`/|S|`)으로 바꿨다. 이 카드가 걱정한 "2026-08-18 이후 코드가 움직였을 수 있음" 은 반대 방향이었다 — 이슈 당일에 이미 움직였다.
3. **표준편차 정의는 문서와 일치한다.** `xpe_calib_generate_offset_methods.cpp:124` 이 `sum_sq / values.size()` 로 나눈다. §9.8.2.1 의 `σ = sqrt((1/|S|) Σ (F_k − μ)²)` 그대로다.
4. **미구현 조항 1건 발견 — `N_min` 하한.** §9.8.2.1 은 `N_min = max(3, ⌊N/4⌋)` 이고 `|S| < N_min` 이면 **해당 픽셀을 정적 결함으로 마킹**하라고 요구한다. 구현에는 `N_min` 계산도, 결함 마킹 경로도 없다. 이 조항은 **고치지 않고 보고한다** — 사유는 §4.
5. 회귀 방지용 conformance 스위트 4케이스 추가. ci-preprocess 494 → **498/498 PASS**, 기존 SigmaClip 5케이스 무회귀.

## 2. 정의 인용 + 구현과의 차이

### XPE-ALG-001 §9.8.2.1 (인용)

> **반복 1~max_iter:**
> $$\mu = \frac{1}{|S|} \sum_{k \in S} F_k(x,y)$$
> $$\sigma = \sqrt{\frac{1}{|S|} \sum_{k \in S} (F_k(x,y) - \mu)^2}$$
> **클리핑:** $\text{reject} = \{k \in S : |F_k(x,y) - \mu| > \kappa \cdot \sigma\}$
> 기본값: $\kappa = 3.0$, `max_iter = 5`
> **최소 프레임 수 제약:** $N_{\text{min}} = \max(3,\ \lfloor N/4 \rfloor)$
> 유효 프레임 수 $|S| < N_{\text{min}}$이면 해당 픽셀을 **정적 결함으로 마킹**.

### 차이 (조항별 한 줄)

| 조항 | 구현 | 판정 |
|---|---|---|
| 모집단 σ (`/|S|`) | `stddev_of`: `sqrt(sum_sq / values.size())` (`:124`) | **일치** (7fd3436 에서 교정 완료) |
| 반복 클리핑, 매 반복 평균 갱신 | `sigma_clip_value`: 루프마다 `mean_of(clipped)` 재계산 (`:150-153`) | **일치** |
| κ·σ 기준 제거, 기본 κ=3.0 / max_iter=5 | `config.sigma{3.0}`, `max_iter{5}` (`.hpp:26-27`) | **일치** |
| 클리핑 후 평균 (§9.8.2.2) | `return mean_of(clipped)` (`:166`) | **일치** |
| **`N_min = max(3, ⌊N/4⌋)`, 미달 시 정적 결함 마킹** | 루프 조건이 `clipped.size() > 1u` 뿐. `N_min` 계산 없음, 결함 마킹 경로 없음 | **미구현** |

한 줄 요약: **표준편차·반복·평균 갱신은 문서와 일치하고, `N_min` 하한과 결함 마킹만 구현되지 않았다.**

## 3. 증거 (Evidence)

### 이슈 재현 시뮬레이션 — 현재 구현 (`a26-green.log`)

`{100, 110, 105, 108, 500}`, κ=1.0 로 실제 구현을 통과시킨 결과:

```
353/499 Test #353: SigmaClipConformanceTest.PopulationStddevReproducesTheSpecifiedResult ..........   Passed    0.01 sec
```

이 케이스는 `EXPECT_DOUBLE_EQ(106.5, ...)` 이다. 반복 추적:

| 반복 | S | μ | σ (모집단) | κ·σ | 제거 |
|---|---|---|---|---|---|
| 1 | {100,110,105,108,500} | 184.6 | 157.74 | 157.74 | 500 |
| 2 | {100,110,105,108} | 105.75 | 3.7666 | 3.7666 | 100, 110 |
| 3 | {105,108} | 106.5 | 1.5 | 1.5 | 없음(1.5 는 1.5 초과 아님) → 종료 |

**결과 106.5.** 표본 σ 였다면 반복 2의 σ 가 4.3493 이 되어 110 이 살아남고 #97 이 보고한 **109** 로 수렴한다. 즉 106.5 를 못 박는 것은 값이 아니라 **정의**를 못 박는 것이다.

`MultiOffsetTest.SigmaClip_CustomSigma` 의 기대치 `106±2` 는 106.5(uint16 반올림 106)와 맞으므로 통과한다.

### 실측으로 정정한 내 가정 1건

이 스위트 초안에서 κ=3.0 이면 outlier 만 제거되어 105.75 가 나오리라 적었는데, 실행 결과는 **184.6** 이었다(`a26-run.log:729`):

```
error: Expected equality of these values:
  105.75
  clipTo({100, 110, 105, 108, 500}, 3.0)
    Which is: 184.60000610351562
```

이유는 **단일 outlier 자기 은폐**다. 500 하나가 σ 를 157.74 로 부풀려 3σ 문턱이 473.2 가 되고, `|500 − 184.6| = 315.4` 는 그보다 작아 제거되지 않는다. 첫 반복에서 아무것도 안 빠지면 루프가 즉시 끝나 평균 184.6 이 나온다. 이것이 `SigmaClip_CustomSigma` 가 기본값 대신 σ=1.0 을 쓰는 이유다. 케이스명을 `DefaultKappaIsMaskedByASingleLargeOutlier` 로 바꾸고 실측값을 단언한다.

### 선행 교정 기록 (`a26-prior-fix.txt`)

```
commit 7fd3436a17577fa20411bcdeba41b968e3bb213e
Date:   Tue Aug 18 17:29:53 2026 +0900

    fix(preprocess): SigmaClip 표준편차를 모집단 정의로 수정 — 과도 클리핑 해소
```

### 재실측 (`a26-green.log`, exit=0)

```
100% tests passed, 0 tests failed out of 498
```

기존 SigmaClip 5케이스 전부 통과 — `SigmaClip_RemovesOutlier`, `SigmaClip_NoOutliers_SameAsMean`, `SigmaClip_CustomSigma`, `SigmaClip_PerPixelOutlier`, `SigmaClip_AllIdentical_NoDivision`.

## 4. `N_min` 을 고치지 않은 사유 (leader 판정 요청)

카드는 "구현을 문서 정의에 맞춘다" 이고, 문서는 틀리지 않았다 — 그러니 원칙대로면 구현해야 한다. 그럼에도 이 카드에서 구현하지 않은 이유는 **문서가 요구하는 출력이 현재 API 에 없기 때문**이다.

§9.8.2.1 은 `|S| < N_min` 인 픽셀을 "정적 결함으로 마킹" 하라고 한다. 그런데:

- `xpe_calib_generate_offset(frames, n, integration_ms, temp_c, output_path)` 은 **오프셋 맵 파일 하나**만 쓴다. 결함 마스크를 실어 보낼 채널이 없다.
- §9.8.3 의 참조 Python 구현은 `(cal_map, defect_mask)` **두 개를 반환**한다. 즉 문서는 결함 마스크가 별도 출력이라고 전제한다.
- 마킹을 어디로 내보낼지 — XCal DEFECT 파일 추가 생성? 헤더 config_json 에 좌표? 새 out-parameter? — 는 **공개 계약 결정**이고 api-spec 은 leader 소유다.

값만 슬쩍 바꾸는 선택지(예: `|S| < N_min` 이면 클리핑을 멈춘다)는 문서가 말한 것이 아니다. 문서는 "그 픽셀은 결함" 이라 했지 "덜 클리핑하라" 고 하지 않았다. 그렇게 구현하면 **문서에 맞춘 것이 아니라 새 규칙을 발명**하는 것이다.

그래서 이 카드는 **차이를 실행 가능한 형태로 고정**하는 데서 멈춘다: `KnownDivergence_NMinFloorIsNotEnforced` 케이스가 현재 반환값 106.5 를 기록하고, 그 값이 `{105, 108}` 두 표본에서만 나올 수 있다는 사실로 `|S| = 2 < N_min = 3` 을 관측 가능하게 만든다. 나중에 누군가 `N_min` 을 구현하면 이 케이스가 먼저 깨지면서 계약 변경을 강제한다.

## 5. 미검증 (Gaps)

- **`N_min` 미구현이 실제 촬영 품질에 미치는 영향은 측정하지 않았다.** 소수 표본으로 평균 낸 오프셋 값이 얼마나 부정확한지, 그 픽셀이 최종 영상에서 눈에 띄는지는 이 카드가 다루지 않는다.
- **`generate_offset_values` 를 직접 호출한다.** conformance 스위트는 공개 진입점이 아니라 내부 함수를 쓴다 — uint16 반올림 없이 float 결과를 봐야 정의 차이(106.5 대 109)가 드러나기 때문이다. 공개 경로(`xpe_calib_generate_offset` → XCal 파일)로 같은 값이 나오는지는 별도로 확인하지 않았다.
- **`max_iter` 경계는 검증하지 않았다.** 5회 안에 수렴하지 않는 입력에서 어떻게 끝나는지 다루지 않았다.
- **다른 방법(median / winsor)의 문서 대조는 하지 않았다.** 카드 범위가 SigmaClip 이다. §9.8 의 다른 절이 구현과 어긋나는지는 모른다.
- **#97 이슈 본문을 직접 읽지 않았다.** 카드가 요약한 "expected 106±2, actual 109" 와 "시뮬레이션이 109 를 재현" 을 근거로 삼았고, 109 라는 값은 표본 σ 가정에서 산술로 재구성했다. GitHub 이슈 원문 대조는 하지 않았다.

## 6. 잔여 위험 (Residual risk)

- **`N_min` 미구현은 조용한 종류의 결함이다.** 프레임을 과도하게 버려도 함수는 `XPE_OK` 를 돌려주고 그럴듯한 숫자를 낸다. 어떤 신호도 나오지 않으므로, 이 보고서와 characterization 케이스 말고는 이 사실을 알릴 통로가 없다.
- **단일 outlier 자기 은폐는 기본 설정에서 일어난다.** κ=3.0 기본값으로는 큰 outlier 하나가 스스로를 숨긴다(위 184.6). §9.8.2.1 대로 구현한 결과지만, "기본값으로 outlier 가 제거된다" 고 기대하는 호출자는 틀린다. 문서에도 이 성질은 적혀 있지 않다.
- **characterization 케이스는 양날이다.** 지금은 divergence 를 보이게 하지만, 읽는 사람이 "106.5 가 정답" 으로 오해할 수 있다. 케이스명과 주석에 `KnownDivergence` 를 박아 완화했을 뿐이다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 현황 실측 (등록·통과 여부) | QA-A-26 |
| 이슈 재현 시뮬레이션 재실행 | QA-A-26 |
| §9.8.2.1 인용 + 조항별 차이 | QA-A-26 |
| conformance 스위트 4케이스 + 재실측 | QA-A-26 |
| **`N_min` 하한 + 정적 결함 마킹 구현** (출력 채널 결정 필요) | **신규 카드 필요 — leader 판정** |
| median / winsor 문서 대조 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a26-run.log` | 첫 실행 — 내 κ=3.0 가정이 틀렸음을 드러낸 실패 (exit=8) |
| `a26-green.log` | 정정 후 ci-preprocess 498/498 PASS (exit=0) |
| `a26-prior-fix.txt` | 7fd3436 커밋 기록 + 해당 diff |
