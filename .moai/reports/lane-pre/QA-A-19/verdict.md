# QA-A-19 — `xpe_test_inject_alert` 개명 3/3: 별칭 제거, export 16 복귀 (#111 종결)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #111
**baseline**: `origin/main` 병합 → `b56f295` (B-24 호출부 전환 포함)

## 1. 주장 (Claim)

`xpe_test_inject_alert` 별칭의 선언과 본문을 제거해 export 를 **17 → 16** 으로 되돌렸다.
dumpbin 실측 16 == 헤더 선언 16 이며, 그 일치는 테스트로 고정된다.

## 2. 증거 (Evidence)

### 2.1 착수 조건 — B-24 반영 확인

```
$ git grep -c "xpe_test_inject_alert" -- modules/enhance_basic
0건
```

카드가 요구한 착수 조건(enhance_basic 잔재 0건)을 병합 직후 실측으로 확인했다.

### 2.2 dumpbin 실측 — 16 (`a19-dumpbin.log`)

```
xpe_alert_push              xpe_get_pending_alert
xpe_alloc_image             xpe_get_pending_alert_count
xpe_clear_alerts            xpe_init
xpe_configure               xpe_log_flush
xpe_copy_image              xpe_log_set_file
xpe_error_string            xpe_log_set_level
xpe_free_image              xpe_shutdown
xpe_get_param_range         xpe_version

개수: 16
```

`xpe_test_inject_alert` 는 목록에 없다. A-18 에서 17이던 것이 REQ-P0-008 정본 16 으로 복귀했다.

### 2.3 테스트로 고정 (16)

`HeaderDeclaresSeventeenExportedFunctions` → `HeaderDeclaresSixteenExportedFunctions`,
목록에서 별칭 항목 제거, `EXPECT_EQ(16u, …)`. 두 이름의 주소가 다른 것을 확인하던
단언은 별칭이 사라졌으므로 함께 제거했다. `DeprecatedAliasPushesOntoTheSameQueue`
삭제 — 그것이 옛 이름의 마지막 호출처였다.

A-18 이 심은 `EXPECT_EQ(17u, …)` 가 이 되돌림을 강제하는 기계적 장치였고, 실제로
그 역할을 했다(값을 고치지 않으면 이 커밋은 빌드는 되나 테스트가 실패한다).

### 2.4 재실측

| 구성 | 결과 | 로그 |
|---|---|---|
| `ci-common` | `100% tests passed, 0 failed out of 63` | `a19-common.log` |
| `ci-preprocess` | `100% tests passed, 0 failed out of 404` | `a19-pre.log` |

64 → 63, 405 → 404 은 삭제한 별칭 테스트 1건.

### 2.5 저장소 전체 grep (카드 4항, `a19-grep.log` 전문 첨부)

수정 후 잔재 **37건**, 분포:

| 위치 | 건수 | 성격 |
|---|---|---|
| `tests/common_unit/test_xpe_common.cpp` | 13 | 고아 트리 — A-13 이 처분. 빌드되지 않음 |
| `tests/common/test_xpe_common.cpp` | 11 | 동일 |
| `docs/project/api-spec.md` | 4 | 문서 — leader 소유 |
| `.moai/project/api-spec.md` | 4 | 동일(사본) |
| `.moai/specs/…` 4개 파일 | 4 | SPEC 이력 문장 — leader 소유 |
| `modules/common/include/xpe/common/xpe_common_api.h` | 1 | **이력 문장** (아래) |

마지막 1건은 export 수를 설명하는 NOTE 안의 문장이다:

```
 * the deprecated xpe_test_inject_alert alias is gone, so the count is back to
```

호출도 선언도 아니고 "왜 16인가"를 설명하는 서술이라 남겼다. 카드가 예상한
"두 트리 + 문서 이력 문장" 범주에 해당한다고 판단했다 — 제거를 원하면 알려달라.

**`modules/common` 의 호출·선언 잔재는 0건이다.**

### 2.6 변경 범위

| 파일 | 내용 |
|---|---|
| `xpe_error.h` | 별칭 선언 + `@deprecated` 문서 블록 제거 |
| `xpe_common.cpp` | 별칭 본문(1줄 포워딩) 제거 |
| `xpe_common_api.h` | 주석 3곳: 17 → 16, 목록에서 별칭 삭제 |
| `modules/common/tests/test_xpe_common.cpp` | 별칭 테스트 삭제, export 단언 16 |

`tests/common*`, api-spec, SPEC 은 카드 지시대로 손대지 않았다.

## 3. baseline 귀속

`origin/main` 병합 트리 `b56f295`. dumpbin 은 이번 빌드의
`build/ci-preprocess/bin/xpe_common.dll` 에 대해 실행. 모든 수치는 이번 실행 관측.

## 4. Gaps (미검증)

- **enhance_basic 을 빌드하지 않았다.** `ci-common`/`ci-preprocess` 모두
  `BUILD_ENHANCE_BASIC=OFF` 다. B-24 가 호출부를 전환했음은 grep 0건으로 확인했으나,
  **링크가 실제로 되는지는 관측하지 않았다** — CI 가 판정한다.
- **C# P/Invoke 는 확인하지 않았다.** A-18 때 leader 가 grep 0건을 보고했으나 이번에
  재확인하지는 않았다. 옛 이름을 잡는 바인딩이 남아 있다면 지금 끊긴다.
- `tests/common`, `tests/common_unit` 의 24건은 **컴파일되지 않는 트리**라는 전제로
  두었다. 두 트리가 어떤 경로로도 빌드되지 않음을 이번에 직접 확인하지는 않았다
  (`tests/CMakeLists.txt` 는 앞선 병합에서 삭제됨).

## 5. 잔여 위험

- **ABI 축소는 되돌릴 수 없는 방향이다.** 17-export 버전을 이미 링크한 바이너리가
  외부에 있다면 그 호출자는 이제 진입점을 찾지 못한다. A-18 → A-19 사이에 배포가
  없었다는 전제 위에 있다.
- 별칭 제거로 `xpe_common.dll` 의 export 표면이 A-18 이전과 동일한 개수가 됐지만
  **이름은 다르다**(`xpe_alert_push`). 개수만 보는 검증은 이 차이를 잡지 못한다 —
  §2.3 테스트가 이름 단위로 주소를 취하는 이유다.
