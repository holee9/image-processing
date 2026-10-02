# QA-A-211b — 211 의 Codex #71 보류 3건 수정 (Refs #233)

기준: dev/preprocess `dc0d9d56`(211 `56a05cc5` → 212 보고서 `994cd53c` → 212b 임시 저장). 증거: `evidence/`. 푸시하지 않음.

## 결론

| # | Codex #71 | 수정 | 근거 |
|---|---|---|---|
| 1 높음 | 뭉친 결함 덩어리 안쪽이 0 으로 채워짐 | 덩어리 화소도 3×3 이 비면 반경 2~16 의 고리로 넓혀 **가장 가까운 정상 화소가 있는 고리의 중앙값**으로 채움. 반경 16 안에 정상 화소가 없으면 입력값을 그대로 두고 프레임당 알림 1건 — 0 은 쓰지 않음 | 아래 §1 |
| 2 높음 | `bypassGain=true` 에서 비닝+분류 거부(D4)가 빠짐 | 비닝 직전, 게인 우회일 때도 "결함 단계로 전달될 저장 목록 > 0 이고 비닝 켜짐" 이면 `CONFIG_INVALID` + 같은 알림 | §2 |
| 3 중간 | 적재·생성 알림이 아직 안 한 보정을 끝난 것처럼 말함 | "fills them from their neighbours" → "marked defective: gain 1.0 is used and they are listed for the defect correction stage". 접두사 5개 유지, 전체 문구를 시험으로 고정 | §3 |

## 1. 덩어리 안쪽 (`defect_correct.cpp`)

**원인 확인**: `median_filter_cluster` 가 3×3 에 정상 화소가 없으면 `0.0f` 를 돌려줬다. 반경 1~3 고리는 단독 결함의 `xpe_interpolate_pixel` 에만 있었다.

**측정 (실행 결과)** `evidence/10_distance_to_valid.txt`, 스크립트 `09_m211b_dist.py`(칩보드 거리: 마스크 화소에서 가장 가까운 정상 화소까지):

| CalData_6, 준위 0 | 마스크 | 3×3 에 정상 화소 없음 | 최대 거리 |
|---|---|---|---|
| 결함 맵만(`BPMap.map`) | 23,505 | 15,685 (66.7%) | 4 |
| 분류만 | 43,873 | 30,757 (70.1%) | 9 |
| 합집합 | 53,103 | 38,519 (72.5%) | 10 |

(준위 4 도 거의 같다: 합집합 76.0%, 최대 10.) 그래서 탐색 반경 상수는 **16** (측정한 10 에 여유). 반경 8 이면 합집합 2,847 화소가 범위 밖이 된다.

**알고리즘**: 3×3 → 비면 반경 2, 3, … 16 의 고리(테두리 화소만) → 처음 정상 화소가 나온 고리의 중앙값. 읽는 것은 마스크가 아닌 화소뿐이라 제자리(in-place) 보장이 유지된다. 끝까지 없으면 `found=false`, 입력값 유지, 프레임당 `XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: N masked pixel(s) have no valid pixel within 16 pixels to fill them from and keep their input value` (경고, 개수는 마스크 화소 수). **단독 결함 경로는 바뀌지 않았다**(그 경로는 4-이웃이 항상 정상이라 고리에 가지 않는다).

**기존 출력이 바뀌는 범위 (측정)** `12_e2e_old.txt`(210d 빌드 DLL, 결함 단계 이전 동작) 대 `13_e2e_new.txt`:

| 입력 | 이전 | 이후 |
|---|---|---|
| CalData_6 `BPMap.map` 만(게인 우회), bright03 | 결함 맵 화소 **15,685 개(66.7%)가 정확히 0** | **0 개**. 비결함 화소의 변화 0 (이전·이후 모두), 출력 sha256 이 다름(`9e1af188…` → `d7f49e96…`) |
| cyan_test 스칼라 게인 + 결함 맵 `BPM.raw`(250) 5개 준위 | 합친 sha256 `56ac0584…` | **같음**(250 개 결함 중 3×3 이 빈 덩어리 없음 — 출력 불변) |
| 기존 시험 | — | 854 건 전부 통과(골든 포함: 기존 시험 어느 것도 "덩어리 안쪽이 0" 을 기대하지 않았다) |

즉 **이전 출력이 0 을 포함하던 것은 실제 결함이었다**: 실데이터 결함 맵(CalData_6)에서 마스크 화소의 2/3 가 0 으로 나갔다 — 211 이 분류 화소를 더하기 전부터. cyan_test 와 기존 시험의 결과는 변하지 않는다.

**CalData_6, 게인 분류 포함**(스칼라 준위 0·4, `13_e2e_new.txt` Part 3): 마스크 53,103 / 48,994 개 중 "3×3 에 정상 화소 없음" 38,519 / 37,245 개 — 정확히 0 으로 쓰인 것 **마스크 전체 0, 안쪽 0**, 비유한 0, 알림 없음(모두 반경 16 안), 안쪽 값이 정상 화소 출력 범위 안인 것 38,519/38,519. **한계**: CalData_6 의 이 준위 영상은 전 화소가 같은 값(350.6 / 2883.2)이라 정상 화소 범위가 한 점이다 — 0 이 아니라는 것과 범위 안이라는 것만 말해 주고, 이웃 값의 구조를 따라간다는 것은 말해 주지 않는다. 그것은 합성 시험(아래)이 가둔다.

### 시험 (`test_defect_fill_interior.cpp`, 11건)

