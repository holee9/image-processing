# QA-A-132 (#186) — 준위 수와 `N ≥ 10` 의 출처

Lane A (pre), `dev/preprocess`, `origin/main a9174e3`. **읽기 전용 — 코드 변경 없음.**
빌드·ctest **안 돌렸습니다**.

## 한 줄 답

**(a) 최대 6단계입니다 — 제 QA-A-131 보고(5개)보다 하나 더 있었습니다.**
**(b) `N ≥ 10` 은 인용 없이 적힌 수이고, 같은 SRS 안의 다른 요구와 어긋납니다.**

그래서 차단 요인이 **"자료가 요구를 못 채운다" 가 아니라 "요구 쪽이 흔들린다"** 로 바뀝니다.

## (a) 데이터셋 전부의 선량 준위

`.raw` 에 손이 닿지 않으므로 **README·이름 규칙**으로 셌습니다 — `cyan_test` 의 5개를 알아낸 그 방법입니다.

| 데이터셋 | 선량 준위 | 근거 |
|---|---|---|
| **`CalData_6`** | **6** | `README.md:26` *"**6단계 선량 레벨**"*, `:49-54` 가 `bright01.raw`~`bright06.raw` 를 **"선량 레벨 1"~"선량 레벨 6"** 으로 한 줄씩 기술 |
| `cyan_test` | **5** | `README.md:29` *"5개 선량 레벨"*, `:109-111` 의 `CalSet_14037/17285/20985…raw` |
| `Grid_abnormal` | **0** | 용도가 *"MC 대 Blue BPM 알고리즘 성능 비교 및 그리드 아티팩트 보정 검증"*(`:6`). 선량·dose·레벨 검색 0건 |
| `calibration_cases` | **0** | *"local test-data staging area"*(`:3`). 선량 관련 서술 없음. `.raw` 는 *"must be copied from local source media"* |

**대조군**: `Grid_abnormal/README.md` 는 563행, `calibration_cases/README.md` 는 126행입니다 — **빈 파일이라 0건이 나온 것이 아닙니다.**

### QA-A-131 정정: 최대는 5가 아니라 **6** 입니다

QA-A-131 에서 *"`cyan_test` 는 5개 준위"* 만 보고 `CalData_6` 을 확인하지 않았습니다. **`CalData_6` 이 6단계이고, 이름 자체가 그 수입니다.** 미검증으로 적어 둔 항목이 실제로 결론을 움직였습니다.

**그리고 `CalData_6/README.md` 는 LUT 생성 절차를 직접 적습니다**(`:164-178`):

> Step 1: 다양한 선량 평면에서 신호 수집 … 각 dose 에 대해 flat-field 평균 신호: `S_meas[i]`
> `S_ideal[i] = gain × dose[i]`
> … 모든 입력값에서 보간 오차 < 0.3% ADU

**SRS 6a 절차와 같은 내용이고, 이 데이터셋이 그 용도로 만들어졌습니다**(`:6` *"다중 선량 레벨 게인 맵 생성 및 **비선형성 LUT 검증**"*).

**즉 `N ≥ 10` 만 아니면 `CalData_6` 으로 6a 를 검증할 수 있게 설계돼 있습니다.**

## (b) `N ≥ 10` 의 출처 — **없습니다. 그리고 같은 SRS 안에서 어긋납니다**

### 인용이 없습니다

`N ≥ 10` 은 SRS 에서 두 번 나옵니다:

- `:67` 6a LUT — *"Acquire flat-field images at **N ≥ 10** dose levels spanning 5% to 95% ADC full scale"*
- `:103` 6b 다항식 — *"Use the same **N ≥ 10** dose-level flat-field dataset as LUT method"*

**둘 다 참고문헌·근거 문장이 붙어 있지 않습니다.** 검색 범위: `docs/` 전체 + `.moai/specs/` 전체의 `N ≥ 10`·`N >= 10`·`10 dose`·`10개 선량`·`10단계`.

