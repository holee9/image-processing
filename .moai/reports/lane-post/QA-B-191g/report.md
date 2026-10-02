# QA-B-191g — main 의 doxygen 이 `ai_api.h` 로 빨강

카드: QA-B-191g · 관련: #130 · 근거: main `f9803071`(191 병합)의 Documentation Generation 실패

## 주장 (Claim)

원인은 **백틱 안에 든 역슬래시**였다. `ai_api.h` 205행(M4e 가 넣은 라벨 허용 문자 문장)이 ``except `"` and `\` `` 로 끝나는데, doxygen 은 `\`` 을 "닫는 백틱을 이스케이프" 로 읽는다. 그래서 두 번째 코드 스팬이 닫히지 않고 `<tt>` 가 열린 채 주석 블록(191~310행)이 끝났다("end of comment block while expecting command `</tt>`"). 나머지 두 오류(`xpe_bodypart_recognize` 의 매개변수·반환형이 문서화되지 않음)는 그 뒤 따라온 것이다: 블록 전체가 깨져 `@param`·`@return` 이 함수에 붙지 못했다. 문장을 낱말("the double quote and the backslash")로 바꿨고 헤더 주석 한 줄만 바뀐다. 런타임 문자열·동작은 그대로다.

리더가 짚은 "백틱은 줄마다 짝이 맞는다" 는 맞다. 개수가 맞아도 `\`` 가 하나의 백틱을 가린다 — 백틱 개수 세기로는 안 보이는 모양이다.

## 증거 (Evidence)

방법: `build/tools/doxygen/doxygen.exe`(1.12.0, CI 와 같은 버전)를 CI 와 같은 순서로 — `docs/help/doxygen` 에서 `doxygen Doxyfile`(`WARN_AS_ERROR = FAIL_ON_WARNINGS`, `INPUT` 은 `modules/*/include`; `doxygen-awesome` 은 이미 클론돼 있고 `docs/help/generated/doxygen` 을 만들어 둠) — 돌렸다.

- **재현(대조군)**: 수정 전 `exit=1`, `: (warning|error)` 줄 4개 — `ai_api.h:310 end of comment block while expecting command </tt>`(두 번 출력), `ai_api.h:191 parameters of member xpe_bodypart_recognize are not documented`, `ai_api.h:191 return type of member xpe_bodypart_recognize is not documented`. CI 가 보고한 세 오류와 같다.
- **수정 후**: `exit=0`, `: (warning|error)` 줄 **0개**. 같은 실행의 전체 모듈(`ai_worker_protocol.h` 포함)이 경고 0 이다(`ai_worker_protocol.h` 는 191 이 여러 번 바꾼 다른 헤더: 같은 실행에서 경고 없음).
- `python tools/docs/check_header_docs.py` → `20 headers, 0 declarations skipped as unparseable, 0 findings`.
- `git diff`: `ai_api.h` 한 줄(205행) 외 변경 없음.
- 전후 출력: `doxygen_before_after.txt`.

## 이번에 놓친 이유 (정직하게)

191 보고서들이 "doxygen 은 이 PC 에 없다(CI 판정)" 라고 적었다. 그러나 `build/tools/doxygen` 에 187b 때 받은 1.12.0 이 있었다. 도구의 부재를 확인하지 않고 이전 세션의 문장을 옮겨 적었다. 기존 기억 "검증 안 된 사유의 전파" 와 같은 실수다. 앞으로 헤더 주석을 바꾸는 카드는 커밋 전에 위 doxygen 을 돌리고, 보고서 검증 항목에 `exit` 와 `: (warning|error)` 줄 수를 넣는다(명령: `cd docs/help/doxygen && mkdir -p ../generated/doxygen && ../../../build/tools/doxygen/doxygen.exe Doxyfile`).

## 미검증 (Gaps)

- 이 PC 의 doxygen 은 1.12.0 이고 CI 는 `ssciwr/doxygen-install@v2` 로 1.12.0 을 받는다. 같은 버전이지만 CI 러너에서 직접 돈 결과는 아니다.
- 다른 곳에 같은 모양(`\`` 또는 홀수 백틱)이 더 있는지는 doxygen 이 이 실행에서 경고를 내지 않았다는 것으로만 확인했다. 헤더 주석 전체를 이 모양으로 검색하지는 않았다(`ai_api.h` 만 `\`` 검색: 수정 후 0건).

## 잔여 위험

- 헤더 주석 안의 백틱 코드 스팬에 역슬래시를 넣는 모양은 이 방식(doxygen 실행)으로만 잡힌다. `check_header_docs.py` 는 이것을 못 잡는다(0 findings 였던 것이 증거).
