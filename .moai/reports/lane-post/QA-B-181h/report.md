# QA-B-181h — Codex #60 보류 2건: bone_suppress 의 비유한 결과, JPEG LL 의 Nf=0 (#233 #235)

> **정정 (QA-B-181i)**: 아래 §1.3~1.4 의 worker 경로 "5건 거부"는 수정 후 시험이 5건 모두 브리지 검사를 거쳤다는 증거가 아니었다(3번째에서 워커가 꺼져 4·5번째는 즉시 반환 분기). 거부 코드도 `INVALID_INPUT` 에서 `PROCESSING_FAILED` 로 바뀌었다. QA-B-181i 보고서가 대체한다.

## 요약

| 항목 | 결과 |
|------|------|
| `xpe_bone_suppress` 가 비유한 결과를 성공으로 반환 | **실행으로 확인했다**(수정 전 5/5 케이스가 `rc=OK` 에 `+inf`). 결과 전체를 복사 **전에** 비트로 검사해 `XPE_ERR_INVALID_INPUT` 으로 거부, 출력은 한 바이트도 바뀌지 않는다. 경로는 프로세스 안과 worker 둘 다. |
| JPEG LL `Nf=0` 이 성분 수 대조를 통과 | 맞는 지적이었다. SOF 를 읽었다면 `Nf` 는 정확히 1 이어야 하고, `Nf` 바이트가 세그먼트 길이 안에 없으면 거부한다. |
| 생산자 표 | 공개 헤더에서 기계적으로 다시 뽑았다. 파서가 읽은 `XPE_API` 선언 119개(grep 119개와 일치), 출력 버퍼를 쓰는 함수 48개. 대조군 `xpe_bone_suppress` 가 잡혔다. 빠진 행 0 (§3). |

## 1. 항목 1 — `xpe_bone_suppress`

### 1.1 먼저 실행으로 확인했다

코드만 보고 쓴 지적이었으므로 실행 증거부터 얻었다 (`bone_suppress_red_before_fix.txt`). `models_x2/bone_suppress.onnx` 는 `Y = 2X` 다. 유한한 `FLT_MAX` 한 화소(첫·가운데·마지막 위치), `-FLT_MAX`, `FLT_MAX/2` 바로 위의 float 를 넣으면:

- 프로세스 안 경로: 5건 모두 `rc=0`(성공), 호출자 버퍼에 `±inf`.
- worker 경로: 5건 모두 같다. worker 는 `success:true` 로 답하고 브리지가 그대로 복사했다.

### 1.2 수정과 위치의 선택

| 경로 | 검사 위치 | 거부 코드 |
|------|-----------|-----------|
| 프로세스 안 | `OnnxSession::Run` 결과 `out.value` 전체를 `softTissueOut` 으로 `memcpy` 하기 **전에** (`modules/ai/src/ai.cpp`) | `XPE_ERR_INVALID_INPUT` |
| worker | 브리지가 worker 의 응답을 `pixels_out` 으로 복사하기 **전에** (`modules/ai/src/ai_ipc_bridge.cpp`). 연결은 끊지 않는다 — 프레임은 정상이고 화소가 모델이 계산한 값이기 때문이다 | `XPE_ERR_INVALID_INPUT` |

검사는 지수부 비트(`(u & 0x7F800000) == 0x7F800000`)로 한다. `std::isfinite` 도 범위 비교도 쓰지 않는다 (181f 와 같은 규약). 헬퍼는 `modules/ai/src/ai_finite.h`.

**worker 쪽이 아니라 브리지에서 거부한 이유**: worker 는 별도 프로세스이고 그 응답은 이쪽에서 보증할 수 없다. 브리지의 검사는 검사 없는 모델, 가짜 worker, 어떤 worker 가 `success` 라고 보낸 비유한 화소 모두에 성립한다. worker 안에서만 거부하면 "worker 가 거르는 한" 이라는 조건이 붙는다. 대가로, 거부할 이미지도 파이프를 한 번 건넌다. 쓰지 않은 방식: worker 와 브리지 양쪽 검사 (같은 화소를 두 번 훑는다. 브리지 하나로 정확성이 이미 성립한다).

**worker 경로의 출력**: 이 경로의 문서화된 실패 규약(REQ-AI-092)은 "출력 = 입력 복사" 이고 `ai.cpp` 가 그것을 한다. 그래서 worker 경로의 거부는 "출력 불변"이 아니라 "출력이 입력과 같다"로 끝난다. 프로세스 안 경로는 호출자가 넘긴 그대로(센티널 `-7`)다. 이 차이는 의도한 것이고 `ai_api.h` 문서에 적었다.

