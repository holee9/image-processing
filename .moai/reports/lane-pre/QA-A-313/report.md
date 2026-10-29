[2026-10-28 22:03 KST] [xpe-leader 대행 QA-A-313]

# QA-A-313 — 캘 알고리즘 현재 코드 기준선 재측정 (Refs #273)

xpe-pre 세션이 컨텍스트 포화로 멈춰(15:56, 21:25) 리더가 하위 작업자로 대행했다. 측정만 했고 `modules/`·`tests/`·`tools/`·CMake 는 바꾸지 않았다.

## 1. 목적과 기준
- O: PRE 캘 알고리즘 완성과 검증. 이 카드의 KR 은 "현재 코드가 고정 기준표의 어디까지 통과하는가"의 기준선 한 번 측정이다.
- 코드: `dev/preprocess` `eaf076f3`(코드 기준 `57ddbd2e`). 방법 커밋 `2719d90c`(결과 전), 결과 커밋 `1020b1aa`(증거). push 없음.
- 기준표: main `.moai/project/milestone-1-pre-basic.md` 의 B12 행과 "B16 합격 수치" 표를 그대로 썼다.

## 2. 결과

### B12 — 합격 (10-17 값과 동일)
| 항목 | 기준 | 10-17 | 이번 | 판정 |
|---|---|---|---|---|
| DarkBias 부호 있음 (Dark_15 / Dark_16) | \|DarkBias\| ≤ 5 화소값 또는 DarkReduction ≥ 10 dB | +1.6494 / −4.0542 | +1.6494 / −4.0542 | 합격 |
| FlatResidualPct offset+gain (Bright_29 / 30) | ≤ 1.0 % | 0.6428 / 0.6437 % | 0.6428 / 0.6437 % | 합격 |
| FlatResidualPct 결함 보정 포함 (판정 시험 A250c) | ≤ 1.0 % | 0.6420 / 0.6429 % | — | 측정 불가 |

측정 불가 사유(원문): `test_zz_a241_measure.cpp(2211): Expected equality … 8 / perRole["dark"] Which is: 7` — 생성기가 첫 다크를 `dark_excluded_first` 로 따로 기록해 시험의 다크 개수 단언(8)과 어긋난다. 시험 수정은 범위 밖.

### B16 — 미달 (판정 값이 QA-A-260 과 바이트 동일)
| 항목 | 기준 | 10-17 = 이번 | 판정 |
|---|---|---|---|
| dead·hot·nonlinear 0.2·noisy m=4·대조군 띠·줄 밖 행·계산 불가 규칙 | 행별 기준 | 1.000·1.000·1.000·0.766·0/448·266/261 등 | 합격 9 |
| 새 오경보 | ≤ 10 | 48 (대조군 위 31) | 불합격 |
| 줄 D_n \|비\| > 3 (프레임 1 / 2) | ≤ 6 | 10 / 15 | 불합격 |
| 줄 밖 열 \|D_n\| > 3 (프레임 1 / 2) | 깨끗한 출력 +10 % 이내 | 9 / 9 (깨끗한 8) | 불합격 |
| 보정 오차 z | 그룹마다 ≤ 1판 | 118 그룹×프레임 초과 | 불합격 |
| slope | ≥ 0.95 / 0.98 | 현재 주입에 항목 없음 | 측정 불가 |
| 프레임당 보정 시간 (QA-A-296 `timing.sh`, 3072×2560) | < 95 ms | s1 28.0 / s0 22.3 / s6 27.9 ms | 합격 |

시간 구간(s6, 31회 중 첫 회 제외 중앙값): 결함+이득 27.86 ms, 이득만 8.22 ms, 결함만 29.56 ms, 둘 다 끔 0.39 ms. QA-A-315 의 15 ms 목표 기준으로는 결함 단계가 병목이다.

## 3. 바뀐 값과 원인 후보
- 판정 값 변화 없음. score·overlaps·lines 파일은 바이트 동일, correct json 의 차이 96개 항목은 모두 평가 도구가 새로 추가한 `area_contract` 필드다.
- QA-A-296 이 값에 나타나지 않은 이유(후보, 확인 안 함): 평가 도구가 `defectLineInterpolation` 을 주지 않아 기본값(끔)으로 돌았고, 속도 변경은 출력 비트 동일 변경이다. 이 기준선은 "줄 보간 끔" 상태의 값이다.

## 4. 측정 방법 차이
- 도구 재빌드 없음(`ninja -n` "no work to do", `modules/preprocess/src` 는 HEAD 와 동일). DLL 이 `57ddbd2e` 로 빌드됐다는 직접 증거는 없다(`evidence/20_run_env.txt` SHA-256 기록).
- B16 스크립트는 QA-A-260 것을 경로만 바꿔 썼고, 판정 값이 아닌 진단 블록 P1~P3 은 뺐다.
- FlatResidualPct offset+gain 경로(QA-A-263 `21_dll_check`, 경로만 변경)는 A250c 가 실행 불가로 확인된 뒤, B12 값을 보기 전에 추가했다. 이 스크립트는 결과 커밋에만 있다.

## 5. 제안 (기준은 그대로)
1. 보정 단계를 `"defectLineInterpolation":"rows"` 로 한 번 더 재서 줄 보간 효과를 별도 열로 본다(QA-A-314 에서 함께).
2. A250c 의 다크 개수 단언을 고치는 시험 수정(pre 소유).

## 6. 증거
`evidence/51_b16_table_side_by_side.md.txt`(B16 표), `62_compare_with_260.txt`·`64_correct_json_diff.txt`(바이트 비교), `B12_signed_offset.out.txt`·`B12_dll_check.out.txt`·`B12_a250c_v2.out.txt`(B12 원문), `T1_timing.out.txt`(시간), `20_run_env.txt`(도구 SHA-256).

지운 임시 폴더: `D:/workspace-github/xpe-data/qa-313-tmp`(약 443 MB, 삭제 확인)
남긴 증거: `.moai/reports/lane-pre/QA-A-313/evidence/`(43개, 텍스트만)
남은 측정 프로세스: 0
