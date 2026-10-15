# QA-B-207b — Codex #120 보류 2건 (C3, E6)

기준: `dev/postprocess` 183494fc 위. Refs #251. 증거는 모두 이 디렉터리, 접미 `b_`.

## 1. C3 — 그룹 0002 메타가 전혀 없는 파일

**재현 (수정 전)**: 서문 128바이트 + `DICM` + 메타 없는 Explicit LE 데이터셋 → `xpe_dicom_open` 이 `XPE_OK` 와 핸들을 돌려줬다 (`b_c3_red.txt`, 시험 `PreambleAndMagicWithoutAnyGroup0002ElementIsDicomInvalid` 실패 2건: 반환 코드 0, 핸들 non-null).
통제: 같은 데이터셋에 원래 메타를 붙인 파일은 열린다 (같은 시험 안에서 먼저 단언).

**수정**: `DicomReader.cpp` `open()` — `getMetaInfo()` 가 돌려준 객체의 `card() == 0` 이면 `XPE_ERR_DICOM_INVALID`. 메타가 있으나 TransferSyntaxUID 만 빠진 경우는 #167 TS-less 정책 그대로(아래 분기).
**두 경우를 나눠 시험**: 새 시험 = 메타 없음(거부). TS-less 쪽은 기존 `WriteTsLessPart10` 시험들이 그대로 통과(전체 158/158 중).

## 2. E6 — 좁은 유한 범위

**재현 (수정 전)**: `4095.0f / range` 가 범위 < 4095/FLT_MAX = 1.2034e-35 에서 float 오버플로 → int 변환 미정의. 실측: `rc=0` 인데 범위의 96.9 % 만큼 틀림 (`b_e6_red.txt`: 4/8 케이스 DIFFER, 통제 4건 MATCH). C++ 시험도 같은 4건 실패, 나머지 26개 통과 (`b_e6_cpp_red.txt`).

**독립 기준**: `clahe_ref_exact.py.txt` — 구간을 `fractions.Fraction` 정확 유리수로 계산(float 중간값 없음). 기존 `clahe_ref.py` 는 구간 식을 C++ 과 같은 float32 식으로 계산해 이 결함을 볼 수 없었다. C++ `ReferenceClahe` 도 같은 식을 복제하고 있었다 → `floor(d*4095/range)`(곱 먼저, 나눗셈 마지막)로 바꾸고, 정확성은 위 Python 쪽이 근거임을 시험 주석에 적었다.

**수정**: `contrast_enhance.cpp` — 배율·구간 계산을 double (`bin_of(float, float, double)`, `range_d`).

**리더 결정과 다른 점 (근거 있음)**: 결정문은 "배율이 유한하지 않거나 구간이 [0,4095] 를 벗어날 수 있으면 평탄 취급, 경계를 수치로" 였다. double 에서는 가장 좁은 양의 float 범위(비정규수 한 단계 1.4e-45)도 배율 2.9e48 로 유한(DBL_MAX 1.8e308)이라 그 분기는 도달 불가능한 코드가 된다. 분기를 만들지 않고, 대신 `0 과 1e-37f` 가 평탄이 아니라 **정상 처리**된다 — 구별되는 값을 평탄으로 버릴 이유가 없다. 경계 수치는 `bin_of` 주석과 헤더에 적었다(1.4e-45 → 2.9e48; 반대쪽 끝 float 범위 오버플로는 기존 `INVALID_INPUT`). 평탄 취급을 원하면 임계를 정해 주면 된다.

**시험**: `NarrowFiniteRange_MatchesTheReferenceAndIsNotTreatedAsBroken` — 0~1e-37, 0~FLT_MIN, −5e-38~+5e-38, float 배율이 막 유한해지는 범위의 아래(1.19e-35)·위(1.21e-35), 0~1e-30, 통제 0~1. 값은 구간 중심에 두어 경계 값 모호성이 없다.

## 증거 (수정 후)

| 주장 | 명령 / 산출 | 관측 |
|---|---|---|
| C3 새 시험 통과, dicom reader 전체 | `_run_b.bat test_dicom_reader *` → `b_c3_green_full.txt` | 158 tests PASSED |
| E6 C++ 전체 | `_run_b.bat xpe_enhance_basic_tests *` → `b_e6_cpp_green_full.txt` | 183 tests PASSED |
| E6 Python 정확 기준 vs DLL | `clahe_ref_exact.py` → `b_e6_green.txt` | 0 of 8 differ, 최대 9.5e-08 |
| ci-post 전체 | `_ctest_b.bat ci-post post` → `b_ctest_post.txt` | 1440/1440 passed (AiOom 7건 Skipped, Gsvg 1건 Disabled — 기존) |
| ci-dicom 전체 | `_ctest_b.bat ci-dicom dicom` → `b_ctest_dicom.txt` | 367/367 passed |
| Doxygen (헤더 문서 변경) | `docs/help/doxygen` 에서 `doxygen Doxyfile` → `b_doxygen.txt` | exit 0, 경고 0 |
| 속도 3072² 구 코드 vs 신 코드, 번갈아 6회 | `b_e6_perf_ab.txt` (프로세스 5회 중앙값) | 구 38.6~40.9 / 신 41.7~44.7 ms, 예산 50 |

## Gaps

- C3: 수정 전 적색 → 수정 후 녹색은 봤으나, `card()==0` 조건만 따로 약화시킨 반증 팔은 돌리지 않았다. TS-less(메타 있음) 쪽이 안 깨졌다는 근거는 기존 시험 통과뿐이다.
- E6: 비정규수 단계 몇 개짜리 범위(예: 7 단계)는 시험하지 않았다. 배율×차이의 반올림이 구간 경계에서 정확 유리수와 갈릴 수 있는 입력이라 중심값 방식이 성립하지 않는다.
- 곱 먼저 나눗셈 나중 방식이 정확하다는 논증(`ReferenceClahe` 주석)은 수학적 논증이며, 이를 직접 반증하는 실험은 하지 않았다.
- CI 에서의 속도는 미관측.

## Residual risk

- 속도: 구간 계산을 double 로 바꾼 값 약 +4 ms (중앙값 39.6 → 43.7). 예산 50 ms 와의 여유가 6 ms 로 줄었다. CI 러너가 느리면 `Performance_3072x3072_Within50ms` 가 먼저 흔들린다. 그러면 구간을 한 번만 계산해 uint16 으로 보관하는 안이 있다(메모리 19 MB).
- 평탄 취급 임계가 없다: 값 차이가 1e-37 수준인 영상도 정상 처리된다. 결과는 부동소수 표현 한계(비정규수) 안에서 정확하지만, 그런 영상이 실제로 올 일은 없다.