- Codex 재현 그대로(20×20 전부 100, 결함 맵 비어 있음, 게인 맵 중앙 3×3 만 0): 파이프라인 출력 400 화소 전부 50.0(= 100/게인 2), 0 은 0 개
- 결함 맵 덩어리 5×5 동일 사례, 7×7 블록의 제자리/제자리 아님 일치(+값이 정상 이웃 범위 안, 0 아님, 입력(9999)도 아님)
- 가장 가까운 고리가 이긴다: 블록 중앙에서 거리 2 인 정상 점 하나(700)가 먼 배경(100)을 이긴다
- 반경 한계: 36×36 블록의 중앙 4×4(거리 17~18)는 입력값 유지 16 개, 나머지는 채움, 0 은 0 개, 알림 전체 문구 단언
- 모든 마스크 화소에 정상 이웃이 있으면 알림 없음

## 2. `bypassGain=true` + 비닝 (`pipeline.cpp`)

**원인 확인**: 비닝 거부가 `!cfg.bypassGain` 분기 안에만 있었고, 결함 단계는 우회와 무관하게 스냅샷의 저장 목록을 합집합에 넣었다. **수정**: 게인 분기 밖, 비닝 직전에 `bypassGain && !bypassDefect && 저장 목록 > 0 && 비닝 켜짐` 을 검사(목록이 결함 단계로 실제 전달되는 경우만). 게인이 돌면 이전 검사가 그대로 잡는다. 둘 다 우회하면 목록을 아무도 읽지 않으므로 거부하지 않는다(시험으로 고정). 거부 알림 문구는 그대로 `XPE_WARN_GAIN_PIXELS_WITH_BINNING:`.

시험: 분류 1개 + 결함 맵 + 게인 우회 + 비닝 → `CONFIG_INVALID` + 알림; 분류 없음 → 이 규칙으로는 거부 안 됨(대조); 게인·결함 둘 다 우회 + 비닝 → 거부 안 됨.

## 3. 알림 문구 (레인 간 계약 — 새 전체 문구)

- 적재·생성: `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: N of TOTAL pixel(s) (P%) have a gain outside [0.1, 10.0] and are marked defective: gain 1.0 is used and they are listed for the defect correction stage (K in the outermost 64-pixel band; first: I). Limit: 5.0%`
- 적용(다항식): `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: N pixel(s) of this frame evaluate to a gain outside [0.1, 10.0] and are marked defective (gain 1.0) and listed for the defect correction stage`
- 새 알림: `XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: N masked pixel(s) have no valid pixel within 16 pixels to fill them from and keep their input value` (경고, 결함 단계, 프레임당 1건)
- 나머지(`OVER_LIMIT`, `UNCORRECTED`, `WITH_BINNING`, `DEFECT_UNION_OVER_LIMIT`)와 접두사 5개는 그대로. 적재·적용 문구 전체와 새 알림 전체는 `DefectFillTest` 가 문자열 전체로 단언한다.

## 반증 (한 번에 하나, 전체 빌드, `20_arm_*.txt`)

| 손상 | 빨강이 된 시험 |
|---|---|
| d1 고리 넓히기 제거 / d1b 거기에 0 으로 되돌림 | 각 6건(재현·덩어리·가장 가까운 고리·제자리·반경·"알림 없음") |
| d2 가장 가까운 고리를 건너뜀(반경 4 부터) | `TheNearestRingDecides…` 1건 |
| d3 반경 40 / d3b 반경 8 | `…SearchRadius…` 각 1건 |
| d4 알림 제거 / d5 못 채운 화소를 0 으로 | 각 1건 |
| d6 게인 우회 비닝 검사 제거 | `ABypassedGainStage…` 1건 |
| d7 적재 알림 옛 문구 / d8 적용 알림 옛 문구 | 각 1건(문구 계약) |

## 문서용 (리더가 옮김): R² 의 한계 한 문장

REQ-P1A-011/SRS-CALIB-FUNC-027 보강안에 추가: "The fit_r_squared a polynomial file records is computed over the pixels that were fitted; the pixels classified defective by the gain calibration are not part of it and their number is not recorded in `XpeCalibQualityMeta` (it is reported once, in the `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT` alert at generation), so a reported R² can be better than the R² of the whole frame."

## 미검증 (Gaps)

- CalData_6 의 이 준위 영상은 한 값으로 균일해서 채워진 값이 이웃의 *구조*를 따르는지는 실데이터로 못 봤다(0 이 아님·범위 안만 봄).
- 반경 16 은 CalData_6(최대 10) 기준이다. 다른 검출기의 결함 덩어리가 16 을 넘으면 알림과 함께 입력값이 남는다 — 정책이지 보장이 아니다.
- 6개 준위 중 2개(0·4)만 실행.
- 큰 마스크의 시간: 반경 16 의 최악 경우(마스크 화소마다 1,089 개 확인)를 시간으로 재지 않았다. CalData_6 은 영향이 없었으나 정량 측정은 아니다.
- 212b(임시 저장 `dc0d9d56`)는 아직 마무리하지 않았다: 전체 검증·`fix_report.md` 가 남음.

## 잔여 위험

- 결함 덩어리가 반경 16 을 넘는 검출기는 그 화소에 입력값(결함 값)이 남는다 — 알림이 개수를 말한다.
- 이전 빌드로 만든 결함 보정 영상(덩어리 안쪽 0)과 이번 영상이 CalData_6 같은 데이터에서 달라진다. 비교·회귀 데이터가 옛 출력을 기준으로 삼고 있었다면 갱신해야 한다.
