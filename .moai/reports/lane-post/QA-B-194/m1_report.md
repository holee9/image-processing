# QA-B-194 M1 — 공통 검증기와 비유한 입구 (REQ-AI-090)

카드: QA-B-194 M1 · 관련: #130 · 설계 승인: `design.md` D1~D8(리더 2026-10-03)
장난감 모델·스텁 기준이다. 아래 어떤 문장도 실제 모델의 동작을 말하지 않는다.

## 주장 (Claim)

1. `validateImageBuffer` 가 세 형식 모두에 화소 크기를 준다(UINT8 1, UINT16 2, FLOAT32 4). 이전에는 UINT8 이 화소 크기 0 으로 통과해 크기 검사를 건너뛰었다(`UINT8` 4000×4000 에 데이터 1바이트가 통과, 설계 §3 측정). 세 형식이 아닌 값은 이미지가 아니므로 `XPE_ERR_INVALID_INPUT`.
2. 새 `checkImageFinite`(한 곳)를 `xpe_bone_suppress`·`xpe_bodypart_recognize`·`xpe_dl_denoise`·`xpe_stitch_images` 입구에서 부른다. FLOAT32 영상에 NaN/±Inf 가 하나라도 있으면 아무것도 쓰기 전에 `XPE_ERR_INVALID_INPUT` 과 오류 알림 1건이고, 알림은 **입력** 탓을 말한다(D2). `xpe_stitch_estimate_size` 는 화소를 읽지 않으므로 검사하지 않는다.
3. 부위 인식은 거부할 때 라벨도 신뢰도도 쓰지 않는다(`UNKNOWN`·0.0 도 아님, D2). 스텁 빌드·모델 유무와 무관하게 같다(검사가 스텁 반환과 모델 적재보다 앞).
4. 뼈 억제의 거부는 모듈 잠금·모델·워커보다 앞이라 워커를 시작하지 않고 워커 실패 카운트를 건드리지 않는다.

## 알림 문구 (레인 간 계약)

`<태그> <N> pixel(s) of <what> are NaN or infinite (first: index <I>, x=<X>, y=<Y>); <맺음말>` — 오류 심각도, preprocess 와 같은 모양(`api-spec.md` "비유한 입력").

| 함수 | 태그 | `<what>` | 맺음말 |
|---|---|---|---|
| `xpe_bone_suppress` | `XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE:` | `the input frame` | `the image was not processed and the output buffer was not changed` |
| `xpe_bodypart_recognize` | `XPE_WARN_BODYPART_INPUT_NOT_FINITE:` | `the input frame` | `the image was not classified and the label and confidence were not written` |
| `xpe_dl_denoise` | `XPE_WARN_DL_DENOISE_INPUT_NOT_FINITE:` | `the input frame` | `the image was not denoised and the buffer was not changed` |
| `xpe_stitch_images` | `XPE_WARN_STITCH_INPUT_NOT_FINITE:` | `input part <P>` (처음으로 실패한 파트) | `the parts were not stitched and the output buffer was not written` |

