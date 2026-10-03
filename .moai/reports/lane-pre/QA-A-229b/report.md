# QA-A-229b — Codex #93 보류 3건 (229 M4 세션 일치, #245)

## 1. 결과

세 건 모두 고치고 각각 반증을 달았다. 전체 `xpe_preprocess_tests` 1002 통과·8 건너뜀·종료 0(기본 순서와 셔플 시드 1·2·9 모두), `xpe_preprocess_oom_tests` 73 통과, doxygen 1.12.0 종료 0·경고 0. 반증 8개 전부 발화(§5).

## 2. 보류 1(높음) — 캐시 적중이 옛 세션 ID 를 쓴다

**재현**: 같은 크기의 다른 세션 값으로 헤더를 고치고 쓰기 시각을 되돌리면 직접 로더는 `CONFIG_INVALID`, 캐시 적중은 `OK`. 원인은 코드에서 확인했다: `get_copy` 가 `FileStamp`(크기·쓰기 시각)만 비교하고 엔트리의 `meta.sessionId` 를 `install_*` 에 넘긴다. 헤더는 SHA-256 대상(`config || payload`) 밖이라 헤더 편집은 해시도 안 바뀐다. 시험 `HeaderOnHit.ACachedHitJudgesTheSessionOfTheFileAsItStands` 가 세 종류(offset·gain·defect) 모두에서 직접 로더와 적중의 판정이 같음을 단언한다.

**수정**: 적중 경로가 판정에 쓰는 값을 현재 파일에서 읽는다. 적중 때 이미 하던 "열 수 있는가" 검사(`can_open_for_read`)가 `open_check` 로 바뀌어 같은 열기에서 헤더 152바이트를 함께 읽는다. 엔트리는 발행 때의 헤더 바이트를 들고 있고(`EntryMeta::header`, 파일을 적재하기 **전에** 읽어 스탬프와 같은 순서 — 그 사이에 파일이 바뀌면 엔트리의 헤더가 낡아 다음 적중이 어긋남을 본다), `get_copy` 가 둘을 `memcmp` 해 다르면(또는 한쪽이 읽히지 않았으면) 엔트리를 버리고 Miss 로 돌려 평범한 로더가 파일을 다시 읽고 판정한다. 비용은 적중당 파일 열기 1회(이미 하던 것)에 152바이트 읽기.

**같은 문제가 있는 다른 값 — 표** (적중이 판정에 쓰는 것을 전부 훑음):

| 적중이 판정에 쓰는 값 | 어디서 오나 (수정 전) | 수정 후 | 확인한 시험 |
|---|---|---|---|
| 세션 id | 엔트리(`meta.sessionId`) | 현재 헤더와 같을 때만 사용 | `ACachedHitJudgesTheSessionOfTheFileAsItStands` ×3종류 |
| 만료 시각 | 엔트리(`meta.expiryMs`) | 현재 헤더와 같을 때만 사용 | `ACachedHitJudgesTheExpiryOfTheFileAsItStands` ×3종류(헤더 만료를 과거로 편집 → 직접·적중 모두 `CALIBRATION_EXPIRED`) |
| 파일 종류(type) | 엔트리 `kind`(발행한 로더) | 현재 헤더의 type 도 비교 | `ACachedHitRefusesAFileWhoseTypeWasEditedLikeTheMissDoes` ×3종류 |
| 크기·형식·너비·높이 | 엔트리 버퍼 | 헤더 비교에 포함 | 위 시험이 type 편집으로 같은 경로를 탐(너비·높이·형식은 개별 시험 없음) |
| 생성 시각(`timestamp`) | 엔트리 | 헤더 비교에 포함 | 개별 시험 없음 |
| 파일 SHA-256 칸 | 사용 안 함 | 헤더 비교에 포함 → **내용을 새로 쓴 파일**(새 해시)이 크기·쓰기 시각이 같아도 알아챈다 | `CacheSameVerdict.ARewriteThatKeepsBothSizeAndWriteTimeIsNoticedThroughTheHeader` |
| 페이로드 바이트 | 엔트리(해시 재계산 없음) | **그대로 — 헤더가 같고 페이로드만 제자리 편집하면 알아채지 못한다**(문서화된 한계) | `APayloadEditThatLeavesTheHeaderAloneIsNotNoticedUntilCacheClear` |
| config JSON(gain 품질 메타) | 엔트리(파싱된 값) | **그대로 — 헤더가 같은 config 제자리 편집은 알아채지 못한다**(같은 한계) | 시험 없음 |

기존 시험 하나가 바뀌었다: `CacheSameVerdict.AChangeThatKeepsBothSizeAndWriteTimeIsNotNoticedUntilCacheClear` 는 "새로 쓴 파일(새 해시)이 크기·시각이 같으면 모른다"를 고정하고 있었는데, 이제 헤더의 해시 칸 때문에 알아챈다. 시험 둘로 갈랐다: 새로 쓴 파일은 **알아챈다**(위 표), 페이로드 한 바이트를 제자리에서 뒤집은 파일은 여전히 모르고 직접 로더는 `CONFIG_INVALID`(문서화된 불일치로 유지, 캐시를 비우면 같은 호출이 직접 로더의 판정에 닿는다). 파일 머리말의 주석도 같은 내용으로 고쳤다.

## 3. 보류 2(보통) — 경고 플래그가 잠금 밖에서 옛 `mixed` 로 설정

