# QA-A-211 설계 메모 — 게인 범위를 벗어난 화소를 결함으로 분류해 교정을 진행한다 (구현 전 리더 확인용, Refs #233)

기준: `main`(`86356648`)을 받은 dev/preprocess. 이 메모는 코드를 바꾸지 않는다. 증거: `evidence/`(분류 크기 측정 `10_m211_union.py`, `11_classification_sizes.txt`).

## 0. 결정의 요약 (사용자 결정 + 이 설계가 내놓는 것)

사용자 결정: 범위 **[0.1, 10]**(SRS-CALIB-FUNC-002, 두 경로 통일)를 벗어난 화소는 **게인 1 + 결함 분류**로 진행, 개수 알림, 상한 초과 시 거부.

제안 설계(한 줄): **스칼라는 적재 시, 다항식은 적용 시 분류한다. 분류된 화소의 게인은 1, 위치는 "게인 유래 결함" 목록으로 저장소에 두고 결함 단계가 결함 맵과 합집합으로 읽는다. XCal 형식·공개 함수는 바뀌지 않는다.**

| 층 | 스칼라 `xpe_calib_generate_gain` / `_load_gain` | 다항식 `…_generate_gain_polynomial` / 적용 |
|---|---|---|
| 생성 | 값은 그대로 쓴다. 범위 밖 화소를 세어 알림, 상한 초과면 거부(쓰기 전) | 범위 밖(측정 준위 중 하나라도) 화소는 **계수 0 으로 저장**(분류 표지), 센다, 알림, 상한 초과면 거부. 범위 안 화소는 210e 의 저장 후 검증 그대로 |
| 적재 | 범위 밖 화소를 분류: 메모리 맵의 값을 1 로, 위치를 저장소 목록에, 개수 알림, 상한 초과면 거부 | (정적 분류 없음: 적재 비용 때문) |
| 적용 | 변화 없음(이미 1) | 평가한 게인이 범위 밖이면 그 화소 게인을 1 로, 그 프레임의 목록에 추가, 상한 초과면 프레임 거부 |
| 결함 단계 | 유효 결함 마스크 = 결함 맵 ∪ 저장소 목록 ∪ (그 프레임 목록) | 〃 |

## 1. 분류는 어디서 일어나는가

### 선택지와 장단

| 위치 | 장점 | 단점 |
|---|---|---|
| 생성 시 | 파일 만드는 쪽에서 문제를 보고함 | 옛 파일(이미 0 이나 범위 밖 값을 가진 맵)을 못 다룬다. 생성기가 **값을 바꿔 쓰면** 결함 표지가 파일에서 사라진다(게인 1 은 정상 게인과 구별 불가) — 형식 변경 없이는 불가 |
| 적재 시 | 옛 파일 포함 모든 맵을 같은 규칙으로. 스칼라는 이미 적재 때 전 화소를 훑어 범위를 검사한다(추가 비용 거의 0). 파이프라인의 3-파일 세트 적재(stage → commit)에 그대로 들어간다 | 다항식은 정적으로 분류하기 어렵다(아래) |
| 적용 시 | 실제로 계산한 게인으로 판정 — 반올림·클램프·화소값 의존까지 다 덮는다. 평가 루프에 비교 하나만 추가 | 프레임마다 달라질 수 있다(다항식은 화소값에 따라 평가점이 달라진다). 결함 단계에 프레임별 목록을 넘겨야 한다 |

### 제안

- **스칼라 = 적재 시.** 맵은 정적이고 적재가 이미 범위 검사를 하는 자리다. 옛 파일도 같은 규칙으로 처리된다(파일 불변). 생성 시에는 같은 규칙으로 **세어서 알리고 상한을 검사**한다(분류를 파일에 쓰지 않는다 — 값 자체가 표지: 0, 0.04 …).
- **다항식 = 적용 시.** 게인은 `G(x)` 이고 `x` 는 화소값이므로 "이 화소의 게인이 범위 안인가"는 프레임이 정한다. 적재 때 정적으로 하려면 화소마다 구간 끝점 평가와 단조 보증이 필요하고 비용이 9.4M 화소 × 수십 flop(0.2~0.4 s 추정, SRS-CALIB-PERF-003 의 200 ms 적재 예산을 넘길 수 있음)이다. 적용 시에는 이미 하는 평가에 비교 하나를 얹을 뿐이다. 생성 시에는 **의도한 게인(측정 준위 또는 double 적합값)이 범위 밖인 화소의 계수를 0 으로 저장**해 결정적인 분류 표지로 삼는다(적용기가 0 을 범위 밖으로 읽는다) — 스칼라의 "죽은 화소 = 0" 과 같은 관례.

