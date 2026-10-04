# QA-A-234c — Codex #135 의 CI 패치 보류 4건(+기록 1건)

카드: QA-A-234c · Refs #253 · 제품 코드 변경 없음(패치·설정·스크립트·보고서만). 패치는 다시 `.txt`, 깨끗한 main `cdf85e39` 에 `git apply --check` 통과.

## 0. 처리 요약

| # | 발견 | 처리 | 확인 |
|---|---|---|---|
| 1 | 개수만 비교해 같은 개수 안의 교체를 못 잡음 | 기준선을 **진단 단위** `(점검, 파일, 정규화한 메시지, 지적된 소스 줄의 텍스트)` 로 바꿈(줄 번호는 안 씀). 같은 단위는 개수로 센다 | §1 반증 |
| 2 | 비정상 종료·도구 실패가 0건으로 통과 | 모든 실행의 비정상 종료·`error:`·`Error while processing` 은 즉시 실패, 컴파일 DB 가 비었거나 대상 파일의 명령이 없으면 실패, **양성 대조**(확실한 지적을 심은 파일에서 그 지적이 나와야 함) 추가 | §2 반증 3종 |
| 3 | swappable 을 두 모듈 전체에서 끔 | `.clang-tidy` 두 파일에서 **다시 켬**. 25건을 가르면 공개 C ABI 3 / 내부 22. 모두 기준선에 넣어 새 지적을 봄 | §3 |
| 4 | cppcheck 잡의 전환 조건·담당 없음 | 잡 이름 `cppcheck (measurement only)`, 워크플로 주석에 전환 조건(첫 CI 결과를 리더가 읽고 기준선 승인 → 게이트 단계로 교체), 후속 이슈는 리더가 연다 | §4 |
| 5 | 빈 catch 20곳 중 일부는 내부 함수(문구 과장) | 분류해 정정: 공개 C ABI 3 / 내부 17. QA-A-234 `report_m2.md` 에 정정 표시 | §5 |

## 1. 진단 단위 기준선 (`patches/check_clang_tidy.py.txt`)

- 신분: `(점검, 파일 basename, 정규화한 메시지, 소스 줄 텍스트)` + 개수. 줄 번호가 움직여도 같은 지적은 같고, 지적된 줄이 고쳐지거나 다른 곳에 새로 생기면 다르다. 기준선 줄은 탭으로 나눈다(`clang_tidy_baseline_*.txt`: preprocess 35줄, common 0줄).
- **반증(교체)**: `xpe_verify_metrics.cpp` 에서 narrowing 한 건을 고치고(`static_cast<double>(total)`) 다른 곳에 새 narrowing(`double d = n;`)을 만들면 (점검, 파일)별 개수는 4 → 4 로 **같다**(옛 방식이면 통과). 새 게이트는 `NEW: bugprone-narrowing-conversions xpe_verify_metrics.cpp | … | static double a234c_probe(size_t n) …` 로 종료 1 이고, 고친 쪽은 `below baseline (tighten it)` 으로 알린다(`evidence/31_gate_swap_new_gate.txt`). 복원 뒤 `35 findings … baseline 35`, 종료 0 (`30_gate_clean.txt`).
  - 옛 게이트를 같은 상태에서 돌려 대비하려 했으나, 옛 기준선이 swappable 을 끈 설정에서 만든 것이라 비교가 오염돼 그 실행은 증거에서 뺐다. "옛 방식이면 통과"는 개수 4 → 4 의 계산이고 옛 스크립트의 실행이 아니다.
- 한계: 지적된 줄을 서식만 바꿔도(공백 정리) 줄 텍스트가 달라져 NEW + below 가 한 쌍 나온다 — 기준선을 `--write-baseline` 으로 갱신해야 한다(이것이 의도한 보수성이다).

## 2. 도구 실패를 통과로 읽지 않는 게이트

| 대역(`CLANG_TIDY`) | 동작 | 결과 | 증거 |
|---|---|---|---|
| 진짜 clang-tidy | — | common 0건/기준선 0, preprocess 35건/기준선 35, 종료 0 | `30_` |
| `--version` 0, 분석은 종료 1 + stderr 만 | 카드의 대역 | 양성 대조에서 실패("positive control failed") | `33_` |
| 종료 0, 아무것도 출력 안 함 | 아무것도 분석하지 않는 도구 | 양성 대조에서 실패 | `34_` |
| 심은 대조 파일은 진짜로 분석하고 모듈 파일에서는 종료 1 + stderr 만 | 비정상 종료 점검 | `clang-tidy exited 1 on modules/common/src\xpe_common.cpp: the findings would not be complete`, 종료 1 | `35_` |

첫 두 대역은 새로 넣은 양성 대조가 먼저 막아서 모듈 파일의 종료 코드 점검까지 가지 못한다. 그래서 세 번째 대역으로 그 점검을 따로 눌렀다. 코드의 순서는 양성 대조 → 컴파일 DB → 파일별 명령 → 실행별 종료 코드·`error:`.
**시험하지 않은 갈래**: 컴파일 DB 가 비었을 때(`entries` 0), 대상 파일의 명령이 DB 에 없을 때의 실패는 코드에 있으나 실행으로 누르지 않았다(DB 를 비우거나 한 항목을 빼는 시험을 만들지 않았다).

