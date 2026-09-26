# QA-B-33 게이트 보고서 — `tests/ai_tests/` → `modules/ai/tests/` 이동

**카드**: QA-B-33 (#109 #113, B-31 정정)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-33/`
**커밋**: `680c0d6`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 6파일을 `git mv` 로 이동했다 — 내용 변경 0 | PASS |
| C2 | CMake 경로·주석·`EXISTS` 가드를 새 위치로 갱신했다 | PASS |
| C3 | **빌드 그래프가 실제로 새 경로를 참조한다** — "빌드가 됐다" 가 아니라 | PASS |
| C4 | 케이스 수 불변: 이동 전 202 → 이동 후 **202** | PASS |
| C5 | 남은 `tests/ai_tests` 참조를 전수 조사했다 — 전부 main 소유 문서 | PASS |
| C6 | 무회귀: ci-post 445 / ci-dicom 131 | PASS |
| C7 | 빌드 경고 0 (`XPE_WARNINGS_AS_ERRORS=ON`) | PASS |

---

## 2. 증거 (Evidence)

### 2.1 이동 (C1)

```
$ git status --short
R  tests/ai_tests/test_ai_abi.cpp              -> modules/ai/tests/test_ai_abi.cpp
R  tests/ai_tests/test_ai_fallback.cpp         -> modules/ai/tests/test_ai_fallback.cpp
R  tests/ai_tests/test_ai_ipc_bridge.cpp       -> modules/ai/tests/test_ai_ipc_bridge.cpp
R  tests/ai_tests/test_ai_model_card.cpp       -> modules/ai/tests/test_ai_model_card.cpp
R  tests/ai_tests/test_ai_model_versioning.cpp -> modules/ai/tests/test_ai_model_versioning.cpp
R  tests/ai_tests/test_ai_worker_isolation.cpp -> modules/ai/tests/test_ai_worker_isolation.cpp
```

git 이 **rename 으로 인식**했고 diff 는 `6 files changed, 0 insertions(+), 0 deletions(-)` —
파일 내용은 한 바이트도 바뀌지 않았다. 테스트 단언을 건드리지 않았다는 기계적 증거다.

`tests/ai_tests/` 디렉터리는 비었고, git 은 빈 디렉터리를 추적하지 않으므로 트리에서 사라진다.

### 2.2 CMake 갱신 (C2) — `modules/ai/CMakeLists.txt`, +8/−5

```diff
-    # Note: Test files are in tests/ai_tests/ directory, not modules/ai/tests/
+    # Test sources live in modules/ai/tests/, like every other module.
+    # They used to sit in the repository-root tests/ai_tests/ and were pulled in
+    # from here by absolute path, which made the tree read as orphaned from the
+    # root CMakeLists (#109). Moved in QA-B-33; the target is unchanged.

-    # Check if test files exist in tests/ai_tests directory
+    # Check if test files exist in this module's tests/ directory
-        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/tests/ai_tests/${_src}")
+        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src}")
-            message(STATUS "xpe_ai test source missing: tests/ai_tests/${_src}")
+            message(STATUS "xpe_ai test source missing: modules/ai/tests/${_src}")

-            list(APPEND _XPE_AI_TEST_SOURCE_PATHS "${CMAKE_SOURCE_DIR}/tests/ai_tests/${_src}")
+            list(APPEND _XPE_AI_TEST_SOURCE_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/tests/${_src}")
```

`CMAKE_SOURCE_DIR`(저장소 루트) → `CMAKE_CURRENT_SOURCE_DIR`(모듈 디렉터리)로 바꿨다.
주석에는 **왜 옮겼는지와 이슈 번호**를 남겼다 — 다음에 이 구조를 보는 사람이 같은 오판을
하지 않도록.

### 2.3 그래프가 새 경로를 가리키는가 (C3) — 이 카드의 실질 검증

"빌드가 통과했다" 는 충분한 증거가 아니다. 옛 경로가 캐시에 남아 있었다면 파일이 사라진
시점에 실패했어야 하므로 통과 자체가 어느 정도 신호이긴 하나, 그래프를 직접 봤다:

```
$ grep -oE "modules.ai.tests.test_ai_[a-z_]*\.cpp" build/ci-ai-b20/build.ninja | sort -u
modules\ai\tests\test_ai_abi.cpp
modules\ai\tests\test_ai_fallback.cpp
modules\ai\tests\test_ai_ipc_bridge.cpp
modules\ai\tests\test_ai_model_card.cpp
modules\ai\tests\test_ai_model_versioning.cpp
modules\ai\tests\test_ai_worker_isolation.cpp
```
**6/6 새 경로.** 옛 소스 경로(`tests\ai_tests\*.cpp`)는 0건.

`ai_tests` 문자열은 여전히 50번 나오지만 분류해 보면 전부 **타깃 이름**이다:
```
8  modules\ai\CMakeFiles\xpe_ai_tests.dir\
8  bin\xpe_ai_tests.pdb
7  cmake_object_order_depends_target_xpe_ai_tests
6  bin\xpe_ai_tests.exe
...
```
타깃명 `xpe_ai_tests` 는 바꾸지 않았다 — 카드 범위가 아니고, 바꾸면 CI·문서의 아티팩트
이름이 함께 흔들린다.

`xpe_ai test source missing` 메시지는 구성 로그에 **0건** — `EXISTS` 가드가 새 경로에서
6파일을 모두 찾았다는 뜻이다.

### 2.4 케이스 수 불변 (C4)

| 시점 | ci-ai 전체 ctest | 근거 |
|---|---:|---|
| 이동 **전** | **202** | `_before.log` (이 카드에서 직접 측정) |
| 이동 **후** | **202** | `_after.log` |

```
_before.log:  100% tests passed, 0 tests failed out of 202
_after.log :  100% tests passed, 0 tests failed out of 202
```

**카드가 적은 "192/192" 는 QA-B-32 시점의 값이다.** 그 뒤 병합으로 Lane A 테스트가 들어와
202 가 됐다. 합격선의 취지는 "이동으로 케이스가 늘거나 줄지 않을 것" 이므로, 이 카드 안에서
**이동 직전과 직후를 같은 명령으로 재서** 비교했다. 192 라는 숫자를 그대로 인용했다면
기준선이 틀린 채로 통과를 주장하는 셈이 된다.

### 2.5 남은 참조 전수 (C5)

```
$ grep -rn "tests/ai_tests" --include=*.txt --include=*.cmake --include=*.yml \
      --include=*.yaml --include=*.json --include=*.cpp --include=*.h --include=*.md .
```

| 파일 | 건수 | 소유 | 조치 |
|---|---:|---|---|
| `docs/project/vvp_ai.md` | 9 | **main** | 손대지 않음 |
| `docs/project/rtm_ai.md` | 2 | **main** | 손대지 않음 |
| `docs/project/sdd_ai.md` | 2 | **main** | 손대지 않음 |
| `.moai/project/lane-sessions.md` | 1 | **main** | 손대지 않음 |
| `modules/ai/CMakeLists.txt` | 2 | Lane B | **의도적 잔존** — 이동 이력 주석(:217)과 #113 이력 주석(:294) |

빌드·CI 에 영향을 주는 참조는 **0건**이다. 문서 4종은 카드 2항대로 목록만 보고한다.

특히 `docs/project/vvp_ai.md:62` 는 지금 **거짓이 됐다**:
> `tests/ai_tests/`, not `modules/ai/tests/`

이 문장은 이동 전 사실을 적은 것이고 지금은 반대다. main 소유라 레인이 고치지 않는다.

### 2.6 무회귀 (C6, C7) — `_verify.log`, 필터 없음

```
===CI_POST===    100% tests passed, 0 tests failed out of 445   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 202   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 131   ===DICOM_EXIT=0===
```

빌드 경고 0 (`_after.log` 에 `warning C` 0건, `XPE_WARNINGS_AS_ERRORS=ON`).

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이동 전 ci-ai | 202/202 | `_before.log` — 이 카드에서 이동 직전 실측 |
| 이동 후 ci-ai | 202/202 | `_after.log` |
| ci-post / ci-dicom | 445 / 131 | `_verify.log` (이번 실행) |
| 파일 내용 변경 | 0 | `git diff --stat` = `6 files changed, 0 insertions, 0 deletions` |

---

## 4. 미검증 (Gaps)

- **`ci-post` 구성이 새 경로를 쓰는지 그래프로 확인하지 않았다.** ci-post 는 445/445 로
  통과했고 그 안에 ai 테스트가 포함되지만(QA-B-26 관측), `build/ci-post/build.ninja` 를
  직접 뒤지지는 않았다. ci-ai 만 그래프까지 확인했다.
- **깨끗한 구성(clean configure)에서 검증하지 않았다.** 기존 빌드 디렉터리가 재구성된
  경로만 확인했다. 새 디렉터리로 처음부터 구성했을 때도 동일한지는 관측 밖이다.
- **CI 러너에서의 결과는 모른다.** 로컬 3개 프리셋만 봤다.
- **타깃명 `xpe_ai_tests` 와 실행 파일명은 그대로 두었다.** 이름이 여전히 옛 디렉터리를
  연상시키지만 바꾸면 CI 아티팩트·문서가 함께 흔들려 카드 범위를 넘는다.
- **문서 4종의 갱신 여부는 leader 몫이다.** §2.5 의 `vvp_ai.md:62` 처럼 **지금 거짓이 된
  문장**이 있다는 것까지가 이 보고서의 관측이다.

---

## 5. 잔여 위험 (Residual-risk)

- **문서가 코드보다 뒤처진 상태로 남는다.** `vvp_ai.md` 는 "129 cases across 6 files in
  `tests/ai_tests/`" 라고 적고 있고 경로가 틀렸다. 케이스 수 129 자체는 여전히 맞다.
  문서를 근거로 경로를 찾는 사람은 헛다리를 짚는다 — main 이 갱신할 때까지의 창이다.
- **`tests/` 아래에는 이제 `e2e_post_pipeline` 만 남는다.** 루트 CMakeLists 의 설명
  (`:154-161`)과 실제 구조가 이제 일치한다 — B-31 의 오판을 낳은 불일치가 해소됐다.
  다만 `tests/enhance_advanced_tests` 같은 다른 트리가 같은 유형인지는 이 카드가 보지 않았다.
- **되돌리기는 쉽다.** rename 커밋 하나이고 내용 변경이 없어 `git revert` 로 충분하다.
- 커밋은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_ai.bat` | ci-ai 빌드 + 전체 ctest |
| `_before.log` | **이동 전** 202/202 |
| `_after.log` | **이동 후** 202/202, 경고 0, `source missing` 0건 |
| `_verify.log` | 무회귀 445 / 202 / 131 |