**알려진 영향**: worker 경로의 모든 비OK 결과는 연속 실패 3회 상한에 센다(기존 리더 결정). 극단 영상을 연속 3번 넣으면 그 세션의 worker 가 꺼진다. 새로 만든 규칙이 아니라 기존 규칙의 적용이다.

### 1.3 시험 (실제 ONNX 빌드, `ai-onnx` CI 잡과 같은 구성)

`modules/ai/tests/test_bone_suppress_nonfinite.cpp`, 시험 5개:

| 시험 | 내용 |
|------|------|
| `InProcessRefusesAFiniteInputWhoseResultLeavesFloat` | x2 모델 + `FLT_MAX`(3 위치)·`-FLT_MAX`·`FLT_MAX/2` 다음 float → `INVALID_INPUT`, 출력 센티널 불변, 입력 불변 |
| `WorkerPathRefusesAFiniteInputWhoseResultLeavesFloat` | 같은 입력, `use_worker` → `OK` 아님, 출력에 비유한 값 없음, 출력 = 입력 |
| `OrdinaryPixelsStillGiveExactlyTwiceTheInputInBothPaths` | 정상 입력은 두 경로 모두 `2X` 와 **비트 동일** |
| `ALargestFiniteResultIsNotRefused` | `FLT_MAX/2` 는 정확히 `FLT_MAX` 가 되어 통과 (과잉 거부가 아님) |
| `AiStubProducers.StitchAndDenoiseNeverWriteTheirOutputInAnyBuild` | §3.2 |

빨강 → 초록: 수정 전(검사 둘 다 제거한 빌드) 시험 2개 빨강 / 2개 초록 (`bone_suppress_red_before_fix.txt`), 수정 후 4개 모두 초록 (`bone_suppress_green_after_fix.txt`).

### 1.4 반증 (`bone_suppress_arms.txt`)

| 약화 | 결과 |
|------|------|
| P1 프로세스 안 검사 제거 | 프로세스 안 시험만 빨강 |
| P2 브리지 검사 제거 | worker 시험만 빨강 |
| P3 검사가 첫 화소만 봄 | 프로세스 안 시험 빨강 (마지막·가운데 위치 케이스가 잡는다) |
| P4 둘 다 제거 (= 수정 전) | 두 시험 빨강 |
| 복원 후 | 소스 바이트 동일 확인, 대조군 4개 초록 |

### 1.5 회귀 시험

- `ci-ai` 프리셋(`ai-onnx` 잡과 같은 `cmake --preset ci-ai` 빌드 + `XPE_AI_EXPECT_ONNX=1 ctest --test-dir build/ci-ai`): **393개 중 실패 0** (`ctest_summaries.txt`). 신규 시험 5개 포함. 스텁 전용 시험 5개만 건너뛴다.
- `ci-post`(스텁 AI): 1090개 중 실패 0. 신규 ONNX 시험 4개는 스텁이라 건너뛰고 `AiStubProducers` 는 통과한다.

로컬에서 ONNX 빌드가 되므로 CI 로만 확인해야 하는 부분은 없다. 단 CI 러너에서의 실행은 아직 보지 못했다 (Gaps).

## 2. 항목 2 — JPEG LL 의 `Nf`

`jpeg_frame_dimensions` 는 `Nf` 를 못 읽었을 때와 `Nf` 가 0 일 때 둘 다 0 을 냈고, 호출부의 `frameComponents != 0 && != 1` 이 0 을 "읽지 않음" 으로 취급했다. 수정: `Nf` 바이트가 세그먼트 길이 안에 있을 때만 읽고 없으면 0 (이제 0 은 "성분 없음 또는 `Nf` 바이트 없음" 이라는 **실제 답**이다). 호출부는 SOF 를 읽었다면 `frameComponents != 1` 이면 알림과 함께 `XPE_ERR_DICOM_INVALID` 로 디코드 전에 거부한다.

시험 `Scope_JpegLosslessComponentCountMustBeExactlyOneAndPresent` (수정 전 실측 `jpegll_nf_red_before_fix.txt`):

| 케이스 | 수정 전 | 수정 후 |
|--------|---------|---------|
| `Nf=1` (대조) | `OK` | `OK` |
| `Nf=0` | rc=-3 (`PROCESSING_FAILED`, 원인 없이) | `DICOM_INVALID` + 알림 |
| `Nf=2`, `Nf=3` | -13 (`DICOM_INVALID`) | 그대로 |
| `Nf` 바이트가 세그먼트 밖 (길이 7) | -3 | `DICOM_INVALID` |
| 세그먼트 길이 0 | -3 | `DICOM_INVALID` |

모든 거부 케이스에서 출력 불변, 메타데이터 조회는 그대로 성공, 알림이 "component" 를 지목한다.

