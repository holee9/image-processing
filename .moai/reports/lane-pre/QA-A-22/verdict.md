# QA-A-22 검증 보고서 — cached loader 소유권을 캐시 소유 뷰로 통일 (#127, #105)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-22 (`.moai/lanes/pre/inbox/QA-A-22.md`)
- 정본: `docs/project/api-spec.md` §6 "Cached loaders — ownership (normative, 2026-09-10, #127)"
- 선행 병합: `git merge origin/main` → `7b2f711`
- 커밋: `8ff5658`

## 1. 주장 (Claim)

1. `xpe_calib_load_offset_cached` / `_gain_cached` / `_defect_cached` 세 진입점의 **미스 경로**를 계약 B(항상 캐시 소유 뷰)로 고쳤다. 버퍼를 한 번만 할당해 캐시에 소유권을 넘기고, 캐시 엔트리를 다시 읽어 **캐시의 포인터**를 반환한다.
2. 호출자가 넘긴 `XpeImageBuffer*` 는 **값만 채운다.** 들어온 `data` 포인터를 `free` 하지도 `realloc` 하지도 않는다 — 그 자리에 남아 있는 값이 다른 캐시 엔트리의 살아 있는 포인터일 수 있기 때문이다.
3. 미스 경로가 캐시용 두 번째 버퍼를 추가로 할당하던 것을 없앴다. 이제 미스 1회당 할당은 1회다.
4. 세 로더의 중복된 미스 꼬리를 `publish_and_view()` 하나로 합쳤다 (164줄 중 96줄 교체, 순증가 0줄).
5. `test_calibration_cache.cpp` 의 `free` 부재가 계약상 옳게 되었으므로, 파일 상단 주석을 "결함을 감수한다" 에서 "이것이 계약이다" 로 교체했다.
6. 회귀 6케이스 신설. RED 3건 실패 → GREEN, ci-preprocess **493/493 PASS**.

## 2. 증거 (Evidence)

### RED — 수정 전 (`a22-red.log`, exit=8)

```
314/494 Test #314: CalibCacheOwnershipTest.MissAndHitReturnTheSamePointer .................***Failed    0.02 sec
315/494 Test #315: CalibCacheOwnershipTest.OutParameterIsOverwrittenNotReallocated ........***Failed    0.38 sec
319/494 Test #319: CalibCacheOwnershipTest.GainLoaderFollowsTheSameContract ...............***Failed    0.03 sec

99% tests passed, 3 tests failed out of 493
```

실패한 3건은 전부 **포인터 동일성** 계약이다. 나머지 3건(누수 게이트·축출·shrink)은 수정 전에도 통과했다 — 옛 구현에서도 캐시 자체의 LRU 동작은 옳았고, 어긋난 것은 반환 포인터의 소유권뿐이었다는 뜻이다. 실패해야 할 것만 실패했다.

### GREEN — 수정 후 (`a22-green.log`, exit=0)

```
100% tests passed, 0 tests failed out of 493
```

주석 교체 뒤 재확인 (`a22-green2.log`, exit=0):

```
100% tests passed, 0 tests failed out of 493
```

### 변경 규모 (`a22-diffstat.txt`)

```
 modules/preprocess/CMakeLists.txt                  |   2 +
 modules/preprocess/src/calibration_cache.cpp       | 164 ++++++++++-----------
 modules/preprocess/tests/test_calibration_cache.cpp |  26 ++--
 3 files changed, 96 insertions(+), 96 deletions(-)
```

## 3. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 487/487 PASS (QA-A-29, `../QA-A-29/a29-green.log`) | **493/493 PASS** (`a22-green2.log`) | +6 = 신규 케이스, 회귀 0 |

`a22-red.log` 도 493건 기준(신규 6건 포함)이며 그중 3건만 실패했다. 즉 기존 487건은 수정 전후 모두 통과했고, 이번 변경으로 깨진 것은 없다.

## 4. 카드 항목별 대응

| 카드 항목 | 대응 |
|---|---|
| (a) 미스 뒤 히트에서 `data` 포인터 동일 | `MissAndHitReturnTheSamePointer` — 실측 통과 |
| (b) 두 경로 모두 호출자 free 없이 누수 0 | **G3 방식 채택.** `RepeatedLoadsDoNotAccumulateBuffers`: warm-up 50회 뒤 같은 키를 300회 로드하며 매 반복 포인터가 동일한지 검사한다. 아래 사유 참조 |
| (c) `cache_clear` 뒤 이전 포인터 사용 금지 | **문서화만.** UB 라 테스트로 검출할 수 없다. `test_calibration_cache.cpp` 상단 주석과 `publish_and_view()` Doxygen 에 유효 기간(clear / set_max_size 축출 / shutdown)을 명시 |
| (d) `set_max_size(1)` 축출 순서 | `EvictionDropsTheLeastRecentlyUsedEntry` + `SetMaxSizeEvictsDownToTheNewCapacity` — 실측 통과 |
| 3. `test_calibration_cache.cpp` 주석 사유 명시 | 파일 상단 주석 교체 (diff 26줄) |
| 4. ci-preprocess 재실측 | 493/493 PASS |

