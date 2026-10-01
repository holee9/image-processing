# QA-A-194 (#220) — xpe_verify_gain 후속 수정 (Codex #16 보류 2건 + 문구 1건)

시작 HEAD `cc27e2ee` (QA-A-193 위). 출처: 리더 메시지(Codex #16 감사 결과 QA-A-192 보류 2건·med).

| # | 내용 | 처리 |
|---|---|---|
| ① | 공개 헤더에 ABI 비호환 명시, "시험 밖 호출자 없음" 범위 한정 | `preprocess_api.h` 의 `xpe_verify_gain` 에 `@warning`: 같은 수출 이름이라 링크·로드는 되지만 4인자 호출은 오동작, 호출자 모두 재빌드·동시 배포. "저장소 검색 범위(QA-A-189)에서 못 찾았고 그 밖은 검색하지 않았다"로 한정 |
| ② | 0 크기 프레임: 형식 검사가 먼저 걸려 UNSUPPORTED_FORMAT, 0화소 분기는 도달 불가 | 치수 0 을 형식 검사보다 먼저 `XPE_ERR_INVALID_INPUT` 으로 처리(치수 불일치 검사 뒤), 도달 불가였던 `pixel_count == 0` 분기 제거, `@return` 정정 |
| ③ | overall_pass 는 Phase 1 판정이라는 한 줄 | `@note`: 단독으로 릴리스 승인이 아니며 릴리스 게이트는 `gain_semantics != UNKNOWN` 을 따로 요구(FUNC-018) |

## TDD

- 시험 2건 추가 (`test_verify_gain_semantics.cpp`): `AZeroDimensionIsInvalidInputNotUnsupportedFormat`(0×1·1×0·0×0 × 의미 3종), `AFormatMismatchOnANonEmptyFrameStaysUnsupportedFormat`(새 검사가 0 크기만 잡는다는 대조).
- 빨강 (`evidence/02_red_run.txt`): 0 크기 시험만 실패, 실제 반환 `-7`(UNSUPPORTED_FORMAT). 형식 불일치 시험은 통과.
- 초록 (`04_`): GainSemantics 5건 통과.
- 반증 팔 (`05_`, 빌드 `BUILD_EXIT=0`): 0 크기 검사를 지우면 `AZeroDimensionIsInvalidInputNotUnsupportedFormat` 만 빨강, 원복 후 파일 동일.
- 전체 시험 736 통과 0 실패(직전 734 + 2), 셔플(시드 1940) 통과, `check_header_docs.py` 0 findings.

## Gaps

- 수출 이름 목록·`ctest -N`·프리셋 점검은 이 카드에서 다시 돌리지 않았다 (시그니처 불변, 소스 한 분기와 주석 변경). QA-A-193 의 같은 점검 결과가 직전 상태 기준이다.
- 치수 불일치(BUFFER_TOO_SMALL)가 0 크기 검사보다 먼저 판정된다 (0×1 대 1×1 은 BUFFER_TOO_SMALL). 요구된 순서가 아니라 기존 순서를 유지했다.
- `api-spec.md` 쪽 문구는 리더 몫이라 건드리지 않았다.