## 3. swappable-parameters 25건 (`evidence/20_swappable_classified.txt`)

| 구분 | 건수 | 파일 |
|---|---|---|
| 공개 C ABI 함수 | 3 | `xpe_calib_generate_gain`(1), `xpe_calib_generate_offset`(2) |
| 내부 함수 | 22 | `ghost_correct.cpp` 6, `xpe_defect_gen.cpp` 4, `xcal_reader.cpp` 2, `xpe_calib_generate_gain.cpp` 2, `pipeline.cpp` 2, `calibration_cache.cpp` 1, `defect_correct.cpp` 1, `rle_codec.cpp` 1, `xpe_calib_generate_nonlin_lut.cpp` 1, `xpe_calib_generate_offset_methods.cpp` 1, `xpe_verify_metrics.cpp` 1 (함수 이름이 풀리지 않은 줄 4곳은 내부로 분류) |

**QA-A-234 에서 "대부분 C ABI 가 순서를 고정"이라 쓴 것은 틀렸다**: 공개 C ABI 는 3건이고 22건은 내부 함수다. 그래서 전역 비활성화를 거둬 `.clang-tidy` 두 파일에서 다시 켰다(`modules/` 라 레인 소유). 처리는 **모두 기준선**이다: 공개 C ABI 3건도, 내부 22건도 기준선에 넣었고 소스에 NOLINT 를 달지 않았다(제품 코드를 건드리지 않는 카드이고, 새 지적은 NEW 로 보인다).
**하지 않은 것**: 22건 각각에서 인자 순서를 바꿔 쓴 호출이 실제로 있는지 하나씩 보지 않았다. 점검은 "바뀌어도 컴파일되는 이웃 인자 쌍"을 센 것이지 결함을 찾은 것이 아니다.

## 4. cppcheck 잡

`name: cppcheck (measurement only)`. 워크플로 주석(패치 `static_analysis_jobs.yml.txt`): 첫 CI 결과(아티팩트 `cppcheck-results`)를 **리더가 읽고 기준선으로 승인**한 뒤에야, `tools/ci/check_clang_tidy.py` 와 같은 모양(진단 단위 기준선·비정상 종료 실패·양성 대조)의 게이트 단계가 `--error-exitcode=0` 실행을 대체하고 이름에서 "(measurement only)" 가 빠진다. 후속 이슈는 리더가 연다. 그때까지 이 잡은 아무것도 막지 않는다.

## 5. 빈 catch 20곳 분류 정정 (`evidence/21_catches_classified.txt`)

함수 이름으로 가름(공개 헤더의 `XPE_API` 선언 118개 이름과 대조): **공개 C ABI 함수 3곳**(`xpe_init`, `xpe_shutdown`, `xpe_preprocess_shutdown`) / **내부 최선 노력 로그·경보·정리 17곳**(경보 푸시 보조 함수, 로그 쓰기·리셋 보조, `xcal_writer` 의 보고 함수들, 게인 경고 함수, 세션 경고 등). 둘 이름이 풀리지 않은 줄(`xpe_logging.cpp`, `xpe_calib_load_gain.cpp` 각 1)은 내부로 분류했다. 이유 주석의 내용(경보는 보조라 잃어도 호출 결과를 바꾸지 않음 등)은 정정 전과 같다.

## 6. 패치 (`patches/`, 깨끗한 main `cdf85e39` 기준)

`01_asan_job.patch.txt`(변경 없음, 재확인), `02_static_analysis_and_coverage.patch.txt`(새 스크립트·기준선·cppcheck 이름·주석 반영), 원본 조각들. 임시 색인(`git read-tree cdf85e39` + `git apply --cached --check`)으로: 01 통과, 02 단독 통과, 01 적용 뒤 02 통과. 적용한 결과의 `ci.yml` YAML 파싱·`CMakePresets.json` JSON 파싱 통과, 잡 `asan-tests`·`clang-tidy`·`cppcheck`·`coverage` 확인.

## 7. Gaps / Residual-risk

- 패치는 실제 CI 에서 한 번도 돌지 않았다(러너의 clang-tidy·ASan DLL·`choco install cppcheck` 는 첫 실행이 드러낸다).
- 컴파일 DB 가 비었을 때와 파일 명령이 없을 때의 실패 갈래는 실행으로 누르지 않았다(§2).
- swappable 22+3건의 인자 순서 실수 여부는 개별로 보지 않았다(§3).
- 줄 텍스트 신분은 서식 변경에 민감하다(§1). 기준선 갱신 절차가 필요하다: `python tools/ci/check_clang_tidy.py <모듈> <빌드> <기준선> --write-baseline`.
- cppcheck 은 여전히 측정 전용이고 지적 수를 모른다(§4).
- xpe_common 85% 커버리지 게이트는 QA-A-234 M2 와 같이 후속이다.