반증 (`jpegll_nf_arms.txt`): H1 호출부를 `!= 0 && != 1` 로 되돌리면 이 시험만 빨강, H2 세그먼트 길이 검사를 빼면 이 시험만 빨강, 복원 후 소스 바이트 동일, 리더 시험 90/90 초록. `ci-dicom` ctest 237개 중 실패 0 (건너뜀 1, 기존).

수정 전 코드에서 `Nf=0` 은 `-3` 이었다. 즉 "통과해 `OK`" 가 아니라 이후 DCMTK 가 원인 없이 거부했다. Codex 의 지적(대조를 통과함)은 사실이고, 결과 코드가 `DICOM_INVALID` 가 아니라 `PROCESSING_FAILED` 였다는 것이 실측이 더한 부분이다.

## 3. 생산자 표 — 공개 헤더에서 기계적으로 다시 뽑았다

`extract_producers.py` → `producers_extracted.txt`. 기준: `XPE_API` 함수 중 **const 가 아닌 포인터** 인자가 화소를 담는 타입(`XpeImageBuffer`, `float`, `uint8/16_t`, `void`; 문자열 `char*` 제외)인 것. 같은 패스로 파싱한 `XPE_API` 선언 수를 센다.

- 대조군 1: 파서가 읽은 `XPE_API` 선언 **119개**, `grep` 으로 센 공개 헤더 선언도 119개 — 헤더를 건너뛰지 않았다 (처음 스크립트는 `include/xpe/*.h` 직속 파일 `preprocess_api.h` 를 놓쳤다가 재귀 glob 으로 고쳤다).
- 대조군 2: `xpe_bone_suppress` 가 추출됐다.
- 추출된 함수 48개(§3.1 27 + §3.3 21)의 이름이 이 보고서에 하나씩 모두 적혀 있는지 스크립트로 확인했다 (`table_crosscheck.txt`): 빠진 행 0. (처음 확인에서 약칭 `_masked`·`_ex` 등으로 적은 9개가 걸려 전체 이름으로 고쳤다. 눈으로 대조했다면 지나쳤을 것이다.)

### 3.1 Lane B 소유 모듈과 common (27개)

| 함수 | 출력 | 처리 / 근거 |
|------|------|--------------|
| `xpe_bone_suppress` | 이미지 | **이 카드에서 추가·수정** (§1) |
| `xpe_stitch_images` | 이미지 | 스텁 — §3.2 |
| `xpe_dl_denoise` | 이미지(제자리) | 스텁 — §3.2 |
| `xpe_bodypart_recognize` | 라벨 + `confidenceOut` | 이미지 아님. 스텁은 상수(`"UNKNOWN"`, 0.0)만 쓴다 |
| `xpe_noise_reduce` | 이미지 | 181g §1 표(bilateral·NLM) |
| `xpe_edge_enhance` | 이미지 | 181g §1 표 |
| `xpe_contrast_enhance` | 이미지 | 181f·181g §1 표 |
| `xpe_log_transform`, `xpe_log_inverse` | 이미지 | 181f·181g §1 표 |
| `xpe_apply_modality_lut` (LINEAR/TABLE) | 이미지 | 181g §1 표 |
| `xpe_apply_voi_lut` | 이미지 | 181g §1 표 (3 모드) |
| `xpe_apply_presentation_lut` | 이미지(uint16 클램프) | 181g §1 "훑지 않은 것"(출력 클램프, 181f 입력 검사) |
| `xpe_multiscale_process` | 이미지 | 181g §1·§2 |
| `xpe_fractional_process` | 이미지 | 181g §1 표 (REQ-ADV-032: 비유한 입력을 0 으로 — 리더 결정으로 유지) |
| `xpe_gsvg_process`, `xpe_gsvg_process_masked`, `xpe_gsvg_process_ex` | uint16 이미지 | 181g §1 "훑지 않은 것"(입력 uint16, 설정 극단은 181f) |
| `xpe_dicom_read_image` | 정수 화소 | 파일의 정수 화소를 그대로 디코드 (float 계산 없음). 분류는 182d–182f·181h §2 |
| `xpe_noise_estimate_sigma`, `xpe_calc_exposure_index`, `xpe_adv_calc_exposure_index`, `xpe_get_param_range` | 스칼라 | 이미지 출력이 아님 (181g §1 "훑지 않은 것") |
| `xpe_gsvg_init`, `xpe_gsvg_shutdown` | 핸들 | 이미지 출력이 아님 |
| `xpe_alloc_image`, `xpe_free_image`, `xpe_copy_image` | 버퍼 관리 | 계산 없음, 바이트 복사·할당·해제 (Lane A 소유) |

### 3.2 `xpe_stitch_images`, `xpe_dl_denoise` 가 스텁인 이유

