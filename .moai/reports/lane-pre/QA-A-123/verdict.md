# QA-A-123 (#194 1번) — 게인 다항식을 적합 선량 범위 안에서만 평가

Lane A (pre), `dev/preprocess`, 커밋 `efa6fb0`. `origin/main 449e95e` 병합(`3a91b8e`).
`BUILD_EXIT=0`, `CTEST_EXIT=0` — 726건 전부 통과, 9건 건너뜀/비활성.

## 주장

1. 생성기가 버리던 `dose_min`·`dose_max` 를 `config_json` 에 기록합니다.
2. 적용 시 화소값을 그 구간으로 자른 뒤 평가하고, 걸린 화소가 있으면 프레임당 알림 1건에 건수를 실어 보냅니다.
3. 범위 항목이 없는 옛 파일은 거부하지 않고 적재하되 경고 1건을 띄웁니다. 클램프는 적용되지 않습니다.
4. QA-A-122 의 역전이 사라집니다 — 포화 화소가 `D_max` 화소보다 어둡지 않습니다.

## (a) 기록의 형태

`xpe_calib_generate_gain.cpp` 의 `config_json` 에 두 항목을 추가했습니다. **기존 항목들과 같은 자리·같은 문법**이고, 값은 `:507-508` 이 이미 계산해 단조성 검사에 쓰던 그 두 수입니다 — 새로 만든 값이 아닙니다.

```
"actual_dose_levels":5,"dose_min":14037.000000,"dose_max":42677.000000,"fit_r_squared":...
```

버퍼는 `char meta[512]` → `[640]`. 두 항목이 들어갈 자리를 확보한 것입니다.

적재기는 `xpe_calib_load_gain.cpp:124` 가 이미 `config_json` 을 문자열로 되돌리고 있어 **새 경로를 만들지 않았습니다.** 같은 블록에서 `xpe_json_get_double()` 로 읽습니다.

**부재 판정은 텍스트 대조가 아닙니다.** 기본값을 달리해 두 번 물어, 두 답이 같을 때만 "있다" 로 봅니다(`-1.0` / `-2.0`). 키가 실제로 있으면 두 번 모두 같은 값이 나옵니다.

## (b) 두 경로 — 시험 출력

```
[==========] Running 7 tests from 1 test suite.
[       OK ] GainPolyDoseRangeTest.TheGeneratedFileCarriesTheFittedDoseRange (14 ms)
[       OK ] GainPolyDoseRangeTest.ASaturatedPixelReceivesTheDMaxGainInsteadOfTheExtrapolatedOne (15 ms)
[       OK ] GainPolyDoseRangeTest.ASaturatedPixelIsNotDarkerThanADMaxPixel (13 ms)
[       OK ] GainPolyDoseRangeTest.ClampingRaisesExactlyOneAlertCarryingTheCount (15 ms)
[       OK ] GainPolyDoseRangeTest.AFrameInsideTheRangeRaisesNoClampAlert (18 ms)
[       OK ] GainPolyDoseRangeTest.AFileWithoutARangeStillLoadsAndSaysWhy (22 ms)
[       OK ] GainPolyDoseRangeTest.AFileWithoutARangeIsNotClamped (17 ms)
[  PASSED  ] 7 tests.
```

| 경로 | 고정한 것 |
|---|---|
| 범위 있음 | 생성 파일에 `"dose_min":14037` · `"dose_max":42677` 가 들어간다 |
| 범위 있음 | 포화 화소가 `D_max` 게인을 받는다 (3.8955 → 1.3999) |
| 범위 있음 | 포화 화소가 `D_max` 화소보다 밝다 (역전 해소) |
| 범위 있음 | 알림은 프레임당 **1건**, 본문에 `64 pixel(s)` |
| 범위 있음 | 범위 안 프레임은 **알림 없음** |
| 범위 없음 | **적재 성공** + `"without a dose range"` / `"before the range field existed"` 경고 |
| 범위 없음 | 클램프 안 됨 — 포화 화소가 여전히 3.8955 를 받고, 클램프 알림은 없다 |

### 옛 파일을 만드는 방법을 바꿨습니다

처음에는 생성 파일의 바이트에서 키 이름만 고쳐 "범위 없는 파일" 을 만들려 했습니다. **적재기가 `XPE_ERR_CONFIG_INVALID`(-4) 로 거부했습니다** — 파일이 SHA-256 으로 덮여 있어 손으로 고친 것은 옛 파일이 아니라 **손상된 파일**입니다.

