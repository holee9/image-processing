# QA-A-211 — 게인 범위를 벗어난 화소를 결함으로 분류해 교정을 진행한다 (구현 보고, Refs #233)

기준: main `86356648` 위, 설계 메모 `3c686420`(리더 승인 + D1~D6 결정). 증거: `evidence/`. 푸시하지 않음.

## 결론

사용자 결정(범위 [0.1, 10] 하나, 밖의 화소는 게인 1 + 결함 분류, 개수 알림, 상한 초과 시 거부)을 스칼라·다항식 두 경로와 결함 단계에 구현했다. CalData_6 은 두 경로 모두 교정이 진행되고, 정상 데이터(cyan_test)는 결과가 바뀌지 않는다.

| 항목 | 결과 (실행 근거) |
|---|---|
| 스칼라, CalData_6 | 6개 준위 중 2개를 실행(`30_e2e_caldata6.txt`): 적재 `OK`(이전 −17), 분류 43,873 / 39,077 화소(0.465 / 0.414%), 알림에 "99.9% 가 외곽 64 화소 띠" 힌트. 파이프라인 `OK`, 출력 전부 유한, 결함 맵 밖의 분류 화소 29,598 / 25,489 개 중 **인접 정상 화소가 있는 9,810 / 6,973 개 전부가 그 이웃 값 범위 안**, 입력값 그대로 둔 것 0 개 |
| 다항식, CalData_6 | 생성 `OK`(이전 `INVALID_CALIB_DATA`), 분류 44,665 화소(0.473%), 적재·파이프라인 `OK`(레벨 0·5), 프레임마다 "44665 pixel(s) of this frame …" 알림. 영 계수 화소 44,665 개는 "측정 게인이 범위 밖인 화소" 44,658 개 + 최소제곱 직선이 범위를 벗어나는 7 개(두 번째 분류 경로). 두 집합이 같지 않은 이유가 이것이다 |
| 정상 데이터 cyan_test | 스칼라 5개 준위: 알림 0, 파이프라인 출력 sha256 이 **이전 빌드(`olddll210e`, 스칼라·결함 단계가 같은 빌드)와 5개 모두 일치**(`31_…new.txt` 대 `32_…old_dlls.txt`, 합친 해시 `56ac0584…`). 다항식: 영 계수 화소 0, 계수 payload 가 210e 빌드의 파일과 **바이트 단위로 동일**(`33_…txt`, sha256 `50ff4a77…`) |
| Codex #64 | 측정 게인이 범위 안인데 측정점 사이 화소값에서 float32 평가가 하한 아래로 가는 경우(도스 30000/30500/31000, 게인 0.100002766/0.32646404/1.00583434 — 원 재현 데이터가 SRS 범위 밖이라 범위 안으로 옮겨 탐색한 첫 값): 생성은 `OK`, 한 화소가 30001 이면 그 화소만 분류되고 프레임 `OK`, 25 화소(6.25%)면 `XPE_ERR_CONFIG_INVALID` + "not applied" 알림(`GainPolyClassifyTest.ACurveThatLeavesTheRange…`). 원 데이터(게인 0.001 부근)는 생성에서 100% 분류 → 거부(`TheCodexDataAsGiven…`) |
| 전체 시험 | preprocess 838 통과(셔플 `--gtest_random_seed=20261` 포함 838), 할당 실패 60, common 69·12, ctest 총 1028 건, 공개 export 변화 없음(`29_exports_pre_diff.txt` 빈 diff), 헤더 문서 검사 0 건, 프리셋 일치 12/12 |

## 구현

- **스칼라(적재 시)** `xpe_calib_stage_gain`: 범위 밖(비유한 포함) 화소를 세어 색인을 모으고 맵 값을 1.0 으로 바꾼다. 5% 초과면 `XPE_ERR_INVALID_CALIB_DATA` + 거부 알림, 스토어 불변. 색인은 `StagedGain` → `g_calib.gain_defect_idx/count` → `CalibSnapshot` 으로 이동하고 맵과 함께 교체된다.
- **캐시 적중**: 항목에 목록을 함께 보관해 적중이 같은 분류를 설치한다(`install_gain`). 다른 맵으로 스토어를 덮은 뒤 적중하면 분류가 돌아온다(시험).
- **다항식 생성**: 측정 게인이 범위 밖인 화소는 적합 전에 분류(계수 0, 품질 수치에서 제외 — D2), 모든 차수를 시도하고도 최소제곱 직선이 범위를 벗어나는 화소도 분류. 허용 오차(0.1%)를 넘는 적합은 분류가 아니라 이전처럼 거부. 5% 초과면 `INVALID_CALIB_DATA` + 거부 알림, 품질 기록·파일 없음.
- **다항식 적용**: 평가 게인이 범위 밖이면 게인 1.0, 프레임 목록에 추가, 5% 초과면 `XPE_ERR_CONFIG_INVALID`. 단독 호출은 "결함 단계가 따르지 않음" 알림.
- **결함 단계**: 마스크 = 결함 맵 ∪ 스토어 목록 ∪ 프레임 목록. 목록이 비면 이전 경로 그대로(사본 없음). 합집합 밀도가 5% 를 넘으면 프레임당 한 번 경고만(D1, 새 거부 경로 없음).
- **파이프라인**: 비닝 켜짐 + 분류 화소 있음 → `CONFIG_INVALID` + 알림(D4). 결함 단계 우회 → 경고 알림, 진행(D5).
- **범위 통일**: `XPE_GAIN_APPLIED_MIN/MAX` = 0.1 / 10(SRS-CALIB-FUNC-002), 스칼라 적재·다항식 생성·적용기가 한 값을 쓴다. XCal 형식·공개 함수 시그니처 변화 없음, 조회 함수 없음(D6). 헤더 문서(`xpe_calib_load_gain`, `xpe_gain_correct`, 다항식 생성)를 새 계약으로 고쳤다.