두 함수는 검증(NULL·초기화·버퍼 유효성) 뒤 `XPE_ERR_PROCESSING_FAILED` 를 **무조건** 반환한다. 본문에 모델 호출도 IPC 메시지도 없다 ("Full implementation: send STITCH_IMAGES / DL_DENOISE over IPC" 주석만 있다). **ONNX 빌드에서도 마찬가지다** — 스텁 빌드 한정이 아니다. 출력 버퍼를 쓰는 코드 경로가 아예 없으므로 비유한 화소를 내보낼 수 없다.

이 주장을 이름이나 주석이 아니라 실행으로 묶었다: `AiStubProducers.StitchAndDenoiseNeverWriteTheirOutputInAnyBuild` 는 두 함수가 `PROCESSING_FAILED` 를 돌려주고 출력(stitched 센티널, denoise 입력)이 한 바이트도 바뀌지 않음을 단언한다. ONNX 빌드와 스텁 빌드 둘 다 통과했다. 미래에 누군가 구현을 넣어 출력을 쓰기 시작하면 이 시험이 빨개지고, 그때 이 표의 행과 유한성 정책을 다시 처리해야 한다.

### 3.3 Lane A 소유 `preprocess` (21개, 이 카드에서 판정하지 않음)

`xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct`, `xpe_defect_detect_runtime`, `xpe_ghost_correct`, `xpe_temp_compensate`, `xpe_nonlinearity_correct`, `xpe_binning_correct`, `xpe_preprocess_pipeline`, `xpe_preprocess_pipeline_ex`, `xpe_preprocess_pipeline_batch`, `xpe_calib_load_offset_cached`, `xpe_calib_load_gain_cached`, `xpe_calib_load_defect_cached`, `xpe_bpm_generate`, 그리고 핸들·스칼라 함수(`xpe_ghost_create`, `xpe_ghost_reset`, `xpe_ghost_destroy`, `xpe_preprocess_get_param_range`, `xpe_calib_state_load`, `xpe_calib_state_release`). 추출에는 나왔지만 소유 경계 밖이라 이 카드의 처리 대상이 아니다. 리더가 Lane A 로 넘길지 판단할 사항이다.

## Gaps (미검증)

- **CI 러너에서 실행하지 않았다.** 로컬은 `ci-ai` 프리셋 + `XPE_AI_EXPECT_ONNX=1 ctest` 로, `.github/workflows/ci.yml` 의 `ai-onnx` 잡이 도는 명령(`cmake --preset ci-ai` → `--build` → `ctest --test-dir build/ci-ai`)과 같다. 러너에서의 결과는 푸시 뒤에야 본다.
- **모델은 `Y=2X` 장난감 하나뿐이다.** 실제 U-Net 이 NaN 을 내는 입력은 시험하지 않았다. 검사는 모델 출력의 비트를 보므로 원인과 무관하게 동작하지만 실제 모델로는 실행하지 않았다.
- 비유한 **입력**을 `xpe_bone_suppress` 에 넣는 경우는 이 카드의 요구가 아니라 시험하지 않았다(모델이 `inf` 를 전파하면 같은 출력 검사가 잡는다는 추론이지 실행 증거는 아니다).
- 생산자 추출 기준은 "const 가 아닌 화소형 포인터"다. 구조체 출력(`xpe_detect_collimation` 의 영역)·문자열 출력은 기준 밖이라 표에서 "이미지 아님"으로 분류했으며, 이 분류는 사람이 했다 (추출 자체는 기계적).
- 표의 Lane B 행 대부분은 이전 카드(181f·181g)의 측정을 인용한다. 이 카드에서 다시 실행하지 않았다.
- JPEG LL: 세그먼트 길이가 정상인데 `Nf` 바이트만 틀린 경우(`Nf=1` 인데 성분 명세가 없음) 등 SOF 내부의 다른 불일치는 이 카드의 범위 밖이다.

## Residual-risk (잔여 위험)

- worker 경로에서 극단 영상 3회 연속이 worker 를 끄는 효과(§1.2 "알려진 영향")는 기존 규칙의 적용이지만 사용자 눈에는 "극단 영상 때문에 AI 가 꺼짐"으로 보일 수 있다. 리더가 그 정책을 재검토하고 싶다면 분리 기준(모델 거부 vs 결과 거부)이 필요하다.
- 검사는 결과 전체를 한 번 더 훑는다 (3×3 영상에서는 무시할 수준, 큰 영상의 비용은 측정하지 않았다 — 모델 추론에 비해 작은 O(N) 이지만 숫자는 없다).
- 스텁 두 개의 "쓰지 않음" 은 현재 구현에 대한 것이다. 구현이 들어오면 시험이 깨지도록 묶었을 뿐 그 구현의 안전성을 보증하지 않는다.
