# QA Gate — enhance_advanced

- module: enhance_advanced
- build: build/ci-post (Ninja, RelWithDebInfo, /WX=ON)
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | PASS | xpe_* 의존은 `xpe_common.dll` 단 하나. 나머지는 spdlog(3rd-party) + MSVC/UCRT/KERNEL32 |
| 2 | GTest 100% GREEN | PASS | 66/66 (`100% tests passed, 0 tests failed out of 66`) |
| 3 | 메모리 누수 1000 프레임 | GAP | 1000-cycle endurance 테스트(`IntegrationTest.T605_MemoryLeakEndurance`)는 존재·GREEN이나 **힙 사용량을 측정하지 않음** — crash/handle-leak 내구성만 검증 |
| 4 | /WX 0 warning | PASS | `_build_post.log` 전체 warning 라인 0건 (grep -ci warning → 0), 89/89 스텝 완료, 모듈 소스 6개 신규 컴파일됨 |
| 5 | P/Invoke ABI 심볼 수 일치 | PASS (MATCH) | export 7개 == 헤더 XPE_API 선언 7개 == `docs/project/sdd_adv.md` 문서 7개. set diff → IDENTICAL |
| 6 | CODEOWNERS 경계 | PASS (vacuous) | `CODEOWNERS:12` → `/modules/enhance_advanced/ @holee9`. `git diff main...HEAD` 는 **공집합** — HEAD가 main의 조상이라 이 브랜치 고유 커밋이 0개 |

## Evidence (verbatim)

### 1. Cross-module dependency

```
$ dumpbin /dependents build\ci-post\bin\xpe_enhance_advanced.dll

Dump of file build\ci-post\bin\xpe_enhance_advanced.dll

File Type: DLL

  Image has the following dependencies:

    xpe_common.dll
    spdlog.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    api-ms-win-crt-convert-l1-1-0.dll
    api-ms-win-crt-locale-l1-1-0.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-string-l1-1-0.dll
    KERNEL32.dll
```

판정: `xpe_*` 접두 의존은 `xpe_common.dll` 하나뿐. `xpe_preprocess` / `xpe_enhance_basic` / `xpe_display` / `xpe_dicom` / `xpe_gsvg` / `xpe_ai` 중 어느 것도 링크되지 않음. → PASS

### 2. Google Test 100% GREEN

테스트 집합 확정 절차 (모듈 귀속을 추정하지 않고 빌드 산출물에서 도출):

```
$ grep -o 'add_test(\[=\[[^]]*\]=\]' \
    'build/ci-post/modules/enhance_advanced/test_xpe_enhance_advanced[1]_tests.cmake' \
  | sed 's/add_test(\[=\[//; s/\]=\]//' | sed 's/\..*//' | sort | uniq -c
     12 CollimationDetectTest
     11 EdgeEnhancementTest
      3 EnhanceAdvancedApiHeaderTest
      8 EnhanceAdvancedLifecycleTest
      9 ExposureIndexTest
     10 IntegrationTest
     13 MfpScalarTest
   (합계 66)
```

실행:

```
$ ctest --test-dir build\ci-post ^
    -R "^(CollimationDetectTest|EdgeEnhancementTest|EnhanceAdvancedApiHeaderTest|EnhanceAdvancedLifecycleTest|ExposureIndexTest|IntegrationTest|MfpScalarTest)\." ^
    --output-on-failure

100% tests passed, 0 tests failed out of 66

Total Test time (real) =   4.10 sec
```

전체 로그: `tests.log` (동일 디렉터리)

주의(정규식 충돌 배제): `ExposureIndexTest`는 enhance_basic의 `ExposureIndex`와, `IntegrationTest`는 `EnhanceIntegration` / `DisplayIntegration`과 다른 스위트. `^...\.` 앵커로 분리 확인됨.

### 3. Memory leak, 1000 frames

테스트 존재 확인:

```
$ grep -n "T605" -A 45 modules/enhance_advanced/tests/test_integration.cpp
348:TEST(IntegrationTest, T605_MemoryLeakEndurance) {
352-    const int CYCLES = 1000;
353-    const int IMG_SIZE = 128;  // Smaller size for endurance test
355-    // Note: Actual memory leak detection requires platform-specific tools:
356-    // - Linux: Valgrind --leak-check=full
357-    // - Windows: CRT debug heap (_CrtDumpMemoryLeaks)
358-    // - ASan: compile with -fsanitize=address
```