## 알림 문구 (레인 간 계약, 접두사로 매칭 — 확정본)

| 접두사 | 심각도 | 언제 |
|---|---|---|
| `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:` | 경고 | 적재·생성: "N of TOTAL pixel(s) (P%) have a gain outside [0.1, 10.0] … (K in the outermost 64-pixel band; first: I). Limit: 5.0%". 적용(다항식): "N pixel(s) of this frame evaluate to a gain outside [0.1, 10.0] and are treated as defective (gain 1.0)" |
| `XPE_WARN_GAIN_PIXELS_OVER_LIMIT:` | 오류 | 상한 초과, 끝이 "the calibration was not loaded / generated / applied" |
| `XPE_WARN_GAIN_PIXELS_UNCORRECTED:` | 경고 | 결함 단계 우회, 또는 `xpe_gain_correct` 단독 호출 |
| `XPE_WARN_GAIN_PIXELS_WITH_BINNING:` | 오류 | 비닝 + 분류 화소 → 프레임 거부 |
| `XPE_WARN_DEFECT_UNION_OVER_LIMIT:` | 경고 | 결함 맵 ∪ 분류 가 5% 초과(D1) |

**영향 받는 곳(통보 대상)**: 이전에 `xpe_calib_load_gain` 이 범위 밖 화소로 `INVALID_CALIB_DATA` 를 돌려주던 입력이 이제 적재된다(범위 밖이 5% 이하일 때). 기존 시험 7건이 거부 알림을 비우지 않아 위생 가드(`test_global_state_hygiene.cpp`)에 걸려 고쳤다 — clients/ 가 이 반환 코드나 알림 개수에 기대는지는 보지 않았다.

## 시험

새 파일 `test_gain_defect_classify.cpp`(13건, 스칼라·결함 단계·상한·캐시·우회·비닝·합집합 경고)와 `test_gain_poly_classify.cpp`(10건, 생성 두 경로·상한·품질 제외·적용·단독 호출·Codex), `test_oom_injection.cpp` 에 스윕 4건(스칼라 적재 17, 캐시 적중 4, 결함 단계 11, 다항식 적용 3 할당 지점). 바뀐 기존 시험: `ASingleOutOfRangePixelIsRejected` → `…ClassifiedDefectiveNotRejected`, 거부 시험 7건에 알림 비우기, 다항식 정책 시험 둘의 합성 데이터를 새 범위 안으로(base 100 → 2, 곡률 16개로 넓혀 대조군 유지; Codex 재현 base 0.001 → 0.1).

### 반증 (한 번에 하나, 전체 빌드, `21_arm_*.txt`·`22_arm_*.txt`)

| 손상 | 빨강이 된 시험 |
|---|---|
| a1 분류 때 맵 값을 1 로 안 바꿈 | 7건(채움·우회·비닝·합집합·캐시 등) |
| a2 결함 단계가 스토어 목록 무시 | 3건 |
| a3 상한 검사 제거 / a3b 상한을 `>=` 로 | 4건 / 1건(정확히 5% 경계) |
| a4 캐시 적중이 목록 버림 | 1건 |
| a5 비닝 거부 제거 / a6 우회 알림 제거 / a7 합집합 경고 제거 / a8 분류 알림 제거 | 각 1 / 1 / 1 / 2건 |
| b1 측정 게인 선분류 제거 | 1건 — **처음에는 0건**이었다(최소제곱 직선 규칙이 같은 화소를 잡아 줌). 이 규칙만 거르는 입력 (1, 11, 1) 을 구성해 시험을 더한 뒤에 터졌다 |
| b2 생성 상한 제거 | 3건 |
| b3 직선 범위 이탈 분류 제거 | 1건 |
| b4 분류 화소도 적합 | 2건 |
| b5 프레임 목록을 넘기지 않음 / b9 결함 단계가 프레임 목록 무시 | 각 2건 |
| b6 적용 상한 제거 | 1건 |
| b7 적용에서 나쁜 게인을 1 로 안 바꿈 | 4건 |
| b8 범위 상수를 옛 값으로 | 11건 |

