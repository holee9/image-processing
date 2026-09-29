# QA-A-89 — 시험 사이로 새는 전역 상태를 막는다 (#176)

**카드**: `.moai/lanes/pre/inbox/QA-A-89.md` · **브랜치**: `dev/preprocess` · **커밋**: `f355208` (미푸시) · **병합 기준**: `7c65ebd` · **기계**: Intel Core i7-12700

바뀐 파일은 시험 5개뿐이다. 제품 코드는 바꾸지 않았다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **(다)의 원인을 실측으로 확정했다**: `xpe_offset_correct returned -8` (`BUFFER_TOO_SMALL`). 캐시 시험이 남긴 **8×8 활성 오프셋 맵** 때문이다. 캐시 미스가 1-인자 로더로 활성 맵까지 설치한다 |
| C2 | 세 순서 의존 시험을 **각 시험이 자기 전제를 스스로 만들도록** 고쳤고, (나)(다)는 오염원 쪽 정리도 넣었다 |
| C3 | (가)는 품질 메타를 되돌릴 **공개 수단이 없어서** 자식 프로세스(`EXPECT_EXIT`)에서 단언하게 했다. 공개 API 는 바꾸지 않았다 |
| C4 | **기본 순서 0 실패, 셔플 시드 1~16 전부 0 실패** (수정 전 16회 중 12회 실패) |
| C5 | **반증 7건 모두 예측대로**: 한쪽만 되돌리면 초록(다른 쪽이 막음), 양쪽 다 되돌리면 해당 시험만 빨강 |
| C6 | `xpe_preprocess_shutdown` 이 모드·품질 메타를 남기는 것이 **의도라는 기록은 찾지 못했다**. 그리고 **공개 헤더("미초기화면 no-op")와 코드(항상 `g_calib` 을 지움)가 다르다**. 바꾸지 않았다 |
| C7 | CI 직접 실행: `build/ci-preprocess/bin/xpe_preprocess_tests.exe` → 개발 기계 **40.6 s**, 종료 0 |
| C8 | `ci-preprocess` 647/647, `ci-common` 69/69, 빌드 경고 0 |
| C9 | 측정 도중 **무효 실행 세 번**을 산출물 확인으로 잡아 버렸다(§2 끝) |

---

## 2. 증거 (Evidence)

### C1 — (다)의 원인

메모리 시험의 반환값 단언에 값을 붙였다(이 변경은 남긴다 — 다음 실패를 읽을 수 있게):

```cpp
EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_ERR_CALIB_NOT_LOADED)
    << "xpe_offset_correct returned " << rc;
```

두 시험만 걸러 시드 1 로 실행:

```
Note: Randomizing tests' orders with a seed of 1 .
[ RUN      ] CalibCacheConcurrencyTest.LoadUnderConcurrentEvictionNeverReportsSpuriousFailure
[ RUN      ] XpePreprocessEndurance.NoMemoryLeakAfter1000Frames
xpe_offset_correct returned -8
```

- `-8` = `XPE_ERR_BUFFER_TOO_SMALL`. `offset_correct.cpp:185-186` 이 활성 맵의 크기와 입력 크기가 다르면 이 값을 낸다.
- 크기: 캐시 시험 `W = 8, H = 8` (`test_calib_cache_concurrency.cpp:45`), 메모리 시험 512×512 (`test_xpe_preprocess_memleak.cpp:56-57`).
- `calibration_cache.cpp` 의 `xpe_calib_load_offset_cached`: 캐시 미스 → "load from file via 1-arg API (populates g_calib)". 캐시 픽스처의 `TearDown` 은 `xpe_calib_cache_clear()` 만 불러 **캐시는 비우지만 활성 맵은 남긴다.**

QA-A-88 에서 "오프셋 맵으로 추정"이라 적은 것이 이제 측정이다.

### C2 · C3 — 수정

