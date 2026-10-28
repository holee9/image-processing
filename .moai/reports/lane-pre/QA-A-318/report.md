[2026-10-29 07:26 KST] [xpe-leader 대행 QA-A-318]

# QA-A-318 보고 — FT 툴 세트(20250501_12)에서 PRE offset·결함 맵·이득·보정 vs 기존 엔진 (Refs #278)

- 코드 기준: `dev/preprocess` HEAD `e44fd0c7`. DLL `build/ci-preprocess/bin/xpe_preprocess.dll` sha256 `cd411f99…5356`.
- 입력(NAS, 읽기만): `Z:/공통자료/Solution팀 자료/소프트웨어 자료/자체 개발/FT툴 관련/202550526/20250501_12/`.
- 커밋: `eeb2e83f`(1단계, 비교 전), `d3ef6769`(2~5단계). 하네스가 작업자의 보고서 쓰기를 막아 본문은 리더가 옮기고 §8 을 덧붙였다.
- 근거: `.moai/reports/lane-pre/QA-A-318/evidence/`, 방법·입력 규칙 `00_method_and_feed_rules.md`.
- 실행 안 함: caleng.exe·RunCal.exe·2G DLL·TAS. 코드 변경 없음. 공개 API 를 부르는 임시 하니스만(`13_qa318_gen.cpp.txt`).

## 1. 파일 의미 (확인된 사실, `11_facts.txt` / `10_facts.py.txt` / `12_sha256_inputs.json` / `01_` / `02_`)

| 항목 | 측정 |
|---|---|
| MasterDark | floor(mean(dark03_00..09)) 와 100.0000 % 일치. rint 55.1978 %, 첫 장 뺀 9장 평균의 floor 69.4635 % |
| CalData_20·CalData_6 의 dark.raw | MasterDark 와 바이트 동일 |
| `__OC__` | int16 으로 읽은 (원시 − MasterDark) 와 100 % 일치(Bright02_00..09, Dark04_00, Bright11..14_00, 원시 bright03_00..19, bright05_00..19). 음수도 바닥 처리 없이 저장 |
| MasterBright | trunc(mean(int16 Bright02_00..09 `__OC__`)) 와 100.0000 % 일치(floor 99.7701 %) |
| BrightAvg | 시험한 후보 모두 0.63 % 이하 일치 → **미확인** |
| CalData_20 구성 | Bright01..10 = Bright02_00..09 `__OC__`. bright11..20 = Bright03_{00,01,03,04,07,11,13,17,18,19} `__OC__` (바이트 동일) |
| 원시 밝은 프레임 | Bright02 10장(순 신호 3104.8~3160.7, 한 선량). bright03 20장(순 신호 49.1~11344.1, 여러 선량). bright05 는 _10..19 가 _00..09 와 바이트 동일 |
| 엔진 결함 맵 | BPMap.map, Total_Defect_Map, TOTAL_BPM(images·corrected), TOTAL_BPM_original 이 모두 같은 12,898 화소 집합. BPM_with_Filter 12,774 ∪ BPM_without_Filter 12,458 = 이 집합. AED_BPM 312, Stitch_BPM·PositionMap 0 |
| 엔진 줄 결함 | 행 0개, 열 15~19 의 5개(줄의 50 % 넘게 결함인 것만 셈, 탐색 규칙) |
| corrected 출력 크기 | 14,993,280 B = 3040 × 2466 × 2. `CalEng.ini` Cut 15/2/17/92 와 산술로 맞음 |
| 엔진 설정 원문 | `CalEng.ini`: DarkFile=MasterDark, Bright0..5=MasterBright 6번, BadPixelMapFile=TOTAL_BPM, BPMStep=512, MaxValue=32767, CalUnsigned=0. `runcal.ini`: BPMCondition=45. `S4335-WS.par`: UseDarkBPM=0, CalStepCount=6 |

## 2. offset — `xpe_calib_generate_offset` (mean)
offset 생성기에는 첫 다크 제외 규칙이 없다(받은 프레임 전부 평균, `xpe_calib_generate_offset_methods.cpp:290-302`). 첫 장 제외는 결함 맵 2판에만 있다(`xpe_bpm_v2.cpp:52,63`).