b4 는 `TheClassifiedPixelsAreNotInTheQualityFigures` 를 빨갛게 만들지 못했다(분류 화소를 적합해도 평탄한 데이터라 완벽히 맞아 R² 가 안 변함) — 그 시험은 분류 화소가 품질 수치에서 빠진다는 것을 **직접 가두지 못한다**. 다른 두 시험이 같은 손상을 잡는다.

## 미검증 (Gaps)

- **간헐 실패 1종**: `ConfigStrictParse.*`(시험 파일이 임시 XCal 을 쓰는 곳)이 일부 실행에서 `write_xcal_file` 이 `-9`(`XPE_ERR_IO_FAILED`)를 돌려줘 실패한다 — 같은 시험을 따로 15번 돌린 중 1번 재현(`test_config_strict_parse.cpp:357`). 내 변경이 쓰는 경로가 아니며(쓰기 보조 함수 실패), 전체 실행·셔플 모두 최종 통과했으나 원인(Windows 에서 임시 파일 교체 중 경합 추정)은 확인하지 않았다.
- CalData_6 은 6개 준위 중 2개(0·4)의 스칼라 파이프라인만 실행했고, 다항식은 레벨 0·5 프레임. 선량이 저장소에 없어 준위 평균으로 대신했다(210e와 같은 가정).
- 분류 화소 중 **정상 이웃이 없는 것**(CalData_6 에서 약 2/3, 가장자리 띠 안쪽)은 "이웃 값 범위 안" 검사에서 빠졌다. 그 화소는 결함 단계가 3×3 보간 대신 반경 1~3 고리 보간으로 채운다 — 결과가 맞는지 값으로는 보지 않았다.
- 다항식 CalData_6 의 `fit_r_squared` 는 0.5677(< 0.999, `XPE_WARN_CALIB_POOR_FIT`) — 데이터(선량 대신 평균, 준위 간 산포)의 성질이며 분류와 무관함을 확인하지 않았다.
- 비닝이 켜진 파이프라인에서 분류 화소가 **없는** 경우 거부하지 않음은 시험했으나, 켜졌을 때의 전체 결과 정확성은 보지 않았다.
- clients/(GUI) 가 새 알림 접두사·`INVALID_CALIB_DATA` 변화에 의존하는지 보지 않았다.

## 잔여 위험

- 5% 상한의 근거는 SRS-CALIB-FUNC-003 의 결함 밀도 허용치 하나이고, 합집합(결함 맵 ∪ 분류)이 5% 를 넘는 경우는 경고뿐이다(D1). 분류가 많은 프레임의 보정이 이웃 보간에 크게 의존한다.
- 결함 맵이 없는 구성에서는 결함 단계가 `CALIB_NOT_LOADED` 로 멈추므로(기존 계약) 분류만으로는 보정이 일어나지 않는다.

## 요구사항 문안 (리더가 SRS·SPEC 에 넣을 최종본)

- **SRS-CALIB-FUNC-002 (보강)**: "…Values shall be in range [0.1, 10.0]. A pixel whose gain is outside this range (a failed pixel, or the low-sensitivity edge band of some detectors) shall NOT cause the load to fail: it shall be classified as defective, gain 1.0 shall be applied, and the defect correction stage shall treat it as part of the defect map (SRS-CALIB-FUNC-003, REQ-P1A-012). The number of classified pixels shall be reported through the alert queue. A classified fraction above 5% (the defect density tolerance of SRS-CALIB-FUNC-003) shall trigger `XPE_ERR_INVALID_CALIB_DATA`. The same range and rule shall apply to the gain polynomial G(x,y,D) at generation and, evaluated at each pixel value, at correction time (SRS-CALIB-FUNC-027)."
- **SRS-CALIB-FUNC-003 (보강)**: "…A frame whose defect map together with the pixels classified defective by the gain calibration exceeds 5% shall raise a warning (`XPE_WARN_DEFECT_UNION_OVER_LIMIT`) and still be corrected."
- **REQ-P1A-011 (보강)**: "A pixel whose gain value (scalar) or evaluated polynomial value is outside [0.1, 10.0] shall be corrected with gain 1.0 and reported to the defect correction stage of the same pipeline run. `xpe_gain_correct` shall return `XPE_ERR_CONFIG_INVALID` for a polynomial gain only when more than 5% of the frame's pixels evaluate outside the range. When binning is enabled and any pixel is classified, the pipeline shall return `XPE_ERR_CONFIG_INVALID`."
- **REQ-P1A-012 (보강)**: "The defect correction input mask shall be the union of the loaded defect map and the pixels classified defective by the gain stage. With the defect stage bypassed, classified pixels carry the uncorrected value and the pipeline shall warn (`XPE_WARN_GAIN_PIXELS_UNCORRECTED`)."