### Codex #64 의 경우 (측정점 사이 화소값의 하한 이탈)

재현(1×1, 선량 `[30000,30500,31000]`, 게인 `[0.00100007, 0.0032646, 0.0100583]`, 차수 2 → 화소 30001 에서 0.00099945): 새 범위 [0.1, 10] 에서는 측정 게인 자체가 범위 밖이므로 **생성 시점에 분류**된다(1×1 이라 100% > 상한 → 생성 거부). 측정 게인이 범위 안인데 측정점 사이에서 반올림으로 경계를 넘는 경우(예: 0.1000001 → 0.0999999)는 **적용 시 분류가 처리한다**: 그 화소는 프레임 거부가 아니라 그 프레임의 결함 목록에 들어간다.
- 생성 시점 보증을 고집하는 대안: 단조(210d 가 해석적으로 보증)이므로 구간 전체의 극값은 끝점이다. **끝점의 double 값이 범위 경계에서 float32 평가 오차 상한만큼 안쪽**이면 정수 화소 전 구간이 안전하다(전수 평가 불필요). 오차 상한 `γ_n · Σ|c_j x^j|` (n ≤ 4, u = 6e-8)은 화소당 O(차수)로 계산된다. 비용은 낮지만 경계 근처의 정상 화소를 보수적으로 결함 처리하게 되고(경계 폭 ≈ 1e-6 상대), 그래도 외부 파일에는 적용 시 방어가 필요하다. **적용 시 분류를 주(主), 이 경계 검사는 생략 가능(권고 안 함)**.
- 종단 시험에는 이 1×1 데이터를 넣는다(어느 설계든 포함, 아래 7).

## 2. 결함 정보는 어디에 사는가

- **별도 "게인 유래 결함" 목록**을 모듈 전역 교정 저장소의 게인 옆에 둔다: `CalibSnapshot` 에 `gain_defect_idx`(정렬된 `uint32` 목록, `shared_ptr`)와 개수. 결함 맵(`defect_map`)과 **합치지 않고** 읽을 때 합집합을 만든다. 이유: (a) 결함 맵과 게인은 독립적으로 적재·교체된다(순서 무관해야 함), (b) 한쪽을 다시 적재해도 다른 쪽의 정보가 사라지지 않아야 한다, (c) 결함 맵 파일·검증(`expected` 크기 등)에 손대지 않는다.
- 목록이 희소(상한 5% 이하, 보통 0.5%)이므로 희소 목록이 맞다(5% 면 1.9 MB). 스테이징(`StagedGain`)에 목록을 담아 적재-커밋 분리(커밋은 할당 없음)를 유지한다. 캐시(`xpe_calib_load_gain_cached`)의 적중 항목도 목록을 함께 가진다.
- **결함 단계가 읽는 법**: `xpe_defect_correct_in` 은 유효 마스크 = (결함 맵 사본) | (저장소 목록) | (프레임 목록) 를 만들어 기존 커널에 넘긴다(커널은 `dm[idx] != 0` 로 이웃을 건너뛰므로 조밀 마스크가 필요). 호출당 9.4 MB 사본·OR(약 1~2 ms, 결함 호출 18 ms 대비)를 치른다. 게인 유래 목록이 비면 지금과 같은 경로(사본 없음) — 정상 데이터(cyan_test)는 비용·결과 모두 불변. 필요하면 결함 맵·게인 목록이 모두 적재된 시점에 합친 마스크를 캐시하는 최적화를 후속으로(커밋이 할당 없어야 하는 제약 때문에 이번에는 안 함).
- **결함 맵이 적재되지 않은 경우**: 지금 계약을 유지한다 — 결함 단계가 켜져 있고 결함 맵이 없으면 `CALIB_NOT_LOADED`(QA-A-34, api-spec §6 규칙 3). 게인 유래 목록만으로 결함 단계를 돌리는 완화는 제안하지 않는다(결함 맵 부재를 "정상" 으로 읽는 길이므로).
- **결함 단계가 우회된 경우(`bypassDefect`)**: 분류된 화소는 게인 1 그대로(= 보정 안 된 원값) 나간다. **허용하되 프레임마다 경고 알림**(`XPE_WARN_GAIN_PIXELS_UNCORRECTED`, 아래 4)을 제안한다. 거부는 제안하지 않는다(우회는 호출자의 명시적 선택이고 알림이 사실을 말한다).
- **독립 호출** (`xpe_gain_correct` 다음 별도 `xpe_defect_correct`): 저장소 목록(스칼라, 정적)은 `xpe_defect_correct` 의 스냅샷에 들어 있으므로 자동 반영된다. **다항식의 프레임별 목록은 프레임에 속하므로 독립 호출 사이로 넘어가지 않는다** — 파이프라인 안에서만 결함 단계로 전달된다. 독립 `xpe_gain_correct` 호출은 분류 화소를 게인 1 로 두고 알림(결함 보정이 따르지 않음)한다. 공개 문서에 명시.
- **비닝**(`binningMode > 1`, 게인과 결함 사이): 비닝이 켜지면 분류 화소의 게인 1 값이 이웃으로 섞인 뒤에 결함 보정이 일어난다. 이번 설계는 이를 막지 않는다(비닝 기본 꺼짐); 켜진 경우 알림에 사실을 적거나 후속 카드로. 리더 의견 요청(D4).

