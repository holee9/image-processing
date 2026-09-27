# QA-A-28 검증 보고서 — 알림 큐 오버플로 정책 (#110, SRS-ALERT-007)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-28 (`.moai/lanes/pre/inbox/QA-A-28.md`)
- 정본: `docs/project/api-spec.md` §5.17 (normative, HAZ-006)
- 선행 병합: `git merge origin/main` → `e099c8b` 포함, §5.17 이 트리에 존재함을 확인

## 1. 주장 (Claim)

1. `enqueue_alert` 가 심각도를 무시하고 가장 오래된 항목을 조용히 버리던 동작을 §5.17 의 **우선도 보호 FIFO 축출**로 교체했다. 축출 순서는 가장 오래된 Info → Warning → Error, 같은 심각도 안에서는 FIFO.
2. **유실 알림**을 도입했다. 마지막 `xpe_clear_alerts` 이후 누적 축출 수가 0보다 큰 동안, severity `XPE_ALERT_ERROR` 이고 메시지가 `alert queue overflow: <N> alert(s) dropped` 인 항목이 **정확히 1개** 존재한다. 제자리 갱신하며 중복되지 않고, 축출 대상에서 제외되며, 64 슬롯 중 하나를 차지한다.
3. `xpe_init` / `xpe_shutdown` / `xpe_clear_alerts` 세 곳 모두에서 누적 카운터를 0으로 되돌린다.
4. **새 export 없음.** `xpe_common.dll` 은 그대로 16개 (REQ-P0-008).
5. RED → GREEN 실측 확보. ci-common 69/69, ci-preprocess 482/482 PASS.

구현 판단 1건 — 유실 알림은 메시지 문자열이 아니라 `AlertEntry::isLossAlert` 플래그로 식별한다. 모듈이 우연히 같은 접두사로 시작하는 알림을 push 할 수 있고, 그 항목은 정상적으로 축출 대상이어야 하기 때문이다.

## 2. 증거 (Evidence)

### RED — 구현 전 (`a28-red.log`)

```
64/69 Test #64: AlertQueueOverflowTest.WarningEvictsOldestInfoNotItself ..............***Failed    0.01 sec
65/69 Test #65: AlertQueueOverflowTest.WithoutInfoEvictsOldestWarningAndKeepsError ...***Failed    0.01 sec
66/69 Test #66: AlertQueueOverflowTest.LossAlertIsSingleAndUpdatedInPlace ............***Failed    0.01 sec
67/69 Test #67: AlertQueueOverflowTest.LossAlertIsNeverEvicted .......................***Failed    0.01 sec
68/69 Test #68: AlertQueueOverflowTest.ClearAlertsResetsTheDropCounter ...............***Failed    0.01 sec

93% tests passed, 5 tests failed out of 69
```

6번째 케이스 `NoLossAlertWithoutOverflow` 는 RED 단계에서도 통과했다 — 오버플로가 없으면 정책이 보이지 않아야 한다는 요구라서, 옛 구현에서도 참이다. 실패해야 할 것만 실패했다는 확인이다.

### GREEN — 구현 후 (`a28-green.log`, exit=0)

```
100% tests passed, 0 tests failed out of 69
```

### export 불변 (`a28-dumpbin.log`, exit=0)

```
          16 number of functions
          16 number of names
```

16개 이름 전부 기존 목록과 동일(`xpe_alert_push` … `xpe_version`). 새 심볼 없음.

### ci-preprocess 재실측 (`a28-pre.log`, exit=0)

```
100% tests passed, 0 tests failed out of 482
```

## 3. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-common | 64/64 PASS (`a28-red.log` 의 69건 중 신규 5건을 뺀 값; RED 로그에서 기존 64건은 전부 통과) | **69/69 PASS** (`a28-green.log`) | +5 = 신규 케이스, 기존 회귀 0 |
| ci-preprocess | 476/476 PASS (QA-A-25, `../QA-A-25/a25-b9.log`) | **482/482 PASS** (`a28-pre.log`) | +6 = `origin/main` 병합이 가져온 타 레인 테스트 |
| `xpe_common.dll` export | 16 (QA-A-19 에서 확정) | **16** (`a28-dumpbin.log:15-16`) | 변화 없음 |

