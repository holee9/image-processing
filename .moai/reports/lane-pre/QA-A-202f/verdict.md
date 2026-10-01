# QA-A-202f — Codex #41 보류: 부분 품질 필드가 R² 이력을 지움

기준 커밋 `2cb3a943`(QA-A-208b) 위. 감사 원문은 `evidence/00_codex41_audit_original.md`. 이슈 #233.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | `previous_r_squared`(이력)는 **R² 가 알려진 기록**(valid 이고 R² 가 문서화된 범위 0..1 안)일 때만 새 값으로 넘어간다. 다른 품질 키만 있는 기록(R² = -1.0 "없음")이나 품질 없는 기록은 마지막으로 알려진 R² 를 그대로 잇는다. | 빨강 `02_red.txt`, 초록 `04_oom.txt` |
| 2 | 이력을 잇는 세 경로 — 생성 기록(`xpe_calib_record_quality_meta`), 품질 있는 게인 적재, 품질 없는 게인 적재 — 가 **하나의 규칙**(`r2_history_after`)을 쓴다. | `xpe_calib_mode.cpp` 호출 3곳, 반증 `arm_site0/1/2` |
| 3 | Codex 재현(A → 부분 B → C, A → B → 무품질 D)과 리더가 적은 A → D → C 가 모두 기대값이다: C 의 이력은 0.91(전엔 -1.0). 적재 방식 4종(일반·세트·캐시 miss·캐시 hit) 전부에서. | 새 시험 3건 × 방식 |

## 2. 무엇을 바꿨나

- `xpe_calib_mode.cpp`: `r2_history_after(replaced)` = `(replaced.valid != 0 && replaced.r_squared >= 0.0) ? replaced.r_squared : replaced.previous_r_squared`. 세 곳의 `qm.valid ? qm.r_squared : qm.previous_r_squared` 를 이것으로 교체. "기록 있음"(`valid`)과 "R² 있음"(범위 안의 `r_squared`)이 분리됐다. 시작 상태의 이력은 그대로 -1.0.
- `preprocess_api.h`: `previous_r_squared` 의 문서를 "R² 가 있었던 마지막 기록, R² 없는 기록(품질 없음, 또는 `fit_r_squared` 없는 파일)은 건너뜀"으로 맞췄고, `r_squared` 에 "-1.0 = not given" 을 적었다.
- 저장소 소비자는 바뀌지 않는다(구조체 배치·크기 불변; 의미만 좁혔다).

## 3. 시험 (`test_oom_injection.cpp`, 새 3건 — 기존 시험은 고치지 않음)

- `APartialQualityRecordDoesNotInterruptTheR2History_AThenPartialThenC` — 부분 B 는 `valid=1`, `polynomial_degree=2`, `r_squared=-1.0`, 이력 0.91; 이어 C(0.97)의 이력도 0.91.
- `ARecordWithNoQualityAfterAPartialOneKeepsTheR2History_AThenPartialThenNoneThenC` — A → 부분 → 무품질 → 무품질 → C, 매 단계 이력 0.91.
- `AGeneratedRecordFollowsTheSameRule_AThenPartialThenGeneratedThenPartialThenC` — 생성 기록(R² 0.99)의 이력은 부분 기록을 건너뛴 0.91, 이후 C 의 이력은 0.99.
- 앞 둘은 적재 방식 4종(`qcur::ways()`)을 모두 돈다; 새 세트 `QP`(부분: `polynomial_degree` 만)와 `QM`(두 번째 무품질)을 추가했다.
- 수정 중 발견한 시험 쪽 결함: 파이프라인 방식은 영상을 제자리에서 처리해, 같은 프레임으로 두 번째 호출하면 형식 오류가 났다(품질 이력과 무관). 호출마다 프레임을 새로 되돌리는 `freshFrames()` 를 두었다.

## 4. 수정 전 / 후, 반증

수정 전: 3건 모두 빨강(`02_red.txt`: 이력이 -1.0, 기대 0.91 — 16회; 생성 경로 0.99 기대 1회). 수정 후: OOM exe 42/42.

반증(한 번에 하나, 전체 빌드, 복원 뒤 `cmp` 동일):

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| helper_valid_only | 도우미를 `valid` 만 보는 옛 조건으로 | 새 시험 3건 전부 |
| site0 | 생성 경로만 옛 조건 | 생성 기록 시험 |
| site1 | 품질 있는 적재 경로만 옛 조건 | 생성 기록 시험, 부분→C 시험 |
| site2 | 품질 없는 적재 경로만 옛 조건 | 무품질 시험 |

(site1 에서 생성 시험이 함께 빨개지는 것은 그 시험이 부분 → 생성 → 부분 → C 로 품질 있는 적재 경로를 지나기 때문이다. 세 경로가 각각 어떤 시험으로든 잡힌다는 것이 요점이다.)

## 5. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 778 실행 / 770 통과 / 8 건너뜀, 섞기도 동일 |
| preprocess OOM exe | 42 통과 (직전 39 + 3) |
| common OOM / common | 12 / 69 통과 |
| `ctest -N` | 941 (직전 938 + 3) |
| 수출 이름 / 헤더 문서 / 프리셋 | 48·16 불변 / 0 findings / 12 of 12 |

## 6. 기록만 (이번 카드 밖, Codex #41)

캐시 miss 경로(`calibration_cache.cpp`, `xpe_calib_load_gain` 호출 뒤 별도의 `g_calib_mutex` 구역에서 캐시용 맵·품질을 복사)는 일반 적재와 그 복사 사이에 잠금이 풀린다. 그 사이 다른 게인이 적재되면 파일 A 경로의 캐시 항목에 B 의 맵·품질이 게시될 수 있다 — **관측하지 않았고, 코드상 그 구간이 있다는 것만 읽었다**(이 카드의 diff 에 닿지 않음). 동시 적재 계약(호출자 직렬화)과 묶인 결정이라 리더 판단 사항이다. 이 카드에서는 시험도 코드도 건드리지 않았다.

## 7. 미검증 (Gaps)

- 범위 밖 R² 는 시험하지 않았다. 코드를 읽은 바로는 `fit_r_squared` 파서(`xpe_calib_mode.cpp` 328행)가 엄격한 double 변환만 하고 범위를 거부하지 않는다: 음수(예: -0.5)가 적히면 `valid=1` 이지만 "알려지지 않음"으로 취급돼 이력이 이어지고, 1.5 같은 값은 "알려짐"으로 이력에 들어간다. `r_squared >= 0.0` 만 검사하고 상한은 보지 않는다. 범위를 거부할지는 별개 결정이다.
- 동시 적재 중의 이력 순서는 시험하지 않았다(호출자 직렬화 계약).
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만.

## 8. 잔여 위험

- 이력이 R² 0 이하(정확히 0.0)인 기록도 "알려진 R²" 로 취급된다(범위 0..1 의 하한 포함) — 문서화된 범위와 일치.
- 이 카드 뒤 QA-A-208c(NUL 이후 바이트)가 같은 파서를 건드리지만 이력 규칙과 겹치지 않는다.