## 3. 상한

| 자료 | 값 |
|---|---|
| CalData_6, 게인 범위 밖 화소 | 준위별 **0.414~0.468%** (39,077~44,144), 준위 합집합 0.473% |
| 같은 것 ∪ 결함 맵(23,505 = 0.249%) | 준위별 0.519~0.566%, 합집합 **0.569%** |
| cyan_test | 0%, 결함 맵 0.0026% |
| SRS-CALIB-FUNC-003 | "**Maximum 5% defect density tolerance**" (BPM 의 밀도 허용) — 현재 코드에서 강제하는 곳을 찾지 못함 |
| `tests/test_data/CalData_6/fixture.json` | `expected_defect_density_pct_max: 5.0` |

**제안: 상한 5%** — 근거 SRS-CALIB-FUNC-003 의 결함 밀도 허용치 5% (이 시스템에 문서화된 유일한 밀도 상한; CalData_6 의 최악 0.57% 에서 9배 여유). 분류된 화소는 결함 화소와 같은 취급이므로 같은 상한을 쓰는 것이 일관된다. 더 보수적인(예 1%) 상한은 근거가 없어 제안하지 않는다 — 리더가 조일 수 있다(상수 하나, `XPE_GAIN_DEFECT_MAX_FRACTION`).
- 적용 지점: 스칼라 생성(쓰기 전), 스칼라 적재(스토어 변경 전), 다항식 생성(품질 기록·쓰기 전), 다항식 적용(프레임당).
- **초과 시**: 반환 `XPE_ERR_INVALID_CALIB_DATA`(생성·적재, 지금의 "이 데이터로 교정을 만들 수 없다" 코드와 일치), 프레임 적용은 `XPE_ERR_CONFIG_INVALID`(현재 적용기가 무효 게인에 돌려주는 코드를 유지). 알림은 아래 4 의 거부 문구.
- 결함 맵과의 **합집합 상한**(SRS 의 5% 를 결함 맵+분류 합쳐서)은 결함 단계에서만 알 수 있다. 제안: 이번 카드에서는 게인 유래 집합만 상한을 두고, 합집합 검사는 별도 결정(D1)으로 둔다.

## 4. 알림 문구 초안 (레인 간 계약 — 접두사로 매칭 가능)

1. 분류 알림 (생성·적재, 한 번) — `XPE_ALERT_WARNING`:
   `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: <N> of <TOTAL> pixel(s) (<P>%) have a gain outside [0.1, 10.0] and are treated as defective: gain 1.0 is used and the defect correction stage fills them from their neighbours (<K> in the outermost 64-pixel band; first: <index>). Limit: <CAP>%`
   - `<K>` = 가장자리 64 화소 띠 안의 개수(원인 분포의 싼 힌트 — CalData_6 에서 99.9%). 결함 맵 안/밖 분포는 결함 맵을 이 시점에 모를 수 있어 넣지 않는다(결함 단계 시점 알림에 넣을 수 있음).
2. 거부 — `XPE_ALERT_ERROR`:
   `XPE_WARN_GAIN_PIXELS_OVER_LIMIT: <N> of <TOTAL> pixel(s) (<P>%) have a gain outside [0.1, 10.0], above the <CAP>% limit; the calibration was not <generated|loaded>`
3. 다항식 적용 시 프레임별 분류 — `XPE_ALERT_WARNING`(프레임당 1건, 클램프 알림과 같은 방식):
   `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: <N> pixel(s) of this frame evaluate to a gain outside [0.1, 10.0] and are treated as defective (gain 1.0)`
4. 결함 단계 우회·독립 호출 — `XPE_ALERT_WARNING`:
   `XPE_WARN_GAIN_PIXELS_UNCORRECTED: <N> pixel(s) were classified defective by the gain calibration but no defect correction follows (defect stage bypassed or called separately); they carry the uncorrected value with gain 1.0`
