# QA-A-208e — Codex #47 보류 2건: 공개 헤더 문서만

기준 커밋 `79d2e9c5`(QA-A-208d) 위. 감사 원문은 `evidence/00_codex47_audit_original.md`. 이슈 #233. 코드 변경 없음(헤더의 주석만).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | `preprocess_api.h` 안에서 `XpeCalibQualityMeta` 의 Fields·멤버·`xpe_calib_get_quality_meta`·`xpe_calib_load_gain` 설명이 플래그 기준의 한 계약을 말한다. 옛 표지값 규칙과 옛 범위 문구는 0건. | `02_stale_phrases_after.txt` (대조군 포함), `03_remaining_minus_one_mentions.txt` |
| 2 | 새 헤더 + 옛 모듈의 플래그가 정해지지 않는다는 한계와 반대 방향의 안전함이 구조체 설명(`BUILD-MATCHED USE`)과 getter 설명에 적혀 있다. | 헤더 diff |
| 3 | 헤더 문서 검사 경고 0, 빌드·시험은 직전과 같다(코드 무변경). | `31_header_docs.txt`, `21_build_final.txt` |

## 2. 1번 — 무엇을 고쳤나

- `xpe_calib_get_quality_meta`: "품질 없는 기록은 `valid`=0 이고 **현재 기록을 설명하는 필드(`has_r_squared` 포함)가 0**, 이력(`previous_r_squared`, `has_previous_r_squared`)은 직전 기록에서 유지"로. `has_r_squared` 를 `r_squared` 보다 먼저 확인할 것, `fit_r_squared` 없는 부분 기록은 `valid=1`·`has_r_squared=0`·`r_squared=-1.0`(채움 값이며 실제 값일 수도 있어 값이 아니라 플래그가 판별)임을 적음. 이력은 `valid` 이고 `has_r_squared=1` 인 마지막 기록의 R², 존재는 `has_previous_r_squared`.
- Codex 가 지목한 줄 밖에서 같은 종류의 낡은 문구를 더 찾았다: `xpe_calib_load_gain` 설명이 "나머지 세 필드는 [0, 255] 의 정수"라고 적고 있었고(208c 의 0..4 / 1..10 / 0..4 범위, JSON 한 객체·길이 전체·NUL 거부와 어긋남), 구조체의 `valid` 항목이 "`previous_r_squared` 외 전부 0" 이라고 적고 있었다. 둘 다 새 계약으로 고쳤다(적재 설명의 오류 줄에도 "한 객체가 아니거나"를 추가).
- 대조: 옛 문구(`not the -1.0`, `0.0 to 1.0`, `(0-3)`, `0..3`, `[0, 255]`, `if none)`, `r_squared != -1.0`, `"not given")` 꼴)는 0건. 같은 방식의 검색으로 새 문구가 잡히는지가 대조군이다: `has_r_squared` 13, `has_previous_r_squared` 8, `BUILD-MATCHED USE` 2, `1..10` 2건. 남은 `-1.0` 언급 9곳은 전부 새 규칙(채움 값, 실제 값일 수도 있음)과 일관(`03`).

## 3. 2번 — 버전 확인 안내가 불가능한 이유 (리더 결정 필요)

카드의 안내("새 플래그를 쓰는 호출자는 모듈의 기존 버전 함수로 DLL 버전을 확인")는 **쓸 수 없다**. 버전 함수는 있다 — `xpe_preprocess_version()`(`preprocess_api.h:59`, 구현 `src/preprocess.cpp:39`) — 하지만:

- 값은 소스 상수 `"0.1.0"` 이고, 이 상수는 첫 스캐폴딩 커밋 `baead627` 이후 바뀐 적이 없다(`git log -S'"0.1.0"' -- src/preprocess.cpp` 는 그 커밋 하나만 낸다). 202e 의 `valid` 바이트도 이번 두 플래그도 올리지 않았다 → 옛 DLL 과 새 DLL 이 같은 문자열을 돌려준다.
- 시험(`test_preprocess_version.cpp`)도 값을 고정하지 않는다("0.1.0" 은 CMake 프로젝트 버전과도 다른 소스 상수라는 주석).

그래서 문서에는 확인 방법을 지어 쓰지 않고 **사실과 제한**을 적었다: 플래그를 읽는 호출자는 그 플래그를 쓰는 모듈 빌드와 같은 빌드(헤더와 모듈을 한 묶음으로 배포)여야 하고, 이전 헤더 호출자 + 새 DLL 은 안전하며(바이트를 안 봄), 새 헤더 + 옛 DLL 은 안전하지 않으며(패딩), 모듈에는 둘을 구분할 런타임 수단이 없다고(버전 문자열이 상수이므로).

새 ABI 경로는 만들지 않았다. **결정이 필요한 것**: (a) 지금의 문서(한 빌드로 배포) 그대로 두기, 또는 (b) 플래그가 있는 빌드부터 `xpe_preprocess_version()` 의 값을 올리고(예: `0.2.0`) 그 값을 "플래그가 유효한 최소 버전"으로 헤더에 적기. (b)는 값을 고정하지 않은 기존 시험들은 깨지지 않지만, 이 문자열을 R1 바이너리 상태 판별에 쓰는 GUI 프로브(헤더 주석)의 기대는 확인하지 않았다.

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| preprocess DLL 시험 | 780 실행 / 772 통과 / 8 건너뜀, 섞기도 동일 |
| preprocess OOM exe / common OOM / common | 45 / 12 / 69 통과 |
| `ctest -N` | 946 (직전과 같음) |
| 수출 이름 / 헤더 문서 / 프리셋 | 48·16 불변 / 0 findings / 12 of 12 |

## 5. 미검증 (Gaps)

- 옛 DLL 에서 새 헤더 호출자가 실제로 어떤 값을 읽는지는 실행하지 않았다(패딩의 값은 정의되지 않는다는 점만 적었다).
- `xpe_preprocess_version()` 을 읽는 GUI 프로브가 값 변경에 어떻게 반응하는지(옵션 b)는 확인하지 않았다.
- 헤더 문서 검사는 형식·완전성을 보며 문장이 서로 모순되지 않는지는 위 `grep` 대조로 확인했다(완전한 의미 검증은 아님).