`xpe_calib_session_check_locked` 가 커밋과 **같은 잠금 구간 안에서** 혼합 여부를 판정하고 경고 상태 전이(플래그 올림/내림)를 확정해 `shouldWarn` 을 돌려준다. `xpe_calib_session_warn(shouldWarn)` 는 잠금을 잡지 않고 알림만 보낸다(결정은 이미 끝남). 파이프라인 세트 적재는 `xpe_calib_session_transition_locked` 를 커밋 잠금 구간 안에서 부른다. 이전: warn 이 잠금을 풀고 나서 다시 잡아 플래그를 세웠고, 그 값은 다른 커밋이 낡게 만들 수 있는 `mixed` 였다.

시험(`OomInjection.TheSessionWarningStateIsSettledInTheCommitsOwnCriticalSection`, 제품 소스를 직접 컴파일하는 oom 실행 파일, 테스트 전용 훅 `xpe_session_after_commit_hook` — warn 맨 위): 스레드 A 가 혼합 상태를 커밋하고 경고 전에 정지 → B 가 gain 을 지정된 것으로 교체(혼합 아님) → A 재개 → 이후 진짜 혼합 적재. 기대: A 의 경고 1건, 이후 혼합 적재에서 1건 더(합계 2). 구 방식은 A 가 재개하며 플래그를 올려둔 채여서 합계 1.

## 4. 보류 3(보통) — 세션 칸 형식 미검증, 63바이트 절단

`validate_xcal_header` 에 검사 11: 64바이트 칸에 NUL 이 있고(최대 63바이트 텍스트), 첫 NUL 뒤가 전부 0 이고, 앞이 올바른 UTF-8(과잉 인코딩·서로게이트·U+10FFFF 초과·잘린 열·단독 연속 바이트 없음)이어야 한다. 아니면 `XPE_ERR_CONFIG_INVALID`. 빈 칸과 `generated` 는 그대로 허용. 검증기는 로더 셋이 부르므로 한 곳에서 세 맵 종류에 적용된다. (정정 QA-A-229d: 원문은 `xpe_calib_check_expiry` 도 부른다고 적었으나 틀렸다.) 63바이트 절단 비교는 형식 검사로 사라진다(유효한 id 는 ≤63바이트이므로 잘릴 것이 없다).

시험(`SessionField.*`, 세 맵 종류 각각): 종결자 없는 64바이트 거부 / 앞 63바이트가 같고 마지막만 다른 두 64바이트 id 둘 다 거부 / 63바이트이고 마지막 문자만 다른 두 id 는 유효하고 **서로 다름**(대조군: 같은 id 는 일치) / 종결자 뒤 비제로 거부 / 잘못된 UTF-8 6종 거부 / 올바른 UTF-8("세션-1" 대 "세션-2")은 받아들이고 서로 다름으로 판정 / 빈 칸·`generated` 허용.

## 5. 반증 (`evidence/arm_i*.txt`)

| 손상 | 걸린 시험 수 | 비고 |
|---|---|---|
| i1 적중이 헤더를 비교하지 않음 | 4+ | 세션·만료·type·새로 쓴 파일 시험 |
| i2 적중이 헤더 앞 40바이트만 비교(세션 칸 제외) | 1 | `...JudgesTheSession...` 만 — 세션 시험이 세션 칸을 따로 본다는 증거 |
| i3 엔트리가 헤더를 저장하지 않음(적중 불가) | 6 | `AnUnchangedHeaderStillHits` 등 — 적중이 실제로 일어남을 지키는 대조 |
| i4 플래그를 잠금 밖에서 옛 값으로 설정(구 방식) | 1 | 스레드 시험 |
| i5 형식 검사 끔 | 4 | |
| i6 UTF-8 검사 끔 | 1 | |
| i7 제로 패딩 검사 끔 | 1 | |
| i8 종결자 검사 끔 | 2 | |

모두 원본 복원 후 `git status` 에 의도한 변경만 남음.

## 6. 범위 밖 (리더 기록)

헤더 세션 칸이 SHA-256(무결성) 대상이 아닌 것 — 형식 변경이라 #245 에 기록만.

## 7. 미검증 (Gaps) · 잔여 위험

- 캐시 적중의 페이로드·config JSON 제자리 편집(헤더 불변)은 여전히 못 잡는다(§2 표). 해시를 적중마다 재계산하지 않는다는 기존 설계 그대로다.
- 헤더 비교는 적중당 파일 열기 + 152바이트 읽기 추가를 의미한다. 이 비용은 측정하지 않았다(열기는 이미 하던 것).
- 너비·높이·형식·생성 시각을 개별로 편집하는 시험은 없다(헤더 전체 비교에 포함되고 type 편집 시험이 같은 경로를 지난다는 것만 확인).
- 스레드 시험은 훅으로 순서를 고정한 한 가지 인터리빙이다. 다른 인터리빙(예: B 가 먼저 커밋)은 시험하지 않았다. 판정과 상태 전이가 같은 잠금 안에 있으므로 커밋 순서대로 직렬화된다는 것은 코드로 확인했다.
- [정정 QA-A-229d] 이 줄은 틀렸다: 검사 11 은 `xpe_calib_check_expiry` 에 적용되지 않았다. 그 함수는 헤더를 직접 읽어 magic·version 만 보고 OK 를 줬고 `validate_xcal_header` 를 부르지 않았는데, 나는 호출 관계를 열어 보지 않고 "헤더 검증이 같은 함수"라고 적었다(검증기 머리 주석의 낡은 목록을 옮긴 것). 229d 에서 세션 칸 검증을 공통 함수 `validate_xcal_session_field` 로 꺼내 check_expiry 도 부르게 했다.
- 단일 구성(`ci-preprocess`)에서만 돌렸다. GUI(C#) 통합 시험은 돌리지 않았다.