루프 종료부:

```
        // Optional: Check memory every 100 cycles
        if ((cycle + 1) % 100 == 0) {
            // Log checkpoint (no actual memory measurement here)
        }
    }

    xpe_enhance_advanced_shutdown();

    // If we reach here without crash, endurance test passed
    SUCCEED() << "Completed " << CYCLES << " cycles without crash";
```

실행 결과 (tests.log): `IntegrationTest.T605_MemoryLeakEndurance ... Passed`

판정 근거: 1000 사이클 동안 4개 API(`xpe_multiscale_process` / `xpe_fractional_process` / `xpe_detect_collimation` / `xpe_calc_exposure_index`)를 전부 호출하고 crash 없이 완주하는 것은 관측되었으나, **힙 사용량 델타를 단 한 번도 측정하지 않는다** (주석이 직접 그렇게 명시). 따라서 "메모리 누수 없음"은 이 테스트로 주장할 수 없음 → **GAP**. (지시에 따라 신규 테스트 작성하지 않음.)

### 4. /WX 0 warning

```
$ grep -i "XPE_WARNINGS_AS_ERRORS\|CMAKE_BUILD_TYPE" build/ci-post/CMakeCache.txt
CMAKE_BUILD_TYPE:STRING=RelWithDebInfo
XPE_WARNINGS_AS_ERRORS:BOOL=ON

$ grep -m1 -o '/WX' build/ci-post/build.ninja
/WX

$ grep -ci "warning" .moai/reports/lane-post/QA-B-01/_build_post.log
0

$ tail -4 .moai/reports/lane-post/QA-B-01/_build_post.log
[86/89] Linking CXX executable bin\test_e2e_post_pipeline.exe
[87/89] Linking CXX executable bin\gsvg_tests.exe
[88/89] Linking CXX executable bin\test_xpe_enhance_advanced.exe
[89/89] Linking CXX executable bin\test_display_voi_lut.exe
```

no-op 증분 빌드가 아님을 확인 (모듈 소스가 실제로 컴파일됨):

```
$ grep -i "enhance_advanced" .moai/reports/lane-post/QA-B-01/_build_post.log | head -5
[12/89] Building CXX object ...\xpe_enhance_advanced.dir\src\multiscale_process.cpp.obj
[14/89] Building CXX object ...\xpe_enhance_advanced.dir\src\fractional_process.cpp.obj
[16/89] Building CXX object ...\xpe_enhance_advanced.dir\src\exposure_index.cpp.obj
[26/89] Building CXX object ...\xpe_enhance_advanced.dir\src\collimation_detect.cpp.obj
[28/89] Building CXX object ...\xpe_enhance_advanced.dir\src\detail\edge_detection.cpp.obj
```

전체 로그 19개 라인이 enhance_advanced 관련. warning 0건 → PASS.

### 5. P/Invoke ABI symbol count

DLL exports (7):

```
$ dumpbin /exports build\ci-post\bin\xpe_enhance_advanced.dll
          1    0 00001D7F xpe_calc_exposure_index
          2    1 00002B17 xpe_detect_collimation
          3    2 00002B08 xpe_enhance_advanced_init
          4    3 000012E4 xpe_enhance_advanced_shutdown
          5    4 00002B44 xpe_enhance_advanced_version
          6    5 0000193D xpe_fractional_process
          7    6 000010D2 xpe_multiscale_process
```

헤더 선언 (7) — `modules/enhance_advanced/include/xpe/enhance_advanced/xpe_enhance_advanced_api.h`, `XPE_API` 7건:

```
xpe_calc_exposure_index  xpe_detect_collimation  xpe_enhance_advanced_init
xpe_enhance_advanced_shutdown  xpe_enhance_advanced_version
xpe_fractional_process  xpe_multiscale_process
```

문서 (`docs/project/sdd_adv.md`) 대조:

```
$ diff <exports> <sdd_adv.md 추출>
IDENTICAL (7/7)
```

판정: 7 == 7 == 7, delta 0 → **MATCH / PASS**

