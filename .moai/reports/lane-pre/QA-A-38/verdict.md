# QA-A-38 검증 보고서 — SigmaClip N_min 미달 픽셀의 정적 결함 마킹

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 선행: `git merge origin/main` → `8bc2ce3` (fast-forward, 충돌 없음)
- 커밋: `6e7a67a` (RED), `74fee8f` (GREEN)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-38/`
- Refs #138 #97

---

## 0. 원문 인용 (합격 조건 1항)

XPE-ALG-001 §9.8.2.1, 원문 그대로:

> **최소 프레임 수 제약:**
>
> $$N_{\text{min}} = \max(3,\ \lfloor N/4 \rfloor)$$
>
> 유효 프레임 수 $|S| < N_{\text{min}}$이면 해당 픽셀을 정적 결함으로 마킹.

§9.8.3 참조 구현(Python)의 두 출력:

```python
    # 최종 평균
    valid_count = valid.sum(axis=0)
    masked_final = np.where(valid, frames, 0.0)
    cal_map = masked_final.sum(axis=0) / np.maximum(valid_count, 1)

    # 최소 프레임 미만 픽셀 = 정적 결함
    defect_mask = valid_count < min_frames

    return cal_map.astype(np.float32), defect_mask
```

**카드가 물은 "마킹 픽셀의 오프셋 값" 에 대한 답**: 참조 구현은 `cal_map` 을
모든 픽셀에 대해 `max(valid_count, 1)` 로 나누어 계산하고, 결함 픽셀에 0 이나
다른 sentinel 을 대입하지 않는다. 마킹과 평균은 독립된 두 출력이다. 따라서
**마킹은 오프셋 값을 바꾸지 않는다** — 클리핑 결과 평균 그대로다. 이를
테스트로 고정했다(`MarkingDoesNotChangeTheOffsetValue`, 106.5).

§9.8.5 엣지 케이스 표의 관련 행:

> | N < 4 | max_iter=1, min_frames=N으로 강제 (경고 로그) |

이 중 `min_frames = N` 절반만 구현했다. `max_iter = 1` 절반은 클리핑 결과
자체를 바꾸는 알고리즘 변경이라 카드의 "하지 않을 것" 에 걸린다(§4 Gap 3).

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | `|S| < N_min` 픽셀이 정적 결함으로 마킹된다 | PASS |
| C2 | `N_min = max(3, floor(N/4))` — N=8→3, N=16→4, N=40→10 | PASS |
| C3 | 마킹이 오프셋 값을 바꾸지 않는다 (클리핑 평균 그대로) | PASS |
| C4 | 마스크는 기존 결함 맵과 OR 병합된다 — 세운 적 없는 비트를 지우지 않는다 | PASS |
| C5 | 마킹이 0건이면 결함 맵을 만들지 않는다 | PASS |
| C6 | SigmaClip 이 아닌 방식은 아무것도 마킹하지 않는다 | PASS |
| C7 | export 불변 (preprocess 50, common 16) | PASS |
| C8 | ci-preprocess / ci-common 재실측 회귀 없음 | PASS |

---

## 2. 증거 (Evidence)

### RED (`a38-red.log`)

명령: `build\ci-preprocess\bin\xpe_preprocess_tests.exe --gtest_filter=SigmaClipNMinTest.*`

```
[  FAILED  ] SigmaClipNMinTest.PixelBelowTheFloorIsMarkedDefective
[  FAILED  ] SigmaClipNMinTest.NMinFormulaAtThreeFrameCounts
[  FAILED  ] SigmaClipNMinTest.ExistingDefectMapIsOrMergedNotReplaced
[  PASSED  ] 4 tests.
```

통과한 4건은 구현이 깨뜨리면 안 되는 불변식이다(값 불변, 상한 픽셀 미마킹,
기존 비트 보존, Mean 방식 미마킹). RED 는 마킹 자체와 병합 3건.

### GREEN (`a38-green.log`)

```
[  PASSED  ] 10 tests.
```

케이스 구성이 RED 시점과 다르다 — 관측 경계 문제로 재구성했다(§3, §4 Gap 1).

### 재실측

```
ctest --output-on-failure  (build/ci-preprocess)
100% tests passed, 0 tests failed out of 590

