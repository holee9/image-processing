# QA-B-209 — E2 남은 평탄 시험 측정 · C16 로거 공유 사실 (#251) — 부분 보고

이 보고서는 카드의 **측정 단계**만 담는다(E2 전부, C16 의 "공유하는가" 사실). C8 코드와 C16 코드 수정은 이 보고서 시점에 **착수하지 않았다**(§3).

## 1. E2 — 평탄 입력에서 일을 건너뛰는가 (측정, 코드 변경 없음)
방법: 3072×3072, 평탄(500.0) 대 구조 있는 영상(벤치마크 데이터: 의사난수 1..4096), 같은 함수를 **번갈아** 5회, 중앙값. 스크립트 `e2_flat_vs_structured.py.txt`, 원본 출력 `e2_out.txt`, 빌드는 병합 반영 최종 트리(ci-post).

| 함수 | 평탄 중앙값 | 구조 중앙값 | 비 | 평탄 호출 뒤 바뀐 화소 |
|---|---|---|---|---|
| `xpe_edge_enhance` (USM 0.5/2.0/10) | 19.9 ms | 19.4 ms | 1.02 | 0 |
| `xpe_log_transform` (norm 1000) | 5.6 ms | 6.0 ms | 0.93 | 9,437,184(전부) |
| `xpe_log_inverse` (norm 1000) | 5.4 ms | 5.0 ms | 1.07 | 9,437,184(전부) |
| `xpe_noise_reduce` (bilateral 3.0/50) | 38.2 ms | 41.9 ms | 0.91 | 0 |

판정: **네 함수 모두 평탄 입력에서 일을 건너뛰지 않는다**(시간 비 0.91~1.07; 바뀐 화소 0 은 평탄 입력에서 USM·양방향 필터의 결과가 원래 입력과 같기 때문이지 일을 안 해서가 아니다 — 시간이 구조 영상과 같다). 카드 지시대로 시험은 바꾸지 않는다. 통합 성능 시험(`FullPipeline_3072x3072_Within200ms`)은 이 단계 함수들의 합이고 CLAHE 단계만 평탄에서 조기 반환했었다(207 에서 CLAHE 는 해결). 통합 시험 자체의 시간은 이번에 재지 않았다.

덧붙임(사실 하나): USM 은 평탄·구조 모두 중앙값 약 19.4~19.9 ms 로 **예산 20 ms 에 0.1~0.6 ms 여유**다. 로컬 시험(`EdgeEnhance.Performance_3072x3072_Within20ms`)이 기계 부하가 있던 한 번 23 ms 로 실패한 적이 있는 이유와 맞는다. 제품 문제인지 판정선 문제인지는 이번 카드 범위 밖이라 판단하지 않는다.

## 2. C16 — dicom 모듈의 로거는 xpe_common 설정을 따르는가 (실행으로 확인)
방법: `xpe_common.dll` 로 `xpe_log_set_file(임시파일)` + `xpe_log_set_level(L)` 을 호출한 뒤, `xpe_dicom_open` 으로 DICOM 이 아닌 파일을 열어 DICOM 의 로그가 그 파일에 나오는지 센다. 스크립트 `c16_logger_sharing.py.txt`, 출력 `c16_out.txt`.

| 설정한 수준 | set_file / set_level rc | 파일에 나온 DICOM 줄 |
|---|---|---|
| 0 | 0 / 0 | **2줄**: `[xpe_file] [debug] [DicomReader] open: …` 와 `[warning] [DicomReader] not a DICOM Part 10 file…` |
| 3 | 0 / 0 | 1줄: warning 만(DEBUG 진입 줄 사라짐) |
| 6 | 0 / **-1** | 1줄: warning(잘못된 수준 6 은 거부되어 직전 수준 3 이 유지됨) |

판정: **공유한다.** dicom 모듈의 로그가 xpe_common 이 만든 로거(`[xpe_file]`)로 나가고 `xpe_log_set_level` 에 반응한다(DEBUG 줄이 수준 0 에서만 보임). 따라서 REQ-DICOM-043 의 "via the logging subsystem in xpe_common.dll" 은 지금도 맞다 — 카드의 "공유하면" 갈래이다. 남은 두 가지는 코드로 고칠 일이다: (1) 공개 함수 종료 로그가 없다(진입만 DEBUG), (2) 실패 경로가 ERROR 가 아니라 warning 이다(관측: `[warning] [DicomReader] not a DICOM Part 10 file`). 이전 보고서(QA-B-208)가 "공유 여부를 확인하지 못했다"고 적은 Gap 은 이것으로 닫힌다.

## 3. 아직 하지 않은 것 (정직한 상태)
- **C16 수정**: 종료 로그(공개 함수 11개, 로그만·동작 불변)와 실패 경로 warning → ERROR(`src/*.cpp` 의 `spdlog::warn` 36곳 중 오류 조건인 것의 선별 포함) — 착수 전. 재현(수정 전 빨강)은 이 보고서의 `c16_out.txt` 모양(해당 줄이 `[warning]` 이고 `[error]` 가 아님)을 시험으로 옮기면 된다.
- **C8 수정**: `buildFindRequest` 의 시퀀스 구조·MockScp 캡처 시험·`AccessionNumber` 유지·시작일 범위 형식 — 착수 전.
- 이유: 이 세션의 컨텍스트가 인계 기준에 도달해 두 수정을 반쯤 하고 멈추는 것보다 측정을 확정하고 깨끗하게 넘기는 쪽을 택했다.

## 4. Gap / 잔여 위험
Gap
- 공유 확인은 `xpe_dicom_open` 한 경로(DicomReader)다. `DicomNetworkSCU`·`DicomWriter` 가 같은 로거를 쓰는지는 같은 spdlog 기본 로거를 호출한다는 코드 읽기만이고 실행으로 보지 않았다(같은 모듈·같은 정적 spdlog 라 같을 가능성이 크다).
- E2 측정은 이 기계 한 대의 5회 번갈아 중앙값이다.