관찰(비차단): include 디렉터리에 헤더가 2개 존재 — `xpe_enhance_advanced_api.h`(실제 API 7건 선언)와 `enhance_advanced_api.h`(`XPE_API` 1건, `xpe_enhance_advanced_version`만). 후자는 중복/레거시 후보로 보이나 이번 게이트 항목의 판정에는 영향 없음(export 집합과 문서 집합이 일치).

### 6. CODEOWNERS boundary

```
$ grep -nE "enhance_advanced" CODEOWNERS
12:/modules/enhance_advanced/ @holee9
```

```
$ git diff --stat main...HEAD -- modules/enhance_advanced
(출력 없음 — 변경 0건)

$ git rev-list --count --left-right main...HEAD
2	0

$ git merge-base --is-ancestor HEAD main; echo $?
0   → HEAD는 main의 조상
```

판정: 소유권 엔트리는 `CODEOWNERS:12`에 정확히 존재. 다만 `main...HEAD` 차이가 **공집합**인 이유는 "자기 경로만 건드렸기 때문"이 아니라 **dev/postprocess(e67c125)가 main(28ec75f)의 조상이라 고유 커밋이 0개**이기 때문. 즉 경계 위반이 구조적으로 불가능한 상태이므로 PASS이되, 경계 준수를 적극적으로 입증한 것은 아님(vacuous PASS).

## Gaps (미검증)

1. **메모리 누수 실측 (항목 3)** — 1000-cycle endurance는 GREEN이나 힙 델타 측정이 코드상 존재하지 않음(`// Log checkpoint (no actual memory measurement here)`). `_CrtDumpMemoryLeaks` / ASan / Valgrind 어느 것도 이 빌드에 연결되어 있지 않음. 따라서 "누수 없음"은 미검증.
2. **CODEOWNERS 경계의 적극적 입증 (항목 6)** — 브랜치 고유 커밋 0개로 인해 diff가 비어 있어, "타 lane 경로 미침범"을 실제 변경으로 검증하지 못함.
3. **RelWithDebInfo 외 구성** — Debug / 다른 아키텍처 빌드는 측정하지 않음.
4. **런타임 ABI 호환성** — export 심볼 *이름/개수*만 대조했고, 구조체 레이아웃·호출 규약(Pack=8, `__cdecl`)의 실제 P/Invoke 왕복은 이 게이트에서 직접 실행하지 않음. (다만 `EnhanceIntegration.StructSizes_PInvokeCompatible`가 enhance_basic 스위트에 존재하며 GREEN — enhance_advanced 스위트에는 동등 테스트가 보이지 않음.)
5. **테스트 소스 중복** — `tests/enhance_advanced_tests/`와 `modules/enhance_advanced/tests/`에 동일 이름 파일이 병존. 빌드에 실제로 사용된 것은 `modules/enhance_advanced/tests/`(CTestTestfile 경로로 확인). `tests/enhance_advanced_tests/` 쪽은 빌드/실행되지 않았으며 두 사본의 내용 동일성은 대조하지 않음.

## Residual risk

- 관측된 66/66 GREEN은 **단일 실행 1회** 결과. `IntegrationTest.T604_ThreadSafety`, `MfpScalarTest` 등 동시성/부동소수 경로의 flakiness는 반복 실행으로 확인하지 않음.
- 성능 예산 테스트(`T608_PerformanceBudgetVerification`, `T308_PerformanceBudget`, `CollimationDetectTest.LargeImagePerformance`)는 측정 시점의 머신 부하에 민감. GREEN이지만 로드된 환경에서는 결과가 달라질 수 있음.
- 항목 3의 endurance 테스트가 계속 GREEN이더라도 실제 누수는 조용히 누적될 수 있음 — 이 게이트로는 탐지되지 않는 실패 형태.
- 항목 6의 vacuous PASS는 브랜치가 main과 동기화되어 재개될 때 무효화됨. dev/postprocess에 신규 커밋이 쌓이면 재측정 필요.
- 미사용으로 보이는 중복 헤더(`enhance_advanced_api.h`)와 중복 테스트 트리는 향후 어느 쪽이 정본인지 혼동을 유발할 수 있음(이번 측정에서는 수정하지 않음).
