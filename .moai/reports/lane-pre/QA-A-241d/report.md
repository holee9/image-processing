# QA-A-241d — Codex #159: 거부된 적재의 경고, 표현 정정, 개정 문서 기준 재판정

## 1. 결과

| 항목 | 결과 |
|---|---|
| Codex #159 높음: 거부된 적재가 "적재 완료" 경고를 남김 | **수정**. 밀도 초과는 stage 가 기록만 하고, 실제 설치가 성공한 뒤에 한 번 경고한다 |
| 표현 정정 ("샷 잡음 하한") | **정정**: "반복 프레임 없이 공간 잔여에서 역산한 경험적 추정치". B6 수치는 그대로 |
| B3 재판정 (개정 SRS-CALIB-FUNC-003 기준) | **통과** |
| B6 재판정 (개정 평가 프로토콜 §5.3 기준) | **통과, 단 판정 장은 한 장(평탄 6)** — 아래 §3 에 리더 메시지와 문서가 다른 점이 있다 |
| 체크리스트 | **통과 10 / 실패 0 / 0** (B2 는 값 잠정, B6 는 판정 장 한 개) |

## 2. (높음) 거부된 적재가 경고를 남기던 문제

Codex 의 지적이 맞았다. `xpe_calib_stage_defect` 가 5 % 초과를 세는 즉시 "the map is loaded" 경고를 냈는데, 세션 검사는 그 뒤(세트 경로는 `load_calibration_set` 에서 세 파일을 모두 읽은 뒤, 일반 로더는 stage 뒤 잠금 안)에 있다. 세션이 안 맞으면 맵이 설치되지 않는데도 알림에 "로드됨" 이 남았다. `pipeline.cpp` 의 세트 적재 계약은 맵·품질 메타데이터·**알림**이 호출 전 그대로라고 적고 있어서 이 계약도 깨졌다.

수정:
- `StagedDefect` 에 `marked`, `total`, `overLimit` 을 두고 stage 는 **기록만** 한다 (알림 없음).
- 새 `xpe_calib_after_defect_commit(staged)` 가 `overLimit` 일 때 경고를 낸다. `xpe_calib_after_gain_commit` 과 같은 패턴이다.
- 부르는 곳은 설치가 성공한 뒤의 두 경로뿐이다: 일반 로더(`xpe_calib_load_defect_map`, 잠금을 놓은 뒤)와 세트 적재(`load_calibration_set`, 커밋 뒤). 캐시 미스는 일반 로더를 부르니 같고, 캐시 적중은 이미 적재를 통과한 맵이라 반복하지 않는다.

시험 (`A241Safety`):
- `ARefusedDefectLoadLeavesTheAlertQueueAndTheStoreAsItFoundThem`: offset·gain 이 세션 "a241" 로 적재된 상태에서 **다른 세션 "other"** 의 과밀 결함 맵(400/3072 ≈ 13 %)을 ① 일반 로더, ② 캐시 로더 미스(두 번: 거부된 파일은 캐시에 남지 않아 두 번째도 거부), ③ 세트 경로(`xpe_preprocess_pipeline(…, calibPath)`: 세 파일의 세션 불일치)에 준다. 모두 `XPE_ERR_CONFIG_INVALID`, 알림 큐는 호출 전과 같은 0건, 이어서 같은 프레임을 처리한 결과 바이트가 호출 전과 같다(저장된 맵이 그대로).
- `AnOverLimitDefectMapThatIsInstalledIsReportedExactlyOnce`: 같은 세션이면 일반 로더와 세트 경로 모두 경고 정확히 1건.
- 기존 `ADefectMapAboveFivePercentIsReportedAtLoadAndTheLimitItselfIsNot` (경계 0/499/500/501/3000, RLE, 캐시 미스·적중)도 그대로 통과.

**반증** (`evidence/43_mutation_log.txt`, 한 스크립트의 연속 출력): 고친 트리에서 빌드·DLL/시험 파일 SHA-256·시험 통과 → 경고를 stage 에서(커밋 전에) 내는 변형으로 빌드: 새 시험 둘과 경계 시험이 모두 실패 → 원복 파일을 `touch` 하고 다시 빌드, 소스 해시가 처음과 같음, 시험 통과. 지난 번처럼 DLL 바이트는 같은 소스의 두 빌드에서 다르다(링커의 타임스탬프와 디버그 식별자). 그래서 같은 코드임은 소스 해시와 시험 결과로 보이고, B6 측정 로그의 DLL 해시(`39126a69…`)는 원복 후 빌드의 DLL 과 같다.

