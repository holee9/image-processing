# QA-A-08 — 고아 테스트 트리 처분 판정 (#109 마무리)

- **Refs**: #109 · **브랜치**: `dev/preprocess` · **조사만 — 삭제·커밋 없음**
- **결론**: 카드의 전제 두 가지가 실측과 다르다.
  **① 기록된 사유("GTest 충돌")는 원래 사유가 아니다.** 최초 사유는 **타깃 이름 충돌**이었다.
  **② `tests/common` 은 재활성화가 불가능하다.** `CMakeLists.txt` 가 삭제돼 존재하지 않는다.
- 재현 실측은 **차단됐다** — 진짜 차단 지점이 main 소유 파일이라 우회하지 않고 보고한다.

## 1. 최초 비활성화 사유 — 타깃 이름 충돌 (`b513b8b`)

```diff
-add_subdirectory(common_smoke)
-add_subdirectory(common)
-add_subdirectory(common_unit)
+# Note: common tests are built in modules/common/CMakeLists.txt to avoid target name conflicts
+# add_subdirectory(common_smoke)
+# add_subdirectory(common)
+# add_subdirectory(common_unit)
```

원 주석은 **"모듈 쪽에서 빌드하므로 타깃 이름 충돌을 피하려고"** 라고 사유를 명확히 적었다.
`modules/common/CMakeLists.txt:116` 이 `test_xpe_common` 타깃을 만들고, `tests/` 쪽도 같은
테스트를 빌드하려 했다. CMake 는 동일 타깃명 재정의를 오류로 처리한다.

## 2. 사유가 나중에 바뀌었다 — `f057d9e` (2026-04-20)

같은 커밋이 GTest 조달 방식을 바꾸면서 주석도 갈아치웠다:

```diff
-# Include FetchContent for Google Test
-FetchContent_Declare(googletest ... GIT_TAG v1.14.0 ...)
-FetchContent_MakeAvailable(googletest)
+# Use vcpkg-provided GTest only (must be found before enable_testing)
+find_package(GTest CONFIG)

-# Note: common tests are built in modules/common/CMakeLists.txt to avoid target name conflicts
```

현재 주석 `# Temporarily disabled due to GTest conflicts` 는 이때 자리 잡았다.
FetchContent→vcpkg 전환 맥락에서 "GTest" 라는 단어가 붙었지만, **원래 원인(타깃명 충돌)과
다른 것을 가리킨다.** 다음 사람이 읽으면 GTest 조달 문제로 오인한다.

QA-A-04 에서 이 주석을 인용해 "GTest 충돌로 임시 비활성화"라고 보고했는데, 그것은
**주석을 그대로 옮긴 것이고 주석 자체가 부정확했다.** 여기서 정정한다.

## 3. `tests/common` 은 재활성화 자체가 불가능하다

```
tests/ai_tests/                  CMakeLists 있음
tests/common/                    CMakeLists 없음   ←
tests/common_smoke/              있음
tests/common_unit/               있음
tests/e2e_post_pipeline/         있음
tests/enhance_advanced_tests/    있음
tests/preprocess/                있음
tests/preprocess_smoke/          있음
```

`tests/common/CMakeLists.txt` 는 **`f057d9e` 에서 83줄이 삭제**됐다.
`add_subdirectory(common)` 주석을 풀면 CMake 는 즉시 오류를 낸다 — 충돌 이전에
빌드 파일이 없다. 즉 이 트리는 "임시 비활성화"가 아니라 **이미 부분적으로 해체된 상태**다.

`tests/common_unit` 은 CMakeLists 가 남아 있고 타깃명이 `xpe_common_unit` 으로
`test_xpe_common` 과 충돌하지 않는다. 이쪽은 재활성화 가능성이 있다.

## 4. 재현 실측 — 차단됨 (우회하지 않음)

카드는 "한 트리만 `add_subdirectory` 주석을 풀고 빌드"를 지시했다. 그대로 시도했다:

```
tests/CMakeLists.txt:17   # add_subdirectory(common)  →  add_subdirectory(common)
클린 리빌드  →  CONFIGURE_EXIT=0  BUILD_EXIT=0
```

