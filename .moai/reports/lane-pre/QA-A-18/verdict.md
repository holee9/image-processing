# QA-A-18 — `xpe_test_inject_alert` 개명 1/3 (#111 3항)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #111
**baseline**: `origin/main` `ebeb3f4` 병합 (`0 0` 동기)

## 1. 주장 (Claim)

`xpe_alert_push` 를 새 export 로 추가하고, `xpe_test_inject_alert` 는 새 이름을 호출하는
한 줄 별칭으로 남겼다. export 는 임시로 **17** 이며 dumpbin 실측 17 == 헤더 선언 17 이
테스트로 고정된다. 호출부(enhance_basic)와 api-spec/SPEC 은 건드리지 않았다.

## 2. 증거 (Evidence)

### 2.1 dumpbin 실측 — 17 (`a18-dumpbin.log`)

```
xpe_alert_push              xpe_get_pending_alert
xpe_alloc_image             xpe_get_pending_alert_count
xpe_clear_alerts            xpe_init
xpe_configure               xpe_log_flush
xpe_copy_image              xpe_log_set_file
xpe_error_string            xpe_log_set_level
xpe_free_image              xpe_shutdown
xpe_get_param_range         xpe_test_inject_alert
                            xpe_version
```

```
$ dumpbin /exports build\ci-preprocess\bin\xpe_common.dll | (xpe_ 심볼 계수)
17
```

17개 이름이 아래 §2.2 테스트의 17개 항목과 **일대일로 일치**한다.

### 2.2 "17 == 헤더 선언 17" 을 테스트로 고정 (`a18-run.log`)

`HeaderDeclaresSeventeenExportedFunctions` 가 17개 함수의 **주소를 모두 취한다.**
링커가 전부 해소해야 하므로 헤더-바이너리 불일치는 소비 모듈에서 진입점 누락으로
뒤늦게 드러나는 대신 **빌드에서 깨진다.** 개수 단언은 `EXPECT_EQ(17u, ...)` 이며
A-19 에서 16 으로 되돌린다.

```
[       OK ] XpeCommonTest.HeaderDeclaresSeventeenExportedFunctions (0 ms)
```

> 저장소에 기존 export 자동 단언은 없었다. `.moai/specs/SPEC-XPE-P0/export-verification.md:62`
> 의 체크리스트가 "16" 을 적고 있으나 SPEC 수정은 카드가 금지했으므로 손대지 않았다.
> 이 테스트가 그 자리를 기계적으로 대신한다.

### 2.3 두 이름이 같은 큐에 넣는다 (`a18-run.log`)

```
[       OK ] XpeCommonTest.DeprecatedAliasPushesOntoTheSameQueue (0 ms)
```

새 이름으로 하나, 별칭으로 하나 넣고 큐에서 **순서·메시지·severity 를 각각 확인**한다.
두 함수의 주소가 서로 다른 것도 단언한다(별칭은 forwarding 이지 같은 심볼이 아니다).

### 2.4 호출부 전환

`modules/common/tests/test_xpe_common.cpp` 의 호출 **10곳**을 새 이름으로 옮겼다.
(카드에는 7곳으로 적혀 있었으나 실측 10곳이다 — 그 뒤 커밋들에서 늘었다.)
파일 상단의 로컬 `extern "C"` 선언 블록은 제거했다: 이제 `xpe_error.h` 가 두 이름을
모두 선언하고 `xpe_common_api.h:18` 이 그것을 포함한다.

옛 이름은 §2.3 테스트 **한 곳에만** 남아 있다 — 별칭 경로를 실제로 지나게 하는 자리이고,
A-19 에서 별칭과 함께 제거된다.

### 2.5 헤더 주석 갱신

`xpe_common_api.h` 3곳(export 수 NOTE, Error/Alert 목록, 하단 선언 위치 NOTE)에
새 이름과 임시 17을 반영했다. "REQ-P0-008 은 15" / "18-function total" 이라는 기존
불일치 문구는 SRS 결정 사항이라 그대로 두고 문구만 이어 붙였다.

### 2.6 재실측

| 구성 | 결과 |
|---|---|
| `ci-preprocess` (`a18-ctest-pre.log`) | `100% tests passed, 0 tests failed out of 371` |
| `ci-common` (`a18-ctest-common.log`) | `100% tests passed, 0 tests failed out of 64` |

369 → 371 은 이번에 추가한 2건.

## 3. baseline 귀속

`origin/main` `ebeb3f4` 병합 트리. dumpbin 은 `build/ci-preprocess/bin/xpe_common.dll`
(이번 빌드 산출물)에 대해 실행. 모든 수치는 이번 실행 관측.

## 4. Gaps (미검증)

- **enhance_basic 이 여전히 링크되는지 확인하지 않았다.** 옛 이름 호출부는 Lane B 소유라
  건드리지 않았고, `ci-preprocess`/`ci-common` 프리셋은 enhance_basic 을 빌드하지 않는다
  (`BUILD_ENHANCE_BASIC=OFF`). 별칭이 export 로 남아 있으므로 링크는 되어야 하지만
  **관측하지 않았다** — CI 또는 B-24 가 판정한다.
- **C# / P/Invoke 쪽은 확인하지 않았다.** 옛 이름을 P/Invoke 로 잡는 코드가 있다면
  별칭이 살아 있어 지금은 동작하지만, A-19 에서 끊긴다.
- `export-verification.md` 체크리스트의 "16" 은 여전히 16으로 남아 있다(수정 금지).
  문서와 바이너리가 이 카드 기간 동안 어긋난 상태다 — 의도된 임시 상태.

## 5. 잔여 위험

- **export 17은 임시 상태이며, 되돌리는 책임이 A-19 한 장에 몰려 있다.** A-19 가 누락되면
  REQ-P0-008 정본 16과 영구히 어긋난다. 테스트의 `EXPECT_EQ(17u, ...)` 가 그 되돌림을
  강제하는 유일한 기계적 장치다.
- 별칭은 `@deprecated` 주석만 달았고 컴파일러 경고(`__declspec(deprecated)`)는 붙이지
  않았다. 붙였다면 `/WX` 가 켜진 Lane B 빌드를 즉시 깨뜨린다 — 전환 카드(B-24) 전에
  그렇게 하는 것은 이 카드의 범위가 아니다.
- 새 이름이 늘면서 이 DLL 의 ABI 표면이 한 개 넓어졌다. 그 사이에 배포된 바이너리가
  있다면 17-export 버전이 외부에 남는다.
