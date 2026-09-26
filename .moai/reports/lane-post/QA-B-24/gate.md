# QA-B-24 게이트 보고서 — `xpe_test_inject_alert` → `xpe_alert_push` 호출부 전환 (Lane B 경로)

**카드**: QA-B-24 (#111 3항, 개명 2/3)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-24/`
**선행 병합**: `git merge origin/main` → `065615d` (A-18 `xpe_alert_push` export + 별칭 포함)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 병합 후 `xpe_alert_push` 가 공개 헤더에 실제로 있다 | PASS |
| C2 | Lane B 소유 경로의 호출부를 `xpe_alert_push` 로 전환했다 | PASS |
| C3 | Lane B 소유 경로 `xpe_test_inject_alert` grep 잔재 0건 | PASS |
| C4 | 알림 큐 단언 테스트는 바꾸지 않았고 그대로 통과한다 | PASS |
| C5 | 재실측 ci-post 444 / ci-ai 129 / ci-dicom 48 전부 통과 | PASS (1회차 1건 실패 → §2.5) |
| C6 | `modules/common`·`tests/common*` 40곳은 손대지 않았다 (A-18/A-19 몫) | PASS |

---

## 2. 증거 (Evidence)

### 2.1 심볼 도착 확인 (C1)

착수 조건을 남의 말이 아니라 트리에서 확인했다.

```
$ grep -rn "xpe_alert_push" modules/common/include/ modules/common/src/
modules/common/include/xpe/common/xpe_error.h:150:XPE_API void xpe_alert_push(const char* msg, int32_t severity);
modules/common/include/xpe/common/xpe_error.h:153: * @brief Deprecated alias for xpe_alert_push().
modules/common/src/xpe_common.cpp:331:XPE_API void xpe_alert_push(const char* msg, int32_t severity)
modules/common/src/xpe_common.cpp:340:    xpe_alert_push(msg, severity);          ← 별칭이 새 이름으로 위임
```
`xpe_error.h` 는 `extern "C" {` (22-23행) 안에 선언한다 — C 링키지 유지.

### 2.2 변경 (C2) — diff 2파일

```diff
--- a/modules/enhance_basic/src/exposure_index.cpp
@@ -113,7 +113,7 @@
-        xpe_test_inject_alert(alertMsg, XPE_ALERT_WARNING);
+        xpe_alert_push(alertMsg, XPE_ALERT_WARNING);
```

```diff
--- a/modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h
@@ -51,12 +51,11 @@
 /*
- * Post an alert to the xpe_common alert queue.
- * xpe_test_inject_alert is exported from xpe_common.dll but not declared
- * in a public header. We declare it here for internal use by enhance_basic.
+ * Alerts are posted with xpe_alert_push(), declared in xpe/common/xpe_error.h
+ * (included above). The local extern declaration that used to sit here dated
+ * from when the symbol had no public declaration; #111 gave it one, so the
+ * duplicate is gone -- do not reintroduce it.
  */
-extern "C" XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity);
```

**로컬 extern 선언을 남기지 않고 지운 이유.** 이 헤더는 7행에서 이미
`#include "xpe/common/xpe_error.h"` 한다. A-18 이 그 헤더에 `xpe_alert_push` 를 선언했으므로
로컬 선언은 중복이 되고, 무엇보다 **주석의 전제("공개 헤더에 선언되지 않았다")가 거짓이 된다.**
이름만 바꿔 선언을 남겼다면 거짓 주석이 그대로 남았을 것이다 — B-21 에서 정정한 것과 같은 유형이라
선언째 제거하고 사유를 주석에 남겼다.

### 2.3 grep 잔재 (C3)

범위는 leader 판정 1안(Lane B 소유 경로 한정)이다.

```
$ grep -rn "xpe_test_inject_alert" modules/enhance_basic modules/enhance_advanced \
      modules/ai modules/display modules/dicom modules/gsvg \
      tests/ai_tests tests/e2e_post_pipeline | wc -l
0
```

중간 관측 1건 기록: 처음 쓴 주석이 이력 설명으로 옛 이름을 문자 그대로 담고 있어 grep 이 1건
잡혔다. 의미는 유지하고 토큰만 빼도록 다시 썼다(위 diff 가 최종본). **"주석이니까 잔재가 아니다"
로 넘기지 않았다** — 카드 기준은 문자열 grep 이다.

### 2.4 알림 큐 단언 (C4)

`modules/enhance_basic/tests/test_exposure_index.cpp` 가 `xpe_clear_alerts` /
`xpe_get_pending_alert_count` / `xpe_get_pending_alert` 로 알림을 단언한다(179~283행).
**이 파일은 한 글자도 바꾸지 않았다.** 호출 심볼만 바뀌었고 큐 동작은 같으므로 그대로 통과한다.

`_verify2.log` 에서 관련 케이스 전부 Passed (XpeCommonTest 알림군 12~17번 포함).

### 2.5 재실측 (C5)

**1회차 (`_verify.log`)** — 444 중 1건 실패:
```
110/444 Test #110: EdgeEnhance.Performance_3072x3072_Within20ms ...***Failed    0.04 sec
99% tests passed, 1 tests failed out of 444
===POST_EXIT=8===
```

실패한 것은 **시간 예산 케이스**다. 내 변경은 알림 호출 1줄과 헤더 주석뿐이고 `edge_enhance` 는
알림을 부르지 않는다. 다만 "무관해 보인다" 로 넘기지 않고 재현을 봤다 — 단독 5회 (`_flaky.log`):
```
1/1 Test #110: EdgeEnhance.Performance_3072x3072_Within20ms ...   Passed    0.04 sec   ===RUN1=0===
... Passed 0.04 / 0.04 / 0.03 / 0.04 sec                          ===RUN2..5=0===
```
5회 전부 통과. 전체 스위트 재실행 (`_verify2.log`):
```
===CI_POST===    100% tests passed, 0 tests failed out of 444   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 129   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 48    ===DICOM_EXIT=0===
```

**판정: 부하 의존 플레이키.** 임계값도 케이스도 건드리지 않았다(카드·B-19 원칙).
다만 §4 에 미검증으로 남긴다 — 실패 순간의 실측 ms 를 못 잡았다.

### 2.6 소유 경계 (C6)

```
$ grep -rn "xpe_test_inject_alert" modules/common tests/common tests/common_unit | wc -l
40
```
40곳 전부 그대로다. `git status` 에 `modules/common` 변경 없음 — A-18/A-19 몫으로 남긴다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | QA-B-23 실측 | QA-B-24 실측 | 차 |
|---|---|---|---|
| ci-post | 444 | 444 | 0 |
| ci-ai | 129 | 129 | 0 |
| ci-dicom | 48 | 48 | 0 |

B-23 수치는 `.moai/reports/lane-post/QA-B-23/_verify.log`, 이번 수치는 §2.5 의 `_verify2.log` 다.
테스트 수 변화 0 — 이 카드는 케이스를 더하지도 빼지도 않는 순수 전환이다.
병합 기준점: `origin/main` `065615d`.

---

## 4. 미검증 (Gaps)

- **1회차 실패의 실측 ms 를 잡지 못했다.** `_verify.bat` 이 `--output-on-failure` 없이 돌아
  `Within20ms` 가 실제로 몇 ms 였는지 로그에 없다. 재현 5회가 전부 통과했다는 것까지가 관측 범위이며,
  **"확실히 부하 때문"이라고 단정할 근거는 없다.**
- **알림이 실제로 큐에 들어가는 것을 이 카드에서 새로 관측하지는 않았다.** 기존 단언 케이스가
  통과한다는 사실에 의존한다. 전환 전후로 큐 내용을 비교하는 실험은 하지 않았다.
- **별칭 경로(`xpe_test_inject_alert` → `xpe_alert_push` 위임)는 시험하지 않았다.**
  Lane B 는 이제 새 이름만 부른다. 별칭이 살아 있는지는 common 테스트 40곳이 확인할 일이다.
- **`XPE_API` 매크로 가시성**: 로컬 extern 을 지운 뒤 `xpe_error.h` 선언으로만 링크되는데,
  DLL import/export 한정자가 두 경로에서 동일한지는 **빌드 성공으로 간접 확인**했을 뿐
  심볼 테이블을 직접 뜯어보지는 않았다.
- **Linux 빌드 미검증** — Windows/MSVC 에서만 확인했다.

---

## 5. 잔여 위험 (Residual-risk)

- **`EdgeEnhance.Performance_3072x3072_Within20ms` 는 다시 실패할 수 있다.** 20 ms 예산에
  실측이 0.03~0.04초(ctest 보고값, 케이스 내부 측정과는 다른 값)로 붙어 있어 머신 부하에
  민감하다. 이 카드에서 손대지 않았으므로 CI 에서 재발하면 별건이다.
- **개명은 2/3 단계다.** 지금 저장소에는 두 이름이 공존하고 `xpe_common.dll` 은 export 17개다
  (REQ-P0-008 의 16개가 아니다). A-19 가 옛 이름을 지워야 원복된다 — 그때까지 export 수를
  근거로 한 판정은 유보해야 한다.
- **로컬 extern 을 지웠으므로 enhance_basic 은 이제 `xpe_error.h` 에 의존한다.** 그 헤더에서
  선언이 사라지면 컴파일이 깨진다(조용한 실패가 아니라 빌드 오류라 안전한 방향이다).
- 커밋은 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_verify.bat` | 3개 config 재실측 |
| `_verify.log` | 1회차 — 444 중 1건 실패(시간 예산) |
| `_flaky.bat` / `_flaky.log` | 실패 케이스 단독 5회 — 전부 통과 |
| `_verify2.log` | 재실행 — 444 / 129 / 48 전부 통과 |