- 기존 210e 의 `XPE_WARN_GAIN_POLY_NOT_APPLICABLE:` 는 **충실도 실패**(범위 안 화소가 float32 로 못 실림) 전용으로 남고, 범위 위반은 위 문구로 옮긴다.

## 5. 범위 통일과 210e 의 저장 후 검증

- 적용기의 범위 상수 `XPE_GAIN_APPLIED_MIN/MAX`(210e, `xpe_preprocess_internal.h`)를 **[0.001, 1000] → [0.1, 10]**(= `XPE_CALIB_GAIN_MIN/MAX`, SRS-FUNC-002)로 바꾼다. `xpe_gain_value_valid` 와 `xpe_gain_poly_eval_f32` 는 그대로 생성기·적용기가 공유한다. `gain_correct.cpp` 의 `MIN/MAX_GAIN_VALUE` 도 같은 상수가 된다. 정상 데이터(게인 0.7~1.16)에서는 결과 불변.
- 210e 의 저장 후 검증과 맞물리는 방식:
  - **범위 위반**은 이제 거부가 아니라 분류: 의도한 게인(측정 준위·double 적합값)이 범위 밖인 화소 → 계수 0 저장 + 개수·상한. 210e 에서 "범위 밖 → 차수 낮춤 → 1차도 실패 → 생성 전체 거부" 이던 경로가 "분류 → 상한 이내면 진행" 으로 바뀐다.
  - **충실도 실패**(허용치 0.1%, 범위 안 화소의 float32 평가가 적합에서 벗어남)는 그대로 거부 경로(`INVALID_CALIB_DATA` + `XPE_WARN_GAIN_POLY_NOT_APPLICABLE`): 선량이 서로 너무 가까운 데이터의 문제라 모든 화소에 영향을 주고, 틀린 값이 "유효한 게인" 으로 조용히 적용되는 것을 막는다(분류로는 잡히지 않는다).
  - 210e 의 종단 시험 중 *Codex 재현(게인 0.001+…)* 과 *범위 시험(5e-4, 2000)* 은 100% 분류 → 상한 초과 거부라 같은 반환 코드(`INVALID_CALIB_DATA`)를 유지한다. 허용치·R² 시험은 불변. R² 는 분류된(계수 0) 화소를 어떻게 셀지 정해야 한다(D2): 제안은 **R² 계산에서 분류 화소를 제외**(그 화소는 보정에 쓰이지 않으므로 적합 품질에 속하지 않는다), 제외 개수는 알림에.

## 6. 공개 API·파일 형식 변화

- **XCal 형식: 변화 없음.** 분류 표지는 값(스칼라는 범위 밖 값 자체, 다항식은 계수 0)이고 목록은 메모리에서 도출된다. 옛 파일은 그대로 읽힌다.
- **공개 함수·구조체: 변화 없음.** 새 함수 불필요. 개수는 알림으로 전달된다. (공개 개수 조회 `xpe_calib_get_gain_defect_count` 는 선택 사항 — `XpeCalibQualityMeta` 는 BUILD-MATCHED 구조체라 필드 추가를 피한다. 필요하면 별도 함수 하나가 안전.)
- **내부**: `CalibSnapshot`/`StagedGain` 에 목록과 개수, `xpe_gain_correct_in` 과 `xpe_defect_correct_in` 에 프레임 목록 인자(기본값 null) 추가. 공개 헤더의 문서 문단(스칼라 적재의 거부 대신 분류, 다항식 생성·적용의 분류)과 `xpe_gain_correct` 의 반환 코드 문서 갱신.
- **동작 변화(호출자가 겪는 것)**: (1) 범위 밖 화소가 상한 이내인 스칼라 맵은 이제 적재된다(이전: `INVALID_CALIB_DATA`); (2) 다항식 생성은 같은 데이터에서 이제 진행된다(이전: 거부); (3) 알림 새 문구 3종.

## 7. 시험 계획