세 수치 모두 이 워크트리에서 이번 턴에 직접 측정했다. ci-preprocess 의 +6 은 이번 카드의 변경이 아니라 병합으로 들어온 것이며, 코드 변경은 `modules/common/**` 에만 있다.

## 4. 미검증 (Gaps)

- **스레드 경합 검증 안 했다 (카드가 명시적으로 요구한 항목).** 축출·유실 알림 갱신은 전부 `g_mutex` 안에서 일어나므로 자료구조 경합은 구조적으로 배제되지만, 동시 push 를 실제로 돌려 본 테스트는 없다. 여러 스레드가 동시에 오버플로를 일으켰을 때 누적 카운터가 정확한지는 **추론이지 측정이 아니다.**
- **ASan 재측정 없음.** `build/asan-a17` 트리는 preprocess 용으로 구성돼 있고 common 만 따로 돌린 적이 없다. `sync_loss_alert_locked` 가 `push_back` 뒤 `&g_alertQueue.back()` 포인터를 쓰는 구간이 있는데(같은 잠금 안에서 재할당 없이 즉시 사용), 이 패턴의 검증은 컴파일러·테스트 통과에만 의존했다.
- **C# 소비자 미확인.** 유실 알림을 Error 로 표시하는 쪽은 Lane C(GUI-C-19) 소유라 손대지 않았고 실행하지도 않았다.
- **64 라는 용량 자체는 테스트가 재발견한 값**이다. export 되지 않는 구현 상수라 헤더로 확인할 수 없어, 큐를 채워서 관측한 수를 상수로 적었다.
- 카드 범위 밖 발견: `modules/common/tests/test_xpe_error_safety_violation.cpp` 는 디스크에 있으나 `modules/common/CMakeLists.txt` 어디에도 등록돼 있지 않다. QA-A-25 와 같은 유형(등록 누락)이며 이번 카드에서는 손대지 않았다.

## 5. 잔여 위험 (Residual risk)

- **유효 용량이 63으로 줄어든다.** 유실 알림이 슬롯 하나를 차지하므로, 한 번이라도 오버플로가 나면 실제 알림은 최대 63개만 남는다. §5.17 이 명시한 설계이지만, 소비자가 "64개까지 보관" 을 가정하고 있다면 어긋난다.
- **한 번의 push 가 2건을 축출할 수 있다.** 첫 오버플로에서는 들어오는 알림용 1건 + 유실 알림 슬롯용 1건, 합쳐 2건이 빠진다. 그래서 첫 유실 알림의 N 은 1이 아니라 2다. 카운터가 "몇 번 push 가 넘쳤나" 가 아니라 "몇 개가 실제로 버려졌나" 라는 점을 소비자가 오해할 수 있다.
- **Error 만 가득한 큐**에서는 Error 끼리 FIFO 축출된다. §5.17 이 정한 대로지만, 이 경우 유실 알림 외에는 어떤 Error 가 사라졌는지 알 방법이 없다.
- 누적 카운터는 `uint64_t` 라 실질적으로 넘치지 않으나, 메시지 버퍼는 64바이트 고정이다. `%llu` 최대 20자리 + 접두·접미 문구가 63자를 넘지 않는지는 계산으로만 확인했고 극단값을 실행해 보지는 않았다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 우선도 보호 FIFO 축출 구현 | QA-A-28 |
| 유실 알림 보장 구현 | QA-A-28 |
| export 16 실측 | QA-A-28 |
| RED/GREEN + 양쪽 프리셋 재실측 | QA-A-28 |
| 스레드 경합 검증 | 신규 카드 필요 |
| `test_xpe_error_safety_violation.cpp` 등록 누락 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a28-red.log` | 구현 전 ci-common, 5건 실패 (RED) |
| `a28-green.log` | 구현 후 ci-common 69/69 PASS (exit=0) |
| `a28-pre.log` | ci-preprocess 482/482 PASS (exit=0) |
| `a28-dumpbin.log` | `dumpbin /exports xpe_common.dll` — 16 functions / 16 names |