**대조군 — 근거가 붙은 수는 실제로 그렇게 적혀 있습니다.** 같은 SRS 의 `FUNC-031`(`:221`)은 8을 말하며 출처를 답니다:

> *"Industry standards (**Schmidgunst 2007, Rayence DR panels**) recommend **8 calibration points** as the optimal balance between accuracy and acquisition time"*
> *"(7) Default mode shall be `XPE_CALIB_MULTI_POINT_8` (**industry standard per Rayence, Schmidgunst 2007**)"*

**8에는 출처가 있고 10에는 없습니다.** 검색이 헛돈 것이 아닙니다.

### 저장소가 이미 같은 지적을 했습니다

`docs/calibration/PRIOR-ART-BPM-ALGORITHM.md:275`:

> **현재 상태:** SRS-CALIB-FUNC-006에서 **"N ≥ 10"이라고만 명시**
>
> **권고:** 최소 요구 **5~10 프레임**(신호 안정화) / 표준 권고 15~20 / 우수 품질 25~30

*"라고만 명시"* 가 **인용 없음을 지적하는 문장**입니다. 그리고 그 권고의 최소가 **5~10** 이라 `CalData_6` 의 6이 그 안에 듭니다.

> **주의 — 이 권고는 다른 것을 셉니다.** `:5.2` 제목이 *"다중 게인 보정 **프레임 개수**"* 이고 본문도 *"프레임 개수는 통계적 안정성에 직접 영향"* 입니다. **한 준위에서 몇 장 찍느냐**이지 **몇 개 준위**가 아닐 수 있습니다. 두 수는 다릅니다(`cyan_test` 는 단일 준위 20장 + 5준위 각 1장 구조입니다). **이 구별을 확정하지 못했습니다.**

### 같은 SRS 안에서 10이 상한으로도 쓰입니다

`FUNC-031`(`:221`)·`FUNC-032`(`:239`):

| 모드 | 준위 수 | 다항식 차수 |
|---|---|---|
| `SINGLE_POINT` | 1 | — |
| `DUAL_POINT` | 2 | 선형 보간 |
| `MULTI_POINT_5` | 5 | ≤ 2 |
| **`MULTI_POINT_8`** | **8** | ≤ 3 | ← **기본값** |
| `MULTI_POINT_10` | **10** | ≤ 4 | ← *"maximum allowed"* |

`FUNC-032 (4)`: *"**Hard Cap at 10 Points**: Maximum calibration points shall be 10 regardless of user input"*

**즉 같은 SRS 안에서 10이 6a 에서는 하한(`N ≥ 10`)이고 FUNC-031/032 에서는 상한(hard cap)입니다.**

그리고 **기본 모드가 `MULTI_POINT_8`(8준위)** 입니다. 6a 를 글자대로 읽으면 **기본 모드로 취득한 자료로는 LUT 를 만들 수 없습니다** — 8 < 10.

### 다항식 차수 ≤ 5 와의 관계

카드가 물은 부분입니다. **두 수가 서로 다른 절에서 다르게 적힙니다:**

| 위치 | 차수 상한 |
|---|---|
| `FUNC-006`(`:55`) | *"polynomial degree **≤ 5**"* |
| `FUNC-006-EXT 6b`(`:97`) | *"**4th-degree** global polynomial fit"* |
| `FUNC-031` `MULTI_POINT_10` | *"polynomial degree **≤ 4**"* |
| 구현 `xpe_calib_generate_gain_polynomial` | `max_degree` **1~4** (QA-A-122 에서 4차가 준위 5개로 `rc=-1`) |

**`≤ 5` 는 FUNC-006 본체에만 있고 EXT·FUNC-031·구현 셋 다 4입니다.**

최소제곱 적합은 차수 `d` 에 대해 **`d+1` 개 이상의 점**이 필요합니다. 차수 4면 **5점**, 차수 5면 **6점**입니다. **`N ≥ 10` 은 그 최소의 두 배이고, 왜 두 배인지가 적혀 있지 않습니다.**