| 대상 | 쪽 | 수정 |
|---|---|---|
| `CalibModeTest.QualityMeta_InitialState` | 의존 | 단언 전체를 `EXPECT_EXIT` 안으로. 자식이 stderr 에 `QUALITY_META_INITIAL_OK` 와 필드 값을 쓰고 `exit(ok ? 0 : 1)`. 부모는 `ExitedWithCode(0)` + 그 문자열을 요구 |
| `CalibModeTest` 픽스처 | 오염원(모드) | `TearDown` 에서 `xpe_calib_set_mode(XPE_CALIB_MULTI_POINT_8)` |
| `PipelineStageTest` 픽스처 | 의존 | `SetUp`: `(void)xpe_preprocess_init` → `xpe_preprocess_shutdown` → `ASSERT_EQ(XPE_OK, xpe_preprocess_init)` |
| `EnduranceTest` 픽스처 | 오염원 | 시험별 `TearDown`: `(void)xpe_preprocess_init` → `xpe_preprocess_shutdown` |
| `NoMemoryLeakAfter1000Frames` | 의존 | 시험 시작에서 `(void)xpe_preprocess_init` → `xpe_preprocess_shutdown` |
| `CalibCacheConcurrencyTest` 픽스처 | 오염원 | `TearDown` 에 `(void)xpe_preprocess_init` → `xpe_preprocess_shutdown` |

**왜 `init` 을 먼저 부르나** — `shutdown` 을 **초기화된** 모듈에서 부르기 위해서다. 공개 헤더는 미초기화 상태의 `shutdown` 을 no-op 이라 적는데, 코드는 그때도 `g_calib` 을 지운다(C6). 시험이 그 차이에 기대면, 누가 코드를 헤더에 맞추는 날 정리가 조용히 사라진다. 첫 `init` 의 반환값을 버리는 것은 의도다 — 앞 시험이 초기화된 채 남겼으면 `XPE_ERR_INVALID_INPUT` 이 오고, 어느 쪽이든 `shutdown` 시점에는 초기화돼 있다.

**(가)에 공개 API 를 추가하지 않은 이유** — 시험 바이너리는 DLL(`xpe_preprocess`)에 링크되고(`CMakeLists.txt:428-431`), 품질 메타를 쓰는 `xpe_calib_record_quality_meta`·`xpe_calib_apply_quality_meta_json` 은 내부 헤더의 **비내보내기** 함수다(`xpe_preprocess_internal.h:279`, `:299`). 되돌리는 함수를 내보내면 시험을 위해 공개 표면을 넓히게 된다. Windows 의 gtest 데스 테스트는 fork 가 없어 **항상 바이너리를 재실행**하므로 자식의 DLL 전역은 처음 상태다.

**`GTEST_FLAG_SET(death_test_style, "threadsafe")` 를 넣었다가 뺐다**:

```
BUILD_EXIT_fix=1
test_calib_mode.cpp.obj : error LNK2001: ... testing::FLAGS_gtest_death_test_style
bin\xpe_preprocess_tests.exe : fatal error LNK1120
```

gtest 1.14.0 이 DLL 로 빌드돼 이 플래그를 내보내지 않는다. 주석에 이 사실을 남겼다.

### C4 — 수정 후 실행

```
BUILD_EXIT_fix2=0
[2/2] Linking CXX executable bin\xpe_preprocess_tests.exe
default:  [  PASSED  ] 570 tests.  fails=[]
S1  seed of 1   [  PASSED  ] 570 tests. fails=[]
S2  seed of 2   [  PASSED  ] 570 tests. fails=[]
...
S16 seed of 16  [  PASSED  ] 570 tests. fails=[]
```

16개 시드 전부 로그에 `seed of N` 이 찍혔고, 전부 요약 줄(`tests from 79 test suites ran`)이 있다. gtest 총계: `578 tests from 79 test suites ran` — 578 중 570 통과이고 나머지는 원래 건너뛰는 시험이다(`YOU HAVE 1 DISABLED TEST` 포함). 수정 전 같은 바이너리 구성에서도 569 + 1 = 570 이었다.

### C5 — 반증

도구 `a89_toggle.py`: 수정본 5개 파일을 백업하고, 지정한 수정만 되돌린다. 각 토글이 **해당 파일만** 바꾸는 것을 `git diff --stat` 으로 먼저 확인했다(예: `na_dep` → pipeline 파일 15줄 → 2줄, `da_dep` → memleak 13줄 → 3줄로 `rc` 메시지만 남음).

각 실험 = 되돌림 → 재빌드(`Building CXX` 로그 확인) → 실행.

