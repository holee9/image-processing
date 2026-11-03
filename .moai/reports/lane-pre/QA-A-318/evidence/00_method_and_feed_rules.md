[2026-10-29 07:19 KST] [xpe-leader 대행 QA-A-318] 1단계 사실·방법·입력 규칙 (비교 결과 전에 커밋)

# QA-A-318 방법과 입력 규칙 (Refs #278)

- 코드 기준: `dev/preprocess` HEAD `e44fd0c7`. 공개 API 를 직접 부르는 임시 하니스(`qa318_gen.cpp`, 임시 폴더, 모듈·시험·도구 코드 무변경).
  DLL `build/ci-preprocess/bin/xpe_preprocess.dll` sha256 `cd411f99…5356`, 수정 시각 2026-10-29 06:57:46.
  `modules/preprocess/src`·`include`, `modules/common` 의 마지막 커밋 시각 06:57:39, 작업 트리 변경은 `modules/preprocess/tests/` 4개뿐(`git status --short -- modules`).
  → DLL 이 HEAD 소스로 빌드됐다는 것은 시각 비교에 의한 **추정**(재빌드로 확인하지 않음).
- 입력 폴더(NAS, 읽기만): `Z:/공통자료/Solution팀 자료/소프트웨어 자료/자체 개발/FT툴 관련/202550526/20250501_12/` (이하 `B`).

## 1. 1단계 확인 사실 (근거: `11_facts.txt`, 스크립트 `10_facts.py.txt`, 해시 `12_sha256_inputs.json`, 목록 `02_…`, 설정 원문 `01_…`)

| 항목 | 측정 결과 |
|---|---|
| `MasterDark` | = floor(mean(`dark03_00..09`)) 100.0000 % (rint 55.1978 %, 최대 |MasterDark−평균| 0.9000). 첫 장 뺀 9장 평균의 floor 와는 69.4635 % |
| `12_CalData_20/dark.raw`, `12_CalData_6/dark.raw` | `MasterDark` 와 바이트 동일(sha256 `122b11bd…`). `AEDDark.raw` 는 다른 파일(평균 5888.6) |
| `__OC__` | `Bright02_00..09`, `Dark04_00`, `Bright11..14_00`, 소문자 원시 `bright03_00..19`→`Bright03_NN__OC__`, `bright05_00..19`→`Bright05_NN__OC__` 모두 int16 으로 읽으면 (원시 − MasterDark) 100.0000 %. 음수는 int16 음수(uint16 으로는 래핑)로 저장 — 바닥 처리 없음 |
| 원시 밝은 프레임 | `Bright02_00..09`(10장), 소문자 `bright03_00..19`(20장, 순 신호 평균 49.1~11344.1 로 선량이 다름), `bright05_00..19`(`_10..19` 는 `_00..09` 와 바이트 동일 → 서로 다른 것 10장), `bright06/07_00..04`, `Bright11..14_00`, `bright03_0..9`(10장, `__OC__` 짝 없음) |
| `MasterBright` | = trunc(mean(int16 `Bright02_00..09__OC__`)) 100.0000 % (floor 99.7701 %, rint 55.4745 %) |
| `BrightAvg` | `MasterBright` 와 0.63 % 일치. 시험한 후보(Bright02 OC, CalData_20 Bright01..10, CalData_6 bright01..06, Bright03 OC 20장 평균; int16/uint16; rint/floor/trunc) 어느 것과도 0.6 % 이하 → 무엇의 평균인지 **미확인** |
| `12_CalData_20` 구성 | `Bright01..10` = `Bright02_00..09__OC__` 바이트 동일, `bright11..20` = `Bright03_{00,01,03,04,07,11,13,17,18,19}__OC__` 바이트 동일 |
| `12_CalData_6` | `bright04..06` = `Bright02_07..09__OC__`, `bright01..03` = `Bright03_{17,19,05}__OC__`; `BPMap.map`·`PositionMap`·`Total_Defect_Map` 은 CalData_20 과 동일 |
| 결함 맵 | `BPMap.map`(uint8, 0/255) 12,898 = `Total_Defect_Map`(0/16383) 12,898 = `images·corrected/TOTAL_BPM` 12,898 = `TOTAL_BPM_original` 12,898 — 모두 같은 집합. `BPM_with_Filter` 12,774, `BPM_without_Filter` 12,458, 둘의 합집합 = `Total_Defect_Map` 집합(일치 True). `AED_BPM` 312, `Stitch_BPM`·`PositionMap` 0, `DotDefectBPM`·`Bright03_bpm_14` 12,898(집합 동일성은 **미확인**) |
| 줄 결함(엔진 맵) | 행 50 % 초과 결함 0개, 열 50 % 초과 결함 = 열 15~19 (5개) |
| `corrected/*_Oc*.raw` 크기 | 14,993,280 B = 3040 × 2466 × 2 = (3072−15−17) × (2560−2−92) × 2 → `CalEng.ini` `LeftCut 15/TopCut 2/RightCut 17/BottomCut 92`(01_ 행 17~20)와 맞는 크기(산술 일치, 잘린 위치는 **추정**) |
| 기존 엔진 설정 | `CalEng.ini`(01_ 행 2~13): `DarkFile=…\images\MasterDark.raw`, `BrightCount=6`, `Bright0..5=…\images\MasterBright.raw`, `BadPixelMapFile=…\corrected\TOTAL_BPM.raw`, `BPMStep=512`(행 21), `MaxValue=32767`, `CalUnsigned=0`. `runcal.ini` 행 4 `BPMCondition=45`. `S4335-WS.par` 행 31 `UseDarkBPM=0`, 행 78 `CalStepCount=6` |
| 설정과 이 폴더의 연결 | `CalEng.ini` 의 경로는 `…\20231120_23E21G1Y\…` 로 **이 폴더(20250501_12)가 아니다**. 파일 이름 구조만 같다. 이 폴더를 만든 실행이 같은 설정이었는지는 **미확인** |
| 검출기 모델·일련번호 | **미확인** (`CalEng.ini ModelNumber=2097154` 는 다른 폴더의 설정) |

