# QA-B-07 — `xpe_ai_tests` 정본 판정 (#113 선행 1단계)

**레인**: Lane B (`dev/postprocess`)
**대상**: `modules/ai/CMakeLists.txt:217-289` (라이브) vs `tests/ai_tests/CMakeLists.txt` (죽은 등록)
**결론**: `modules/ai` 등록이 정본. `tests/ai_tests/CMakeLists.txt` 삭제. 결손 이식 **불필요**.

---

## 1. 주장 (Claim)

1. 타깃 중복 충돌은 **실재하며 재현된다** — 단, 재현에는 전제가 하나 더 필요했다(§2).
2. 죽은 등록이 라이브 등록에 없는 것 3건은 **전부 결손이 아니다** — 이식할 것이 없다(§3).
3. 죽은 등록은 중복일 뿐 아니라 **이미 깨져 있다** — 지금 살려도 링크가 실패한다(§3.4).
4. 삭제 후 `ci-post` generate 통과, `xpe_ai_tests` **108/108 재실측**.
5. **삭제만으로 #113 이 풀리지는 않는다** — 남은 차단 요인이 main 소유다(§5). 중요.

---

## 2. 1단계 — 충돌 재현

### 2.1 1차 시도: 오류가 나지 않았다

루트 `CMakeLists.txt:164-166` 의 `xpe_add_optional_subdirectory(tests)` 주석을 임시로 풀고
스크래치 디렉터리 `build/scratch-b07` 을 generate 했다. 결과는 **`CONFIGURE_EXIT=0`** —
카드가 예고한 충돌 오류가 나오지 않았다.

카드 지시대로 2단계로 가지 않고 원인을 규명했다. 로그에 답이 있었다
(`_probe_conflict.log`):

```
-- Could NOT find GTest (missing: GTest_DIR)
-- GTest not found — skipping test subdirectories (ci-common preset)
```

`tests/CMakeLists.txt:9` 는 `find_package(GTest CONFIG)` 로만 GTest 를 찾고,
테스트 서브디렉터리 전체를 `if(GTest_FOUND)` 로 감싼다(15행). 이 환경에는 vcpkg GTest 가
기본 검색 경로에 없어 **`add_subdirectory(ai_tests)` 자체가 실행되지 않았다.**
한편 `modules/ai` 는 GTest 를 못 찾으면 FetchContent 로 내려받으므로 정상 구성된다
(같은 로그 `-- xpe_ai tests configured`).

**즉 "오류 없음" 은 충돌 부재가 아니라 도구 부재였다.** 죽은 등록이 읽히지도 않은 상태를
"충돌이 없다" 로 읽었다면 판정 전제를 잘못 기각할 뻔했다.

### 2.2 2차 시도: 전제를 갖추자 즉시 재현됐다

QA-B-02 가 dicom 에 쓴 것과 같은 vcpkg 트리를 `CMAKE_PREFIX_PATH` 로 주어
GTest 를 CONFIG 로 찾게 하고 `build/scratch-b07b` 를 generate 했다
(`share/gtest/GTestConfig.cmake` 존재 확인).

```
CMake Error at tests/ai_tests/CMakeLists.txt:28 (add_executable):
  add_executable cannot create target "xpe_ai_tests" because another target
  with the same name already exists.  The existing target is an executable
  created in source directory "D:/workspace-github/xpe-post/modules/ai".  See
  documentation for policy CMP0002 for more details.

CMake Error at .../GoogleTest.cmake:662 (add_custom_command):
  TARGET 'xpe_ai_tests' was not created in this directory.
Call Stack (most recent call first):
  tests/ai_tests/CMakeLists.txt:59 (gtest_discover_tests)

===CONFIGURE_EXIT=1===
```
로그: `_probe_conflict2.log`. 두 정의 위치가 오류 문구 안에 그대로 찍혔다.

### 2.3 재현 조건 (카드에 추가할 사실)

충돌은 **`tests/` 집계 활성화 + GTest 가 CONFIG 로 발견됨** 두 조건이 동시에 성립할 때만
드러난다. 후자가 빠지면 조용히 통과한다. `ci-post` preset 은 `VCPKG_MANIFEST_DIR` 을
지정하므로 vcpkg GTest 가 있는 CI 환경에서는 두 번째 조건이 충족된다 —
"잠재" 충돌이 아니라 **환경 의존적으로 이미 실재**하는 충돌이다.

