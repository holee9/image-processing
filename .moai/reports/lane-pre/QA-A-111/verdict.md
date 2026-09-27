# QA-A-111 (#186) — 비선형 보정 2단계: 적재와 파이프라인 적용

커밋 `e8e1996`. SPEC 정정 `c293ad9` 를 반영했다.

## 1. 주장

1. **생성 함수를 정정안에 맞췄다**: 상단 항등 고정 폐기, 최고 측정점 위는 마지막 측정 구간의 기울기로 연장, 연장 시작 인덱스를 파일에 기록.
2. **정확도 단언 범위를 넓혔다**: 0.3% 를 **측정 구간 전체**에서 단언한다(A-110 은 항등 고정 때문에 두 번째로 높은 측정점까지만 가능했다). 연장 구간은 단조성만 단언한다.
3. **적재**: `xpe_calib_load_nonlin_lut` 이 항목 수·단조성·연장 기록을 검사하고 어긋나면 `XPE_ERR_INVALID_CALIB_DATA`.
4. **적용**: 파이프라인 3단계가 LUT 를 실제로 적용하고, **적용했을 때만** `XPE_FLAG_NONLINEARITY_CORRECTED` 를 켠다. `panel.linear = true` 면 건너뛰고 플래그도 켜지 않는다. `panel.linear = false` 인데 LUT 가 없으면 알림과 함께 `XPE_ERR_CALIB_NOT_LOADED` — 조용히 넘어가지 않는다.
5. **속도**: 3072² 에서 **3.18 ms**(5회 최소). 파이프라인 예산에서 무시할 수준이다.
6. **반증 작동**: 적용을 항등으로 바꾸면 잔차 시험과 플래그 시험이 빨강, 되돌리면 초록.
7. 전처리 ctest **711건 전부 통과**(`BUILD_EXIT=0`, `CTEST_EXIT=0`), 헤더 `@param` 검사 0건.

## 2. 증거

### 2.1 생성 함수 정정 (카드 1항)

| | A-110 (옛 요구) | A-111 (정정 후) |
|---|---|---|
| 상단 매듭 | `(adc_max, adc_max)` 를 매듭으로 추가 | **없음** |
| 최고 측정점 위 | 항등 끝점까지 3차 보간 | 마지막 측정 구간의 **할선 기울기**로 직선 연장 |
| 상한 처리 | `adc_max` 로 고정 | uint16 포화(65535) |
| 파일 기록 | — | `xcal_nonlin_extension_start` |

**연장 기울기를 할선으로 택한 이유**: 인계점에서 값이 이어지고, 증가하는 두 매듭의 할선은 항상 양수라 단조성을 깰 수 없다. 매듭의 Fritsch-Carlson 접선을 쓰면 같은 연속성을 얻지만, 측정이 없는 구간에 **접선 제한기의 판단이 새어 들어간다**.

**바뀐 기대값과 옛 기대가 틀린 이유**: A-110 의 `TheIdentityEndpointDisagreesWithTheFitAndTheTopIntervalCarriesIt` 은 최상단 구간 오차가 안쪽보다 크다고 단언했다. 그 단언은 **코드에 대해서는 옳았고 요구에 대해서는 틀렸다** — 정정이 항등 고정을 폐기했기 때문이다("검출기의 full scale 이 이상 직선과 같아야 할 물리적 이유가 없습니다"). 새 시험 `AboveTheMeasuredRangeTheTableIsExtendedAndSaysSo` 는 정정된 요구가 실제로 약속하는 것만 단언한다 — 인계점의 연속성, 끝까지의 단조성, 연장 경계의 기록, 그리고 상단이 더 이상 full scale 에 고정되지 않았다는 사실. 마찬가지로 `BoundaryConditionsAreIdentityAtBothEnds` 는 `TheDarkEndIsPinnedToZero` 로 좁혔다.

### 2.2 적재 (카드 2항)

