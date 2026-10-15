# QA-B-194 M2 — 스티치 부분 영상의 개수 상한과 형식 일치 (REQ-AI-090, D5·D6)

카드: QA-B-194 M2 · 관련: #130 · 설계 승인: `design.md` D5·D6
장난감 모델·스텁 기준이다. 실제 모델의 동작은 어디에도 주장하지 않는다.

## 주장 (Claim)

1. `xpe_stitch_images`·`xpe_stitch_estimate_size` 의 `partCount` 에 상한 `XPE_AI_MAX_STITCH_PARTS` = **16** 을 둔다. 상한은 **다른 인자와 함께, `parts[i]` 를 하나라도 읽기 전에** 판정한다. 이전에는 `partCount` 가 크면 두 함수가 배열이 담고 있지 않은 메모리를 끝까지 읽었다.
2. **16 은 구현 안전 상한이지 임상 요구가 아니다.** 헤더(`ai_api.h`)의 상수 설명에 그렇게 적었고(D5 의 리더 지시), SRS·SPEC·PRD 어디에도 스티치 부분 영상의 최대 개수는 없다(검색 결과: 출력 크기의 4096 절단뿐). 제품 값이 정해지면 이 상수 한 곳을 바꾼다. 시험이 값 16 을 고정해 두어 변경이 우연이 되지 않는다.
3. 한 번의 스티치에 든 부분 영상은 **같은 픽셀 형식**이어야 한다(D6). 이전에는 float 부분 영상 사이의 UINT16 하나가 받아들여졌다(설계 §3 측정, 추정 19×8). 형식이 다르면 `XPE_ERR_INVALID_INPUT`, 출력(`widthOut`/`heightOut`, 합성 버퍼)은 그대로.
4. **부분 영상 사이의 크기 규칙은 두지 않는다.** 크기가 서로 어떻게 맞아야 하는지는 아직 없는 스티칭 알고리즘이 정한다. 지금 지어낸 규칙은 아무도 주지 않은 요구다. 시험이 이 **부재**를 고정한다(크기가 다른 세 장은 통과).

## 증거 (Evidence)

- ci-ai: `[  PASSED  ] 419 tests.` 스킵 5, 실패 0, 컴파일 경고 0 (M1 414 → +5). 스텁: `[  PASSED  ] 341 tests.` 스킵 83, 실패 0, 경고 0 (M1 336 → +5; 새 시험 5건 모두 스텁에서 돈다).
- 로컬 doxygen 1.12.0(CI 순서, `WARN_AS_ERROR`): `exit=0`, `: (warning|error)` 줄 0. `check_header_docs.py` 0 findings. 린트 0 error. (헤더 주석을 바꿨다.)
- 새 시험 5건(`test_ai_input_validation.cpp`):
  - `ThePartCountCapIsSixteenAndIsAnImplementationSafetyCap`: 값 16 고정(`static_assert` + `EXPECT_EQ`)
  - `SixteenPartsAreAcceptedAndSeventeenAreRefusedByBothStitchFunctions`: 2·15·16 장은 거부되지 않고(추정 함수는 `XPE_OK`), 17 장은 둘 다 `INVALID_INPUT`, 거부된 추정은 `w=777`·`h=888` 을 건드리지 않음. 17 장이 메모리에 실재하므로 잘못 받아들이는 구현도 유효한 장만 읽는다(안전한 반증)
  - `AWildPartCountIsRefusedWithoutReadingTheArray`: 2 장·개수 40억/`0xFFFFFFFF`
  - `ThePartsOfOneStitchHaveOnePixelFormat`: 이질적인 한 장(UINT16)을 첫째·가운데·마지막 위치에 두고 둘 다 거부 + 출력 그대로 + 알림 없음. 대조군: 모두 UINT16 이면 입구를 통과(`XPE_OK`)
  - `NoSizeRuleBetweenPartsIsInventedBecauseTheAlgorithmDoesNotExistYet`: 6×6·9×4·3×11 세 장이 추정을 통과
