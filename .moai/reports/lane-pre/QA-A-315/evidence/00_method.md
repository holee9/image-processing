# QA-A-315 측정·프로파일 방법 (최적화 전)

- 기계: 12th Gen Intel Core i7-12700 (코어 12, 논리 프로세서 20). 측정 시작 시 CPU 부하 11 % (Win32_Processor LoadPercentage).
- 코드: `dev/preprocess` HEAD `363af038`, `build/ci-preprocess` (RelWithDebInfo), DLL sha256 `e4bc0f3b8b18e4be5b9a6dd9f7827003e32eb03aad7a574691559b8c63516700` (ninja: no work to do).
- 하니스: `build/qa296-work/qa296_run.exe` (sha256 `6ec41e97…1901`), `build/qa296-work/timing.sh` (= QA-A-313 `34_timing.sh.txt`, diff 없음).
- 시간: QA-A-296 §6.3 그대로 — 입력 `batch-20261022-1230/in/<set>/img/001.raw`, 보정 `pre-w3072-20261022-1240/<set>/xcal`, 31회 중 첫 회 제외 중앙값. 세트 s1_um300, s0_gf, s6_gad_pre. 결과 `02_timing_before.txt`.
- 출력 기준값: `10_dumpall.sh.txt` 로 세 세트의 001 한 장을 다섯 설정(default, on = 포화 12800 + 가로줄, gain_only, defect_only, t1 = maxThreads 1)으로 덤프하고 sha256 을 남겼다 → `01_output_sha_before.txt`.
- 프로파일: 소스에 임시 구간 타이머(`// QA315TMP` 표시 줄만, `14_temp_instrumentation.diff.txt`)를 넣어 stderr 로 구간 시간을 찍고 31회 중 첫 회 제외 중앙값을 냈다(`11_agg.py.txt` → `03_profile_stages_31runs.txt`). pipeline 의 `XPE_STAGE_HOOK(n)` 지점(직전 지점부터의 시간; 1 = 작업 버퍼 확보·복사, 40 = 이득 함수, 4 = 포화 처리 등, 6 = 결함 단계 전체, 9 = 마지막 복사)과 `xpe_defect_correct_in` 안의 구간(finite, maskset, copy, list, lines, fill).
- 세부: s1_um300 5회에서 채움 병렬 구간별 시간과 채움 거리(반지름) 분포(`04_…`), maxThreads 1 단일 스레드 구간 시간(`05_…`).
- 임시 타이머는 측정 뒤 `// QA315TMP` 줄 삭제로 제거하고, 원본과 sha256 이 같은지 확인한다.
