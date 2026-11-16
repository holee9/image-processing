[2026-11-16 22:04 KST] [xpe-pre] QA-A-334 — 전수 점검 주장 대조 6건 (사실 확인 전용, 소스 수정 없음)

검색 범위: 이 워크트리(dev/preprocess)의 `clients/`, `modules/preprocess/{src,tools,include}`. "출하 경로" = 시험 프로젝트(이름에 Tests 가 든 폴더, `modules/*/tests`)를 뺀 호출부.

## 1. C ABI 오프셋 생성이 첫 다크를 포함해 평균 — **참**, 출하 경로 영향: **있음(조건이 다름)**
- 코드: `xpe_calib_generate_offset.cpp:11` 기본 Mean, `xpe_calib_generate_offset_methods.hpp:23` `method{OffsetGenerationMethod::Mean}` (감사 B 인용, 이번에 다시 열지 않음 — 미재확인).
- 시험 밖 호출부(grep `xpe_calib_generate_offset`): `clients/ImageProcTest/Services/NativePreprocessPreviewService.cs:211`(대리자 바인딩), 실제 호출 `:446-447` `module.GenerateOffset([frame], 1, ...)` — **프레임 1장**만 넘긴다. 그 1장은 `GenerateOffsetXCal` (`:694-705`)이 고른 보정 파일 하나(`FindCalibration`, 순위 `:792-793` 이름 "cdark" 0, "dark" 포함 1)이다. 진단용 `Diagnostics/XpePreprocessSyntheticOracle.cs:256` 도 바인딩한다.
- 따라서 GUI 경로에서 "여러 장 중 첫 장 포함 평균"은 일어나지 않는다. 대신 오프셋이 다크 1장 그대로다. 그 1장이 세트의 첫 다크인지는 파일 선택 규칙에 달렸고, 이번에 실제 선택 결과를 실행으로 확인하지 않았다(미확인).
- 출하 생성 도구 `xpe_calib_fixture_gen.cpp:162` 는 첫 다크를 뺀다(리더 확인, 이번에 다시 열지 않음).

## 2. 온도 보정이 원시 화소 전체를 나눔 — **참**, 출하 경로 영향: **없음**
- 코드: `temp_compensate.cpp:51-55` 모든 화소 `px[i] / scale`. 파이프라인은 조건 없이 매번 부른다(`pipeline.cpp:495`, 앞에 실행 조건 없음 — `:475-494` 읽음).
- 온도는 설정 키 `detectorTempC`(`pipeline.cpp:114`), 기본 25.0(`:73`). T = T_ref 이면 `exp_T / exp_ref`(`temp_compensate.cpp:42-48`)는 같은 식이라 scale = 1.
- `detectorTempC` 를 쓰는 파일(Grep, clients·gui·modules·tools·tests 의 cs/cpp/hpp/h/py/json): `pipeline.cpp`, `temp_compensate.cpp`, `preprocess_api.h` 와 시험 파일 6개뿐. 시험 밖에서 온도를 넣는 호출부 없음.

## 3. 포화 출력값 `level / median(gain)` 이 오프셋 뺀 기준과 다름 — **참**, 출하 경로 영향: **조건부 있음**
- 코드: `saturation.cpp:68-71` `level / medianGain`, 적용 `:202-209` (raw ≥ level → S_out, 나머지는 S_out 으로 상한). 오프셋 항이 없다.
- 실행(`evidence/02_sat_run.py.txt` → `03_sat_run_out.txt`): cyan_test 원시 프레임에는 65535 화소가 없다(최댓값 44652, Bright_36). 그래서 같은 코드 경로를 `{"saturationLevel":40000}` 으로 켜서 Bright_34(평균 40499)를 출하 파이프라인(`xpe_preprocess_pipeline`, cyan_test 깨끗한 세트 `01_gen_clean_stdout.txt`)으로 돌렸다.
  - 포화로 처리된 화소(raw ≥ 40000, 5,723,143개)의 값: 39557.934
  - 같은 화소의 포화 처리 없는 출력: 중앙값 29469.73 (최소 28860.60, 최대 30208.43)
  - raw 39990~39999 화소(12,513개)의 출력: 중앙값 29478.05, 최대 29914.80
  - 포화 처리 없는 출력의 최댓값(raw < 40000): 30123.43
  - 즉 S_out 은 포화 직전 이웃 출력보다 약 10,080 높다(39557.9 대 29478.0).
