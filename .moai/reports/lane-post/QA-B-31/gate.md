# QA-B-31 게이트 보고서 — `tests/ai_tests/` 는 고아 트리가 아니다 (삭제 중단)

**카드**: QA-B-31 (#109)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-31/`
**코드 변경 0 · 커밋 0 — 카드가 지시한 `git rm -r tests/ai_tests` 를 실행하지 않았다.**

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | **`tests/ai_tests/` 는 고아 트리가 아니다** — `xpe_ai_tests` 타깃의 유일한 소스다 | PASS |
| C2 | `modules/ai/tests/` 는 **존재하지 않는다** — 이식할 대상 스위트가 없다 | PASS |
| C3 | 삭제했다면 ci-ai 의 192건 중 ai 관련 129 케이스가 사라졌을 것이다 | PASS |
| C4 | 카드 전제의 논리적 결함을 특정했다 | PASS |
| C5 | 삭제·이식·커밋을 하지 않고 중단했다 | PASS |

---

## 2. 증거 (Evidence) — `_premise.log`

### 2.1 `modules/ai/tests/` 가 없다 (C2)

```
$ ls -d modules/ai/tests
ls: cannot access 'modules/ai/tests': No such file or directory
```

카드 2항은 "고유 단언만 **모듈 스위트에** 새 TEST 로 이식" 하라고 지시한다.
**이식해 넣을 모듈 스위트가 존재하지 않는다.**

### 2.2 `tests/ai_tests/` 가 곧 모듈 스위트다 (C1)

`modules/ai/CMakeLists.txt:214-226` — 주석이 명시적으로 말한다:

```cmake
    # Note: Test files are in tests/ai_tests/ directory, not modules/ai/tests/
    set(XPE_AI_TEST_SOURCES
        test_ai_abi.cpp
        test_ai_fallback.cpp
        test_ai_model_card.cpp
        test_ai_worker_isolation.cpp
        test_ai_ipc_bridge.cpp
        test_ai_model_versioning.cpp
    )
```

경로 조립 (`:254-256`):
```cmake
        foreach(_src ${XPE_AI_TEST_SOURCES})
            list(APPEND _XPE_AI_TEST_SOURCE_PATHS "${CMAKE_SOURCE_DIR}/tests/ai_tests/${_src}")
        endforeach()
```

**선언만이 아니라 실제 빌드 그래프가 그 파일들을 참조한다** — 생성된 `build.ninja` 에서:
```
modules\ai\CMakeFiles\xpe_ai_tests.dir\__\__\tests\ai_tests\test_ai_abi.cpp
D:\workspace-github\xpe-post\tests\ai_tests\test_ai_abi.cpp
```
6파일 전부(`test_ai_abi` / `fallback` / `ipc_bridge` / `model_card` / `model_versioning` /
`worker_isolation`)가 오브젝트 규칙을 갖는다.

### 2.3 삭제의 실제 결과 (C3)

```
$ grep -c "^TEST" tests/ai_tests/*.cpp
test_ai_abi.cpp:25   fallback.cpp:47   ipc_bridge.cpp:10
model_card.cpp:20    model_versioning.cpp:11   worker_isolation.cpp:16     합계 129
```

이 파일들이 만드는 스위트는 내가 지난 카드들에서 계속 돌려온 바로 그것이다 —
`AiAbi.*`, `AiFallbackTest.*`, `AiIpcBridgeTest.*`, `AiModelCardTest.*`
(QA-B-32 `_verify.log`: `ci-ai 192/192`).

**`git rm -r tests/ai_tests` 를 실행했다면 `xpe_ai_tests` 타깃의 소스가 전부 사라지고,
CMake 의 존재 검사(`if(NOT EXISTS ...)`)가 타깃 자체를 만들지 않는다.**
ci-ai 는 192 → 63 근처로 떨어지고, 이번 세션에서 만든 것들(`AiEndurance`,
`DataSizeGuard_*` 10건 등)도 함께 사라진다. 되돌리려면 revert 가 필요하다.

### 2.4 카드 전제의 결함 (C4)

카드는 이렇게 적었다:

> 루트 CMake 는 `tests/e2e_post_pipeline` 만 추가한다(`CMakeLists.txt:160-162`).
> `tests/ai_tests/` … 는 어떤 프리셋에서도 빌드되지 않는 고아 트리다.

**첫 문장은 참이다.** 루트 `CMakeLists.txt:154-161` 이 그렇게 말하고 실제로 그렇다.
**두 번째 문장이 거짓이다.** 결론이 성립하려면 "테스트 타깃은 루트가 `add_subdirectory`
한 것만 만들어진다" 는 숨은 전제가 필요한데, ai 는 **모듈 CMakeLists 가 상위 경로의
소스를 직접 참조**하는 형태라 그 전제를 벗어난다.

이 형태는 #113(QA-B-07)의 결과물이다. 그때 `tests/ai_tests/CMakeLists.txt` 를 삭제해
중복 타깃을 없앴고(커밋 `4c32621`), **소스는 남기고 모듈이 가져다 쓰도록** 정리했다.
지금 보이는 구조가 정확히 그 상태다 — 정리가 안 된 잔재가 아니라 정리의 결과다.

`tests/common*` 에 A-13 이 한 것과 "같은 절차" 라는 카드의 유추도 여기서 깨진다.
`tests/common*` 은 대응하는 `modules/common/tests/` 가 실재했기 때문에 이식이 가능했다.
ai 에는 그 대응물이 없다.

### 2.5 대조표를 만들지 않은 이유

카드 1항은 6파일과 "모듈 스위트" 의 케이스 대조표를 요구한다.
**대조 대상이 같은 파일이므로 표가 성립하지 않는다.** 129 케이스 전부가 "이미 덮임"
이면서 동시에 "고유" 인 자기참조가 된다. 표를 만들어 제출하는 것은 형식만 채우고
사실을 가리는 일이라 하지 않았다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| `modules/ai/tests/` 부재 | 확인 | `ls -d` (§2.1) |
| 타깃 소스 = `tests/ai_tests/` 6파일 | 확인 | `modules/ai/CMakeLists.txt:214-226, 254-256` + 생성된 `build.ninja` (§2.2) |
| 해당 트리의 케이스 수 | 129 | `grep -c "^TEST"` (§2.3) |
| 현재 ci-ai | 192/192 통과 | `.moai/reports/lane-post/QA-B-32/_verify.log` (이번 세션 실측) |

**재실측은 하지 않았다** — 트리를 바꾸지 않았으므로 QA-B-32 의 수치가 그대로 유효하다.

---

## 4. 미검증 (Gaps)

- **다른 프리셋에서의 동작을 개별 확인하지 않았다.** 증거는 `build/ci-ai-b20` 의 빌드
  그래프 하나다. ci-post 에도 `xpe_ai_tests` 가 잡히는 것은 QA-B-26 에서 관측했지만
  (`AiEndurance` 가 ci-post 목록에 있었다), 이번에 프리셋별로 다시 확인하지는 않았다.
- **삭제 결과를 실험으로 확인하지 않았다.** §2.3 의 "192 → 63 근처" 는 CMake 의 존재 검사
  코드와 케이스 수에서 유도한 것이지, 실제로 지워 보고 잰 값이 **아니다.** 되돌리기 어려운
  작업을 확인용으로 수행하지 않았다.
- **`tests/` 아래 다른 트리는 조사하지 않았다.** 이 카드의 범위는 `tests/ai_tests` 다.
  같은 유형의 오판이 다른 디렉터리에도 있는지는 별건이다.
- **#109 가 원래 무엇을 지목했는지 이슈 본문을 읽지 않았다.** 카드의 서술만 검증했다.

---

## 5. 잔여 위험 (Residual-risk)

- **이 구조는 오해를 부르기 쉽다.** 모듈 CMakeLists 가 상위 디렉터리 소스를 참조하는
  형태는 흔치 않고, 루트만 보면 고아로 보인다. leader 가 그렇게 읽은 것이 자연스럽다.
  같은 오판이 반복되지 않게 하려면 (a) 소스를 `modules/ai/tests/` 로 옮기거나
  (b) `tests/ai_tests/README` 같은 표지를 두는 방법이 있다 — **둘 다 이 카드의 범위 밖이며
  leader 판단 사항이다.** 레인이 임의로 옮기지 않았다.
- **삭제는 되돌릴 수 있지만 비용이 든다.** git 이력이 있으므로 revert 는 가능하다.
  다만 CI 가 먼저 붉어지고, 원인이 "테스트가 사라졌다" 가 아니라 "타깃이 생성되지
  않는다" 로 나타나 진단이 한 단계 꼬인다.
- **이 보고서는 #109 의 종결을 주장하지 않는다.** ai 에 대해서는 "정리할 고아가 없다"
  까지가 결론이다.