**그러나 이 통과는 무의미하다.** 컴파일된 87 TU 중 `tests/` 유래는 0건이었고,
`test_xpe_common` 은 여전히 `modules\common\CMakeFiles\` 에서 나왔다.

원인 — `tests/` 디렉터리 자체가 **루트에서** 비활성화돼 있다:

```cmake
CMakeLists.txt:158-161
# Tests (disabled - use BUILD_TESTING instead to avoid conflicts)
# if(BUILD_TESTS)
#     xpe_add_optional_subdirectory(tests)
# endif()
```

`tests/CMakeLists.txt` 는 **읽히지도 않는다.** 그 안의 두 주석 줄을 풀어도 아무 효과가 없다.
진짜 게이트는 루트다.

**여기서 멈춘 이유**: 루트 `CMakeLists.txt` 는 main 소유다
(`lane-sessions.md` §1 — main: 루트 `CMakeLists.txt`, `cmake/`, `.moai/`, `.claude/`, `docs/`).
임시 변경이라도 소유 경계를 넘는 편집이므로 하지 않았다. 우회하지 않고 보고한다.

프로브는 원복했고 `git status tests/CMakeLists.txt` 로 무변경을 확인했다.

> QA-A-06 에서 "빌드 통과가 곧 검증은 아니다"를 배운 것이 여기서 다시 작동했다.
> 컴파일 TU 출처를 확인하지 않았다면 "충돌 해소됨"으로 잘못 보고했을 것이다.

## 판정 재료 정리

| 항목 | 실측 |
|---|---|
| 기록된 사유 | "GTest 충돌" — **부정확**. 원래는 타깃명 충돌 (`b513b8b`) |
| 사유가 바뀐 시점 | `f057d9e` (FetchContent→vcpkg 전환과 동시) |
| `tests/common` 재활성화 | **불가능** — CMakeLists 가 `f057d9e` 에서 삭제됨 |
| `tests/common_unit` 재활성화 | 가능성 있음 — CMakeLists 존재, 타깃명 `xpe_common_unit` 로 비충돌 |
| 현재 충돌 재현 | **미확인** — 루트가 `tests/` 를 통째로 막고 있고 루트는 main 소유 |
| 고유 커버리지 | 0 (QA-A-04·05 에서 확정) |

## lead 판정 요청

1. **루트 `CMakeLists.txt:158-161` 을 임시로 풀어 재현 실측을 할지** — main 소유라 lead 또는
   main 이 수행해야 한다. 하라고 지시하면 그때 하겠다.
2. `tests/common` 은 CMakeLists 가 이미 없어 "임시 비활성화"라는 설명이 성립하지 않는다.
   해체가 진행되다 만 상태로 볼지 판정 필요.
3. **주석 정정은 별도 사안이다.** 현재 문구가 원인을 잘못 가리키고 있다.
   `tests/CMakeLists.txt` 는 Lane A 소유 경계 밖이 아니나(테스트 디렉터리), 카드가
   "주석 해제를 커밋하지 말 것"이라 했으므로 문구 수정도 지시를 기다린다.

## 미검증 (Gaps)

- **충돌의 현재 재현 여부를 확인하지 못했다.** 루트 게이트가 main 소유라 멈췄다.
  따라서 "충돌이 여전한가 / 해소됐는가" 는 **판정할 수 없는 상태**다.
- `tests/common_unit` 단독 활성화도 시도하지 않았다 — 같은 루트 게이트에 막힌다.
- `BUILD_TESTING` 경로(루트 주석이 권하는 대안)가 실제로 동작하는지 확인하지 않았다.
- `f057d9e` 가 `tests/common/CMakeLists.txt` 를 삭제한 의도는 커밋 메시지에 없다.
  P2-ADV 기능 커밋에 섞여 들어간 정리로 보이나 근거는 없다.

---

# 판정 수령 (2026-09-01, lead) — PASS. 미검증 항목이 정적 분석으로 해소됐다.

## 요청 (a) 루트 임시 해제 → 하지 않는다. 불필요.

lead 가 **빌드 없이 타깃명 정적 대조로 충돌을 특정**했다. 실제 충돌은 **정확히 1건**:

```
xpe_ai_tests    modules/ai/CMakeLists.txt:259   ←→   tests/ai_tests/CMakeLists.txt
```

혼동하기 쉬운 비충돌 쌍도 확인됐다 — `tests/` 의 `test_enhance_advanced` 와 modules 쪽
`test_${MODULE_NAME}`(→ `test_xpe_enhance_advanced`)는 **다른 이름**이다.
나머지(`xpe_common_smoke`, `xpe_common_unit`, `xpe_preprocess_smoke`,
`test_runtime_detection`, `test_gain_correction`, `test_offset_correct`)는 전부 고유하다.

**본 문서 Gaps 의 "충돌 현재 재현 여부 판정 불가"는 이로써 해소된다.**
빌드 프로브가 아니라 정적 대조로 답이 나왔다 — 소유 경계를 넘지 않고도 규명 가능했던
문제였다.

## 요청 (b) `tests/common` 상태 판정 → **해체 잔해**

"임시 비활성화"가 아니다. 근거 3가지가 겹친다:
CMakeLists 없음 + 고유 커버리지 0(QA-A-05 확정) + 동반 3개 파일 이미 삭제됨.
**처분 대상이 맞다.** 실행은 별도 카드로 나온다.

## 요청 (c) 주석 정정 → lead 처리 완료 (커밋 `d42fbaa`)

루트 158 과 `tests/CMakeLists.txt` 양쪽에 실제 원인이 기재됐다.

## 본 조사가 드러낸 더 큰 문제 — 이슈 #113

루트가 `tests/` 를 추가하지 않으므로 **`tests/CMakeLists.txt` 전체가 읽히지 않는다.**
루트는 `tests/e2e_post_pipeline` 만 직접 부른다. 그 안의 모든 것이 무효다:

```
add_subdirectory 5건 / enable_testing() / find_package(GTest CONFIG)
check · check_verbose        → 타깃 없음. make check 실패
coverage · coverage_summary  → 타깃 없음. ENABLE_COVERAGE=ON 무효
```

REQ-P0-005/006 이 이 타깃들을 근거로 삼는다면 **추적성 문제**다. #113 에 기록됐다.

QA-A-04 에서 "`check` 타깃이 자기가 만들지 않는 `test_xpe_common` 에 의존한다"고 남긴
관찰이 여기서 실체를 드러냈다 — 의존이 어긋난 정도가 아니라 타깃 자체가 생성되지 않는다.