- 언제 켜지는가: 설정 키 `saturationLevel`, 없으면 이득 파일의 기록(`pipeline.cpp:76-79`, `:178`). 이득 파일 기록은 `xpe_calib_generate_gain.cpp:368-416` 이 플랫 프레임 히스토그램 꼭대기 몰림에서 찾는다. 같은 실행에서 설정 없이 돌린 결과는 "off" 와 화소 차이 0개(cyan_test 플랫엔 몰림이 없어 기록이 없음). GUI(`clients`)에서 `saturationLevel` 을 넘기는 곳 없음(grep 0건). GUI 는 `xpe_calib_generate_gain` 을 부른다(`NativePreprocessPreviewService.cs:466`) — 포화가 섞인 플랫으로 이득을 만들면 기록이 생겨 켜진다(코드 경로상; 실제 포화 플랫으로 실행하지 않음 — 미확인).

## 4. `dose_levels` 단위와 CalSet 이름 — **참**(문서 근거가 원시 값), 출하 경로 영향: **없음**
- 문서: `preprocess_api.h:627-635` 단위 = 화소값, 근거로 "cyan_test CalSet 이름이 화소값".
- 적용 쪽 가로축: `gain_correct.cpp:394` `src[i]`. 파이프라인 단계 순서 0 readout, 1 temp, 2 offset, 3 nonlinearity, 4 gain (`pipeline.cpp:40`) — 이득 단계 입력은 오프셋 뒤 값.
- 측정(`04_calset_means.txt`, 다크 = Dark_08~16 9장 평균 11023.9):

| CalSet 이름 | 원시 평균 | 다크 뺀 평균 |
|---|---|---|
| 14037 | 13887.8 | 2863.9 |
| 17285 | 16952.1 | 5928.3 |
| 20985 | 20303.7 | 9279.8 |
| 30868 | 29506.2 | 18482.3 |
| 42677 | 40509.0 | 29485.2 |

- 이름은 원시 평균 쪽 값이다(어느 것과도 같지는 않다).
- 다항식 이득을 만드는 호출부: `xpe_calib_fixture_gen.cpp:1414`(옵션 `gainPoly`, 선량 `20000.0 * exposure` `:1408`) 외에 시험 밖에 없다. clients 에 `generate_gain_polynomial` 0건.

## 5. 4096칸 비선형 표 — 코드는 **참**(`nonlinearity_correct.cpp:234-240` 4095 초과를 마지막 칸으로), 출하 경로에서 **적재되지 않음**
- 표를 싣는 `xpe_calib_load_nonlin_lut` / 만드는 `xpe_calib_generate_nonlin_lut` 를 부르는 곳: src 자기 파일과 시험뿐, clients·tools 0건(grep). 표가 없으면 이 분기는 돌지 않는다(`nonlinearity_correct.cpp` 의 `g_calib.nonlin_lut` 사용 분기).

## 6. 결함 코드 의미 불일치 — **참**, 보정 결과 영향: **없음**
- SRS FUNC-003 (`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:37`): 1=dead, 2=hot, 3=stuck, 4=noisy.
- 1판 (`xpe_defect_gen.cpp:49-54`): 1=dead/stuck(다크), 2=hot/noisy(밝은), 3=둘 다.
- 2판 (`xpe_bpm_v2.cpp:1258`): 밝은 쪽 특성이면 1, 다크 쪽이면 2 (감사 B 의 `:1312` 는 현재 `:1258`).
- 읽는 쪽: 적재 `xpe_calib_load_defect_map.cpp:91` 는 0 아닌 값을 센다; 보정 `defect_correct.cpp:72`, `:76` 은 `mask != 0` 만 본다. clients 에서 결함 코드 값을 종류별로 읽는 곳은 찾지 못했다(grep "BpmCode", "dead" 등, cs). 따라서 값의 의미는 보정 영상에 쓰이지 않는다. 화면이나 보고에서 종류를 표시하는 곳이 있는지는 위 검색 밖 — 미확인.

## 임시 폴더
- `D:/workspace-github/xpe-data/qa-334-tmp` 삭제.
