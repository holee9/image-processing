# QA-A-124 (#194) — `dose_levels` 정렬 전제를 생성 시점에 막습니다

Lane A (pre), `dev/preprocess`, 커밋 `b96e6df`.
`BUILD_EXIT=0`, `CTEST_EXIT=0` — **731건 전부 통과**.

## 주장

1. `dose_levels` 가 엄격 오름차순이 아니면 **생성을 거부**합니다(`XPE_ERR_INVALID_INPUT`). 정렬해서 받아주지 않습니다.
2. 가드가 없을 때 뒤집힌 사다리는 **`dose_min=42677, dose_max=14037`** 을 기록하고, 생성·적재 모두 `rc=0` 으로 조용히 통과했습니다.
3. 적재기가 뒤집힌 범위에 대해 하던 말이 **거짓이었습니다** — 갈라서 고쳤습니다.

## (a) 생성 시점 거부

`xpe_calib_generate_gain.cpp` 의 기존 입력 검증 블록(`max_degree` 검사 바로 뒤)에 넣었습니다.

```cpp
for (int32_t i = 1; i < num_levels; ++i) {
    if (!(dose_levels[i] > dose_levels[i - 1])) {
        return XPE_ERR_INVALID_INPUT;
    }
}
```

**정렬해 주지 않는 이유를 주석에 남겼습니다.** 선량 준위는 같은 색인의 게인 맵과 짝을 이룹니다. 호출자가 순서를 섞어 보냈다면 **짝이 이미 어긋났을 가능성**이 있고, 한쪽만 조용히 재배열하면 엇갈린 점들로 곡선을 맞춰 **멀쩡해 보이는 파일**이 나옵니다.

`>` 를 쓴 것은 **같은 선량이 두 번** 나오는 것도 막기 위해서입니다 — 한 x 에 y 가 둘이면 적합의 입력이 아닙니다.

**적용 범위**: `dose_levels` 를 받는 진입점은 `xpe_calib_generate_gain_polynomial` 하나입니다(검색: `xpe_calib_generate_gain.cpp` 안의 `dose_levels[` 사용처 전부 — `:528`, `:529`, `:548`, `:562`, `:690` 이며 모두 이 함수 안).

## (b) 반증 — 두 수

### 1. 거부가 실제로 난다

```
[       OK ] GainPolyDoseRangeTest.DescendingDoseLevelsAreRefused (4 ms)
[       OK ] GainPolyDoseRangeTest.OneOutOfOrderLevelInTheMiddleIsRefused (4 ms)
[       OK ] GainPolyDoseRangeTest.DuplicateDoseLevelsAreRefused (4 ms)
[       OK ] GainPolyDoseRangeTest.AscendingDoseLevelsAreStillAccepted (15 ms)
```

마지막이 **대조군**입니다. 없으면 "전부 거부하는 가드" 도 앞의 셋을 통과시킵니다.

중간 한 쌍만 바꾼 경우(`swap(doses[1], doses[2])`, **양 끝은 그대로**)도 거부됩니다 — 끝점만 보는 검사가 아니라는 확인입니다.

**가드를 껐을 때**(`num_levels < 0 &&` 로 약화, 컴파일러가 접지 못하는 형태): **`BUILD_EXIT=0`** 으로 빌드는 통과하고 위 3건이 빨강. 되돌리면 12/12 초록.

### 2. 가드가 막는 것의 크기 — 기록되는 범위

가드를 끄고 거꾸로 된 사다리(42677 → 14037)로 생성한 실측입니다.

```
[a124] generate rc=0
[a124] recorded: "dose_min":42677.000000,"dose_max":14037.000000,"fit_r_squar
[a124] load rc=0, alerts=1
```

**구간이 뒤집혀 기록되고, 생성도 적재도 오류를 내지 않습니다.**

## 예상 못 한 발견 — 알림이 거짓말을 하고 있었습니다

위 실측의 세 번째 줄에서 알림 1건이 떴는데, 내용이 이랬습니다:

```
[a124] alert[0] sev=1: gain polynomial loaded without a dose range: this file was generated before the range fiel...
```

**"범위 항목 이전에 생성된 파일" — 거짓입니다.** 범위는 파일에 **있고**, 뒤집혔을 뿐입니다.

QA-A-123 의 적재기 판정이 `present && (hi > lo)` 를 한 덩어리(`have`)로 묶고 있어, **"없음" 과 "뒤집힘" 이 같은 가지로 떨어졌습니다.** 클램프를 적용하지 않는 결과는 맞지만, **파일을 잘못 설명합니다.**

갈랐습니다:

| 상태 | 알림 |
|---|---|
| 키 자체가 없음 | (기존 문구 그대로) "범위 항목 이전에 생성됨 … 재생성하면 클램프가 켜진다" |
| 키는 있고 구간이 뒤집힘/비어 있음 | **신규** "inverted dose range [42677.0, 14037.0] … 생성 시 선량 준위가 오름차순이 아니었으므로 계수가 **엇갈린 게인 맵에 맞춰졌을 수** 있다 — 재생성하라" |

시험으로 고정했습니다 — `AnInvertedRangeIsReportedAsInvertedNotAsMissing` 은 `"inverted dose range"` 가 뜨는 것과 **`"before the range field existed"` 가 뜨지 않는 것**을 둘 다 단언합니다.

그 시험이 쓰는 뒤집힌 파일은 **바이트 편집이 아니라** 수정한 `config_json` 을 `write_xcal_file()` 로 다시 써서 만듭니다(QA-A-123 에서 배운 것 — 손으로 고친 파일은 옛 파일이 아니라 손상된 파일입니다).

**범위 밖 작업인지**: 생성 가드가 이 파일을 앞으로 못 만들게 하지만, **가드 이전에 만들어진 파일은 이미 있을 수 있습니다.** 그때 읽는 사람이 엉뚱한 곳을 보게 되는 것이라 같은 카드에서 고쳤습니다.

## baseline 귀속

- 빌드: `cmake --build build/ci-preprocess --config RelWithDebInfo` (타깃 미지정) → `BUILD_EXIT=0`
- 전체: `ctest --test-dir build/ci-preprocess -C RelWithDebInfo` → `100% tests passed, 0 tests failed out of 731`, `CTEST_EXIT=0`
- `GainPolyDoseRangeTest` 12/12 통과 (QA-A-123 의 7건 + 이번 5건)
- 기록되는 범위 값은 가드를 끈 빌드에서 임시 프로브가 찍은 stdout 입니다. 프로브는 **커밋 전에 삭제**했고 가드는 복원했습니다.

## 미검증

- **셔플 시드를 돌리지 않았습니다**(기본 순서 전체만 초록) — QA-A-123 과 같습니다.
- **`xpe_calib_generate_gain`(FUNC-026, 비다항식)** 은 `dose_levels` 를 받지 않아 손대지 않았습니다. 다른 입력의 순서 전제는 보지 않았습니다.
- 뒤집힌 범위를 가진 **실재 파일이 있는지** 확인하지 않았습니다(`tests/test_data` 도 확인 안 함).
- QA-A-123 의 미검증 다섯은 그대로입니다: 화소별 계수 분산 · `cyan_test` 전 구간 · 3072² 클램프 비용 · 범위 아래쪽 별도 시험 · 셔플 시드.

## 잔여 위험

- `num_levels` 가 3 이상인 것은 앞선 검사가 보장하므로 루프는 최소 2회 돕니다. `num_levels` 가 음수면 앞에서 걸립니다.
- **부동소수 비교**입니다. 준위가 극히 가까우면(`1e-15` 차) 통과하지만, 그 경우 적합 자체가 특이행렬로 떨어집니다(`fit_polynomial_ls` 의 `1e-12` 피벗 검사).
- **3072² 클램프 비용**은 `#179` 에서 성능을 다시 볼 때 **클램프가 추가된 뒤의 수**라는 점을 기억해야 합니다. 이번에 재지 않았습니다.
