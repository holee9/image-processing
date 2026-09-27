# QA-A-31 검증 보고서 — 캐시 `put_and_get` 으로 put/get 창 닫기 (#127 후속)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-31 (`.moai/lanes/pre/inbox/QA-A-31.md`)
- 커밋: `2fd15e2`

## 1. 주장 (Claim)

1. `CalibrationLRUCache` 에 `put_and_get` 을 추가해 삽입과 조회를 **한 잠금 구간** 안에서 수행한다. 기존 `put()` 은 새 `put_locked()` 를 감싼 형태로 남는다. **공개 시그니처 변경 없음.**
2. **경합을 실제로 재현했다.** 구현을 일시 원복해 측정한 결과 `put+get` 은 50000 회 중 **734건**의 spurious `XPE_ERR_PROCESSING_FAILED` 를 냈고, `put_and_get` 은 **0건**이다.
3. **테스트 감도를 실측으로 교정했다** — 첫 설계(클리어 스레드 1개 + 2000회)는 **버그가 있는 코드에서도 0/2000** 이었다. 즉 그대로 뒀다면 아무것도 못 잡는 가드를 커밋할 뻔했다(§3).
4. ci-preprocess **554 → 556/556 PASS**.

## 2. 구현

```cpp
bool put_and_get(const std::string& path, XpeImageBuffer* buffer,
                 XpeImageBuffer* out) {
    if (!buffer || !buffer->data) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    put_locked(path, buffer);
    auto it = index_.find(path);
    if (it == index_.end()) return false;
    if (out) std::memcpy(out, &it->second->buffer, sizeof(XpeImageBuffer));
    return true;
}
```

`publish_and_view()` 는 이제 이것만 호출한다. 잠금이 삽입과 조회를 함께 덮으므로, 그 사이에 다른 스레드가 끼어들 자리가 없다.

## 3. 실측 — RED / GREEN, 그리고 감도 교정

카드는 "seam 이 없으면 스레드 2개 + 반복으로 관측하되 flaky 하면 그 사실을 보고" 라고 했다. 관측을 세 단계로 했다.

| 단계 | 구성 | 구현 | 결과 | 로그 |
|---|---|---|---|---|
| 1 | 클리어 스레드 **1개**, 2000회 | `put_and_get` (수정본) | 0 / 2000 | `a31-race-after.log` |
| 2 | 같은 구성 | **`put`+`get` 로 일시 원복** | **0 / 2000** ← 버그를 못 잡는다 | `a31-race-before.log` |
| 3 | 클리어 스레드 **4개**, 50000회 | `put`+`get` (원복 상태) | **734 / 50000 — RED** | `a31-race-probe.log` |
| 4 | 같은 구성 | `put_and_get` (복구) | **0 / 50000 — GREEN** | `a31-race-after2.log` |

**2단계가 이 카드에서 가장 중요한 관측이다.** 원래 설계한 파라미터로는 결함이 있는 코드도 통과한다. 그대로 커밋했다면 "RED→GREEN 확인" 이라고 보고할 수 있었지만, 실제로는 **아무것도 검증하지 않는 테스트**였을 것이다. 창이 너무 좁아(put 반환 → 잠금 해제 → get 잠금 획득) 클리어 스레드 하나로는 거의 끼어들지 못한다.

4개 스레드 + 50000회로 올리자 **734건**이 관측됐다. 그 수치와 이유를 테스트 파일 주석에 남겨, 나중에 "느리니 줄이자" 는 판단이 감도를 죽이지 않게 했다.

RED 로그 (`a31-race-probe.log`):

```
[QA-A-31] iterations=50000 ok=49266 spurious_PROCESSING_FAILED=734 other_failures=0
[  FAILED  ] CalibCacheConcurrencyTest.LoadUnderConcurrentClearNeverReportsSpuriousFailure (187 ms)
```

GREEN 로그 (`a31-race-after2.log`):

```
[QA-A-31] iterations=50000 ok=50000 spurious_PROCESSING_FAILED=0 other_failures=0
[       OK ] CalibCacheConcurrencyTest.LoadUnderConcurrentClearNeverReportsSpuriousFailure (183 ms)
```

원복은 같은 턴에 파일 백업(`a31-cache-fixed.bak`)에서 복구했고, 복구 후 `put_and_get` 참조 3건을 확인한 뒤 재실행했다.

### 부하에 관한 판단

