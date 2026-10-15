# QA-B-194 M3 — 노이즈 제거 메타데이터와 모델 식별자 (REQ-AI-090, D3·D4)

카드: QA-B-194 M3 · 관련: #130 · 설계 승인: `design.md` D3·D4
장난감 모델·스텁 기준이다. 실제 모델의 동작은 어디에도 주장하지 않는다.

## 주장 (Claim)

1. `xpe_dl_denoise` 가 받는 `XpeImageMetadata` 를 입구에서 판정한다(D3), 모든 빌드에서. 규칙은 이것뿐이다.
   - `bodyPart` 는 64바이트 안에서 NUL 로 끝나는 문자열
   - `kVp`·`mAs`·`SID_mm`·`pixelPitch_mm` 는 유한하고 음수가 아님. **0 은 "모름"** 이므로 받아들인다(`xpe_types.h` 의 메타데이터 전체가 0 을 그렇게 쓴다)
   - `acquisitionTime`·`flags` 는 판정하지 않는다(어떤 64비트 시각도, 어떤 비트 패턴도 그 필드가 담을 수 있는 값)
   - 임상 범위는 만들지 않는다("그럴듯한 kVp" 는 아무도 주지 않은 임상 지식). FLT_MAX 도 통과한다
   위반하면 `XPE_ERR_INVALID_INPUT`, 영상은 그대로, 알림 없음(크기 검사와 같은 조용한 INVALID_INPUT). 판정은 영상 크기 검사 뒤·**화소 스캔 앞**이다(싼 검사가 먼저).
