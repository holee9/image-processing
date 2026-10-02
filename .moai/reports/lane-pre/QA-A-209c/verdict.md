# QA-A-209c 판정 — 옛 writer 압축 XCal 호환, XCal 블록 이중 파싱 제거 (Codex #53, Refs #233)

## 1. 주장 (Claim)

| # | 주장 |
|---|------|
| C1 | 옛 writer(`90c1b6b1` 이전)가 만든 압축 XCal 의 두 모양을 새 reader 가 적재한다. 같은 맵, 경고 알림 1건, 저장 바이트는 그대로 |
| C2 | 두 모양을 조금만 벗어난 블록은 계속 거부한다. 수용은 변조 탐지를 약하게 하지 않는다 |
| C3 | XCal 설정 블록은 적재당 한 번만 파싱한다 (reader 가 만든 문서를 gain·nonlinearity LUT 적재기가 받아 쓴다) |

## 2. 옛 writer 가 그 모양을 만든 조건 (코드로 확인)

`90c1b6b1:modules/preprocess/src/xcal_writer.cpp` 의 `write_xcal_file_ex`/`build_config_json` 을 읽었다.

- 압축 메타가 붙는 조건: `compress_defect == true` **그리고** 헤더 타입이 `XCAL_TYPE_DEFECT` **그리고** 페이로드가 비어 있지 않고 **그리고** RLE 결과가 원본보다 작을 때만.
- 호출자 설정이 없거나 길이 0 → 메타만 `{"xcal_compression":N,"xcal_raw_payload_len":M}` — 올바른 JSON (문제 없음).
- 호출자 설정이 있을 때: 마지막 `}` 를 자르고 `,` + (여는 `{` 만 뗀 메타, 자신의 닫는 `}` 포함) + `}` 를 붙임 → 저장된 블록이 `}}` 로 끝난다.
  - 모양 A: `{"mode":"production","xcal_compression":1,"xcal_raw_payload_len":4096}}`
  - 모양 B (호출자 `{}`): `{,"xcal_compression":1,"xcal_raw_payload_len":4096}}`. 호출자가 `{ }`·`{\n}` 면 `{`+공백 뒤에 쉼표가 온다 (쉼표 뒤 공백은 만들 수 없다).
- `xcal_format.h` 와 `preprocess_api.h` 에 이 조건을 적었다.

**해시가 설정 블록을 덮는가**: 예. `read_xcal_file` 은 SHA-256(config || payload)을 **저장된 바이트**로 계산해 헤더와 비교한다(압축 해제 전). 복구는 파싱용 메모리 사본에만 일어나고 저장 바이트는 바꾸지 않으므로 이 수용이 변조 탐지를 약하게 하지 않는다. 시험 `TheRepairLoosensNoIntegrityCheck` 가 모양을 유지하는 한 글자 변조를 해시 불일치로 거부하는지(경고 없이) 확인한다.

## 3. 구현

- `parse_config_block_with_legacy`: 엄격 파싱이 `CONFIG_INVALID` 로 실패했을 때만, 블록이 두 모양 중 하나이면 결정적으로 고쳐(끝의 `}` 하나 제거 / `{`+공백 뒤 쉼표 제거) **다시 엄격 파싱**한다. 고친 결과도 중복·중첩·압축 키 쌍 규칙을 그대로 통과해야 한다. 압축 메타(`xcal_compression`, `xcal_raw_payload_len`)가 블록의 맨 끝 두 멤버(순서 고정, 10진수)인 경우만 후보가 된다.
- 경고는 파일이 해시까지 통과해 받아들여진 **뒤에만** 올린다 (거부된 파일은 복구됐다고 보고하지 않는다).
- 알림 문구 (레인 간 계약 — 접두사 `XPE_WARN_XCAL_LEGACY_CONFIG:` 로 매칭 가능):
  `XPE_WARN_XCAL_LEGACY_CONFIG: the config block of <path> has the doubled closing brace of an older XCal writer and was read after a deterministic repair; regenerate the file with the current writer` (XPE_ALERT_WARNING)
- `read_xcal_file` 에 선택 인자 `XpeConfigDoc* out_config_doc` 추가(기본 nullptr — 기존 호출 39곳 무변경). gain·LUT 적재기는 이 문서에서 품질 필드·선량 범위·확장 시작을 읽는다.

## 4. 증거 (Evidence) — `evidence/`