## 3. 재판정

### B3 — 통과

개정된 SRS-CALIB-FUNC-003 (main): "Maximum 5% defect density tolerance: a map above 5% (exactly 5% is within tolerance) is loaded and the load raises warning `XPE_WARN_DEFECT_MAP_OVER_LIMIT`, consistent with the combined-mask 5% warning." 구현은 이 문구와 같다: 5 % 초과(정확히 5 % 는 허용)면 적재하고 `XPE_WARN_DEFECT_MAP_OVER_LIMIT` 을 낸다. 이제 경고가 **적재가 성공한 뒤에만** 나므로 문구의 "the map is loaded" 와 알림이 항상 일치한다. 이전 판정(241c)이 기대던 "정책 미확정" 은 사용자 결정으로 해소됐다.

### B6 — 통과 (한 장으로 판정)

개정된 평가 프로토콜 §5.3 (main): "a flat frame is used to judge `FlatResidualPct` only when its mean over the ROI is at least **2000 ADU**. ... Frames below the level are still reported, marked 'below the judging signal level'." 실패를 본 뒤 정한 조건이라는 사실도 그 문서에 적혀 있다.

측정은 241c 의 방법과 같고 최종 바이너리로 다시 쟀다 (`evidence/20_b6_rejudge_run.txt`, 재빌드·DLL/시험 파일 SHA-256·측정 출력이 한 로그. 값은 241c 와 같다):

| 남긴 장 | 평균 ADU | `FlatResidualPct` (최종 출력) | 개정 §5.3 의 판정 |
|---|---|---|---|
| 평탄 4 | 867.4 | 1.067 % | **판정 신호 수준 미만** (2000 ADU 미만): 보고만 한다 |
| 평탄 5 | 1395.7 | 0.910 % | **판정 신호 수준 미만** (2000 ADU 미만): 보고만 한다 |
| 평탄 6 | 2196.2 | **0.844 %** | **판정 장**: 1.0 % 이하이므로 통과 |

**리더의 메시지·카드와 문서가 다른 점**: 카드 3번은 "평탄 4 는 판정 신호 수준 미만으로 보고, 5·6 으로 판정" 이라고 적었다. 개정 문서의 문구(평균이 2000 ADU 이상인 장만 판정)를 그대로 적용하면 **평탄 5 도 평균 1395.7 ADU 로 2000 미만**이라 판정에서 빠지고, 판정 장은 평탄 6 한 장뿐이다. 이 보고서는 문서를 따랐다. 평탄 5 를 판정에 넣으려면 문서의 수준이 1400 ADU 이하여야 하는데, 문서는 2000 이다. 리더가 의도한 쪽이 문서가 맞는지 카드가 맞는지 확인해 주길 바란다.

**이 통과의 한계** (보고서에 적어 두어야 하는 것):
- 판정 장은 평탄 6 한 장이다. 여유는 0.156 %p(0.844 대 1.0)다.
- 실패 수치(평탄 4: 1.067 %)는 사라지지 않았다. 문서는 그 장을 "판정에서 제외" 하는 것이지 값을 바꾸는 것이 아니다. 241c 보고서의 "실패" 판정은 241c 시점의 프로토콜 기준으로 맞았고, 이번 재판정은 개정된 기준에 따른 것이다. 판정 기준이 실패를 본 뒤에 바뀌었다는 점은 프로토콜 문서가 이미 적고 있다.
- "평탄 4·5·6 이 한 조건" 은 여전히 추정이다 (선량·관전압 기록 없음). 검출기는 하나다.

### 체크리스트 B1~B10

| # | 판정 | 비고 |
|---|---|---|
| B1 offset 적재 | 통과 | 이번 카드에서 다시 재지 않음 |
| B2 gain 범위 | 통과 (값 잠정) | 이번 카드에서 다시 재지 않음 |
| B3 결함 맵 | **통과** | 개정 FUNC-003 기준, 경고는 적재 성공 뒤에만 |
| B4 안전 동작 | 통과 | 이번 카드에서 다시 재지 않음 (캐시 만료는 241c 에서 닫힘) |
| B5 보정 순서 | 통과 | 다시 재지 않음 |
| B6 보정 품질 | **통과** (개정 §5.3, 판정 장 평탄 6 한 장, 평탄 4·5 는 판정 신호 수준 미만) | 위 §3 |
| B7, B8, B9 | 통과 | 다시 재지 않음 |
| B10 기준 영상 | 통과 | 옛 `42e78fac…`, v2 `d152e8eb…` 이번 빌드에서도 바이트 동일 (`20_b6_rejudge_run.txt`) |