ctest --output-on-failure  (build/ci-common)
100% tests passed, 0 tests failed out of 69
```

로그: `a38-ctest-preprocess.log`, `a38-ctest-common.log`.

### export 불변 (`a38-exports.txt`, `a38-export-diff.txt`)

```
          50 number of names      <- xpe_preprocess.dll
          16 number of names      <- xpe_common.dll
```

`diff a37-old-names.txt a38-new-names.txt` → 빈 출력 (exit 0).

### 변경 규모

커밋 1 — RED (`a38-diff-commit1.txt`):

```
 modules/preprocess/CMakeLists.txt                 |   2 +
 modules/preprocess/tests/test_sigma_clip_nmin.cpp | 285 ++++++++++
 2 files changed, 287 insertions(+)
```

커밋 2 — GREEN (`a38-diff-commit2.txt`):

```
 modules/preprocess/src/xpe_calib_generate_offset.cpp        |  74 ++++-
 modules/preprocess/src/xpe_calib_generate_offset_methods.cpp|  70 +++-
 modules/preprocess/src/xpe_calib_generate_offset_methods.hpp|  61 +++-
 modules/preprocess/tests/test_sigma_clip_conformance.cpp    |  39 +--
 modules/preprocess/tests/test_sigma_clip_nmin.cpp           | 235 +++----
 5 files changed, 327 insertions(+), 152 deletions(-)