- 빨강 먼저 (`02_red_dll.txt`, `02_red_oom.txt`): 옛 모양 적재·변조 시험 2건 실패(rc −4), 적재당 파싱 횟수 시험 2건이 `1` 대신 `2`.
- 초록 (`60_verify_summary.txt`): DLL 803 실행 / 795 통과 / 8 건너뜀(기존과 동일), 셔플 동일, OOM 55, common 69 + 12, `ctest -N` 979, 내보내기 차이 0, 헤더 문서 0건, 프리셋 12/12.
- 시험 (`test_xcal_compression.cpp`): 옛 writer 식을 `90c1b6b1` 소스 그대로 시험 안에 재현해(`oldWriterConfig`, 재현이 카드가 적은 두 모양을 만드는지 대조 시험 포함) 호출자 5종(`{"mode":...}`, `{}`, `{ }`, 중첩·배열 포함, `{\n}`)의 파일을 만들고 `readDefect` 와 공개 적재기 둘 다 같은 맵 + 경고를 확인. 현재 writer 파일은 경고 없음. 거부 시험 16종: 끝 `}` 셋·넷, 뒤에 공백, `{,` 와 쌍 사이 멤버, 쉼표 둘, 중복 키(쌍 뒤), 쌍이 마지막이 아님, 복구 뒤 쌍이 중첩, 쌍 반복, 부호 있는 수, 문자열 값, 순서 뒤바뀜, 쌍 없음, 쉼표 누락, 쉼표 뒤 공백.
- 반증 (한 번에 하나, 전체 빌드, 복원 후 `cmp`; `50_arms_round1.txt`, `51_arms_round2.txt`, `52_arms_final.txt`):

| 손상 | 빨개진 시험 |
|------|-------------|
| 레거시 경로 제거 | 옛 모양 적재 + 무결성 시험 |
| 경고를 해시 검사 전에 올림 | 무결성 시험 (거부된 파일이 경고를 냄) |
| gain 적재기가 블록을 다시 파싱 | 적재당 파싱 횟수 시험 |
| LUT 적재기가 블록을 다시 파싱 | 적재당 파싱 횟수 시험 |
| 모양 판정에서 끝 쌍 검사를 뺌 | 거부 시험 |
| 모양 B 판정(쉼표 앞 공백만)을 느슨하게 | 옛 모양 적재 + 무결성 시험 |

  (1차 반증 중 두 손상은 `/WX` 로 빌드가 실패해 무효였고 약화 방식으로 다시 했다 — `50_arms_round1.txt`.)
- **반증이 드러낸 중복 방어 두 개를 제거**했다: (i) 복구본 문서가 쌍을 최상위로 갖는지 따로 보는 검사, (ii) "쉼표 앞의 접두사가 `,` 로 끝나는지" 분기. 둘 다 없애도 어떤 시험도 빨개지지 않았고(`51_arms_round2.txt`), 코드로도 이유가 있다 — 쌍이 문자열의 맨 끝에 있고 엄격 파싱이 성공하면 그 쌍은 최상위 멤버이거나(중첩이면 바깥 객체가 닫히지 않아 파싱 실패), 쉼표가 빠졌으면 파싱 실패다. 판정은 엄격 파싱이 한다는 것을 코드 주석에 적었다.
- 할당 실패 스윕 지점 수 (`41_sweeps_before.txt` ↔ `42_sweeps_after.txt`, 63559d35 → 이 커밋): `xpe_calib_load_gain` 59 → 35, 다항식 57 → 34 (범위 있음·없음 동일), 캐시 미스 69 → 45, 캐시 다항식 59 → 36, 파이프라인 75/89 → 73/87 및 100/100 → 82/82. offset·defect·LUT 적재는 변화 없음(원래 한 번 파싱).
- 린트: 통과 (`70_lint.txt`).

## 5. 미검증 (Gaps)

- 옛 writer 의 모양은 `90c1b6b1` 소스를 읽어 재현했다. 현장에 실제로 보관된 압축 파일로는 확인하지 못했다(이 저장소에 해당 파일이 없다).
- 옛 writer 가 호출자 설정이 JSON 객체가 아닌 경우(예: 임의 텍스트)에 만든 블록은 구분 없이 `}}`·쉼표 규칙만 보므로 복구되지 않는다 — 의도된 거부.
- 같은 모양이 `DEFECT` 가 아닌 파일 타입에도 나타나는지는 확인하지 않았다(옛 writer 는 DEFECT 만 압축했다).
- 이 로컬 MSVC 구성 한 곳에서만 실행했다.

## 6. 잔여 위험 (Residual-risk)

- 이 수용은 임시 호환이다. 모든 현장 파일이 새 writer 로 다시 만들어지면 제거해야 한다(경고 알림이 그 시점을 알려 준다).
- 알림 문구는 레인 간 계약이다 — 바꾸면 접두사 매칭하는 쪽이 깨진다.