모델의 **출력**이 비유한인 것은 다른 사건이고 그대로다(`XPE_ERR_PROCESSING_FAILED`, "AI model output was non-finite …"). 이전에는 입력의 NaN 이 모델을 거쳐 이 알림으로 나왔다 — 입력 탓을 모델 탓으로 보고했다(설계 §3).

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 414 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (기준 403: 새 시험 +11). 스텁: `[  PASSED  ] 336 tests.` 스킵 83, 실패 0, 경고 0 (기준 325·스킵 83: 새 시험 11건이 모두 스텁에서 돈다 — 검사가 모델보다 앞이라 모델이 필요 없다).
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0. `check_header_docs.py` 0 findings. 린트 0 error. (헤더 주석을 바꿨다 — 191g 의 교훈대로 커밋 전에 돌렸다.)
- 시험 `test_ai_input_validation.cpp` 11건:
  - `ControlAFiniteFrameIsNotRefused…`: 유한 프레임(FLT_MAX·-FLT_MAX 전체 포함)은 거부되지 않고 비유한 알림도 없음 — "전부 거부" 가 아님의 대조군
  - `ANonFiniteFrameIsRefusedBeforeAnythingIsWrittenByEveryFunctionThatTakesPixels`: 네 함수 × (NaN, +Inf, -Inf) × (첫·가운데·마지막 화소) = 36 조합. 각각 `INVALID_INPUT`, 출력 불변(뼈 억제 출력 버퍼, 부위 인식 라벨 'x' 채움과 신뢰도 보초값, 노이즈 제거 프레임 바이트, 스티치 출력), 알림 정확히 1건·오류 심각도·문구 전체 일치(색인·x·y 는 시험의 입력에서 계산), "model output" 이 문구에 없음
  - `TheAlertCountsEveryNonFinitePixelAndNamesTheFirst`: 비유한 3개 → "3 pixel(s)", 첫 색인
  - `IntegerFramesAreNeverScanned`: UINT16 프레임(같은 바이트가 float 이면 NaN)은 거부·비유한 알림 없음
  - `StitchNamesTheFirstPartThatFails`: 파트 1·2 가 모두 나쁠 때 파트 1 을 이름으로
  - `TheEstimateReadsNoPixelsSoItHasNothingToRefuse`: 추정 함수는 NaN 이 있어도 거부·알림 없이 10×6(`6 × 1.7`)
  - `ABodyPartRequestIsRefusedWhetherOrNotAModelExists`: 모델이 없는 디렉터리에서 유한 이미지는 `UNKNOWN`(대조군), 비유한은 `INVALID_INPUT`·라벨 'x' 그대로·신뢰도 그대로
  - `AiInputValidationWorker.ANonFiniteBoneFrameIsRefusedBeforeAnyWorkerIsStarted`: `use_worker` 켠 세션에서 거부, 워커 프로세스 0, 상태 ACTIVE·실패 0, 알림은 입력의 것("AI worker" 없음)
  - `AUint8ImageThatDeclaresMoreThanItSuppliesIsRefused`: 설계의 측정 케이스가 이제 `INVALID_INPUT`; 대조군: 선언한 만큼, `dataSize 0`(미지정, #123), 1바이트 부족
  - `AFormatValueThatIsNotOneOfTheThreeIsNotAnImage`: 값 7 → 네 입구와 추정 함수 모두 `INVALID_INPUT`
  - `TheDeclaredSizeOfEveryFormatIsBoundedByTheModuleMaximum`: 형식별 최대(UINT8 67108864, UINT16 33554432, FLOAT32 16777216 화소)는 통과, +1 은 거부(화소를 읽지 않는 추정 함수로 — 데이터 포인터가 1바이트라 화소를 훑는 함수를 쓰면 남의 메모리를 읽는다)
- 기존 시험 한 건 갱신: `AiExceptionGuard.StitchEstimateOfAHugeWidthIsTheLimitNotAWrappedValue` 가 UINT8 의 크기 구멍으로 폭 2,526,451,329 를 만들어 float→uint32 변환 경로를 시험하고 있었다. 구멍이 막혀 그 입력은 이제 거부되므로(첫 단언) 4096 으로 자르는 시험은 검증기가 받아들이는 가장 넓은 UINT8 폭(64 MB)으로 옮겼다. 시험의 목적(추정값을 자르는 것)은 그대로다.
- 반증 9개(`m1_arms_out.txt`), 매번 빌드 성공, `ai.cpp` 바이트 동일 복원, 대조군 128/128:
  - J1 UINT8 크기 미검사 → UINT8 시험 둘 + 위의 갱신된 기존 시험 빨강
  - J2 알 수 없는 형식을 이미지로 → 형식 시험만 빨강
  - J3·J4·J5·J6 네 입구 각각의 검사를 뺌 → 해당 함수의 표 시험과 개수 시험 빨강(뼈 억제는 워커 시험도, 부위 인식은 "모델 유무" 시험도, 스티치는 파트 이름 시험도). **J6 은 처음에 스티치 전용 시험 하나만 빨개졌다**: 스티치가 36 조합 표에 빠져 있었다. 표에 넣고 J6 을 다시 돌려 표 시험이 빨개지는 것을 확인했다
  - J7 알림이 모델 탓을 다시 말함 → 문구 시험 전부 빨강
  - J8 스티치 알림이 틀린 파트 번호 → 파트 이름 시험 빨강
  - J9 부위 인식 거부가 신뢰도를 0.0 으로 씀 → 표 시험과 모델 유무 시험 빨강
  - 돌리지 않은 반증: "정수 형식은 스캔하지 않는다" 의 되돌림(`UINT16` 프레임을 float 로 훑음)은 시험 버퍼 밖을 읽게 되어 안전하게 돌릴 수 없어 하지 않았다(`IntegerFramesAreNeverScanned` 는 그 성질의 정방향 증거만 가진다).

## 기준 (Baseline)

같은 트리 `dev/postprocess`(main `f9803071` 병합 위, 191g 포함 전), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력. 이전 수치: `QA-B-193b/report.md`.

## 바뀌는 반환 코드와 호출자

- `xpe_bone_suppress`·`xpe_bodypart_recognize`: 비유한 입력이 이전의 `-3`(+모델 탓 알림)에서 `-1`(+입력 탓 알림). C# 이 직접 부르는 것은 `xpe_bone_suppress` 뿐이고 GUI 는 `-1` 을 이미 "시도하지 않음" 으로 읽는다(설계 §7). GUI 입력은 16비트 정수를 나눈 float 라 이 입구가 GUI 경로에서는 발동하지 않는다.
- `xpe_dl_denoise`: UINT8 의 크기 부족이 `-3` 에서 `-1`. 알 수 없는 형식도 `-1`.
- 헤더(`ai_api.h`) 파일 설명에 "INPUT VALIDATION" 절과 알림 태그를 적었다.

## 미검증 (Gaps)

- 워커 쪽 `ParseImageRequest` 에 같은 검사를 더하지는 않았다. 부모가 입구에서 막으므로 워커는 유한한 입력만 받는다(뼈 억제·부위 인식 둘 다 `xpe_*` 입구를 거친다). 워커를 다른 경로로 직접 부르는 시험(`WorkerBoneSuppress.*` 등)은 영향이 없었다.
- `dataSize` 가 0("미지정", #123)이면 검증기는 선언한 크기를 믿고 `checkImageFinite` 는 `width × height × 4` 바이트를 `data` 에서 읽는다. 다른 모든 읽기와 같은 신뢰이며 이 카드가 새로 만든 위험은 아니지만 "미지정" 이 "읽어도 됨" 의 약속이라는 가정이다.
- 비유한 스캔 시간은 설계 단계에서 쟀다(3072² 약 1.4 ms, 캐시에 있을 때). 이 구현의 입구에서 다시 재지는 않았다.
- `denoise`·`stitch` 는 스텁이라 검사를 통과한 뒤 `-3` 으로 끝난다. "통과한 뒤 모델이 처리한다" 는 증명 대상이 없다.
- `xpe_stitch_images` 의 파트 개수 상한·형식 일치는 M2 의 일이다(이 마일스톤은 비유한과 크기 검증기).

## 잔여 위험

- 이전에 통과하던 입력이 `INVALID_INPUT` 이 된다(UINT8 크기 부족, 알 수 없는 형식, 비유한). 이 모듈의 소비자는 현재 GUI(뼈 억제만)뿐이고 영향이 없음을 확인했다. 다른 소비자의 입력 가정은 확인된 적이 없다.

## 정정 (Codex #83, 낮음, 리더 전달)

위 "기존 시험 한 건 갱신" 에서 "시험의 목적(추정값을 자르는 것)은 그대로다" 라고 적은 것은 과했다. 검증기가 모든 형식의 크기를 64 MB 로 묶은 뒤에는 검증된 입력으로 만들 수 있는 가장 큰 추정값이 약 1억 1천4백만이라 `UINT32_MAX` 를 넘지 못한다. 옛 결함(캐스트 뒤에 4096 으로 자름 → 범위 밖 변환)에는 이 함수를 통해 도달할 수 없고, 갱신한 시험은 그 회귀를 더는 잡지 못한다(자르기를 캐스트 뒤로 옮겨도 통과한다). 시험이 지키는 것은 두 가지로 좁혔다 — 과대 입력은 거부되고 아무것도 쓰이지 않음, 유효한 입력의 추정값이 4096 을 넘으면 4096 으로 잘림 — 이름과 주석을 그렇게 바꿨다(`StitchEstimateRefusesAnOversizedPartAndLimitsAValidOneTo4096`).
