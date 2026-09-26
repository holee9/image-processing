# QA-B-35 게이트 보고서 — 죽은 `responseToJson` 삭제 + `build/ci-post` 재생성

**카드**: QA-B-35 (#120, B-34 후속)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-35/`
**커밋**: `d8cd3a5` (선행 병합 `origin/main` 6d7063e)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | `responseToJson` 선언·정의를 삭제했다 (.cpp 26줄 / .h 3줄) | PASS |
| C2 | 삭제 근거는 **호출자 0** 이다 — 동작 대조는 하지 않았고, 할 필요도 없다 | PASS |
| C3 | `:236-238` 방어 코드는 **유지**했다 | PASS |
| C4 | `build/ci-post` 를 지우고 프리셋으로 재구성했다 — 빌드·테스트 통과 | PASS |
| C5 | **이전 445 는 stale 기준선이었다** | PASS |
| C6 | 재실측 ci-dicom **136** / ci-post **451** / ci-ai **198**, 경고 0 | PASS |
| C7 | **카드가 적은 140·455 와 다르다** — 차이는 전부 병합분이고 삭제와 무관하다 | **정정** (§4) |

---

## 2. 증거 (Evidence)

### 2.1 삭제 (C1, C2)

```
$ git diff --stat HEAD~1
 modules/dicom/src/DicomNetworkSCU.cpp | 26 --------------------------
 modules/dicom/src/DicomNetworkSCU.h   |  3 ---
 2 files changed, 29 deletions(-)
```

지운 것은 `DicomNetworkSCU::responseToJson(void*)` 전체(`.cpp:344-368`)와 그 선언
(`.h:68-69` + 주석). 계측 기준으로는 **19줄이 미커버**였다(B-34 `_uncovered7.txt`).

**대조표를 만들지 않았다.** 카드도 요구하지 않았고, 요구할 근거가 없다 —
호출자가 0이면 비교 대상이 **한 번도 실행된 적이 없다.** 인라인 구현
(`cfindMwl:254-287`)과 "같은 결과를 내는지" 를 확인하는 것은 실행된 적 없는 코드에
동등성을 부여하는 일이라, 삭제 근거를 강화하지 못한다. 근거는 참조 전수 하나다:

```
$ grep -rn "responseToJson" modules/ tests/     # 삭제 전
modules/dicom/src/DicomNetworkSCU.cpp:344:std::string DicomNetworkSCU::responseToJson(...)
modules/dicom/src/DicomNetworkSCU.h:69:    static std::string responseToJson(void* responseList);
```

### 2.2 유지한 것 (C3)

`:236-238`(`findPresID == 0`)은 B-34 §6.3 에서 **이 API 형태로 도달 불가**라고 논증한
줄이지만 지우지 않았다. 도달 불가와 죽은 코드는 다르다 — 전자는 호출 경로 위에 있는
방어 분기이고, 후자는 호출 경로 자체가 없다. leader 도 같은 판정이다.

### 2.3 `build/ci-post` 재생성 (C4, C5) — `_post.log`

```
$ rm -rf build/ci-post build/ci-post-b34
===CFG=0===
===BUILD=0===
100% tests passed, 0 tests failed out of 451
===CTEST=0===
```

B-34 §6.7 에서 `0xc0000139`(entry point not found)로 재빌드가 깨졌던 디렉터리다.
프리셋으로 새로 구성하니 구성·빌드·테스트가 모두 통과한다. B-34 가 우회용으로 만든
`build/ci-post-b34` 도 함께 지웠다 — 정본은 `build/ci-post` 하나다.

### 2.4 재실측 (C6)

| 프리셋 | 결과 | 로그 |
|---|---|---|
| ci-dicom | **136 / 136** | `_dicom.log` |
| ci-post | **451 / 451** | `_post.log` |
| ci-ai | **198 / 198** | `_ai.log` |

세 로그 모두 `warning C` 0건 (`XPE_WARNINGS_AS_ERRORS=ON`).

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 임계 0.80 | SPEC-XPE-P0 `spec.md:48` | 저장소에서 직접 확인 (2026-09-11 결정, #120) |
| 삭제 전 ci-dicom | 140 | B-34 `_scu_run1.log` (병합 **전**) |
| 삭제 후 ci-dicom | 136 | `_dicom.log` (병합 **후**) |
| ci-post / ci-ai | 451 / 198 | `_post.log` / `_ai.log` (이번 실행) |

---

## 4. 정정 — 카드의 140 / 455 는 병합 전 값이다 (C7)

카드는 "ci-dicom 140 불변", "ci-post 455 기준" 이라 적었다. 실측은 **136 / 451** 이다.
숫자가 다르므로 그대로 통과를 주장하지 않고 차이를 특정했다.

**원인은 `origin/main`(6d7063e) 병합이며, 이 카드의 삭제와는 무관하다.**
테스트 이름 목록을 병합 전후로 비교했다:

```
$ comm -23 <B-34 목록> <B-35 목록>      # 사라진 것
DegradedMode.BP06_GsvgMissingReportsR0
DegradedMode.BP07_CollimationMissingEnhanceAdvancedReportsR0
DegradedMode.BP08_EiMissingEnhanceBasicReportsR0
DegradedMode.BP09_DicomMissingReportsR0
DegradedMode.BP10_DisplayMissingReportsR0

$ comm -13 <B-34 목록> <B-35 목록>      # 새로 생긴 것
CommonErrorPrecedenceTest.NullRequiredPointerWinsOverInitializationState
```

`−5 +1 = −4` 가 세 프리셋에 **동일하게** 적용된다:

| 프리셋 | 병합 전 | 예상(−5+1) | 실측 |
|---|---:|---:|---:|
| ci-dicom | 140 | 136 | **136** |
| ci-post | 455 | 451 | **451** |
| ci-ai | 202 | 198 | **198** |

세 프리셋이 모두 `modules/common` 테스트를 포함하므로 같은 폭으로 움직인다.
**삭제한 함수에는 테스트가 없었으므로 이 커밋의 케이스 수 기여는 0이다.**

사라진 5건은 병합 전에도 **전부 skip 상태**였다(B-34 `_scu_run1.log` "did not run").
따라서 **실제 실행 건수는 134 → 135 로 오히려 늘었다** — 새 Lane A 테스트 1건 때문이다.
숫자가 줄었다는 사실만 보면 후퇴로 읽히지만, 실행된 것은 줄지 않았다.

---

## 5. 미검증 (Gaps)

- **커버리지를 다시 재지 않았다.** 로컬 OpenCppCoverage 부재는 B-27 이후 그대로이고,
  0.80 달성 여부는 다음 dispatch 로만 확인된다. 다만 이번 삭제는 **분자가 아니라
  분모를 줄인다** — 미커버 19줄이 통째로 사라지므로 계측 총줄이 1167 → 약 1148 이 되고
  커버율은 938/1148 ≈ **0.817** 로 소폭 오른다(계산이지 관측이 아니다).
- **SPEC-XPE-P0 `spec.md:48` 의 문구가 이 커밋으로 부분적으로 낡았다.**
  "40 in `DicomNetworkSCU.cpp` are dead code, cancel-race or negotiation-impossible"
  이라 적혀 있는데, dead code 19줄은 이제 존재하지 않는다. 남는 것은 21줄(취소 경쟁 6 +
  도달 불가 15)이다. **`.moai/` 는 main 소유라 레인이 고치지 않는다** — 관측만 보고한다.
- **취소 경쟁 6줄·협상 불가 15줄은 시험하지 않았다.** SPEC-XPE-P0 `spec.md:48` 이
  예외로 기록한 항목이고, B-34 §6.3 의 논증을 그대로 승계한다. 이번 카드에서 새로
  반증하거나 재확인하지 않았다.
- **`build/ci-post` 가 왜 깨졌는지는 규명하지 않았다.** 재생성으로 해소했을 뿐,
  `0xc0000139` 의 근인(디버그/릴리스 임포트 라이브러리 혼선으로 보이는 정황)은
  추적하지 않았다. 같은 증상이 다른 빌드 디렉터리에서 재발할 수 있다.

---

## 6. 잔여 위험 (Residual-risk)

- **삭제는 되돌리기 쉽지만, 되돌릴 이유가 생기는 경로가 하나 있다.** 외부(GUI·클라이언트)가
  이 심볼을 링크한다면 삭제가 깨뜨린다. `modules/` 와 `tests/` 만 전수했고 `clients/`·`gui/` 는
  보지 않았다 — 다만 이 함수는 `private` 정적 멤버라 헤더 밖에서 호출할 수 없다.
- **분모가 줄면 커버율이 오른다.** 죽은 코드 제거로 얻은 0.817 은 테스트가 늘어서 오른 것이
  아니다. 임계를 이 방식으로 통과시키는 것은 반복하면 계측을 무의미하게 만든다 —
  이번은 leader·사용자 결정에 따른 1회성 정리로 본다.
- **`build/` 를 지운 것은 산출물 처분이다.** 증거(`.moai/reports/`)와 소스는 손대지 않았다.
- 커밋은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` / `_dicom.log` | ci-dicom 빌드 + 전체 ctest 136/136 |
| `_post.bat` / `_post.log` | `build/ci-post` 신규 구성·빌드·ctest 451/451 |
| `_ai.bat` / `_ai.log` | ci-ai 빌드 + ctest 198/198 |