```

---

## 3. Baseline 귀속 + 전제 정정 2건

- **테스트 총수**: QA-A-37 측정 580 → 590 (+10 = 신규 `SigmaClipNMinTest` 10건).
  기존 케이스 중 이름이 바뀐 것 1건(`KnownDivergence_NMinFloorIsNotEnforced`
  → `NMinMarkDoesNotAlterTheClippedMean`)은 총수에 영향이 없다.
- **빌드 프리셋**: `ci-preprocess`, `ci-common` 동일. `/WX` 유지, 최종 빌드 경고 0건.
- **export baseline**: QA-A-35 의 `a35-dumpbin.log` 50개 이름. A-37 과 동일 파일.

### 전제 정정 (1) — 카드의 "`xpe_calib_generate_offset`(SigmaClip 경로)"

이 공개 진입점은 `parse_offset_generation_config(nullptr, &config)` 를 호출하므로
**항상 Mean 방식을 쓴다**(`xpe_calib_generate_offset.cpp:43`). SigmaClip 은
`generate_offset_to_uint16_buffer(..., config_json)` 로만 선택 가능하고, 이
함수의 호출자는 현재 **테스트뿐**이다(grep 결과 프로덕션 호출 0건).

즉 SigmaClip 경로는 오늘 어떤 출하 진입점에서도 도달하지 않는다. 병합 호출은
두 경로 모두에 심어 두어(공개 진입점에도 빈 마스크로 호출) 앞으로 갈라지지
않게 했지만, 도달성 자체는 이 카드에서 해소하지 않았다(§4 Gap 2).

### 전제 정정 (2) — 병합 함수의 위치

`merge_static_defect_mask` 를 `xpe_calib_generate_offset_methods.cpp` 에 두자
링크가 깨졌다:

```
xpe_calib_generate_offset_methods.cpp.obj : error LNK2001: g_calib
xpe_calib_generate_offset_methods.cpp.obj : error LNK2001: g_calib_mutex
```

이 TU 는 DLL 과 **테스트 바이너리 양쪽에** 컴파일된다(`CMakeLists.txt` 의
`add_executable(xpe_preprocess_tests ... src/xpe_calib_generate_offset_methods.cpp)`).
`g_calib` 은 DLL 내부 심볼이라 테스트 쪽 사본에서 해석되지 않는다. 그래서 둘로 쪼갰다:

- `or_merge_defect_bits` — 순수 비트 OR, methods TU 에 남아 테스트 가능
- `merge_static_defect_mask` — 전역 저장소 접근, DLL 전용 TU 로 이동

이 제약 때문에 RED 시점의 "저장소를 `xpe_calib_save("defect")` 로 되읽는"
케이스 구성이 성립하지 않아 GREEN 에서 재구성했다.

---

## 4. 미검증 (Gaps)

1. **전역 저장소 병합의 실제 동작을 테스트하지 못했다.** 위 링크 제약 때문에
   SigmaClip 을 돌릴 수 있는 테스트 코드에서는 DLL 의 `g_calib` 에 닿을 수 없고,
   `g_calib` 에 닿는 공개 경로(`xpe_calib_generate_offset`)는 SigmaClip 을 쓰지
   않는다. 검증한 것은 (a) 마스크가 올바르게 만들어진다, (b) OR 비트 병합
   함수가 올바르다, (c) 공개 진입점이 결함 맵을 만들지 않는다 — 세 조각이며,
   "SigmaClip 실행이 전역 맵을 실제로 갱신한다" 는 **관측하지 않았다.**
2. **SigmaClip 의 프로덕션 도달성이 없다.** §3 전제 정정 (1) 참조. 이 카드는
   마킹 로직을 넣었을 뿐, 그것이 실행될 출하 경로를 만들지 않았다. 공개
   진입점에 config 인자를 추가하는 것은 ABI 변경이라 카드 범위 밖이다.
3. **§9.8.5 의 `max_iter = 1` 절반 미구현.** `min_frames = N` 클램프만 넣었다.
   `max_iter` 를 바꾸면 클리핑 결과 자체가 달라져 기존 케이스의 기대값이
   움직인다 — 카드의 "κ/MAD 알고리즘 변경 금지" 에 걸린다고 판단했다.
4. **`NMinAppliedAtThreeFrameCounts` 의 생존자 수는 측정으로 정했다.**
   최초 초안은 N=16 에 생존자 6 을 썼고 실행 결과 마킹됐다 — 이상치가 다수면
   평균이 이상치 쪽에 앉아 소수인 1000 들이 오히려 잘려나간다. "상한" 행은
   생존자를 확실한 다수로 바꿔 통과시켰다. 즉 **임의의 목표 |S| 를 만드는
   구성법은 확립하지 않았고**, 검증한 것은 각 N 에서 상/하한 한 점씩이다.
5. **부수 관측(κ=3 자기 은폐)은 손대지 않았다.** A-26 이 기록한
   `DefaultKappaIsMaskedByASingleLargeOutlier` 는 그대로 두었다 — 카드 지시.
6. **결함 마킹 로그는 stderr 로 나간다.** 카드가 spdlog 를 언급했으나 이 모듈은
   spdlog 를 링크하지 않는다(grep 0건). 기존 관례대로 `std::fprintf(stderr, ...)`
   를 썼고, 로그 출력 자체를 테스트로 고정하지는 않았다.

---

## 5. 잔여 위험 (Residual risk)

1. **마킹이 실행될 날, 결함 맵이 갑자기 커질 수 있다.** N_min 은 보수적 기준이
   아니라 클리핑 결과에 직접 의존한다. κ 가 작거나 프레임이 적은 실운영
   설정에서는 상당수 픽셀이 하한 아래로 떨어질 수 있고, 그 전부가 defect 맵에
   OR 로 누적된다 — 한 번 세워진 비트는 이 경로로는 절대 지워지지 않는다.
   상한 가드는 넣지 않았다(임계 발명 금지).
2. **차원 불일치 시 CONFIG_INVALID 로 실패한다.** 기존 결함 맵과 크기가 다르면
   병합을 거절하고 생성 전체가 실패한다. 조용히 리사이즈하는 것보다 낫다고
   판단했지만, 호출자가 이 실패를 "생성 실패" 로 읽을 수는 있다.
3. **`or_merge_defect_bits` 는 크기 불일치를 0 반환으로만 알린다.** 오류 코드가
   아니라 "0건 병합" 이므로, 호출자가 반환값을 무시하면 마스크가 통째로
   무시된 것을 알아채지 못한다. 현재 유일한 호출자
   (`merge_static_defect_mask`)는 크기를 먼저 검사하므로 도달하지 않는다.
4. **RED 근거가 최종 케이스 구성과 다르다.** RED 는 저장소 관측 방식의 7케이스,
   GREEN 은 마스크·순수 OR 관측 방식의 10케이스다. 같은 결함을 서로 다른
   관측면에서 잡은 것이며, "RED 에서 실패한 그 케이스가 GREEN 에서 통과했다"
   는 형태의 증거는 `PixelBelowTheFloorIsMarkedDefective` 한 건뿐이다
   (이름·의미 동일, 관측 경로만 저장소 → 마스크로 변경).
