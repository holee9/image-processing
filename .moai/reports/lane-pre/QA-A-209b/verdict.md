# QA-A-209b 판정 — 설정 JSON 은 한 번 파싱하고, 같은 규칙으로 읽는다 (Codex #49, Refs #233)

## 1. 주장 (Claim)

| # | 주장 |
|---|------|
| C1 | 호출자가 준 설정 텍스트가 빈 문자열·공백뿐이면 `XPE_ERR_CONFIG_INVALID` (NULL 은 기본값). XCal 파일의 설정 블록은 길이 0 이면 "블록 없음"으로 로드되고, 길이가 있는 공백뿐 블록은 거부된다 — 두 규칙을 따로 시험 |
| C2 | 최상위 키가 중복이면 이름과 값이 무엇이든 거부 (`{"future":1,"future":2}` 포함). 같은 한 번의 파싱 안에서 검사 |
| C3 | `xcal_reader` 의 압축 메타는 길이가 지정된 블록을 JSON 객체 하나로 검증하고 최상위 두 키만 읽는다. 중첩 키는 메타가 아니다. 한 쪽 키만 있거나 정수가 아니면 CONFIG_INVALID. 쓰기 쪽의 `{}`·`}}` 결함도 고침 |
| C4 | `xpe_nonlinearity_correct` 는 bad_alloc 을 OUT_OF_MEMORY 로, 그 밖의 예외를 PROCESSING_FAILED 로 바꾸고 프레임을 건드리지 않는다 |
| C5 | 호출당 파싱 1회. 파이프라인 호출 24.7 µs → 약 1.9 µs |
| C6 | 계약 문구: 문자열-또는-토큰 경로와 베어 숫자 경로를 구분해 공개 헤더에 적음 |

## 2. 증거 (Evidence) — 이 실행, 이 트리, 증거 폴더 `evidence/`

- 빨강 먼저: `02_red_dll.txt` — 새 시험 13건 실패 / 19건 통과, `02_red_oom_*` — 비-OOM 시험 2건이 "예외가 C ABI 함수를 빠져나갔다".
- 초록: `04_green_dll.txt` (798 → 이후 시험 1건 추가), 최종 `60_verify_summary.txt`:
  - DLL 시험 799 실행 / 791 통과 / 8 건너뜀(기존과 같은 8), 셔플(seed 20261) 동일
  - preprocess OOM 47, common OOM 12, common 69, `ctest -N` 967 (기준 954), 내보내기 preprocess 차이 0 / common 16, 헤더 문서 0건, 프리셋 12/12, 린트 통과(`70_lint.txt`)
- 반증 (한 번에 하나, 전체 빌드, 복원 후 `cmp` 일치 — `50_arms_round1.txt`, `51_arms_round2.txt`, `52_arms_round3.txt`):

| 손상 | 빨개진 시험 |
|------|-------------|
| 중복 검사 제거 | 중복 행 포함 ConfigStrictParse 10건 + `XCalCompressionMetaTest.ACompressionKeyGivenTwice…` |
| 공백 텍스트를 빈 것으로 | 공백 시험 2건 + init 시험 |
| 중첩 키도 최상위로 | 최상위 읽기 시험 8건 + 압축 중첩 시험 3건 |
| 한쪽 키만 있어도 통과 | `HalfAPairIsRefused…` (1차에서는 **안 터졌다**: 반쪽 키 + RLE 페이로드는 크기 검사가 먼저 걸러 눈이 멀었음 → 원본 크기 페이로드 시험을 추가해 터짐) |
| 쓰기 쪽 쉼표 항상 | `TheWriterMakesValidJsonForAnEmptyCallerObjectToo` |
| nonlinearity 가드 약화 | OOM 스윕 2건 |
| init 검사 제거 | `TheInitConfigFollowsTheSameRule` |
| 길이 있는 공백 블록 허용 | `ABlankConfigBlockOfAFileIsRefused…` |

  (writer·nonlin 반증은 1차에 `/WX` 로 빌드가 실패해 무효였고 약화 방식으로 다시 했다 — `50_arms_round1.txt` 에 실패 기록 보존.)
- 성능 (`40_bench.txt`, 같은 프로그램): 8 bypass 플래그·8×8 프레임 파이프라인 호출 1.99 / 1.93 / 1.87 µs (이전 24.7 µs, 옛 strstr 형태 1.36 µs).
- 할당 실패 스윕 지점 수 (`41_sweeps_209.txt` ↔ `42_sweeps_209b.txt`): 파이프라인 진입점 464/493/567/524/524 → 40/75/89/100/100 (19회 재파싱 제거), offset 로드 6 → 10 (블록을 reader 에서 한 번 파싱하므로 +4), `xpe_nonlinearity_correct` 23 + 표 경로 16 (신규).
- 행위 기준 JSON 읽기 검색 (`10_json_reader_search.sh`, 결과 `11_search_after.txt`): 남은 곳은 `helpers.cpp` 한 곳(SAX 파서)과 `xpe_config_parse*` 호출뿐. 대조군 `12_search_control_prefix_tree.txt` — 수정 전 트리에서 같은 검색이 `xcal_reader.cpp` 의 `strtoul`/`strtoull` 을 잡는다(`cfg.find(key_compression)` 은 변수 키라 `.find("` 패턴으로는 안 잡히고 숫자 스캔으로 잡힘). 검색에서 `preprocess.cpp::xpe_preprocess_init` 의 `json::parse` 가 새로 드러나 같은 규칙으로 통일.

## 3. 기준선 귀속 (Baseline-attribution)

- 기준: 카드 시작 시점 HEAD `bbb1ec40` 의 DLL 788 / OOM 45 / ctest 954. 위 모든 수는 이번 실행에서 `verify209b.sh` 가 같은 트리에서 측정한 값.
- 시험 증가: DLL +11, OOM +2, ctest +13 (954 → 967, 일치).

## 4. 미검증 (Gaps)

- 계획의 "XCal 블록 길이 > 0 이고 공백뿐 → CONFIG_INVALID" 는 내 결정(카드는 두 규칙을 따로 시험하라고만 함). 필요 시 뒤집기 쉽다.
- 베어 비-JSON 토큰(`+2`, `.5`, NaN, `0x10`)은 이제 CONFIG_INVALID — 이전 strtod/무시 동작과의 호환 변화이며 헤더에 명시. 외부 호출자(GUI, clients)가 이런 설정을 보내는지는 이 레인 밖이라 확인하지 못함.
- `xpe_preprocess_init` 의 bad_alloc 은 기존 `catch(...)` 로 PROCESSING_FAILED (OUT_OF_MEMORY 아님) — 기존 동작을 바꾸지 않았다.
- CI 빌드 구성(다른 컴파일러)·실제 하드웨어 실행은 이 로컬 MSVC 실행으로 관측하지 못함.
- 204 3/3 의 나머지(verify/bpm/runtime_detection/nonlin LUT 생성 OOM 가드)는 이 카드 범위 밖.

## 5. 잔여 위험 (Residual-risk)

- `+2` 같은 토큰을 보내던 외부 설정은 갑자기 거부된다 (의도된 정책이지만 배포 전 확인 필요).
- XCal 파일의 설정 블록이 JSON 이 아닌 오래된 파일은 이제 reader 단계에서 거부된다. 레포 안 파일은 전부 시험이 통과했으나 외부 보관 파일은 미확인.
- 위 반증 8건은 모두 시험이 터지는 것을 관측했지만, 파서를 nlohmann 에서 바꾸면 NUL·중복 판정이 달라질 수 있어 NUL 거부는 파서와 독립으로 `memchr` 에 둔다.