`xpe_calib_load_gain` 과 같은 골격(읽기·검증 → 내용 검사 → 뮤텍스 아래 커밋). 적재 시점에 검사하는 이유는 보정이 프레임마다 도는 반면 적재는 한 번이기 때문이다 — 65536 항목 검사를 초당 30회 치를 이유가 없다.

| 검사 | 근거 | 위반 시 |
|---|---|---|
| 항목 수 4096 또는 65536 | 6a "LUT size: 4096 entries ... or 65536" | `XPE_ERR_INVALID_CALIB_DATA` |
| `LUT[i] <= LUT[i+1]` | 6a "Monotonicity check ... (enforced; non-monotone LUT = `XPE_ERR_INVALID_CALIB_DATA`)" | 같음 |
| 연장 경계가 표 안 | 정정안이 연장 구간을 파일에 기록하라고 함 | 같음 |

**대칭되는 `xpe_calib_unload_nonlin_lut` 을 추가했다.** 시험 편의가 아니라 실제 필요 때문이다 — 검출기 프로필을 바꿀 때 이전 패널의 표가 새 검출기 프레임에 계속 적용되는 것을 막을 수단이 달리 없었다(프로세스 재시작뿐).

### 2.3 적용 (카드 3항)

```
[정상]           LUT 적용 → 플래그 ON
panel.linear=true  건너뜀 → 플래그 OFF, 화소 불변
panel.linear=false + LUT 없음 → XPE_ERR_CALIB_NOT_LOADED + ERROR 알림
설정 없음 + LUT 없음 → 무동작, 플래그 OFF (기존 REQ-P1A-013 동작 유지)
```

16비트 프레임은 4096 항목 표의 범위를 넘을 수 있다. 마지막 항목으로 클램프해 **표 자신의 연장을 이어가고**, 범위 밖을 읽지 않는다. 그 구간은 이미 정확도를 주장하지 않는 구역으로 표시돼 있다.

**파이프라인 변경 하나**: 3단계가 설정 JSON을 받지 못하고 `nullptr` 을 넘기고 있었다. `panel.linear` 판단이 불가능했으므로 `PipelineConfig` 에 원본 포인터를 실어 전달한다(빌린 포인터 — 공개 진입점이 호출 내내 문자열을 살려 둔다).

### 2.4 시험 8건 (카드 4항)

기대값은 **닫힌 형태로 독립 계산**한다: 검출기를 `raw = adc_max·(D/D_max)^γ` 로 모사하고 보정 목표를 `ideal = G_nominal·D` 로 이 파일 자신의 산술로 구한다. 생성기·적재기·조회 어느 것이 틀려도 관측값만 움직인다.

| 시험 | 확인하는 것 |
|---|---|
| `CorrectedPixelsFollowTheIdealLinearResponse` | 보정 뒤 잔차 ≤ 0.3% FS |
| `TheUncorrectedFrameIsNowhereNearTheIdealLine` | **대조군** — 보정 전엔 허용 오차의 10배를 넘게 빗나감(관측 331 ADU 대 허용 12.3 ADU). 이게 없으면 "이미 선형인 프레임"을 통과시키는 시험이 된다 |
| `TheFlagFollowsWhetherPixelsActuallyChanged` | 양쪽 방향 — LUT 없으면 OFF, 있으면 ON 이고 화소가 실제로 이동 |
| `ALinearPanelSkipsTheStageAndLeavesThePixelsAlone` | 건너뛴 단계가 보정했다고 주장하지 않음 |
| `ANonLinearPanelWithoutALutIsAnError` | 조용한 통과 없음 + 알림 문구 |
| `ANonMonotoneOrWrongSizedTableIsRefusedAtLoad` | 손으로 쓴 하강 표, 2048 항목 표 |
| `NullPathIsRejected` | 인자 검사 |
| `TheCorrectionRunsAfterOffsetAndBeforeGain` | **순서를 결과로** 보임 — 같은 프레임을 오프셋 켜고/끄고 두 번 돌려 출력이 다름. 비선형이 오프셋보다 먼저였다면 두 실행이 LUT 에 같은 값을 넣어 출력이 같았을 것 |