| # | 되돌린 것 | 순서 | 통과/실패 | 실패한 시험 |
|---|---|---|---|---|
| E1 | (가) 자식 프로세스화 | 기본 | 569 / 1 | `CalibModeTest.QualityMeta_InitialState` |
| E2 | (나) 의존 쪽 | 시드 1 | 570 / 0 | — |
| E3 | (나) 오염원 쪽 | 시드 1 | 570 / 0 | — |
| E4 | (나) 둘 다 | 시드 1 | 569 / 1 | `PipelineStageTest.DefectStageFailsWhenItsMapIsNotLoaded` (`rc` −7) |
| E5 | (다) 의존 쪽 | 시드 9 | 570 / 0 | — |
| E6 | (다) 오염원 쪽 | 시드 9 | 570 / 0 | — |
| E7 | (다) 둘 다 | 시드 9 | 569 / 1 | `XpePreprocessEndurance.NoMemoryLeakAfter1000Frames` (`xpe_offset_correct returned -8`) |

**카드는 "각 수정을 하나씩 되돌리면 그 시험만 빨강"을 요구했다.** (나)(다)는 양쪽을 고쳤기 때문에 한쪽만 되돌리면 초록이다(E2·E3·E5·E6). 이것은 **각 수정이 혼자서도 충분하다**는 증거이고, 양쪽을 다 되돌렸을 때만 빨강(E4·E7)인 것이 **두 수정이 같은 결함을 막는다**는 증거다.

**모드 `TearDown` 은 반증하지 않았다** — 모드 값을 단언하는 시험이 `CalibModeTest` 밖에 없어(QA-A-88), 되돌려도 빨강이 될 시험이 없다.

원복 후: `restored 5`, 재빌드 `BUILD_EXIT_E0=0`, `git diff --stat` 이 수정본과 같은 `5 files changed, 98 insertions(+), 17 deletions(-)`.

### C6 — `xpe_preprocess_shutdown`

| 출처 | 내용 |
|---|---|
| 코드 `preprocess.cpp:71-80` | `g_initialized.store(false)`; `g_calib = CalibrationData{};` — 초기화 여부 검사 없음 |
| 공개 헤더 `preprocess_api.h:79-88` | "Shutdown preprocessing module and release resources" / "Safe to call multiple times. **If module is not initialized, this is a no-op.** After shutdown, module returns to uninitialized state and can be re-initialized." |
| `xpe_calib_mode.cpp:32`, `:35` | `g_calib_mode`, `g_quality_meta` — 정적 초기화 외에 되돌리는 코드 없음 |
| `SPEC-XPE-P1A/spec.md:388` REQ-P1A-020 | `shutdown` 뒤 처리 함수가 `XPE_ERR_NOT_INITIALIZED` 를 낸다는 것만 규정 |

검색 범위: `preprocess_api.h`, `.moai/specs`, `docs`, `modules/preprocess` (`.md .h .cpp`, `build/` 제외), 패턴 `mode.*(persist|survive|retain|across).*shutdown|shutdown.*(mode|quality meta)`. **모드·품질 메타 보존이 의도라는 서술은 0건**(걸린 2줄은 AI 모듈 문서).

따라서 사실은 둘이다:
1. 모드·품질 메타를 남기는 것은 **서술된 의도가 없는 현재 동작**이다.
2. **헤더의 "no-op" 과 코드가 어긋난다** — 미초기화 상태에서도 `g_calib` 이 지워진다.

카드 지시대로 둘 다 바꾸지 않았다.

### C7 — CI 직접 실행

```
명령:   build/ci-preprocess/bin/xpe_preprocess_tests.exe
종료:   DIRECT_EXIT=0
벽시계: 40.6 s
gtest:  [==========] 578 tests from 79 test suites ran. (41388 ms total)   ← 같은 구성의 다른 실행
```

시드 고정 셔플을 함께 돌리려면 `--gtest_shuffle --gtest_random_seed=<N>` 을 붙인다. 셔플 실행의 소요 시간은 재지 않았다. **CI 러너에서는 재지 않았다** — 이 기계와 CI 의 속도 차이는 과거 측정(1.6~1.9배)이 있지만 이 바이너리에 적용한 것은 아니다.

### C8 — 전체

```
PRE_BUILD_EXIT=0   warnings=0
PRE_CTEST_EXIT=0   100% tests passed, 0 tests failed out of 647
COM_BUILD_EXIT=0   warnings=0
COM_CTEST_EXIT=0   100% tests passed, 0 tests failed out of 69
```