"다시 재지 않음" 은 이번 카드가 만진 코드(결함 맵 로더의 경고 위치, 세트 적재의 호출 한 줄)가 그 항목들의 측정 경로를 바꾸지 않기 때문에 이전 카드의 값을 유지한 것이다. 이번에 새로 확인한 것이 아니다.

## 4. 표현 정정: "샷 잡음 하한"

Codex 지적(중간)이 맞다. 242 의 σ ≈ 0.29·√평균 은 **서로 다른 평탄 장의 공간 잔여를 16화소 규모로 분해해서 역산한 값**이고, 같은 조건의 반복 프레임이 없어서 시간적 샷 잡음과 고정 화소 패턴을 가르지 못했다. 그런데 241c 보고서가 "샷 잡음 하한" 이라고 확정적으로 썼다. 다음과 같이 고쳤다 (B6 의 실패 수치와 판정은 그대로):

- `QA-A-241c/report.md` §2: "242 가 서로 다른 평탄의 공간 잔여에서 역산한 경험적 관계 … 반복 프레임 없이 공간 잔여에서 역산한 경험적 추정치 … 추정으로는 평탄 4 의 화소 규모 성분 하나만으로 1.0 % 에 닿는다. (이 해석의 한계는 B6 실패 수치를 바꾸지 않는다)" 와 제안 절의 "사실과 추정" 으로. 시간적 잡음의 하한이라고 주장하려면 같은 조건 반복 평탄의 화소별 시간 분산이 필요하다고 적었다.
- `QA-A-242/report.md`: §1, H5 행, §4 ① 의 묶음 gain 표 아래 문단, §7 의 문구와 증거 표 이름에서 "샷 잡음" 을 "공간 잔여에서 역산한 경험적 추정치" 로.
- `QA-A-241b/report.md`: 같은 문구를 정정.
- `QA-A-241b/evidence/00_b6_method_and_predictions_before_measuring.md` 는 **고치지 않았다**: 측정 전에 고정한 문서라서 그대로 두고, 같은 정정을 `QA-A-241b/report.md` 에 적었다 (측정 전 기록을 사후에 바꾸지 않는 원칙).
- 증거 출력(`QA-A-242/evidence/10_flat_residual_analysis.txt` 의 "H5: is the pixel-scale part shot-noise-like?" 줄)은 당시 스크립트가 낸 출력 그대로다. 그 줄은 "샷 잡음이라면 k 가 일정할 것" 이라는 시험의 문구이고 결론이 아니다. 고치지 않았다.

## 5. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` 전체 | 1040 통과, 실패 0 (`DISABLED_` 59개 제외) | `30_suite_final.txt` |
| ctest 순차 | 100 %, 1232개 중 실패 0 | `31_ctest.txt` |
| clang-tidy 기준선 게이트 (`preprocess`) | `GATE_EXIT=0`, NEW 0 | `41_clang_tidy_gate.txt` |
| doxygen 1.12.0 | 종료 0, 경고 0 | `42_doxygen.txt` |
| 새 시험 | `A241Safety.ARefused…`, `AnOverLimit…` 통과 | `10_new_tests.txt` |

## 6. 미검증 · 한계

- cppcheck 는 이 기계에 도구가 없어 못 돌렸다 (CI 확인 필요). ASan 도 로컬에서 안 돌렸다.
- B6 통과는 판정 장 한 장이다. 더 신뢰하려면 같은 선량의 반복 평탄(프레임 평균)이나 평균 2000 ADU 이상인 평탄이 더 필요하다.
- 시간적 잡음의 하한은 여전히 측정하지 않았다 (§4).
- 이 보고서는 `moai-domain-humanize` 최종 패스를 거치지 않았다.

## 7. 변경 파일

제품: `src/xpe_calib_load_defect_map.cpp`, `src/pipeline.cpp` (세트 적재의 호출 한 줄), `include/xpe/preprocess/xpe_preprocess_internal.h` (`StagedDefect` 필드, `xpe_calib_after_defect_commit`).
시험: `tests/test_a241_safety.cpp`.
보고서 정정: `QA-A-241b/report.md`, `QA-A-241c/report.md`, `QA-A-242/report.md` (문구만).
증거: `.moai/reports/lane-pre/QA-A-241d/`.

## Card Cross-Check

| milestone | card |
|---|---|
| Codex #159 보류 수정, 표현 정정, 개정 기준 재판정 | QA-A-241d (이 보고서) |