## 2. 입력 규칙 (비교 전에 고정)

### 2.1 offset — `xpe_calib_generate_offset`
- 생성기는 받은 프레임 전부를 쓴다: mean 방식은 `num_frames` 장 모두 더해 나눈다(`xpe_calib_generate_offset_methods.cpp:290-302`). 첫 다크 제외 규칙은 offset 생성기에 **없다**(제외 규칙은 결함 맵 2판 `xpe_bpm_v2.cpp:52,63` 에만 있다). 입력은 UINT16(`:54-55`), 출력은 float32 XCal(`xpe_calib_generate_offset.cpp:152-153`), `config=NULL` → mean.
- (O-a) 같은 조건: `dark03_00..09` 10장 그대로 — 엔진 `MasterDark` 와 같은 입력(§1).
- (O-b) 첫 장 제외: `dark03_01..09` 9장 — 결함 맵 2판의 기본 규칙을 offset 에 옮겨 본 것(**탐색 규칙**, 사용자 승인 기준 아님).
- 비교: PRE float 값 v 와 `MasterDark` M — floor(v)==M, rint(v)==M 비율, |v−M| 최대·평균, numpy 로 계산한 float64 평균과의 최대 차(하니스 대조군).

### 2.2 결함 맵 2판 — `xpe_bpm_generate_v2`
- 계약: 다크·밝은 프레임 모두 offset 이 빠지지 않은 UINT16 원값(`preprocess_api.h:2139,2141`), 다크 첫 장 기본 제외(`xpe_bpm_v2.cpp:52`), 제외 뒤 다크 ≥5(`:53,696`), 밝은 ≥6(`:54,697`), 선량 묶음 비 1.25(`:55`), 묶음 ≤8(`preprocess_api.h:2085`).
- 다크 = 원시 `dark03_00..09` 10장, 기본 설정(첫 장 제외 → 9장).
- (V-a) 밝은 = 원시 `Bright02_00..09` 10장(한 선량, §1 순 신호 3104.8~3160.7).
- (V-b) 밝은 = 엔진 `12_CalData_20` 과 같은 프레임의 **원시판**: `Bright02_00..09`(=CalData_20 Bright01..10) + 소문자 `bright03_{00,01,03,04,07,11,13,17,18,19}`(=CalData_20 bright11..20) — 20장, 다선량. 엔진이 `BPM_with/without_Filter` 를 정확히 어느 프레임으로 만들었는지는 **미확인**이므로 이것은 "CalData_20 과 같은 프레임" 이라는 **탐색 규칙**이다.
- (V-c) V-b 와 같고 `margin_left/top/right/bottom = 15/2/17/92`(엔진 Cut 값, **탐색 규칙**).
- 비교: PRE 0 아닌 화소 집합 vs 엔진 `TOTAL_BPM`(=Total_Defect_Map=BPMap) 집합, 그리고 `BPM_with_Filter`·`BPM_without_Filter` 각각. 전 화면과 Cut 안쪽(행 2..2467, 열 15..3054) 두 범위. 줄 결함은 행·열의 50 % 초과 결함으로 센다(**탐색 규칙**). 입력 조건을 못 맞추면 반환 코드와 코드 근거를 적고 그 판은 멈춘다.