| 판 | floor(v)==MasterDark | rint(v)==MasterDark | v − MasterDark | 대조군 max\|v − numpy 평균\| |
|---|---|---|---|---|
| O-a: 10장, 엔진과 같은 입력 | **100.0000 %** | 55.1978 % | 0 ~ 0.8999, 평균 0.4488 | 1.95e-4 |
| O-b: 첫 장 뺀 9장 (탐색 규칙) | 69.4635 % | 25.3564 % | −0.333 ~ 21.444. Cut 안쪽 엔진 결함 아닌 화소 최대 2.0 | 1.09e-4 |

O-a 의 offset 은 float 값이고, 엔진 MasterDark 는 그 값의 floor 다.

## 3. 결함 맵 2판 — `xpe_bpm_generate_v2`
입력: 원시 다크 dark03_00..09(기본 설정이라 첫 장 빼고 9장). 입력 조건을 모두 맞췄고 반환 코드 0(`15_generate_log.txt`).

| 판 | 밝은 프레임 | 선량 묶음 | PRE 결함 | 엔진 TOTAL_BPM | 공통 | PRE만 | 엔진만 |
|---|---|---|---|---|---|---|---|
| V-a | 원시 Bright02 10장 | 1 | 1,122,436 | 12,898 | 5,239 | 1,117,197 | 7,659 |
| V-b | CalData_20 과 같은 프레임의 원시판 20장 (탐색 규칙) | 7 | 1,154,285 | 12,898 | 7,679 | 1,146,606 | 5,219 |
| V-c | V-b + 여백 15/2/17/92 (탐색 규칙) | 7 | 937,573 | 12,898 | 7,931 | 929,642 | 4,967 |

- BPM_with_Filter / BPM_without_Filter 와 따로: V-c 기준 공통 7,915 / 7,864(`22_compare.txt`).
- PRE 특징별 표시 수: V-a 다크 평균 976,843, 평탄 응답 155,834, 밝은 잡음 78,465. V-c 다크 평균 931,827, 나머지 특징은 합쳐 1만 화소 남짓.
- 엔진만 결함인 화소: 세 판 모두 PRE 의 어떤 특징도 표시하지 않았다(특징 비트 0).
- 줄 결함: 엔진 행 0·열 15~19. PRE V-c Cut 안쪽 행 537·열 15~19.
- 다크 평균 특징의 위치(`23_darkmean_rows.txt`): 행 mod 32 = 24~31 인 행에서 49.4 %, 나머지 행 0~0.7 %. 홀수 열 24.7 %, 짝수 열 0.2 %.
- MasterDark 자체 패턴: 행 mod 32 = 24~31 에서 홀수 열 평균 2476.66, 짝수 열 2497.25(차 20.6). 다른 행에서는 차 0.75.

## 4. 이득 — `xpe_calib_generate_gain`
- G-a: 원시 Bright02 10장, dark_reference = MasterDark. 대조군 G-b(`__OC__` int16, 음수 0, dark NULL)와 바이트 동일, numpy 재계산 최대 차 1.1e-7.
- 엔진 이득 표 파일은 폴더에 없다. QA-A-316 과 같은 정의 G_c = MasterBright / mean(MasterBright).

| 범위 | n | 중앙값(G−G_c) | p1 | p99 | 최대 \|.\| | 피어슨 |
|---|---|---|---|---|---|---|
| 전체 | 7,864,320 | 3.59e-6 | −1.56e-4 | 1.59e-4 | 0.0045 (엔진 결함 화소) | 0.99999987 |
| 엔진 결함 아님 | 7,851,422 | 3.47e-6 | −1.56e-4 | 1.59e-4 | 0.0006 | 0.99999987 |
| Cut 안쪽, 결함 아님 | 7,483,742 | −8.0e-7 | −1.56e-4 | 1.49e-4 | 0.0002 | 0.99999820 |

## 5. 보정 결과 — `xpe_offset_correct` → `xpe_gain_correct`
- 원시 bright03 입력, O-a offset + G-a 이득, 결함 보정 없음. 대상은 엔진 `OcGc` 가 있는 8장. PRE 출력을 Cut 대로 잘랐고 ±3 화소 상관 최대 정렬이 8장 모두 (0, 0).
- 대조군: PRE offset 출력이 floor(clip(원시 − PRE offset) + 0.5) 와 100 % 일치(`offset_correct.cpp:92-97`).
- 통계는 엔진 결함 아닌 화소 7,483,742 개.