### 2.4 저장소 원복

두 프로브 모두 루트 `CMakeLists.txt` 임시 수정 후 원본으로 복구했다.
`git diff -- CMakeLists.txt` 출력 없음(§7 참조). 빌드 디렉터리는 새 이름
(`scratch-b07`, `scratch-b07b`, `scratch-b07c`)만 썼고 기존 것은 건드리지 않았다.

---

## 3. 2단계 — 차이 3건 실측

### 3.1 `xpe_common` 링크 → 결손 아님

죽은 등록은 `xpe_common` 을 명시적으로 링크한다. 라이브는 하지 않는다.

- **직접 호출 심볼 0건.** `xpe_common` 이 헤더에 선언하는 함수는
  `xpe_error_string` / `xpe_get_pending_alert_count` / `xpe_get_pending_alert` /
  `xpe_clear_alerts` 4개인데, 6개 테스트 소스에서 호출이 **한 건도 없다**
  (`grep` 결과 무출력). `xpe_common` 문자열이 잡힌 유일한 곳은
  `test_ai_abi.cpp:8` 의 주석이다.
- **전이 링크로 이미 해결된다.** `modules/ai/CMakeLists.txt:128-130` 이
  `target_link_libraries(${MODULE_NAME} PUBLIC xpe_common)` 이므로,
  `xpe_ai` 를 링크하는 `xpe_ai_tests` 에 `xpe_common` 이 그대로 전파된다.

명시 링크는 중복이지 결손이 아니다.

### 3.2 `modules/common/include` → 결손 아님

죽은 등록만 이 include 경로를 준다. 그런데 소스는 공통 헤더를 **직접 include 한다**:

```
tests/ai_tests/*.cpp:  #include "xpe/common/xpe_types.h"
                       #include "xpe/common/xpe_error.h"
```

라이브 등록이 이 경로를 주지 않는데도 컴파일되는 이유는 §3.1 과 같다 —
`modules/common/CMakeLists.txt:54-58` 이 include 디렉터리를 **PUBLIC** 으로 내보내고,
`xpe_ai` 가 `xpe_common` 을 PUBLIC 으로 링크하므로 두 단계를 타고 전파된다.

**실측 근거**: 라이브 경로로 빌드한 결과 컴파일 경고·오류 0, 테스트 108/108 통과(§4).
헤더를 못 찾았다면 컴파일 자체가 불가능하다.

### 3.3 coverage 구성 → 존재함. 이식하지 않음 (leader 판정 대기)

`tests/ai_tests/CMakeLists.txt:66-82` 에 `coverage_ai` 커스텀 타깃이 있다
(OpenCPPCoverage, `--sources modules/ai/src`, cobertura 출력). `if(WIN32 AND ENABLE_COVERAGE)` 가드.

- 리포지토리 전체에서 `coverage_ai` 를 정의·참조하는 곳은 **이 파일뿐**
  (`grep -rn coverage_ai` → 이 파일 2행만).
- `.github/` 워크플로와 `docs/` 어디에서도 `coverage_ai` / `OpenCPPCoverage` 를
  호출하지 않는다(무출력). 즉 현재 **아무도 실행하지 않는 타깃**이다.
- 다만 `REQ-P0-006`(85% 커버리지) 추적성 관점에서 "AI 모듈 커버리지 수단이
  파일과 함께 사라진다" 는 사실은 남는다. 카드 지시대로 **보고만 하고 옮기지 않았다.**

참고로 `tests/CMakeLists.txt:38-73` 에 별도의 `coverage` 타깃(대상: `xpe_common`)이 있고,
이 파일은 삭제 대상이 아니므로 그쪽은 영향 없다.

### 3.4 카드 표에 없던 차이 2건 (중요)

대조 중 표에 없는 차이가 두 개 더 나왔고, 하나는 판정을 강화한다.

