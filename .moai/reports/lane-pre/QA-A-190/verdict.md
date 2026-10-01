# QA-A-190 (#216) — xpe_verify_offset/_gain/_defect 헤더의 REQ-P1A-010/011/012 오인용 정정 (주석만)

시작 HEAD `fb1756c3` (`evidence/00_head.txt`).

## 무엇을 고쳤나

`modules/preprocess/include/xpe/preprocess_api.h` 의 세 문서 블록이 "`SRS-CALIB-FUNC-016 / REQ-P1A-010: Offset correction verification`" (그리고 `017 / 011: Gain …`, `019 / 012: Defect …`)이라고 적고 있었다. REQ-P1A-010/011/012 는 **보정 실행**(`xpe_offset_correct`·`xpe_gain_correct`·`xpe_defect_correct`)을 요구하는 요구이고, 그 본문에는 `xpe_verify_*` 가 없다 (QA-A-189 가 spec.md 본문 43개 요구를 이름으로 세어 확인). 그래서 `xpe_verify_pipeline` 블록(QA-A-183)처럼 사실대로 적었다:

```
 * SRS-CALIB-FUNC-016: Offset correction verification.
 * No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them); the
 * REQ-P1A-010 this line used to cite is "Offset Correction Execution" (xpe_offset_correct),
 * not verification.
 * SRS-CALIB-FUNC-036 states the measured/unmeasured reporting contract for every
 * xpe_verify_*.
```

`017`(Gain, REQ-P1A-011 "Gain Correction Execution", `xpe_gain_correct`)과 `019`(Defect, REQ-P1A-012 "Defect Correction Execution", `xpe_defect_correct`)도 같은 형태다. 요구 제목은 spec.md 의 `#### REQ-P1A-010/011/012:` 헤더 그대로다.

## 증거

| 확인 | 결과 |
|---|---|
| 주석만 변경 | 주석을 지운 뒤 HEAD 와 동일, 대조군(코드 토큰 하나 변경)은 감지됨 (`01_comment_only_check.txt`) |
| 제품 변경 | `git diff` 는 `preprocess_api.h` 하나(+18/−4)뿐 |
| 빌드 | `BUILD_EXIT=0`, 경고 없음 (`02_build.txt`) |
| 수출 | `dumpbin` 이름 53 = 53, C ABI(맹글 제외) 48 = 48, 차이 없음 (`03_exports_after.txt` 와 QA-A-189 `01_dumpbin_exports.txt` 비교) |
| 전체 시험 | 725 통과, 0 실패, 종료 코드 0 (`04_full_suite.txt`) |

## Gaps

- 생성된 Doxygen HTML(`docs/help/generated/`)은 손대지 않았다 (생성물, 다음 문서 생성 때 갱신).
- `SRS-CALIB-FUNC-036` 인용은 그 요구가 `xpe_verify_*` 전체를 "모든 verify 함수"로 부른다는 SRS:467 을 근거로 했다.