| 프레임(엔진 평균) | 피어슨 | 직선 (엔진 = k·PRE + b) | 엔진/PRE 중앙값 (p1~p99) | 잔차 표준편차 |
|---|---|---|---|---|
| bright03_01 (78) | 0.975705 | 1.0690·PRE + 1.38 | 1.0869 (1.075~1.104) | 0.59 |
| bright03_05 (333) | 0.993776 | 1.0834·PRE + 1.69 | 1.0886 | 0.59 |
| bright03_09 (2,139) | 0.998947 | 1.0878·PRE + 2.51 | 1.0891 | 0.62 |
| bright03_13 (5,324) | 0.999427 | 1.0885·PRE + 3.20 | 1.0891 | 0.76 |
| bright03_15 (12,055) | 0.999994 | 1.0892·PRE − 1.14 | 1.0891 (1.0889~1.0894) | 1.24 |

03·07·11 도 같은 경향(`22_compare.txt`). `OcGcDc` 는 결함 아닌 화소에서 `OcGc` 와 통계가 같다.

## 6. 확인된 사실 / 추정 / 미확인
- 확인된 사실: §1~§5 표 값(명령·근거 파일 있음).
- 추정: DLL 이 HEAD 소스로 빌드됐다는 것(수정 시각 비교만). 출력 자르기 위치 (2,15). 엔진 `OcGc` 가 결함 보정 전 단계라는 것(이름 기준). PRE 다크 평균 특징의 대량 표시가 MasterDark 주기 패턴 때문이라는 것(위치 일치까지만 측정, 인과는 코드로 추적 안 함). 저선량 비율 흩어짐 원인이 offset floor 와 정수 출력이라는 것.
- 미확인: `BrightAvg` 정체, `CalEng.ini` 경로(`20231120_23E21G1Y`)와 이 폴더 실행의 동일 설정 여부, 엔진 BPM 입력 프레임, 엔진/PRE 척도 1.089 의 출처, 엔진 이득 표 원본, `DotDefectBPM`·`Bright03_bpm_14` 집합 동일성, 검출기 모델·일련번호, `_14` 폴더 바이트 동일성.
- 탐색 규칙(사용자 승인 기준 아님): O-b 첫 장 제외, V-b 프레임 선택, V-c 여백, 줄 결함 50 % 기준, ±3 화소 정렬 탐색.
- 품질 우열: 미결정.

## 7. 임시 폴더·증거
- 지운 임시 폴더: `D:/workspace-github/xpe-data/qa-318-tmp`(526 MB). 남은 프로세스 0.
- 남긴 증거: `.moai/reports/lane-pre/QA-A-318/evidence/`(00~02, 10~15, 20~24). 생성물은 SHA-256 만(`24_generated_sha256.txt`).

## 8. 리더 확인 (2026-10-29 07:27)
- 커밋 `eeb2e83f`(비교 전)·`d3ef6769` 순서 확인, 인덱스 스테이징 61개 유지.
- 핵심 수치 원문 대조: `22_compare.txt` 의 결함 수(V-a 1,122,436 / V-b 1,154,285 / V-c 937,573, 엔진 12,898)와 이득 피어슨 0.99999987, `23_darkmean_rows.txt` 의 행 mod 32 = 24~31 표시율 0.494·홀수 열 0.247 과 MasterDark 홀수/짝수 열 평균 2476.66/2497.25 를 직접 읽었다. 생성기 반환 코드 전부 0(`15_generate_log.txt`).
- MasterDark = floor(mean(dark03_00..09)) 100 % 는 리더가 07:1x 에 따로 numpy 로 재현했다(QA-A-318 카드).
- 결론:
  - offset 생성은 기존 엔진과 같다(PRE 평균의 floor = MasterDark 전 화소).
  - 이득 생성은 사실상 같다(피어슨 0.99999987, 정상 화소 최대 차 0.0006).
  - 보정 결과는 상관 0.976~0.99999 이고 엔진 출력이 PRE 의 약 1.089 배다 — 척도 출처 미확인.
  - **결함 맵 2판은 이 검출기에서 크게 다르다**: PRE 가 약 94만~115만 화소(전체의 12~15 %)를 결함으로 표시, 엔진은 12,898. PRE 표시의 대부분은 다크 평균 특징이고, 그 위치가 MasterDark 의 주기 구조(32행 중 8행 × 홀수 열, 약 −20.6)와 겹친다. 이 구조를 PRE 가 결함으로 잡는 경로는 코드로 확인하지 않았다 → QA-A-319 진단 카드.