2. `xpe_ai_get_model_card` 의 `modelId` 는 `[A-Za-z0-9._-]` 1~64자여야 한다(D4). 식별자가 카드 JSON 에 그대로 복사되므로 따옴표·역슬래시가 있으면 카드가 깨진 JSON 이 되었다(설계 §3 측정). 위반하면 `XPE_ERR_INVALID_INPUT` 이고 호출자 버퍼에는 아무것도 쓰지 않는다. 문법을 만족하지만 로드되지 않은 식별자는 이전 그대로 `model_not_loaded` 카드와 `XPE_ERR_IO_FAILED` 다 — "없음" 을 "잘못됨" 으로 바꾸지 않는다. 판정 위치는 init 검사와 `bufSize == 0` 검사 뒤(기존 순서를 건드리지 않음).
3. 식별자 검사는 65번째 바이트에서 멈추므로 호출자의 문자열을 합법 식별자와 종단 문자 이상으로 읽지 않는다.

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 425 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (M2 419 → +6). 스텁: `[  PASSED  ] 347 tests.` 스킵 83, 실패 0, 경고 0 (M2 341 → +6; 새 시험 모두 스텁에서 돈다).
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0. `check_header_docs.py` 0 findings. 린트 0 error.
- 새 시험 6건(`test_ai_input_validation.cpp`):
  - `ControlMetadataThatIsMerelyUnusualIsNotRefused`: 평범한 메타데이터, 전부 0, 빈 `bodyPart`, 63자+종단, 네 필드 모두 FLT_MAX, `acquisitionTime`·`flags` 가 모든 비트 1 인 값(UINT64_MAX·0xFFFFFFFF) — 모두 거부되지 않음(판정이 "모든 메타데이터를 거부"가 아니라는 대조군이자, 판정하지 않는 것의 고정)
  - `ADoseOrGeometryThatIsNotAFiniteNonNegativeNumberIsRefusedAndTheFrameIsLeftAlone`: 네 필드 × (NaN, +Inf, -Inf, -1, -0.0001, -FLT_MAX) = 24 조합, 각각 `INVALID_INPUT`, 영상 바이트 불변(bytewise), 알림 0건
  - `ABodyPartThatIsNotATerminatedStringIsRefused`: 64바이트 `'A'`(종단 없음)
  - `TheMetadataIsJudgedBeforeThePixelsAreScanned`: 메타데이터와 화소가 둘 다 나쁘면 메타데이터가 먼저 답하고 NaN 알림은 없음(순서를 결정으로 고정)
  - `AModelIdentifierIsOneToSixtyFourOfLettersDigitsDotUnderscoreHyphen`: 대조군(제품이 쓰는 `bodypart_cnn_v1` 은 `XPE_OK`, 1자, 64자, `Model-1.2_final`) + 거부 13종(빈 문자열, 65자, `"`, `\`, 공백, `/`, 개행, 탭, 제어문자 0x01, 비ASCII, `{}`, `,`, `:`) 각각 `INVALID_INPUT` + 호출자 버퍼 'x' 채움이 그대로
  - `AWellFormedButUnknownIdentifierStillGetsAWellFormedUnavailableCard`: 합법이지만 모르는 식별자는 `IO_FAILED` + `"model_id":"…"`·`"error":"model_not_loaded"` 가 든 카드, `{`로 시작 `}`로 끝남, 이스케이프가 필요한 문자 없음
- 반증 13개(`m3_arms_out.txt`), 매번 빌드 성공, `ai.cpp` 바이트 동일 복원, 대조군 119/119:
  - L1 판정 전체 제거 → 24 조합 시험과 순서 시험 빨강
  - L2 음수 허용 → 24 조합 시험만 빨강
  - L3 NaN 허용 → 24 조합·순서·대조군 빨강(이 반증은 `> 3.4e38f` 로 쓰다 보니 FLT_MAX 도 거부해 대조군까지 빨갛게 했다 — 반증이 의도보다 넓다. NaN 만의 반증은 24 조합 시험이 이미 잡았다)
  - L4 0 거부 → 기존 `AiFallbackTest` 3건과 대조군 빨강(0 이 "모름" 이라는 계약이 기존 시험에도 걸려 있었다)
  - L5 종단 문자 검사 제거 → 종단 시험만 빨강
  - L6 범위를 지어냄(kVp>150 거부) → 대조군(FLT_MAX)만 빨강: 범위를 만들지 않는다는 성질이 지켜짐
  - L7 메타데이터 판정을 스캔 뒤로 → 순서 시험만 빨강
  - M1 식별자 판정 제거 / M2 한도 63 / M3 빈 식별자 허용 / M4 따옴표 허용 / M5 공백 허용 / M6 127 초과 바이트 허용 → 모두 식별자 시험만 빨강(M2·M3 는 길이 경계, M4~M6 은 각 문자 부류를 개별로 잡음)

## 기준 (Baseline)

같은 트리 `dev/postprocess`(M2 21f0317f 위), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 바뀌는 반환 코드와 호출자

- `xpe_dl_denoise`: 비유한·음수 선량/기하, 종단 없는 `bodyPart` → `-3`(스텁은 무조건 `-3`) 에서 `-1`.
- `xpe_ai_get_model_card`: 문법 위반 식별자가 이전의 `XPE_ERR_IO_FAILED`(+ 깨진 JSON 이 든 카드, 따옴표가 있을 때) 에서 `XPE_ERR_INVALID_INPUT`(+ 버퍼 미기록). 빈 문자열은 이전에는 `model_not_loaded` 카드 + `IO_FAILED` 였다.
- 두 함수 모두 C# 이 부르지 않는다(`GuiAiRunner.cs`). 다른 소비자 확인은 못 했다.
- 헤더의 `xpe_ai_get_model_card` 예시를 `"bone_suppress_v1"`(로드되지 않는 이름) 에서 실제 식별자 `"bone_suppress_unet_v1"` 로 고쳤다 — 예시를 그대로 따라 한 호출이 `IO_FAILED` 를 받던 것.

## 미검증 (Gaps)

- 선량·기하의 **상한**(예: 비현실적으로 큰 mAs)은 판정하지 않는다(D3 결정). 상한을 정하려면 임상 근거가 필요하다.
- 실제 모델이 메타데이터를 쓰는 경로(변형 선택, 노이즈 추정)는 없다(`xpe_dl_denoise` 는 스텁). 판정이 막는 것이 실제 모델의 어떤 오동작인지는 이 빌드로 관찰하지 못했다. 현재 막는 것은 "가짜 값이 변형 선택을 오염시킴" 이라는 설계상의 위험이다.
- `bodyPart` 내용(알려진 부위 이름인가)은 판정하지 않는다. EIT 조회의 문제이고(`xpe_ai` 아님), 빈 문자열도 문자열이다.
- 모델 식별자 문법의 출처는 모델 디렉터리의 파일 이름(설계 D4)이다. 제품이 문법을 따로 정한 문서는 찾지 못했다 — `design.md` 에 이미 적은 가정이다.
- 반증 L3 은 의도보다 넓다(위 설명).

## 잔여 위험

- 이전에 받아들여지던 입력이 거부된다: 종단 없는 `bodyPart`, 음수·비유한 값, 문법을 벗어난 식별자(빈 문자열 포함). 두 함수의 소비자는 현재 시험뿐이고 제품 소비자는 확인하지 못했다.
