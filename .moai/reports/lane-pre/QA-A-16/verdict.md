# QA-A-16 — preprocess 지구력 테스트 기준선에 warm-up 추가 (#105, Class A)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #105
**baseline**: HEAD `9fa10fc` (A-11 직후, `origin/main...HEAD` = `0 2`)

## 1. 주장 (Claim)

`ThousandCycles_MemoryGrowthUnderOneMB` 가 warm-up 없이 기준선을 찍어 최초 페이지 폴트와
CRT 아레나 확장이 증가량에 포함됐다. 측정 루프 앞에 100회 warm-up 을 넣어 정상 상태 증가만
채점하게 했다. 임계값(1 MB)·사이클 수(1000)·단언은 그대로다.

## 2. 증거 (Evidence)

### 2.1 warm-up 적용 후 — 5회 반복 (`after16.log`)

```
Repeating all tests (iteration 1..5)
[       OK ] EnduranceTest.ThousandCycles_MemoryGrowthUnderOneMB (879 / 787 / 778 / 868 / 1063 ms)
[  PASSED  ] 1 test.   (5회 모두)
```

### 2.2 민감도 프로브 — 사이클당 4096 B 고의 누수 (`probe16.log`)

측정 루프에 `volatile char* probe_leak = new char[4096];` 를 1줄 주입하고 재빌드:

```
test_xpe_calib_endurance.cpp(117): error: Expected: (after - before) < (ONE_MB),
  actual: 5107712 vs 1048576
Memory grew by 4988 KB over 1000 cycles
[  FAILED  ] EnduranceTest.ThousandCycles_MemoryGrowthUnderOneMB
```

4988 KB ≈ 1000 × 4096 B. **warm-up 이 테스트를 눈멀게 하지 않았다** — 진짜 누수는 여전히 잡는다.

### 2.3 프로브 원복 확인

```
$ grep -c "PROBE\|probe_leak" modules/preprocess/tests/test_xpe_calib_endurance.cpp
0
$ git diff --stat
 modules/preprocess/tests/test_xpe_calib_endurance.cpp | 13 +++++++++++++
 1 file changed, 13 insertions(+)
```

삽입 13줄은 전부 warm-up + 주석. 프로브 잔재 0건, 삭제 0줄.

### 2.4 ctest 재실측 (`ctest16.log`, 프로브 원복 후)

```
100% tests passed, 0 tests failed out of 349
Total Test time (real) =  33.35 sec
```

## 3. Gaps (미검증)

- **CI 환경에서의 재현은 검증하지 못했다.** 원래 실패가 부하 의존이라 로컬 5회 통과가
  CI 통과를 보장하지 않는다. 두 번째 발생(1048576 vs 1048576)은 정확히 경계값이었으므로,
  warm-up 이 충분한지는 CI 연속 통과로만 확인된다.
- **warm-up 100회라는 수치의 근거는 enhance_basic 선례뿐**이다. preprocess 사이클에서
  몇 회부터 정상 상태에 드는지는 측정하지 않았다 — 100회가 과하거나 모자랄 수 있다.
- 로컬은 부하가 낮은 상태에서 측정했다. CI 러너의 동시 부하는 재현하지 않았다.

## 4. 잔여 위험

- warm-up 은 **원인을 없앤 것이 아니라 측정 시점을 옮긴 것**이다. 시작 시 1 MB 넘게 잡는
  구조적 변화가 생기면 이 테스트는 그것을 더 이상 보지 못한다 — 시작 비용 자체를 보는
  별도 테스트는 없다.
- 사이클당 1 KB 미만의 진짜 누수는 1000 사이클에서 1 MB 를 넘지 않아 여전히 통과한다.
  프로브가 확인한 민감도는 4 KB/사이클 수준이다.
