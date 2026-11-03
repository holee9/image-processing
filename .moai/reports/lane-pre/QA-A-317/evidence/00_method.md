[2026-10-28 23:26 KST] [xpe-leader 대행 QA-A-317] — 방법(결과 전 커밋)

# QA-A-317 방법 — A(2G 출력) / B(PRE + 2G 맵) / C(PRE + PRE 이득 맵), 180장 (Refs #278)

코드: `dev/preprocess` HEAD `4dec0a66`. `build/ci-preprocess` 는 `ninja -n xpe_preprocess xpe_common` / `-n xpe_calib_fixture_gen` 모두 "no work to do"(재빌드 안 함). `xpe_preprocess.dll` sha256 `0f15bda3…92d6`(QA-A-316 과 같음). `git diff HEAD -- modules/preprocess/src modules/preprocess/include modules/common` 비어 있음. 모듈·시험·도구 코드는 바꾸지 않는다. 2G DLL·caleng.exe·TAS 는 실행하지 않는다.

## 1. B — PRE + 2G 맵 (QA-A-296 §10 방법 그대로)

- 실행: `cd build/qa296-work && ./run180.sh <T>/out_b ',"saturationLevel":12800,"defectLineInterpolation":"rows"'` — 기존 스크립트·하니스(`qa296_run.exe` sha256 `6ec41e97…1901`) 그대로. 보정 `pre-w3072-20261022-1240/<set>/xcal`(읽기만). 설정은 `pre-w3072-20261023-1748/config.json` 과 같은 문자열.
- 판정: 새 출력 180개의 sha256 을 `pre-w3072-20261023-1748/<set>/out/NNN.f32` 와 한 장씩 비교(같음/다름 개수).

## 2. C — PRE 이득 맵 + 2G dark.raw offset + 2G BPMap defect

- 이득 맵: QA-A-316 하니스(`19_qa316_gen.cpp.txt`, 바꾸지 않음)를 `19_hb316.bat.txt` 와 같은 명령으로 다시 컴파일해 6세트 실행(`gen_c.py`). 입력 규칙은 316 그대로(Bright01..10, int16→uint16 max(0,v), dark NULL, metadata NULL).
- **입력 준비 확인(이미 실행, `11_…`~`14_…`)**: 새 `gain.xcal` 의 파일 sha256 은 316 목록과 6세트 모두 다르다. XCal 헤더에 생성 시각(`created_epoch_ms`, `xcal_format.h:14`)이 있고 설정 JSON 에 `saturation_decided_at` 시각(316 `22_compare_gain.txt`)이 있어 파일 해시는 실행마다 달라진다. 대신 (a) 316 의 비교 스크립트(`21_compare_gain.py.txt`, 경로만 바꿈)를 새 파일에 돌린 출력이 316 `22_compare_gain.txt` 와 해시 줄·포화 기록 줄을 뺀 54줄 중 나머지 전부 같다(포화 기록도 `decided_at` 을 빼면 같다), (b) 같은 하니스를 두 번 돌린 파일의 payload sha256 과 시각을 뺀 설정 sha256 이 6세트 모두 같다. 316 파일 자체는 316 임시 폴더와 함께 지워져 payload 바이트 비교는 할 수 없다(미확인으로 보고).
- offset·defect: B 가 쓴 변환 = `xpe_calib_fixture_gen.exe --caldata`(sha256 `0f391114…1ad8`, B 의 `tool_sha256.txt` 와 같음; 경로 `xpe_calib_fixture_gen.cpp:1269-1345`: dark.raw → `xpe_calib_generate_offset`, bright04/05/06 → 이득, BPMap.map → 결함). caldata 폴더에 `dark.raw`, `BrightAvg.raw`×3(bright04..06), `BPMap.map` 을 넣어 다시 돌린 결과 offset·gain·defect 18개 파일 sha256 이 `pre-w3072-20261022-1240/<set>/xcal` 과 모두 같다(`14_conv_b_out.txt`). C 세트 = 이 offset.xcal + defect.xcal + 위 PRE gain.xcal.
- 실행: `run180_c.sh`(run180.sh 에서 X 한 줄만 바꿈) 같은 설정 문자열.
- B 와 C 의 차이: 이득 맵의 출처·정규화 상수, 그리고 C gain.xcal 에만 있는 포화 기록(`undetermined`). 포화 기록은 출력에 영향이 없다: 설정에 `saturationLevel` 이 있으면 그 값을 쓴다(`pipeline.cpp:186-188`). 알림 내용만 다를 수 있다.

## 3. 비교 (A/B/C, `analyze.py`)

