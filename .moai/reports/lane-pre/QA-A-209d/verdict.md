# QA-A-209d 판정 — Codex #56: 레거시 경로의 OOM 출력 불변, 레거시 판정 범위 (Refs #233)

## 1. 주장 (Claim)

| # | 주장 |
|---|------|
| C1 | `read_xcal_file` 이 레거시 경고 문자열 할당에 실패하면 `OUT_OF_MEMORY` 를 돌려주고 **모든 출력 인자**(헤더·config·payload·문서)가 호출 전 값 그대로다 |
| C2 | 레거시 수용은 옛 writer 가 만들 수 있었던 두 모양, **DEFECT + 방법 1(RLE)** 파일로만 한정된다. 키 쌍만 있는 `}}`, DEFECT 가 아닌 타입의 같은 꼬리, 방법 ≠ 1 은 거부 |

## 2. 구현 (`xcal_reader.cpp`)

1. 경고 문자열을 **출력 인자를 건드리기 전에** 만든다(`std::string legacy_alert`). 그 뒤 출력 확정(할당 없는 대입·이동) → 던지지 않는 `xpe_alert_push`. 순서: 문자열 → 출력 확정 → 알림.
2. 모양 판정: 쌍 앞 접두사는 `{` … `,` 여야 한다. `{`+공백+`,` 뿐이면 모양 B(쉼표와 여분 `}` 제거), `{`와 `,` 사이에 공백 아닌 것이 있으면 모양 A(호출자 멤버 — 여분 `}` 만 제거). 접두사가 `{` 뿐이거나 `,` 로 끝나지 않으면 거부.
3. 복구는 헤더 타입이 DEFECT 일 때만, 복구 뒤 `xcal_compression` 이 정확히 `1` 일 때만. 복구 본문은 그 뒤에도 엄격 파싱(중복·중첩·쌍 규칙)을 통과해야 한다.
4. **같은 시험이 드러낸 인접 결함 하나**: 압축 해제기(`rle_decode`)의 할당 실패가 `OUT_OF_MEMORY` 로 돌아오는데 reader 가 모든 비-OK 를 `CONFIG_INVALID` 로 바꾸고 있었다. `OUT_OF_MEMORY` 는 그대로 전달하도록 고쳤다(출력은 원래 불변).
5. `xcal_format.h` 의 레거시 단락을 새 범위에 맞췄다.

### 209c 에서 내가 한 잘못 (보고)

209c 의 반증 두 개(쉼표 접두사 검사, 복구본 쌍 검사)가 "아무 시험도 빨개지지 않았다"는 이유로 둘 다 **중복 방어**라고 보고 제거했다. 쉼표 접두사 검사는 중복이 아니었다: 접두사가 `{` 뿐인 `{"xcal_compression":1,"xcal_raw_payload_len":N}}` 는 여분 `}` 를 떼면 엄격 파싱을 통과한다 — Codex #56 이 그 반례를 찾았다. 반증이 터지지 않았다는 것은 "그 입력에 대한 시험이 없었다"는 뜻일 수 있었고, 나는 코드를 추론해 "중복"으로 결론 냈다. 이번에 그 형태(`ThePairAloneWithADoubledBraceIsNotWhatTheOldWriterMade`)를 시험으로 만들고 검사를 되살렸다. 복구본 쌍 검사(제거된 다른 하나)는 이번 시험으로도 여전히 어떤 시험에도 걸리지 않지만, 위 3 의 방법 검사가 복구본에서 쌍의 한 키를 읽으므로 그 역할을 일부 대신한다 — 이 부분은 "중복"이 아니라 "시험으로 구별되지 않는 것"으로 적어 둔다.

## 3. 증거 — `evidence/`

- 빨강 먼저 (`02_red_dll.txt`, `02_red_oom.txt`): 키 쌍만 있는 `}}`·DEFECT 아닌 타입·방법 2 가 모두 수용됨(시험 2건 실패), OOM 스윕이 5건을 보고(코드 불일치와 헤더·config·payload·문서의 변경).
- 초록 (`60_verify_summary.txt`): DLL 812 실행 / 804 통과 / 8 건너뜀(기존과 동일), 셔플 동일, OOM 56, common 69 + 12, `ctest -N` 990, 내보내기 차이 0, 헤더 문서 0건, 프리셋 12/12.
- 시험:
  - `ThePairAloneWithADoubledBraceIsNotWhatTheOldWriterMade` — 공백 세 변형 모두 거부
  - `TheOldWritersShapesAreOnlyRepairedForAnRleCompressedDefectFile` — OFFSET 타입 파일(32×32 float32 = RLE 해제 크기와 같은 4096 바이트 — 잘못 복구되면 성공해 버리는 입력)에 두 모양, DEFECT 의 방법 2 두 모양은 거부, 방법 1 은 수용(대조군)
  - `OomInjection.ALegacyFileThatFailsToBuildItsWarningLeavesEveryOutputArgumentUntouched` — 옛 writer 파일(80자 경로)을 `read_xcal_file` 로 읽는 할당 실패 스윕: 실패 결과마다 헤더(감시 바이트), config, payload, 문서가 감시값 그대로이고 알림이 없으며 코드는 `OUT_OF_MEMORY`; 성공하면 해제된 4096 바이트 맵과 문서
  - 기존 209c 시험(옛 모양 적재·거부 16종·무결성) 전부 통과. 시험 fixture 가 옛 파일을 읽은 뒤 알림을 비운다(`TearDown` 의 `xpe_clear_alerts`) — 위생 가드가 알림 잔류를 잡았다.
- 반증 5건 (한 번에 하나, 전체 빌드, 복원 후 `cmp`; `50_arms.txt`):

| 손상 | 빨개진 시험 |
|------|-------------|
| 경고 문자열을 출력 확정 뒤에 만듦 | OOM 스윕 |
| 모양 A 가 호출자 멤버 없이도 통과 | 키 쌍만 있는 `}}` 시험 |
| 헤더 타입 검사 제거 | 타입·방법 시험 |
| 방법 == 1 검사 제거 | 타입·방법 시험 |
| 압축 해제 OOM 을 `CONFIG_INVALID` 로 되돌림 | OOM 스윕 |

## 4. 미검증 (Gaps)

- 옛 writer 가 만든 실제 파일은 구할 수 없어 `90c1b6b1` 소스에서 재현한 파일로 시험했다(209c 와 같은 한계).
- 복구본 문서의 쌍을 따로 확인하는 검사는 되살리지 않았다. 방법 1 확인이 복구본의 `xcal_compression` 을 읽지만 `xcal_raw_payload_len` 을 읽는 시험은 없다(쌍의 꼬리 모양 검사가 숫자열을 이미 보증한다).
- 이 로컬 MSVC 구성 한 곳에서만 실행했다.

## 5. 잔여 위험

- 레거시 수용은 임시 호환이다. 현장 파일이 모두 새 writer 로 다시 만들어지면 제거해야 한다.
- 모양 A 는 호출자 멤버가 유효한 JSON 멤버 목록이라는 것을 엄격 파싱에 맡긴다. 호출자 설정이 JSON 이 아니었던 옛 파일은 계속 거부된다(의도).
