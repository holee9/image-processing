# QA-B-05 — enhance_basic 힙 증가량 계측 (#105 G3, 1/6 모듈)

**레인**: Lane B (`dev/postprocess`)
**범위**: `modules/enhance_basic` 단일. 나머지 5개 모듈은 별도 카드.
**참조 구현**: Lane A `modules/preprocess/tests/test_xpe_calib_endurance.cpp`
`EnduranceTest.ThousandCycles_MemoryGrowthUnderOneMB`

---

## 1. 주장 (Claim)

1. enhance_basic 의 1000회 지구력 루프가 힙 증가량을 **실측 검증**한다.
2. 계측 방식은 Lane A preprocess 와 동일하다 (Windows `GetProcessMemoryInfo` WorkingSet).
3. 계측이 실제 누수를 검출할 수 있다 (고의 누수 프로브로 확인).
4. ctest 무회귀.

## 2. 증거 (Evidence)

### 2.1 변경 파일

| 파일 | 변경 |
|---|---|
| `modules/enhance_basic/tests/test_enhance_integration.cpp` | `get_working_set_bytes()` 헬퍼 + 기존 `NoHeapLeak_1000Iterations` 에 증가량 검증 추가 + 신규 `NoHeapLeak_1000Iterations_AllocatingPaths` |
| `modules/enhance_basic/CMakeLists.txt` | Windows 에서 `psapi` 링크 (`GetProcessMemoryInfo` 요구) |

### 2.2 대상 테스트 실행

```
xpe_enhance_basic_tests.exe --gtest_filter=EnhanceIntegration.NoHeapLeak*
[       OK ] EnhanceIntegration.NoHeapLeak_1000Iterations (1 ms)
[       OK ] EnhanceIntegration.NoHeapLeak_1000Iterations_AllocatingPaths (96 ms)
[  PASSED  ] 2 tests.
```
원본 로그: `_focus.log`

### 2.3 계측 민감도 — 고의 누수 프로브

계측이 실패할 수 있음을 증명하기 위해, 할당 경로 루프에 사이클당 4096 B 를
`malloc` + `memset` 하는 누수를 **일시적으로** 주입해 재빌드·실행했다.

```
Working set grew by 4288 KB over 1000 noise/contrast/edge cycles
[  FAILED  ] EnhanceIntegration.NoHeapLeak_1000Iterations_AllocatingPaths
```
원본 로그: `_probe_leak.log`

주입분 4096 B × 1000 = 4096 KB, 관측 증가 4288 KB — 크기가 일치한다.
프로브는 즉시 원복했고 (`git diff` 에 `malloc(4096)` 0건) 최종 빌드는 프로브 없는 상태다.

### 2.4 ctest 무회귀

```
100% tests passed, 0 tests failed out of 282
```
원본 로그: `_ctest.log`

## 3. baseline 귀속

| 항목 | baseline | 현재 | 출처 |
|---|---|---|---|
| ctest 총계 | 281 passed / 0 failed | 282 passed / 0 failed | `QA-B-01/_ctest_post.log` → `QA-B-05/_ctest.log` |

증가분 +1 은 신규 추가한 `NoHeapLeak_1000Iterations_AllocatingPaths` 정확히 1건이다.
빌드 경고 신규 0건 (`_build.log` 에 warning/error 매칭 없음. CMake 의 기존
`FetchContent_Populate(eigen)` deprecation 은 이 카드 이전부터 존재하는 전역 경고).

---

## 4. 임계값 선택 근거

### 4.1 상한 1 MB — preprocess 와 동일

preprocess T-010 이 1000 사이클 기준 1 MB 를 쓴다. enhance_basic 의 1회 사이클
할당 총량이 preprocess 보다 크지 않으므로(64×64 float 스크래치 버퍼 몇 개, 수십 KB 급)
더 느슨한 값을 쓸 이유가 없다. 모듈 특성에 따른 완화는 하지 않았다.

### 4.2 warm-up 100회를 baseline 스냅샷 앞에 둔 것

Lane A 와 다른 유일한 점이다. WorkingSet 은 **최초 접근 시 페이지 폴트**와
CRT 할당자 아레나 확장을 포함한다. 이걸 그대로 재면 임계값이 "누수량"이 아니라
"기동 비용"을 재게 된다. 그래서 warm-up 100회를 돌린 뒤 `before` 를 찍는다.

preprocess 는 파일 I/O 경로라 첫 로드 비용이 상대적으로 작아 warm-up 없이도
통과했지만, enhance_basic 은 사이클마다 `std::vector` 스크래치를 잡았다 놓으므로
아레나 성장이 초기에 몰린다. warm-up 은 완화가 아니라 **측정 대상의 정정**이다.