## 판정 — 차단 요인의 성격이 바뀝니다

QA-A-131 에서 *"준위 5개 < N ≥ 10 이라 막힌다"* 로 적었습니다. **지금 보면 그 문장의 양쪽이 다 흔들립니다:**

| | QA-A-131 | 지금 |
|---|---|---|
| 자료 | 5준위 | **6준위**(`CalData_6`) |
| 요구 | `N ≥ 10`(확정된 요구로 취급) | **인용 없는 수**, 같은 SRS 의 hard cap 10·기본 8과 충돌 |

**그래서 `#186` 의 차단 요인은 "자료가 모자라다" 가 아니라 "요구의 근거가 불분명하다" 입니다.**

**다만 저는 SRS 를 고치자고 하지 않습니다**(카드 지시이자 `#180` 의 교훈 — 문헌으로 검증하고 맞으면 구현 방법을 찾는 것이 순서입니다). 제가 보고하는 것은 **`N ≥ 10` 이 인용 없는 수이고 같은 문서 안에서 모순된다** 는 사실까지입니다.

**결정에 필요한 것이 무엇인지만 적습니다:**

- 10이 문헌 근거를 가진 수라면 → `CalData_6`(6)로는 부족하고 자료 취득이 선행입니다
- 8처럼 근거를 달 수 있는 수라면 → 그 근거가 6을 허용하는지가 갈림길입니다
- **어느 쪽이든 `#180` 처럼 문헌 확인이 먼저이고, 그건 제 범위 밖입니다**(`docs/` 는 리더 소유, 딥리서치는 사용자 지시 사항)

## 미검증

- **`PRIOR-ART` 권고의 "프레임 개수" 가 준위 수인지 장수인지 확정하지 못했습니다.** 위에 적은 대로 두 수는 다르고, 이게 갈리면 "5~10 권고" 의 의미가 달라집니다.
- **문헌을 찾지 않았습니다.** Schmidgunst 2007 이 10을 말하는지, 다른 표준(IEC 62220 등)이 준위 수를 정하는지 확인하지 않았습니다 — 카드 범위 밖입니다.
- **`CalData_6` 의 6준위가 5–95% full scale 을 덮는지 모릅니다.** README 도 `fixture.json` 도 준위별 ADU 를 적지 않습니다(`cyan_test` 는 파일명에 ADU 가 있어 알 수 있었습니다). **`.raw` 를 열어 평균 신호를 재야 알 수 있고, 이 레인에서는 불가능합니다.**
- **`.raw` 를 열지 않았습니다.** 준위 수는 README 서술과 파일명 규칙에서만 셌습니다.
- ~~`CalData_6/fixture.json` 미확인~~ → **읽었습니다.** `"description": "Multi-step gain calibration: **6 dose levels for nonlinearity LUT** and BPM generation"`, `"min_gain_frames": 6`. 6단계가 재확인되고 **용도가 비선형 LUT 라고 명시**됩니다. 다만 **준위별 ADU 값은 fixture 에도 없습니다** — `srs_refs` 는 `SRS-CALIB-FUNC-022` 하나입니다.
- **다른 저장소·외부 매체의 자료는 모릅니다.** `calibration_cases/README.md:3` 이 *"must be copied from local source media"* 라 적으므로, 더 많은 준위를 가진 자료가 이 저장소 밖에 있을 수 있습니다.
- 빌드·ctest 미실행(코드 변경 없음).

## 잔여 위험

- **6a 의 `N ≥ 10` 과 FUNC-031 의 기본 8이 충돌하는 것은 이 카드 밖에도 영향이 있습니다.** 기본 모드로 취득한 교정 자료로 LUT 를 만들 수 없다는 뜻이고, 교정 절차를 쓸 때 반드시 부딪힙니다.
- **QA-A-131 의 "준위 5개" 는 제가 한 데이터셋만 보고 적은 수였습니다.** 이번에 6으로 정정됐지만, 같은 방식으로 **더 있는지는 저장소 밖을 못 봅니다.**