플래그는 내부 3인자 함수가 아니라 **파이프라인 메타데이터**에서 읽는다. 내부 함수는 DLL 로 내보내지 않으며, 시험이 자기 사본을 링크하면 자기 `g_calib` 까지 갖게 돼 어떤 파이프라인도 읽지 않는 두 번째 교정 저장소를 시험하게 된다.

### 2.5 반증 (카드 4항)

| 변형 | 결과 |
|---|---|
| 조회를 항등으로 교체(`px[i] = raw`) | `CorrectedPixelsFollowTheIdealLinearResponse` + `TheFlagFollowsWhetherPixelsActuallyChanged` 빨강 (`RUN_EXIT=1`) |
| 되돌림 | 8건 초록 |

`BUILD_EXIT=0`, warning 0 — 낡은 바이너리가 아니다. (첫 시도에서 `(void)lut` 만 남기자 `last` 미사용 경고가 `/WX` 로 오류가 됐다. 값을 쓰지 않는 대신 **항등을 쓰도록** 약화해 실제 빌드를 통과시켰다.)

### 2.6 속도 (카드 5항)

```
[a111-speed] rep 1: 3.25 / rep 2: 3.19 / rep 3: 3.58 / rep 4: 3.25 / rep 5: 3.18 ms
[a111-speed] 3072x3072 nonlinearity LUT, min of 5: 3.18 ms
```

화소당 한 번의 표 조회(4096 항목 = 8 KB, L1 에 상주)와 한 번의 쓰기다. 파이프라인 전체 예산(SRS-CALIB-PERF-001 의 500 ms)에서 **0.6%** 이고, QA-A-105 가 잰 교정 적재(130–137 ms)나 결함 보정(18.6 ms)에 비하면 작다. #179 의 성능 게이트는 런타임 결함 검출기를 재므로 이 단계의 영향을 받지 않는다.

측정용 프로브 시험은 커밋 전에 삭제했다.

## 3. 기준 귀속
- 모든 수치는 이 워크트리 `build/ci-preprocess` RelWithDebInfo 빌드, i7-12700 에서 위 로그 원문으로 관측했다.
- 전체: `CTEST_EXIT=0`, 711건 통과. `python tools/docs/check_header_docs.py` → 0 findings.

## 4. 미검증
- **실제 검출기 데이터로 재지 않았다.** γ 는 모사값이다.
- 65536 항목 표의 적용은 시험하지 않았다(적재 검사만). 16비트 프레임에서 클램프가 아니라 실제 65536 조회가 도는 경로다.
- `dark_reference` 를 준 생성 경로는 여전히 실측 암전류 프레임으로 시험하지 않았다.
- 파이프라인 전체를 3072² 로 끝까지 돌려 총 시간을 재지는 않았다(이 단계만 쟀다).
- 요구 6b(다항식)·6c(선택 로직), `panel.nonlinearity_mode` 는 범위 밖이다.
- 다중 스레드에서 적재와 적용이 겹치는 경우는 뮤텍스로 막았지만 경합 시험은 없다.

## 5. 잔여 위험
- `panel.linear` 는 평면 키로 읽는다(`"panel.linear"` 문자열 그대로). 설정을 중첩 객체(`{"panel":{"linear":true}}`)로 쓰는 호출자는 **키를 찾지 못해 건너뛰기가 동작하지 않는다** — 기존 JSON 헬퍼가 중첩으로 내려가지 않기 때문이다. 이 파일의 다른 키(`bypass*`)도 같은 규약이라 일관되지만, 클라이언트가 어느 형태로 쓰는지는 확인하지 않았다.
- 알림은 프레임마다 쌓인다. `panel.linear=false` 이고 LUT 가 없는 상태로 연속 촬영하면 알림 큐가 채워진다 — 큐 자체에 상한이 있는지는 이 카드에서 확인하지 않았다.