빌드 로그가 23 바이트(`ninja: no work to do.`)인 것은 원복 직후 E0 단계에서 이미 재빌드했기 때문이다(`BUILD_EXIT_E0=0`).

### C9 — 무효로 버린 실행 세 번

| 무엇 | 어떻게 드러났나 | 처리 |
|---|---|---|
| 배치 인자에 `=` 가 있어 cmd 가 인자를 쪼갬 | 시험 로그 파일이 **없음** | 명령을 배치 안에 직접 쓰는 방식으로 바꿈 |
| `printf` 가 경로의 `\x` 를 이스케이프로 읽어 배치가 깨짐 | 로그 **없음**, 그런데 `rc_EXIT=0` 이 찍힘 | heredoc 으로 생성 |
| 링크 실패(C3) 뒤 16회 실행 | 종료 `9009`, 로그 132 바이트, **요약 줄 없음** — `fails=[]` 로만 보면 전부 통과로 읽힘 | 전부 버리고 재빌드 후 재실행 |

그리고 반증 7건의 첫 판독이 전부 `INVALID(no summary)` 였다 — 제 스크립트에서 `run_bin` 이 전역 변수 `tag` 를 덮어써 요약 함수가 **없는 파일**(`a89-E1-run-run.log`)을 읽었다. 실제 로그(`a89-E1-run.log` 등, 각 98~99 KB, 요약 줄 1개)를 직접 판독한 것이 C5 표다. 실험 자체는 되돌림 → 재빌드 → 실행 순서로 차례대로 돌았으므로 로그는 유효하다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 수정 전 실패율(16 중 12)은 QA-A-88 의 같은 시드 목록 결과다.
- 원인 측정(C1)은 `rc` 메시지만 추가한 바이너리로, 수정 전에 실행했다.
- 수정 후·반증·전체 ctest 는 모두 `7c65ebd` + 이 카드의 변경으로 빌드한 바이너리다.
- 로그: `a89-rc.log`, `a89-fix2-default.log`, `a89-fix2-S1~S16.log`, `a89-E1~E7-run.log`, `a89-pre/com-*.log`, `a89-ci-direct.log` — 전부 스크래치패드, 요약 줄 존재 확인.

---

## 4. 미검증 (Gaps)

- **CI 러너에서 직접 실행 시간을 재지 않았다.**
- **셔플은 여전히 시드 16개뿐이다.** 드문 순서에서만 나오는 의존은 남아 있을 수 있다.
- **`EXPECT_EXIT` 자식이 정말 새 프로세스인지**는 E1(되돌리면 기본 순서에서 실패, 되돌리지 않으면 통과)로 간접 확인했다. 자식 프로세스 생성을 직접 관찰하지는 않았다.
- **(가)의 다른 오염원 후보**(`CalibQualityMetaTest`, `GainPolyLoadTest`)는 이번 수정이 의존 쪽이라 개별 확인이 필요 없어졌지만, 여전히 전역을 채우고 되돌리지 않는다.
- **다른 전역**(알림 큐, 캐시 최대 크기 `xpe_calib_cache_set_max_size` 등)은 전수하지 않았다. 캐시 크기는 캐시 시험이 바꾸고 `TearDown` 에서 되돌리지 않는다 — 이번 셔플에서 드러나지 않았을 뿐이다.
- **`ci-common`·`ci-post` 바이너리의 순서 의존**은 보지 않았다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **헤더와 코드가 어긋난 `shutdown`** — 누가 코드를 헤더("no-op")에 맞추면, `init` 없이 `shutdown` 만 부르는 다른 시험이나 호출자가 조용히 정리를 잃는다. 이번 수정은 그 경우에도 동작하도록 `init` 을 먼저 부른다.
- **자식 프로세스 단언은 느리고 디버깅이 번거롭다.** 실패하면 부모 쪽 메시지에 자식의 stderr(필드 값)가 나오도록 했지만, 디버거로 따라가기는 어렵다.
- **ctest 의 프로세스 분리는 계속 누수를 숨긴다.** CI 에 직접 실행이 들어가기 전까지는 같은 형태가 다시 생겨도 CI 는 모른다.
- **`f355208` 은 미푸시다.**

---

Refs #176