종단(공개 API만):
1. **CalData_6 축소본**(가장자리 띠 + 결함 + 정상 영역을 담은 작은 크롭, 값은 실데이터에서 잘라 헤더로): 스칼라 생성 → 적재 `OK` + 분류 알림(개수·띠) → `xpe_gain_correct` `OK`, 분류 화소는 게인 1 → 파이프라인(결함 맵 적재)에서 분류 화소가 **결함 보정으로 이웃 값으로 채워짐**(보정 후 값이 이웃에서 나옴, 원값 아님). 다항식도 같은 흐름.
2. **Codex #64 의 1×1 데이터**(`[30000,30500,31000]`, 게인 `[0.00100007, 0.0032646, 0.0100583]`, 차수 2): 새 범위에서 분류 → 100% 상한 초과 → 생성 거부(`INVALID_CALIB_DATA`), 파일 없음. 그리고 **측정 게인은 범위 안인데 측정점 사이에서 경계를 넘는** 변형(예: 0.1 근처에서 시작해 사이에서 내려가는 합성): 적용 시 해당 화소만 분류되고 프레임은 `OK`, 나머지 화소 불변.
3. **상한**: 5% 를 넘는 합성 맵 → 스칼라 생성·적재 거부, 다항식 생성 거부, 다항식 적용 프레임 거부(`CONFIG_INVALID`); 정확히 상한 직전은 통과.
4. **결함 맵 미적재**: 결함 단계 켜짐 + 맵 없음 → `CALIB_NOT_LOADED`(계약 유지); **우회**(`bypassDefect`): 분류 화소가 게인 1 그대로, `UNCORRECTED` 알림; **독립 호출**: 스칼라 목록이 `xpe_defect_correct` 에 반영됨, 다항식 프레임 목록은 넘어가지 않음(문서 그대로).
5. **정상 데이터 불변**: cyan_test 3072² 로 스칼라·다항식 생성·적재·적용·파이프라인 결과가 변경 전과 **비트 동일**(목록이 비면 새 경로를 타지 않음), 알림 추가 없음.
6. **옛 파일**: 이미 범위 밖 값을 가진 맵(현재 저장소의 시험 픽스처 포함)이 새 규칙으로 적재되고 같은 화소가 분류됨.
7. **할당 실패 스윕**(OOM 실행 파일): 새 적재·적용 경로의 모든 할당 지점에서 `OUT_OF_MEMORY`, 스토어·출력 불변.
8. 반증(각 한 번에 하나): 분류 제거 → 종단 시험 빨강; 목록과 결함 맵의 합집합을 합집합 아닌 대체로 → 결함 맵 화소가 보정 안 됨 빨강; 상한 제거 → 상한 시험 빨강; 범위 상수를 옛 값으로 → 범위 시험 빨강; 프레임 목록 전달 제거 → 다항식 종단 시험 빨강.

## 8. 요구사항 문안 초안 (문서 수정은 리더)

- **SRS-CALIB-FUNC-002 (보강)**: "…Values shall be in range [0.1, 10.0]. A pixel whose gain is out of range (a failed pixel, or the low-sensitivity edge band of some detectors) shall NOT fail the load: it shall be classified as defective — gain 1.0 is applied and the defect correction stage (SRS-CALIB-FUNC-003 / REQ-P1A-012) shall treat it as part of the defect map. The number of classified pixels shall be reported through the alert queue, and a classified fraction above 5% (the defect density tolerance of SRS-CALIB-FUNC-003) shall trigger `XPE_ERR_INVALID_CALIB_DATA`. The same range and rule apply to the gain polynomial G(x,y,D) evaluated at each pixel value (SRS-CALIB-FUNC-027)."
- **SRS-CALIB-FUNC-003 (보강)**: "…Maximum 5% defect density tolerance applies to the union of the loaded BPM and the pixels classified defective by the gain calibration."(합집합 검사를 하기로 하면)
- **REQ-P1A-011 (보강)**: "A pixel whose gain value (scalar) or evaluated polynomial value is outside [0.1, 10.0] shall be corrected with gain 1.0 and reported to the defect correction stage of the same pipeline run; xpe_gain_correct shall not return an error for such pixels unless their fraction exceeds 5% (`XPE_ERR_CONFIG_INVALID`)."
- **REQ-P1A-012 (보강)**: "The defect correction input mask shall be the union of the loaded defect map and the pixels classified defective by the gain stage."

## 9. 리더에게 묻는 것

- **D1** 상한 5%(SRS-FUNC-003 의 밀도 허용치)를 분류 집합에만 둘지, 결함 맵과의 합집합까지 결함 단계에서 검사할지.
- **D2** 다항식 R² 에서 분류 화소를 제외(제안)해도 되는지.
- **D3** 다항식을 **적용 시** 분류(제안) 대 적재 시 정적 분류(비용 문제)를 확인.
- **D4** 비닝이 켜진 경우의 처리(알림만 / 후속 카드).
- **D5** `bypassDefect` 시 경고만(제안) 대 거부.
- **D6** 개수 공개 조회 함수가 필요한가(제안: 불필요, 알림으로 충분).