### (b) 를 G3 대신 포인터 동일성으로 측정한 사유

카드는 "G3 방식(warm-up → N회 → 증가량) 또는 ASan `detect_leaks` 중 실제로 동작하는 쪽" 을 요구했다. 둘 다 이번 결함에는 맞지 않는다:

- **Windows MSVC ASan 은 leak 검출을 지원하지 않는다** (`detect_leaks` 는 Linux/macOS 전용). 카드도 이 경우를 예상해 G3 를 대안으로 뒀다.
- **G3 의 상주 메모리 증가량 측정도 이 결함을 잡지 못한다.** 옛 미스 경로의 누수는 **미스 1회당 1건**인데, 같은 키를 반복 호출하면 두 번째부터는 히트라 미스가 다시 일어나지 않는다. 서로 다른 키로 N회 돌리면 캐시가 정상적으로 보관하는 메모리와 누수가 섞여 구분되지 않는다.

그래서 **누수의 원인 자체**를 측정했다: 매 호출이 같은 포인터를 돌려주면 호출당 새 할당이 없다는 뜻이고, 옛 구현의 누수 형태(호출자 소유 버퍼를 매번 새로 만들고 아무도 free 하지 않음)는 주소 변화로 반드시 드러난다. 상주 메모리 증가량보다 직접적이고 임계값 튜닝이 필요 없다.

## 5. 미검증 (Gaps)

- **프로세스 상주 메모리는 측정하지 않았다.** 위 사유로 포인터 동일성으로 대체했으므로, "누수 0" 은 **할당 횟수 기준**의 주장이지 RSS 관측이 아니다. 캐시 자체가 보관하는 메모리(정상)와 진짜 누수를 분리하는 계측은 하지 않았다.
- **ASan 미실행.** Windows MSVC ASan 이 leak 검출을 지원하지 않는다는 것은 문서 지식이며, 이번 턴에 `detect_leaks=1` 을 실제로 걸어 "미지원" 출력을 확인하지는 않았다.
- **defect 로더는 코드 경로만 동일하고 테스트는 없다.** 신규 스위트는 offset 과 gain 만 실행한다. 세 로더가 같은 `publish_and_view()` 를 호출하므로 동작은 같지만, 실행으로 확인한 것은 둘뿐이다.
- **스레드 경합 미검증.** `publish_and_view()` 는 `put` 과 `get` 을 **연속된 두 개의 잠금 구간**으로 호출한다. 그 사이에 다른 스레드가 `cache_clear()` 나 축출을 일으키면 `get` 이 실패해 `XPE_ERR_PROCESSING_FAILED` 가 나온다. 동시 호출 테스트는 하지 않았다 — 아래 잔여 위험 참조.
- **`xpe_copy_image` 로 사본을 뜨는 경로**를 실제로 실행해 보지는 않았다. api-spec 이 권하는 방법이지만 이번 스위트는 다루지 않는다.

## 6. 잔여 위험 (Residual risk)

- **put/get 사이의 창(TOCTOU).** 위 Gaps 에 적은 그대로, 미스 처리 중 다른 스레드가 캐시를 비우면 방금 넣은 엔트리를 읽지 못해 실패를 반환한다. 데이터는 캐시가 소유하므로 누수나 이중 해제는 없지만, 드물게 `XPE_ERR_PROCESSING_FAILED` 가 보일 수 있다. 한 번의 잠금 안에서 삽입과 조회를 함께 하는 `put_and_get` 을 캐시 클래스에 추가하면 닫히는 창이며, 공개 시그니처를 건드리지 않으므로 후속 카드로 가능하다.
- **반환 포인터의 수명은 호출자가 지켜야 한다.** `cache_clear()` 나 축출 뒤에 이전 포인터를 쓰면 UB 이고, 이를 기계적으로 막을 방법은 없다. 특히 `set_max_size(1)` 처럼 작은 용량에서는 다음 로드 한 번으로 직전 포인터가 무효가 된다.
- **파이프라인이 프레임마다 이 로더를 호출한다**(`@MX:ANCHOR` 사유). 캐시 용량 기본값이 4라, 서로 다른 맵 5종을 번갈아 쓰는 사용 패턴에서는 매번 미스가 나고 이제 미스마다 파일 I/O + 할당이 일어난다. 이번 변경이 만든 문제는 아니지만(옛 구현도 같았다), 용량 튜닝은 아무도 측정한 적이 없다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 미스 경로 캐시 소유 뷰 전환 | QA-A-22 |
| 회귀 6케이스 (포인터 동일성·할당 횟수·축출) | QA-A-22 |
| `test_calibration_cache.cpp` 주석 사유 명시 | QA-A-22 |
| ci-preprocess 재실측 | QA-A-22 |
| put/get 창 제거 (`put_and_get`) | 신규 카드 필요 |
| defect 로더 계약 테스트 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a22-red.log` | 수정 전 — 포인터 동일성 3건 실패 (exit=8) |
| `a22-green.log` | 수정 후 493/493 PASS (exit=0) |
| `a22-green2.log` | 주석 교체 뒤 재확인 493/493 PASS (exit=0) |
| `a22-diffstat.txt` | `git diff --stat` |
