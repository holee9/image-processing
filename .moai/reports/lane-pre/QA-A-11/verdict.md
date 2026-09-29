# QA-A-11 — xpe_shutdown 이 spdlog 파일 싱크를 놓게 하기 (#116, Class B)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #116 (#98 후속)
**baseline**: `git merge main` → HEAD `a3ae133` (ff 34커밋, `origin/main...HEAD` = `0 0`)

## 1. 주장 (Claim)

`xpe_log_internal_reset()` 이 정의만 되고 **호출처가 0건**이었다. `xpe_shutdown` 은 자기
`g_logFile`(ofstream)만 닫고 spdlog 파일 싱크는 열어둔 채 끝나, Windows 에서 로그 파일을
지울 수 없었다. `xpe_shutdown` 에서 헬퍼를 호출하도록 배선하고 헬퍼에 명시적 flush 를 넣었다.

## 2. 증거 (Evidence)

### 2.1 1단계 — 헬퍼 호출처 전수 조사 (leader 관측 확인)

```
$ grep -rn "xpe_log_internal_reset" --include=*.cpp --include=*.h --include=*.hpp modules/ clients/
modules/common/src/xpe_logging.cpp:117:void xpe_log_internal_reset() {
```

정의 1건, **호출 0건**. leader 관측이 맞다 — 반증할 것 없음.

### 2.2 RED — 수정 전 (`red11-run.log`)

```
[ RUN      ] XpeCommonTest.ShutdownReleasesLogFileSink
test_xpe_common.cpp(240): error: Expected equality of these values:
log file is still held open after xpe_shutdown()
[  FAILED  ] XpeCommonTest.ShutdownReleasesLogFileSink (2 ms)
[  PASSED  ] 8 tests.
[  FAILED  ] 1 test
```

`LogFlushAfterShutdownDoesNotCrash` 는 RED 단계에서도 통과했다. 이것은 결함 재현이 아니라
**기존 보호(xpe_logging.cpp:74-81)가 이번 변경으로 깨지지 않는지 지키는 가드**다 (카드 4단계).

### 2.3 GREEN — 수정 후 (`green11-run.log`)

```
[==========] 9 tests from 1 test suite ran. (156 ms total)
[  PASSED  ] 9 tests.
```

### 2.4 무회귀 — ci-preprocess 전체 (`ctest11.log`, 이번 실행 실측)

```
100% tests passed, 0 tests failed out of 349
Total Test time (real) =  30.47 sec
```

347 → 349 는 이번에 추가한 2건. 실패 0건.

### 2.5 변경 diff

| 파일 | 내용 |
|---|---|
| `modules/common/src/xpe_common.cpp` | 헬퍼 `extern "C"` 선언 + `xpe_shutdown` 에서 호출 |
| `modules/common/src/xpe_logging.cpp` | 헬퍼에 명시적 `g_logger->flush()` 추가 (+3) |
| `modules/common/tests/test_xpe_common.cpp` | RED 1 + 가드 1 (+45) |

## 3. 설계 판단 2건

**(1) `extern "C"` 선언 — 첫 시도가 LNK2019 로 깨졌다.**
헬퍼 정의(117행)가 `xpe_logging.cpp` 의 `extern "C"` 블록(43-158행) **안**에 있어 C 링키지다.
C++ 링키지로 선언했더니 `?xpe_log_internal_reset@@YAXXZ` 미해결로 링크 실패했다. 실측 로그:

```
xpe_common.cpp.obj : error LNK2019: "void __cdecl xpe_log_internal_reset(void)"
  (?xpe_log_internal_reset@@YAXXZ) ... 확인할 수 없는 외부 기호
bin\xpe_common.dll : fatal error LNK1120
```

`extern "C"` 로 고쳐 해소. 공개 헤더에 넣지 않은 이유: DLL 외부로 내보내는 API 가 아니라
내부 생명주기 훅이고, export 수(16)를 바꾸면 REQ-P0-008 실측치와 어긋난다.

**(2) `g_mutex` 를 놓은 뒤 호출한다.**
`xpe_shutdown` 의 `lock_guard` 를 내부 스코프로 감싸고, 스코프를 벗어난 뒤 헬퍼를 부른다.
헬퍼는 `g_logMutex`(xpe_logging.cpp 전용)를 잡는다. 실측으로 두 뮤텍스는 서로 겹치지 않는다
(`grep g_logMutex modules/common/src/xpe_common.cpp` → 0건, 역방향도 0건) — 지금은 교착이
불가능하지만, 잠금을 겹쳐 잡지 않는 편이 나중에 한쪽이 늘어나도 안전하다.

## 4. Gaps (미검증)

- **CI 및 C# 통합 테스트 미검증.** Lane C 소유라 실행하지 않았다. leader 가 push 후 판정.
- `ci-preprocess` 단일 구성에서만 검증 (`ci-common`, `ci-fullstack` 미실행).
- **파일 핸들 해제를 `std::remove` 성공으로만 관측했다.** 핸들 수준(`handle.exe` 등) 확인은
  하지 않았다 — 다른 이유로 remove 가 성공했을 가능성을 완전히 배제하지는 못한다.
- 헬퍼의 null-sink 복원 경로는 `xpe_log_flush` 2회 호출로만 확인했다. 다른 default-logger
  소비자(`spdlog::info` 직접 호출 등)까지는 확인 범위 밖.

## 5. 잔여 위험

- `xpe_shutdown` 이 이제 로깅 모듈 상태를 만진다. 이전에는 두 모듈이 독립이었다 —
  종료 순서에 의존하는 코드가 생기면 이 결합이 문제가 될 수 있다.
- `xpe_init` → `xpe_shutdown` 반복 사이클에서 매번 null-sink 로거를 새로 만든다.
  `ConcurrentInitShutdownDoesNotCrash`(153ms) 는 통과했으나 장시간 반복 부하는 미측정.