### 4.3 테스트를 2개로 나눈 근거 (중요)

카드는 "기존 1000회 루프 테스트에 힙 증가량 계측을 추가" 를 지시했다.
그대로만 하면 **실패할 수 없는 테스트**가 된다:

- `NoHeapLeak_1000Iterations` 이 도는 `xpe_log_transform` 은 **in-place** 로,
  힙을 한 번도 잡지 않는다 (`modules/enhance_basic/src/log_transform.cpp`).
- 즉 이 루프의 증가량은 구현이 어떻든 항상 0 이다. G3 를 형식상 충족하지만
  정보량이 0 인 vacuous PASS 다.

그래서 지시대로 기존 테스트에 계측을 넣되(회귀 감시로는 유효하다 — 향후
log_transform 이 할당을 도입하면 잡힌다), **실제로 할당하는 경로**를 도는
두 번째 테스트를 추가했다. 할당 위치 근거:

| 함수 | 사이클당 힙 할당 | 위치 |
|---|---|---|
| `xpe_log_transform` | 없음 (in-place) | — |
| `xpe_noise_reduce` | `sp_w`, `ring`, `wsum_buf`, `vsum_buf`, `output` | `noise_reduce.cpp:115,125,126,127,213` |
| `xpe_contrast_enhance` | `tiles`, `output` | `contrast_enhance.cpp:162,183` |
| `xpe_edge_enhance` | `wt_buf`, `ring`, `blurred_row` | `edge_enhance.cpp:93,118,119` |

§2.3 프로브가 실패시킨 것은 이 두 번째 테스트다. 첫 번째 테스트는 프로브가
들어가지 않아 그대로 통과했고, 이는 두 테스트의 역할 분리와 일치한다.

### 4.4 `if (after > before)` 가드를 유지한 것

Lane A 와 동일. WorkingSet 은 OS 가 회수하면 줄어들 수 있고, 감소는 결함이 아니다.
부호 없는 정수 뺄셈 언더플로도 함께 막는다.

---

## 5. 미검증 (Gaps)

- **Linux/POSIX 경로 미실행.** `get_working_set_bytes()` 는 비-Windows 에서 0 을
  반환하고, 할당 경로 테스트는 `GTEST_SKIP` 한다. 실측은 Windows 에서만 이루어졌다.
- **WorkingSet 은 힙 사용량의 근사다.** 실제 힙 점유가 아니라 상주 페이지 집합이므로,
  1 MB 미만의 소규모 누수는 페이지 단위 반올림에 묻힐 수 있다. §2.3 이 확인한 것은
  "4 MB 급 누수를 잡는다" 이지 "임의의 작은 누수를 잡는다" 가 아니다.
  CRT 디버그 힙(`_CrtMemCheckpoint`)이나 ASan 이 더 정밀하지만, 참조 구현과
  방식을 맞추라는 카드 지시에 따라 도입하지 않았다.
- **다른 5개 모듈(enhance_advanced, ai, display, dicom, gsvg) 미착수.** 각각 별도 카드.
- **동시 실행 하 계측 미검증.** 두 테스트 모두 단일 스레드다. 다른 gtest 케이스와
  같은 프로세스에서 순차 실행되므로, 앞선 테스트가 남긴 아레나가 `before` 에
  반영된다 — warm-up 이 이를 흡수하도록 설계했으나 별도로 격리 측정하지는 않았다.

## 6. 잔여 위험 (Residual risk)

- WorkingSet 은 OS 메모리 압력에 따라 트리밍될 수 있어, 부하가 높은 머신에서는
  `after < before` 가 되어 검증이 무해하게 건너뛰어질 수 있다(가드 §4.4).
  즉 **거짓 실패는 없지만 거짓 통과는 가능**하다. CI 부하 상황에서 이 테스트는
  누수 검출을 보장하지 않는다.
- 1000 사이클 × 64×64 는 96 ms 로 저렴하지만, 누수량이 사이클당 1 KB 미만이면
  총 1 MB 에 못 미쳐 통과한다. 임계값을 낮추는 대신 사이클 수를 늘리는 선택지가
  있으나, preprocess 와의 대칭을 우선했다.

---

## 7. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 1000프레임 실행 중 힙 증가량 상한 이내를 테스트가 실측 검증 | PASS | §2.2, §2.3 |
| ctest 무회귀 | PASS | §2.4, §3 (281 → 282, 실패 0) |
| 임계값 선택 근거를 gate.md 에 기록 | PASS | §4 |