**(a) 라이브만 `src/ai_ipc_bridge.cpp` 를 직접 컴파일한다** (`modules/ai:262-265`).
사유가 주석에 있다 — IPC 프로토콜은 `REQ-AI-003` 상 모듈 내부이며 DLL 에서 export 되지
않는다(QA-B-03 에서 심볼 5건을 내부화한 그 건이다). 죽은 등록에는 이 소스가 없다.

`test_ai_ipc_bridge.cpp` 가 호출하는 심볼과 그 정의처를 대조했다:

| 테스트가 호출 | 정의 위치 |
|---|---|
| `xpe_ai_ipc_bridge_create` / `_connect` / `_send` / `_receive` / `_destroy` | `modules/ai/src/ai_ipc_bridge.cpp` (export 안 됨) |

즉 **죽은 등록은 지금 되살려도 미해결 외부 심볼 5건으로 링크가 실패한다.**
중복 등록일 뿐 아니라 QA-B-03 이후 상태와 어긋난 **낡은 등록**이다.
이것이 "라이브가 정본" 판정의 가장 강한 근거다.

**(b) 죽은 등록만 CTest 속성을 준다** — `LABELS "unit;ai;SPEC-XPE-P3-AI"`, `TIMEOUT 30`
(`tests/ai_tests/CMakeLists.txt:59-63`). 라이브에는 없다.
`tests/enhance_advanced_tests/CMakeLists.txt:59` 도 같은 스타일의 LABELS 를 쓰므로,
`tests/` 트리의 관례였던 것으로 보인다. **기능 결손은 아니지만**(테스트는 라벨 없이도 돌고
108/108 통과) SPEC 추적 라벨이 사라지는 것은 사실이다. §3.3 과 같은 성격의
추적성 항목이라 함께 보고하고 이식하지 않았다 — 이식하면 카드가 금지한
"leader 판정 사안 선반영" 이 된다.

---

## 4. 3단계 — 삭제와 검증

```
git rm tests/ai_tests/CMakeLists.txt      # 1파일. 테스트 소스 6건은 그대로
```

`ls tests/ai_tests/` → `test_ai_abi.cpp`, `test_ai_fallback.cpp`, `test_ai_ipc_bridge.cpp`,
`test_ai_model_card.cpp`, `test_ai_model_versioning.cpp`, `test_ai_worker_isolation.cpp` (6건 유지).
라이브 등록이 이 파일들을 절대경로로 참조하므로 디렉터리는 유지해야 한다.

검증(`_verify.log`, 삭제 후 실행):

| 항목 | 결과 |
|---|---|
| `cmake --preset ci-post` | `Configuring done` / `Generating done`, `POST_CFG_EXIT=0` |
| `cmake --build build\ci-ai` | `AI_BUILD_EXIT=0` |
| `ctest -R "Ai\|AI"` | **100% tests passed, 0 failed out of 108** (`AI_TEST_EXIT=0`) |
| `ctest` (전체) | 100% passed, 0 failed out of 166 (`AI_ALL_EXIT=0`) |

108 은 카드 지시대로 **재실측**이며 이전 값을 인용한 것이 아니다.

---

## 5. 삭제만으로 #113 이 풀리지 않는다 — 반드시 읽을 것

삭제가 실제로 차단을 푸는지 확인하려고, §2.2 와 동일한 조건(집계 활성화 + GTest 발견)으로
`build/scratch-b07c` 를 generate 했다. 결과(`_probe_after_delete.log`):

```
CMake Error at tests/CMakeLists.txt:25 (add_subdirectory):
  The source directory
    D:/workspace-github/xpe-post/tests/ai_tests
  does not contain a CMakeLists.txt file.
===CONFIGURE_EXIT=1===
```

- **중복 타깃 오류는 사라졌다** — 이 카드의 목적은 달성됐다.
- 그러나 `tests/CMakeLists.txt:25` 의 `add_subdirectory(ai_tests)` 가 이제 빈 디렉터리를
  가리켜 **다른 오류로 여전히 막힌다.**