스레드 4개가 짧게 도는 구성이다. 케이스 전체가 **184ms** 안에 끝나고 모든 스레드를 `join` 한 뒤 단언한다 — 케이스 밖으로 나가는 부하는 없다. 경합이 필요한 검증이라 접촉을 만들 수밖에 없고, `join` 으로 수명을 보장했다.

### 축출 경합

같은 형태로 용량 1에서 축출 경합도 관측했다(25000회, 0건). 이쪽은 원복 상태에서도 0건이었다 — 축출은 `put_locked` **안에서** 일어나므로 put/get 창과 무관하고, 애초에 이 결함의 경로가 아니다. 케이스는 회귀 방지로 남긴다.

## 4. 재실측

```
100% tests passed, 0 tests failed out of 556     (a31-full.log, exit=0)
```

변경 규모 (`a31-diffstat.txt`): `calibration_cache.cpp` +43/−6, `CMakeLists.txt` +2, 신규 테스트 1파일.

## 5. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 554/554 PASS (QA-A-30, `../QA-A-30/a30-pre.log`) | **556/556 PASS** (`a31-full.log`) | +2 = 신규 경합 케이스 2건 |

## 6. 미검증 (Gaps)

- **`put_and_get` 이 잠금을 더 오래 잡는다.** 삽입과 조회를 함께 덮으므로 임계 구역이 길어진다. 그 영향을 측정하지 않았다 — 캐시 경합이 심한 상황에서 처리량이 얼마나 떨어지는지 모른다.
- **734/50000 이라는 수치는 이 기계의 것이다.** 코어 수·스케줄러가 다르면 재현율이 달라진다. 다른 기계에서 이 테스트가 결함 있는 코드를 놓칠 가능성은 배제하지 못한다.
- **`get()` 은 그대로 남아 있다.** 히트 경로가 계속 쓰므로 삭제하지 않았지만, `publish_and_view` 외에 `put()` 을 직접 쓰는 곳이 생기면 같은 창이 다시 열릴 수 있다. 현재 `put()` 의 유일한 호출자는 없다(모두 `put_and_get` 경유) — 다만 이를 강제하는 장치는 없다.
- **ASan / TSan 없음.** 특히 TSan(데이터 경합 검출)은 이 변경에 가장 어울리는 도구인데 Windows MSVC 에서 쓸 수 없었다. 잠금 정확성은 코드 검토와 위 관측에 의존한다.
- **다른 세 로더(gain/defect)의 경합은 측정하지 않았다.** 같은 `publish_and_view` 를 쓰므로 동작은 같지만, 실행으로 확인한 것은 offset 경로뿐이다.

## 7. 잔여 위험 (Residual risk)

- **감도가 파라미터에 묶여 있다.** 반복 횟수나 스레드 수를 줄이면 이 테스트는 조용히 무력해진다(2단계가 그 증거다). 주석으로 방어했지만 기계적 강제는 없다.
- **경합 테스트는 본질적으로 확률적이다.** 0건이 "창이 닫혔다" 를 증명하지는 않는다 — 구조적 근거(한 잠금 구간)가 주장이고, 관측은 그 주장을 뒷받침하는 증거다. 반대로 실패가 나오면 그것은 확실한 반증이다.
- **부하가 있는 CI 에서 184ms 가 더 길어질 수 있다.** 스핀하는 스레드 4개가 다른 테스트와 코어를 다툰다. 현재 전체 스위트에서 문제되지 않았으나, 병렬 실행 설정이 바뀌면 재평가가 필요하다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| `put_and_get` 추가 + `publish_and_view` 전환 | QA-A-31 |
| 경합 재현 (RED 734/50000 → GREEN 0/50000) | QA-A-31 |
| 감도 교정 관측 (1스레드/2000회는 결함을 못 잡음) | QA-A-31 |
| ci-preprocess 재실측 | QA-A-31 |
| gain / defect 로더 경합 확인 | 신규 카드 필요 |
| 잠금 구간 확대의 처리량 영향 측정 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a31-race-after.log` | 1스레드/2000회, 수정본 — 0건 |
| `a31-race-before.log` | 1스레드/2000회, **원복본 — 0건**(감도 부족의 증거) |
| `a31-race-probe.log` | 4스레드/50000회, 원복본 — **734건, RED** |
| `a31-race-after2.log` | 4스레드/50000회, 수정본 — **0건, GREEN** |
| `a31-full.log` | ci-preprocess 556/556 PASS (exit=0) |
| `a31-diffstat.txt` | `git diff --stat` |