- 값 영역: QA-A-294/296 `cmp2g.py` 변환 `v = where(box, min(rint(p·C/M), 12800), 0)`, box 행 2..2467·열 15..3054, C = `cmp2g.py` 의 CMEAS. M_B = mean(BrightAvg, uint16 읽기)(= cmp2g.py 의 Cp, B 이득 맵 정규화 평균). M_C = PRE 생성기가 C 이득 맵을 나눈 평균 mG(`xpe_calib_generate_gain.cpp:304-322` 과 같은 식을 numpy 로 계산, `gen_c.json`). A = `out/<set>/NNN.raw` 그대로.
- 화소 분류(cmp2g.py): 정상 / 포화(raw ≥ 12800) / 결함(BPMap) / 전체(box).
- 쌍 A–B, A–C, B–C: 정수 |차이| 히스토그램 → 일치율(차이 0), 구간 0/1/2–5/6–20/>20, p50/p99/최대, 평균 |차이|, 평균 부호 차이. 세트별·전체.
- 재현 확인: `cmp2g.py` 를 임시 폴더에 복사해(원본 위치에 json 을 쓰지 않도록) 새 B 출력에 돌린 값을 QA-A-296 `cmp2g_out_final.txt` 와 비교.
- B–C 는 같은 엔진·같은 설정·같은 offset/defect 파일이므로 차이는 이득 맵 출처 하나다(정규화 상수는 M_B/M_C 로 각각 되돌린 뒤 비교).

## 4. 보정 품질 지표 (같은 코드를 A·B·C 에)

- 하니스 `qa317_metrics.cpp` 가 기존 헤더 `xpe_pre_stripe.hpp`(`measureStripe`)·`xpe_pre_defect_metrics.hpp`(`measureDefectOracle`)를 그대로 부르고, 평탄 잔차는 `xpe_pre_e2e.cpp:411-430` 식(100·표본 표준편차/평균)을 옮겼다. 합성 대조(`15_ctrl_metrics_out.txt`): 상수 영상 평탄 잔차 0·결함 잔차 100(정답 100), 잡음만 줄무늬 비 1.01, 짝수 행 +3 줄무늬 비 11.8.
- 공통 ROI = box ∩ B 이득 유한·>0 ∩ C 이득 유한·>0(결함 제외 없음, `xpe_pre_e2e.cpp:432` 와 같이 공간 제외 없음; box 는 A 가 box 밖을 0 으로 두기 때문).
- 평탄 잔차·줄무늬 비: 이름에 "Empty" 가 있는(물체 없는 노출) 프레임 중 ROI 안 raw ≥ 12800 화소가 0 인 것만. 물체 영상은 행·열 평균에 물체 구조가 들어가 이 두 지표가 정의(평탄 입력)에 맞지 않으므로 측정하지 않는다.
- 결함 잔차: 모든 프레임, 정답 = box 안쪽 한 화소(행 3..2466, 열 16..3053)의 BPMap 위치.
- 판정 규칙(이 카드에서 미리 정함, 제품 합격 기준 아님): 지표가 낮을수록 그 지표상 나은 방향. 한 쌍에서 측정한 모든 프레임의 부호가 같고 프레임별 상대 차이 중앙값이 5 % 이상이면 "그 지표에서 낮음/높음"으로 적는다. 그렇지 않으면 미결정. 동등 여유 기준을 정한 사양이 없어 "동등"은 판정하지 않는다. 정답 영상이 없으므로 지표가 낮다는 것이 보정이 맞다는 뜻은 아니다.

## 5. 취득 부담

- caleng 입력: `cal_source` 폴더 목록(`gen_c.json`)의 파일 수.
- PRE 최소 프레임: 코드 인용(gain `xpe_calib_generate_gain.cpp:228`, offset `xpe_calib_generate_offset_methods.cpp:30`, 결함 2판 `xpe_bpm_v2.cpp:54-55,70,696-697`, 도구 `xpe_calib_fixture_gen.cpp:188,530`).
- PRE 이득 생성 시간: `qa317_time.cpp`, 프레임을 메모리에 한 번 읽은 뒤 생성기 호출만 7회, 첫 회 제외 중앙값, 6세트. offset·결함 맵은 이 자료로 생성 불가(QA-A-316 §4)라 시간을 재지 않는다.

## 6. 비교 계약 (`image-processing/tools/calibration/comparison_manifest.py` 4판)

- 대표 프레임 1장(s4_csi_pre 001, Empty)에 대해 A–B, A–C 를 `e2e-non-isolated` 로, B–C 를 같은 표지로 매니페스트를 만들어 `validate` 를 돌리고 출력을 원문 그대로 남긴다. 파일은 임시 폴더 안에 복사해 경로가 매니페스트 기준 폴더 안에 있게 한다. 거부되면 사유 코드를 그대로 적는다.

## 7. 임시·종료

임시 폴더 `D:/workspace-github/xpe-data/qa-317-tmp` 하나. 출력 영상은 sha256 목록만 남기고 끝에 폴더째 지운다.