- 이 줄은 카드가 명시적으로 손대지 말라고 한 곳이다("main 이 #113 후속에서 집계 파일
  전체를 손본다"). 그래서 손대지 않았다.

**따라서 "QA-B-07 완료" 를 "#113 해소" 로 읽으면 안 된다.** 이 카드는 선행 1단계이고,
집계 재활성화에는 main 쪽 후속 작업(최소한 `tests/CMakeLists.txt:25` 처리)이 남아 있다.
이 사실을 관측으로 확정해 둔 것이 이 절의 목적이다.

---

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-ai -R "Ai\|AI"` | 108 (`QA-B-06/gate.md` §9, `QA-B-01/_gate_ai.log`) | **108 passed / 0 failed** | 동일 |
| `ci-ai` 전체 | 166 (`QA-B-06/gate.md` §9) | **166 passed / 0 failed** | 동일 |
| `ci-post` generate | 통과 (QA-B-06) | 통과 (`POST_CFG_EXIT=0`) | 동일 |
| 저장소 상태 | — | `git status`: `D tests/ai_tests/CMakeLists.txt` 1건 + main 소유 미추적 2건 | 프로브 흔적 0 |

`ci-post` 는 `BUILD_AI=OFF` 라 ai 를 빌드하지 않는다. 그래서 generate 통과만 확인하고
테스트 수치는 `ci-ai` 에서 측정했다.

---

## 7. 미검증 (Gaps)

- **`ci-post` 빌드·ctest 미실행.** 카드가 요구한 것은 generate 통과였고 그것만 했다.
  삭제 파일이 `ci-post` 구성에 들어오지 않으므로 영향이 없다고 판단했으나, 빌드로
  관측하지는 않았다.
- **`ENABLE_COVERAGE=ON` 빌드 미실행.** `coverage_ai` 가 실제로 동작하던 타깃이었는지
  (OpenCPPCoverage 설치 여부 포함) 실행으로 확인하지 않았다. 정적 참조 조사만 했다.
- **죽은 등록이 링크 실패한다는 것(§3.4)은 정적 대조로 확정했고 빌드로 관측하지 않았다.**
  타깃 중복 때문에 generate 단계에서 먼저 막혀 링크까지 도달할 수 없다 — 관측하려면
  라이브 등록을 임시로 끄는 별도 조작이 필요해 하지 않았다.
- **`tests/CMakeLists.txt` 의 나머지 서브디렉터리 미검증.** `common_smoke`,
  `enhance_advanced_tests`, `e2e_post_pipeline` 이 집계 활성화 시 또 다른 중복을
  일으키는지는 §5 오류가 먼저 나서 확인하지 못했다. #113 후속에서 드러날 수 있다.
- **Linux/비-MSVC 미검증.**
- **`git merge main` 미수행** (leader 가 통합을 main 쪽에서 하기로 확인).

## 8. 잔여 위험 (Residual risk)

- `coverage_ai`(§3.3)와 CTest LABELS/TIMEOUT(§3.4b)은 삭제와 함께 사라진다. 둘 다
  현재 아무도 호출하지 않지만, `REQ-P0-006` 추적성 문서가 이 타깃을 근거로 삼고 있다면
  공백이 생긴다. **leader 판정 대상으로 남긴다.** 복구가 필요하면 이 커밋을 되돌리는 대신
  라이브 등록에 이식하는 편이 맞다(중복이 되살아나지 않는다).
- `TIMEOUT 30` 상실은 무한 대기 테스트가 CI 를 붙잡을 여지를 남긴다. 현재 108건이
  모두 빠르게 끝나므로 실증된 위험은 아니다.
- §2.3 대로 충돌 재현이 환경 의존적이므로, GTest 가 없는 환경에서 회귀 테스트를 돌리면
  이 카드의 검증이 조용히 무의미해진다. 재검증 시 `CMAKE_PREFIX_PATH` 를 반드시 줄 것.

---

## 9. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 1단계 충돌 오류 로그 (또는 오류 없음의 원인 규명) | PASS | §2.1 원인 규명 + §2.2 재현 로그 |
| 2단계 차이 3건 각각 실측 + 결손 여부 | PASS | §3.1 결손 아님 / §3.2 결손 아님 / §3.3 존재·미이식 (+ §3.4 추가 2건) |
| 삭제 후 `ci-post` generate 통과, 108/108 재실측 | PASS | §4 |
| 5절 구조 (Claim/Evidence/Baseline/Gaps/Residual-risk) | PASS | §1 / §2-5 / §6 / §7 / §8 |