### 2.3 이득 — `xpe_calib_generate_gain`
- 엔진 입력(§1): `MasterBright` = trunc(mean(int16 (Bright02 원시 − MasterDark))) 를 6번.
- (G-a) PRE 같은 입력: 원시 `Bright02_00..09` + `dark_reference = MasterDark`(UINT16). 생성기는 프레임마다 max(0, f−d) 를 더해 평균, 전 화소 평균으로 정규화(`xpe_calib_generate_gain.cpp:292-322`).
- (G-b) 대조: `Bright02_NN__OC__` 를 int16 으로 읽어 max(0,v) uint16 으로, `dark_reference=NULL` (QA-A-316 §2 규칙). G-a 와 수학적으로 같아야 한다(하니스 대조군).
- 엔진 쪽 비교값: 엔진의 이득 표 자체는 폴더에 없다(**미확인**). QA-A-316 과 같은 정의로 G_c = MasterBright_int16 / mean(MasterBright_int16) 를 만들어 화소별 G−G_c 의 중앙값·p1·p99·최대 절댓값과 피어슨 상관을 전 화소, 엔진 결함 아닌 화소, Cut 안쪽 결함 아닌 화소에서 낸다. 우열 판정 안 함.

### 2.4 보정 결과 — `xpe_offset_correct` → `xpe_gain_correct`
- 원시 `bright03_NN` 이 있으므로 offset 우회 없이 원시 입력을 쓴다(§1).
- PRE: O-a offset + G-a 이득을 불러 `bright03_{01,03,05,07,09,11,13,15}`(엔진 `corrected/Bright03_NN_OcGc.raw` 이 있는 8장) 에 적용. 결함 보정은 하지 않는다(엔진 `OcGc` 도 Dc 전 단계 — 이름 기준 **추정**).
- 엔진 `OcGc` 는 3040×2466, PRE 출력은 3072×2560 → PRE 를 행 2..2467, 열 15..3054 로 잘라 맞춘다(§1 추정 위치. 잘린 위치는 상관이 가장 높은 정렬을 ±3 화소에서 찾아 확인 — **탐색 규칙**).
- 엔진 출력 값의 척도(정규화·MaxValue)는 **미확인**이므로 비교는 숫자 사실만: 엔진 결함 아닌 화소에서 피어슨 상관, 최소제곱 직선(기울기·절편), 직선 잔차의 p1/p99, 비율 엔진/PRE 의 중앙값과 p1/p99. `OcGcDc` 는 결함 보정이 들어 있어 같은 비교를 결함 아닌 화소에서만 참고로 낸다.

## 3. 임시 폴더·증거
임시 폴더 `D:/workspace-github/xpe-data/qa-318-tmp` 하나. 생성 맵·xcal·보정 출력은 SHA-256 만 남기고 끝에 지운다. 하니스·비교 스크립트는 `.txt` 증거로 남긴다. caleng.exe·RunCal.exe·2G DLL·TAS 실행 없음.