- 반증 6개(`m2_arms_out.txt`), 매번 빌드 성공, `ai.cpp` 바이트 동일 복원, 대조군 93/93:
  - K1 추정 함수의 상한 제거 → 17 장 시험만 빨강
  - K2 합성 함수의 상한 제거 → 17 장 시험만 빨강
  - K3 상한이 하나 어긋남(17 장 허용) → 17 장 시험만 빨강
  - K4 형식 비교 제거 → 형식 시험만 빨강
  - K5 마지막 부분 영상을 비교에서 빼먹음 → 형식 시험만 빨강(이질적인 한 장을 마지막에 둔 케이스가 잡음)
  - K6 크기 규칙(첫 장과 같은 폭)을 지어냄 → 크기 규칙 부재 시험만 빨강
- **판별력이 없는 시험 하나**: `AWildPartCountIsRefusedWithoutReadingTheArray` 는 K1·K2(상한 제거)에서도 초록이었다. 상한이 없으면 루프가 2 장짜리 배열 뒤의 쓰레기를 읽는데, 그 쓰레기가 우연히 검증을 통과하지 못해 `INVALID_INPUT` 이 나온다(정의되지 않은 동작). 즉 이 시험이 지키는 것은 "읽지 않음" 이 아니라 "거부" 뿐이고, 판별하는 시험은 17 장 시험이다. 남겨 둔 이유는 40억이라는 극단값이 기록되어 있다는 것뿐이며, 읽기 자체를 관찰하는 시험은 이 계측으로는 쓸 수 없었다(시험이 배열 뒤 메모리를 보호해 주지 않는다).

## 기준 (Baseline)

같은 트리 `dev/postprocess`(M1 f8203dda 위, main 병합 후 — 이 실행 직전 `git merge main` 은 gui 파일만 가져왔고 ai 에는 영향 없음), `cmake --build --preset ci-ai` 와 `ci-post`, 이 실행의 출력.

## 바뀌는 반환 코드와 호출자

- `xpe_stitch_images`·`xpe_stitch_estimate_size`: 17 장 이상 → `XPE_ERR_INVALID_INPUT`(이전: 개수가 배열을 넘으면 정의되지 않은 읽기), 형식이 다른 부분 영상이 섞이면 → `XPE_ERR_INVALID_INPUT`(이전: 받아들임).
- 두 함수는 C# 이 부르지 않는다(`GuiAiRunner.cs` 는 init/shutdown/worker_state/bone_suppress 만). 다른 소비자의 사용은 확인하지 못했다.
- 헤더: 상수 `XPE_AI_MAX_STITCH_PARTS` 와 두 함수의 `@param partCount`·`@return` 갱신.

## 미검증 (Gaps)

- `partCount` 상한 16 에 대한 제품 요구는 없다. 값 자체의 타당성은 근거가 없고 "큰 값을 막는 안전 상한" 이라는 역할만 주장한다.
- `xpe_stitch_images` 출력 버퍼 용량 검사(설계 D6 의 선택 항목)는 하지 않았다. 알고리즘이 없어 필요한 용량이 정해지지 않았고, `XPE_ERR_BUFFER_TOO_SMALL` 의 의미도 그대로다.
- 형식이 다른 경우의 코드를 `INVALID_INPUT` 으로 골랐다(D6 의 `UNSUPPORTED_FORMAT` 과의 선택). 형식 자체는 지원되고 조합이 잘못이므로 입력 오류로 읽었다. 반대 의견이 있으면 코드 한 줄과 시험 한 줄이다.
- 위의 "판별력 없는 시험" 한 건.

## 잔여 위험

- 이전에 받아들여지던 혼합 형식 입력이 거부된다. 현재 두 함수의 소비자는 시험뿐이며 제품 소비자는 확인하지 못했다.