그래서 **같은 계수를** `write_xcal_file()` 로 `config_json` 없이 다시 써서 진짜 유효한 옛 형식 파일을 만들었습니다. 계수가 그대로이므로 **3.8955 가 이 경로의 기대값으로 그대로 성립합니다.**

## (c) 클램프

`gain_correct.cpp` 의 Horner 루프 **바로 앞**입니다. 역수·AVX2 적용 경로는 한 줄도 건드리지 않았습니다.

```cpp
float x = static_cast<float>(src[i]);
if (poly_has_range) {
    if (x < (float)poly_dose_min) { x = (float)poly_dose_min; ++clamped_count; }
    else if (x > (float)poly_dose_max) { x = (float)poly_dose_max; ++clamped_count; }
}
```

알림은 루프가 끝난 뒤 **한 번**입니다. 화소마다 밀면 3072² 에서 수백만 줄이 되어 큐 자체가 못 쓰게 됩니다.

**전체 거부를 택하지 않은 판단을 주석에 남겼습니다** — 포화 화소 몇 개로 프레임 전체를 버리는 쪽이 더 무거운 실패입니다.

## (d) 반증

클램프 조건을 런타임에 거짓이 되게 약화(`&& (n == 0)`)했습니다. **`/WX` 때문에 `if (false && ...)` 는 빌드가 깨져** 반증이 안 되므로, 컴파일러가 접지 못하는 형태를 썼습니다.

- **`BUILD_EXIT=0`** — 빌드는 통과했습니다. 낡은 바이너리가 아닙니다
- **3건 빨강**: `ASaturatedPixelReceivesTheDMaxGainInsteadOfTheExtrapolatedOne`, `ASaturatedPixelIsNotDarkerThanADMaxPixel`, `ClampingRaisesExactlyOneAlertCarryingTheCount`
- 되돌리면 7/7 초록

**역전 시험이 클램프의 되풀이가 아닙니다.** `EXPECT_GT(at_sat, at_max)` 는 클램프를 언급하지 않고 "밝게 들어온 것이 밝게 나온다" 만 단언합니다. 클램프를 빼면 그 단언이 깨집니다.

## baseline 귀속

- 빌드: `cmake --build build/ci-preprocess --config RelWithDebInfo` (타깃 미지정) → `BUILD_EXIT=0`
- 전체: `ctest --test-dir build/ci-preprocess -C RelWithDebInfo` → `100% tests passed, 0 tests failed out of 726`, `CTEST_EXIT=0`
- 3.8955 · 1.3999 · 16823 · 30485 은 QA-A-122 프로브의 실측값이고, 이번 시험이 같은 사다리·같은 차수로 재현합니다.

## 미검증

- **셔플 시드를 돌리지 않았습니다.** 기본 순서 ctest 전체만 초록입니다.
- **화소별 계수 분산은 여전히 안 봤습니다** — 시험 자료는 모든 화소가 같은 곡선입니다. 클램프는 화소마다 독립이라 분산이 있어도 동작은 같지만, 실제 검출기에서 몇 %가 걸리는지는 모릅니다.
- **실제 `cyan_test` 자료로 끝까지 돌리지 않았습니다.**
- 3072² 에서 클램프가 더하는 비용을 재지 않았습니다(화소당 비교 2회).
- 범위 아래쪽(`dose_min` 미만) 클램프는 **같은 코드가 처리하지만 별도 시험이 없습니다.** 위쪽만 고정했습니다.

## 잔여 위험

- **알림이 프레임마다 뜹니다.** 포화가 상시인 촬영에서는 매 프레임 1건이 쌓입니다. 억제 정책은 이 카드 범위 밖입니다.
- `dose_min`/`dose_max` 는 `dose_levels[0]`·`[n-1]` 을 그대로 씁니다 — 호출자가 **정렬되지 않은** 준위를 넘기면 기록된 범위가 실제 최소·최대와 다를 수 있습니다. 단조성 검사가 쓰던 값과 같게 맞춘 것이라 새로 생긴 위험은 아니지만, 그 전제가 어디에도 강제돼 있지 않습니다.
- `#194` 2번(단위)은 손대지 않았습니다.
