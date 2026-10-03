# QA-A-231 — SPEC-BENCH-PRE(BP-01~05)와 SPEC-XPE-P0(공용 모듈) 실태 대조

보고서만 작성했다. 제품 코드·SPEC·RTM·헤더는 고치지 않았다. 수정안은 `fixes/*.txt` 초안으로, 결함 후보는 §5 목록으로 둔다.
관측 기준: 로컬 트리 `dev/preprocess` (HEAD c7db5639), CI 는 `main` 의 최근 실행 두 건(아래 표기).

## 0. 결론

1. **BP-01~05 의 벤치마크는 어디서도 돌지 않는다.** manifest 가 정의한 판정(온도 6점 × PREP 4점 잔류 dark, 9단계 다중 게인 선형성 R²≥0.9999, heel 효과 RMSE, 결함 NMSE)을 단언하는 시험은 `modules/preprocess` 에 없고, 데이터셋·러너 디렉터리도 디스크에 없다. 벤치마크 워크플로(`benchmark-regression.yml`)는 `ci-post` 로 BP-06~09 만 돈다. `ci.yml` 이 돌리는 것은 이름만 `BP01`~`BP05` 인 `PreprocessDegraded.*` 6개뿐이며, 그 내용은 manifest 의 BP 번호와 대응하지 않는다.
2. **"동결"이 동결한 것은 문서 버전(1.1.0)과 그 6개 스모크 시험의 이름이다.** 데이터셋 디렉터리, `manifest.json` 해시, `tools/benchmark/run_bp0N.py`, `test_results.json` 은 디스크에 없고, manifest §8 동결 체크리스트 6항목은 전부 미체크다. 이슈 #54 의 제목도 "BP-01~05 Benchmark 동결 — Preprocess DegradedMode GTest" 로, 동결의 범위가 처음부터 스모크 시험이었음을 보여 준다.
3. **SPEC-XPE-P0 는 구현과 시험이 문서를 앞서 있다.** `xpe_common.dll` 은 16개를 내보내고(헤더 16 = 신선한 DLL 16), `modules/common/tests` 의 81개 시험이 CI 에서 81/81 통과한다. 반대로 문서는 따라오지 못했다: 내보내기 개수를 15·16·18·20 네 가지로 적고, REQ-P0-026~028a 는 본문이 없고, `tasks.md` 는 7개 작업이 전부 `pending`, 그리고 `RTM-COMMON-001` 은 52행이 전부 ✓ 인데 인용한 시험 이름 52개 중 존재하는 것이 0개다.
4. 리더가 물은 metadata 문장(GUI-C-213): `preprocess_api.h` 에 **"읽는다"고 남은 문장이 4곳** 있다 (§4).

## 1. SPEC-BENCH-PRE

### 1.1 CI 가 실제로 돌리는 것 (관측)

| 워크플로·잡 | 명령 | 실행 (관측) | 전처리 시험 |
|---|---|---|---|
| `ci.yml` `preprocess-tests` (windows-2025) | `cmake --preset ci-preprocess` 빌드 후 `ctest --test-dir build/ci-preprocess --build-config RelWithDebInfo --output-on-failure`, 이어서 `xpe_preprocess_tests.exe` 를 기본 순서 + 섞기 시드 1·2·9 로 단일 프로세스 실행 | main `f1f3e54d`(run 37090327999, job 111109146811): ctest `100% tests passed out of 1119`, 단일 프로세스 4회 모두 `ran=968 PASSED 960` | 전부 |
| `benchmark-regression.yml` `build-full-pipeline` (windows-2025) | `cmake --preset ci-post`, `ctest ... -V -R <BenchmarkFreeze·BP07~09·FullPipelineE2E 정규식>` | main `483e8fd2`(run 37095895166, job 111125631804): 20개 실행, `100% tests passed out of 20` | **0건** (이름이 `PreprocessDegraded`·`xpe_preprocess`·`BP0[1-5]_` 인 줄 0개, `evidence/bench_run_37095895166_tests.txt`) |

`ci-post` 프리셋은 `BUILD_PREPROCESS=OFF` 다 (`CMakePresets.json` 에서 확인). 워크플로 이름과 주석도 "BP-06~09" 라고 스스로 적는다.

기계: 두 잡 모두 GitHub 호스티드 러너. `preprocess-tests` 로그의 `[perf-gate-machine] cpu="AMD EPYC 9V45 96-Core Processor" logical=4 avx2=1 bandwidth=43.0 GB/s`. 로컬은 i7-12700.

### 1.2 BP 별 대조

"출력이 로그에서 보이는가" 는 `ctest --output-on-failure` 가 통과한 시험의 출력을 버리기 때문에, 통과 시험은 `Passed 0.01 sec` 한 줄만 남는다. `PreprocessDegraded` 6개도 그 한 줄이 전부다 (`evidence/ci_37090327999_preprocess_degraded_parity_ctest.txt`).

| BP | manifest 가 정의한 것과 판정선 | 이름이 같은 시험 (실제로 단언하는 것) | 돌리는 곳 | 로그에서 보이는 것 | 기계 |
|---|---|---|---|---|---|
| BP-01 | 온도 15~40 °C 6점 × PREP 0.5~10 s 4점, 보정 후 잔류 dark 평균 <2 ADU, σ <3 ADU | `PreprocessDegraded.BP01_OffsetNullCalibrationReturnsNotInitialized`: 교정을 안 적재하면 `XPE_ERR_NOT_INITIALIZED`, 64×64 호출 100 ms 미만 | `ci.yml` ctest | `Passed 0.01 sec` | AMD EPYC 9V45 4 vCPU |
| BP-02 | 노출 9단계 선형성 R²≥0.9999, 평탄도 σ/mean <0.5 %, 단조성 위반 0 | `BP02_GainIdentityPreservesInputStatistics`: 교정이 없으면 `NOT_INITIALIZED` + 입력 불변 + 100 ms. 이름의 "identity 가 통계를 보존" 은 단언하지 않는다 (`if (rc == XPE_OK)` 가지는 직전 단언 때문에 도달하지 못한다) | 동일 | 동일 | 동일 |
| BP-03 | heel 효과 RMSE 80 % 이상 감소, 1500 mm SID 균일도 <1 % | `BP03_DefectEmptyListIsNoOp`: **번호가 다른 기능**(결함 보정). 같은 `NOT_INITIALIZED` 단언 | 동일 | 동일 | 동일 |
| BP-04 | 결함 보정 NMSE 10배 개선, 3×3 군집 NMSE <150, TPR ≥99.9 %, FPR <0.001 % | `BP04_GhostZeroLagCoefficientIsIdentity`: **번호가 다른 기능**(고스트) | 동일 | 동일 | 동일 |
| BP-05 | lag 이력 (P1B 소관, Pre 는 입력 계약만) | `BP05_TempCompensateReferenceIsIdentity`, `BP05_NonlinearityNullConfigIsIdentity`: **번호가 다른 기능**(온도·비선형) | 동일 | 동일 | 동일 |
| BP-SIMD | 6연산 × 3형상 × 100입력 + 에지 30 = 1830건, 전부 0건 실패 | `SimdParityTest` 이름의 시험 **없음**. `*Parity*` 이름 시험 40개가 ctest 로 돈다 (`OffsetCorrectAVX2ParityTest`, `GainCorrectAVX2ParityTest`, `Avx2ParityTest`, `Median8ParityTest` 등) | `ci.yml` ctest (별도 게이트 없음) | `Passed` 줄 | 동일 |

- 이름이 `BP0N` 인 시험의 번호는 manifest 의 번호와 어긋난다 (DEG 의 BP-03 은 결함, BP-04 는 고스트, BP-05 는 온도·비선형. manifest 는 BP-03 heel, BP-04 결함, BP-05 lag). 시험 파일 머리 주석의 대응표가 이를 그대로 적고 있어, 어긋남은 한 번도 문서에 반영되지 않았다.
- 판정선을 단언하는 시험이 있는지 코드 전체에서 찾은 결과 (`evidence/bench_pre_declared_artifacts.txt`, 대조군 `xpe_defect_correct` 은 같은 방법으로 23개 파일을 찾는다): `heel`/`Duo-SID` 0개, `NMSE` 0개, `SimdParityTest` 0개. `R²≥0.9999`·`linearity` 를 단언하는 시험은 없다 (`fit_r_squared` 는 교정 파일의 메타 필드일 뿐이다).
- BP-04 의 TPR·FPR 판정선에 가장 가까운 시험은 있다: `RuntimeDetectionRatesTest.TprReachesTheFloorAtTenSigma`·`FprOnCleanFramesMeetsTheRequirement`(REQ-P1A-013, 10σ 과도 결함 TPR ≥99.9 %, 깨끗한 프레임 FPR <0.001 %)와 `DefectOracle.RecallFprAndGoodPixelDeltaMeetTheCanonicalLines`(SRS-CALIB-FUNC-019, 재현율 100 %, FPR <0.001 %). 두 파일 모두 `modules/preprocess/CMakeLists.txt` 에 등록돼 있고 그 시험들이 `ci.yml` preprocess-tests 의 ctest 목록에 1줄씩 나온다. 다만 BP-04 의 NMSE 두 줄과 군집 5×5 줄은 대응 시험이 없다.
- BP-01 과 BP-03 은 **대응하는 기능이 구현에 없다.** `preprocess_api.h` 가 온도 보간·PREP 시간 모델·kVp/SID 보간을 "NOT IMPLEMENTED" 로 적고(229 M2b 에서 철회), 시험 `OffsetCorrect_MetadataDoesNotChangeTheCorrection`, `KvpAndSidDoNotChangeTheCorrection` 이 "메타데이터는 결과를 바꾸지 않는다" 를 못 박는다. 측정할 알고리즘이 없으므로 BP-01·BP-03 의 판정선은 지금 어떤 시험으로도 만족시킬 수 없다.

### 1.3 SPEC-BENCH-PRE 요구별 판정

| 요구 | 표의 상태 | 실제 | 근거 |
|---|---|---|---|
| REQ-BPRE-001 DegradedMode 6/6 PASS | ✅ 6/6 PASS, 근거 `test_preprocess_degraded.cpp`, "CI archive" | 6개는 맞다. 그러나 "BP-01~05 + BP-SIMD" 6개가 아니라 BP05 가 둘이고 BP-SIMD 시험은 없다. "CI archive"(`test_results.json`)는 디스크에 없다 | §1.2, `bench_pre_declared_artifacts.txt` |
| REQ-BPRE-002~005 | DegradedMode PASS ✅, 전체 데이터셋은 M2 대기 | DegradedMode 시험은 판정선(2 ADU·R²·heel·NMSE)을 단언하지 않는다. "PASS" 는 판정선과 무관한 스모크의 통과다 | §1.2 |
| REQ-BPRE-006 SIMD 동등성 1830/1830 | 부분 구현, 게이트 `ctest -R Parity` | `-R Parity` 는 40개 시험을 고르고 모두 통과하지만 1830 이라는 수는 어디에도 없다 (`1830`·`1800` 문자열은 시험에서 SID 값 1800 mm 와 무관한 수 두 곳에만 나온다) | 로그, grep |
| REQ-BPRE-007 PR 마다 BP-01~05 를 돌려 회귀를 막는다 | "CI workflow exists" | 워크플로는 있으나 BP-06~09 용이다. BP-01~05 를 막는 게이트는 없다 | §1.1 |
| §4 동결 표 | BP-01~04 `DegradedMode ✅ / Full Dataset Pending` | 데이터셋 경로 `benchmark/datasets`, 러너 `tools/benchmark/run_bp0N.py` 둘 다 없다. 시험 파일 `test_simd_parity.cpp` 도 없다 | `bench_pre_declared_artifacts.txt` |

### 1.4 "동결"이 동결하는 것

- 동결되는 것: manifest 문서의 버전 문자열, 그리고 `PreprocessDegraded` 6개 시험의 이름(이름이 바뀌면 SPEC 의 인용이 끊긴다 — 단, `check_spec_test_refs.py` 는 `.moai/specs/*/*.md` 만 보므로 manifest 의 `benchmark/` 문서는 그 점검 밖이다).
- 동결되지 않는 것: 판정선 값(문서에만 있음), 데이터, 해시, 러너, 회귀 게이트, 결과 기록.
- 그래서 "Active — Frozen" 이 말하는 회귀 방지는 현재 작동하지 않는다. BP-01~05 의 어떤 판정선도 어긋나도 CI 는 빨개지지 않는다.

이슈 이력: #54(닫힘)는 DegradedMode GTest 로 동결을 닫았다. 이 보고서가 지목하는 공백(판정선 시험·데이터·게이트 부재)을 추적하는 열린 항목은 찾지 못했다.

## 2. SPEC-XPE-P0 (modules/common)

### 2.1 작업(T-001~T-007)

`tasks.md` 의 상태 열은 7개 모두 `pending`, 수락 기준 체크박스 30개는 전부 비어 있다 (`evidence/p0_status_markers.txt`). 반면 `spec.md` 는 "Completed — All deliverables implemented", `progress.md` 는 "11/11 (100%)". 같은 SPEC 의 세 문서가 서로 다른 상태를 적는다.

| 작업 | 표 상태 | 실제 | 근거 | 단언하지 않는 문구 |
|---|---|---|---|---|
| T-001 모듈 5개 스캐폴딩 | pending | 디렉터리 5개 모두 존재, 각 `CMakeLists.txt` 있음. 버전 함수는 enhance_advanced·ai·display·gsvg 에 있고 **dicom 에 없다** | `xpe_dicom_version` grep 0건, `xpe_gsvg_version` 은 `modules/gsvg` 에 있음 | AC "`cmake --preset release` 가 8개 모듈을 모두 빌드" — `release` 프리셋을 빌드하는 CI 잡이 없다. `ci.yml` 이 쓰는 프리셋은 `ci-common`·`ci-preprocess`·`ci-post`·`ci-ai` 뿐이고 8개를 한 번에 설정하는 `ci-fullstack` 은 어떤 워크플로도 쓰지 않는다 |
| T-002 시험 기반 통합 | pending | CTest 통합 완료. 계획한 `tests/common`·`tests/common_smoke` 는 2026-09-10 `bf6348f7` 에서 삭제(고유 단언 2건 이식), 시험은 `modules/common/tests` 에 있다. 커버리지는 §2.2 REQ-P0-006 | CI common-build 81/81 | AC "`ctest --preset default`" — 테스트 프리셋 `default` 는 있으나 CI 가 실행하는 형태는 `ctest --test-dir build/ci-common` 이다. AC 의 `tests/common/` 구조는 삭제됨 |
| T-003 C++17 통일 | pending | 완료. `modules/common/CMakeLists.txt` 에서 C++23 줄은 주석으로 남아 있고 `CXX_STANDARD 17`. 모든 모듈이 `cxx_std_17` | grep | 없음 |
| T-004 내보내기 검증 | pending | 16개 = 16개. 이 작업이 못 박은 "정확히 18개" 가 틀린 수다 | 신선한 DLL(2026-10-03 08:09) 내보내기표 16, 헤더 `XPE_API` 16, 불일치 없음 (`p0_exports_fresh_dll.txt`) | AC "`dumpbin /exports` 가 18개" — 이 기계에는 dumpbin 이 없어 PE 내보내기표를 직접 읽었다. CI 에는 내보내기를 세는 단계가 없다 |
| T-005 Pack=8 static_assert | pending | 완료. `#pragma pack(push, 8)`, `static_assert` 8개 (`XpeImageBuffer`=40, `XpeImageMetadata`=96, `XpePixelFormat`=4, 오프셋 5개) | `xpe_types.h` | AC "모든 구조체 검증" — 구조체는 둘뿐이고 둘 다 검증됨 |
| T-006 C# WPF | pending | `clients/ImageProcTest` 존재, `net8.0-windows`. 16개 내보내기 이름 모두 C# 소스에 있음, `DllName = "xpe_common.dll"`, `Pack = 8` 2곳. `StructLayoutParityTests`·`AbiLayoutTests` 가 구조체 크기를 `Marshal.SizeOf` 로 확인. `RealXpeCommonBackend.cs` 가 `xpe_init(null)`·`xpe_version()`·`xpe_shutdown()` 호출 | grep. CI `dotnet-tests` 잡 success (run 37090327999) — 잡의 내용은 열어 보지 않았다 | AC "18개 함수 선언" — 실제 16 |
| T-007 CI | pending | `ci.yml` 있음, 푸시·PR 마다 configure·build·test. 시험 결과는 `Testing/` 아티팩트로 올림. **커버리지 단계는 푸시에서 돌지 않는다** (`coverage` 잡은 `if: github.event_name == 'workflow_dispatch'`) | `ci.yml` | AC "Coverage 단계, 커버리지 보고서 아티팩트, 85 % 게이트" — 수동 실행에서만 |

### 2.2 요구 (REQ-P0-001~033, 028a)

"본문 상태" 는 `spec.md` §7 deliverable 열(전부 ✅ DONE)을 가리킨다.

| 요구 | 실제 | 근거 | 단언하지 않는 문구 |
|---|---|---|---|
| 001 단일 루트에서 8개 타깃 | 루트 `CMakeLists.txt` 가 모듈을 `if(EXISTS …/CMakeLists.txt) add_subdirectory` 로 추가. 한 번에 8개를 설정하는 CI 구성 없음 | `ci.yml` 프리셋 | "all module targets … from a single root" 를 확인하는 단계 없음 |
| 002 프리셋 4개 (Debug·Release·CI·ci-common) | 프리셋은 11개: `default`(Debug)·`release`·`ci`·`ci-common`·`ci-post`·`ci-ai`·`ci-preprocess`·`coverage`·`coverage-post`·`coverage-dicom`·`ci-fullstack`. 이름 `Debug` 는 없다 | `CMakePresets.json` | §5.3 "4개 프리셋 모두 깨끗이 컴파일" — `default`·`release`·`ci` 를 빌드하는 잡이 없다 |
| 003 vcpkg 최소 의존성 (… opencv4, eigen3 …) | `vcpkg.json` 은 spdlog·nlohmann-json·fmt·gtest·dcmtk·openjpeg. **opencv4·eigen3 없음.** OpenCV 는 빌드 파일 어디에도 없고, Eigen 은 루트가 `find_package` 후 `FetchContent` 로 대체 | `vcpkg.json`, 루트 `CMakeLists.txt` | 명시한 의존성 목록과 실제가 다르다 |
| 004 선택적 서브디렉터리 | 구현됨 (루트 `if(EXISTS …)`) | 루트 `CMakeLists.txt` | 디렉터리를 지워 보는 시험 없음 |
| 005 gtest + CTest | 충족. `gtest_discover_tests`, CI 81/81 | `common_build_ci_run_37090327999.txt` | — |
| 006 커버리지 ≥85 % (gcov/lcov) | 도구는 gcov/lcov 가 아니라 OpenCppCoverage(Cobertura), 문턱 `XPE_COVERAGE_MIN 0.85`. 마지막 수동 실행(2026-09-12, run 34662146043)에서 `coverage` 프리셋 line-rate 0.8861908265881776 ≥0.85 통과. 이 값은 xpe_common 단독이 아니라 프리셋이 묶은 DLL 전체의 합산이다 | `coverage_dispatch_run_34662146043.txt` | "for xpe_common.dll" 단독 수치는 관측하지 못했다. 푸시마다 도는 게이트가 아니다 |
| 007 API 마다 정상·널·경계 시험 | 이름으로는 16개 중 대부분 충족. 단 `xpe_shutdown`·`xpe_log_flush` 시험이 확인하는 것은 충돌·예외가 없다는 것뿐이다 (`SUCCEED()`, `EXPECT_NO_THROW`) | `test_xpe_common.cpp` | 두 함수의 "정상 경로"에서 결과를 단언하지 않는다 |
| 008 정확히 16개 내보내기 | 충족 (§2.1 T-004). 다른 문서는 15·18·20 (§2.5) | `p0_export_count_statements.txt` | CI 에 세는 단계 없음, 시험은 선언된 주소를 링크로만 확인 |
| 009 Pack=8, 구조체 `XpeImageBuffer`·`XpeImageMetadata`, 열거형 `XpePixelFormat`·`XpeAlertSeverity`·`XpeErrorCode` | 구조체·두 열거형은 맞다. **`XpeErrorCode` 는 열거형이 아니라 `typedef int32_t` + `#define XPE_OK 0` 류 상수** | `xpe_error.h` | 문구 "Enum types: … XpeErrorCode" |
| 010 오류는 `XpeErrorCode`, C ABI 밖으로 예외 금지 | `xpe_error_string` 있음. 예외 유출은 `CommonOom` 12개 시험이 init·configure·로그 파일·알림 푸시 경로에서 할당 실패를 주입해 `XPE_ERR_OUT_OF_MEMORY` 반환으로 확인 | `common_build_ci_run_37090327999.txt` | `xpe_alloc_image` 등 나머지 경로의 예외 유출은 같은 방법으로 확인하지 못했다 |
| 011 `xpe_init` 이 `XPE_ERR_OK` 반환, 기본 로그 stderr·INFO | `XPE_ERR_OK` 는 없다 (`XPE_OK`). 시험 `InitReturnsOk` 는 반환값만 본다. 코드는 기록 수준의 기본값을 `g_currentLevel = 0 // Default: TRACE` 로 두고 `xpe_init` 은 수준을 설정하지 않는다 | `xpe_error.h`, `xpe_logging.cpp`, 시험 | 기본 수준 INFO 와 stderr 출력을 단언하는 시험이 없고, 코드의 기본값은 TRACE 라고 스스로 적는다 |
| 012 `xpe_shutdown` 이 코드 반환, init 없이 호출하면 `NOT_INITIALIZED` | **`void xpe_shutdown(void)`.** 시험 `ShutdownReturnsNotInitializedWhenNotInit` 는 이름과 달리 `SUCCEED()` 뿐이다 | `xpe_common_api.h`, 시험 | 문구 자체가 구현 불가 |
| 013 `xpe_version` "X.Y.Z" | 충족 (`VersionFormatSemanticVersioning`) | 시험 | — |
| 014 잘못된 JSON → `XPE_ERR_INVALID_PARAM` | 코드는 `XPE_ERR_CONFIG_INVALID`, 시험 `ConfigureWithInvalidJsonReturnsInvalid` 가 그 값을 못 박는다. `XPE_ERR_INVALID_PARAM` 은 존재하지 않는 이름 | 헤더, 시험 | 문구의 코드 이름 |
| 015 `xpe_alloc_image` 0 초기화 | 충족 (`AllocImageZeroInitializesMemory`) | 시험 | — |
| 016 `xpe_free_image` 이중 해제는 `INVALID_PARAM`, 크래시 없음 | **다르다.** 이미 해제된 버퍼(`data == nullptr`)에 다시 호출하면 `XPE_OK`. 시험 `FreeImageNullDataDoesNotCrash` 가 `XPE_OK` 를 단언한다 | 시험 | 문구의 반환값 |
| 017 `xpe_copy_image` | 충족 (`CopyImageSucceeds`, `CopyImageReproducesEveryByte`, 작은 버퍼 시험) | 시험 | — |
| 018 `xpe_error_string` | 충족 (`ErrorStringReturnsNonNullForAllCodes` 등) | 시험 | — |
| 019 `xpe_get_pending_alert_count` = 읽지 않은 알림 수 | 큐 길이를 돌려준다. 알림을 읽어도 줄지 않는다(§020) | 헤더 | "unread" |
| 020 `xpe_get_pending_alert` 가 **가장 오래된** 알림을 복사하고 큐에서 **제거** | **다르다.** 인덱스를 받고(`int32_t index`), 헤더가 "큐는 바뀌지 않는다, 비우려면 `xpe_clear_alerts`" 라고 적는다. 시험도 인덱스 0, 1 로 읽는다 | `xpe_error.h`, `AlertQueueFifoOrder` | 문구의 시그니처·제거 동작 |
| 021 `xpe_clear_alerts` | 충족 | 시험 | — |
| 022 `xpe_get_param_range` "지정한 파라미터 ID" | 시그니처가 `(bodyPart, paramName, …)`. 시험 4개 있음. 시험 파일의 구역 제목 "Concurrent Access Tests (REQ-P0-022 thread safety)" 는 요구 번호를 잘못 인용한다 | 헤더, 시험 | 문구의 "parameter ID" |
| 023 `xpe_log_set_level` 0~5, 임계 미만은 조용히 버림 | 수준 5 가 문서마다 다르다: 요구 `CRITICAL`, 소스 `to_spdlog_level` 는 5→`critical`, 헤더는 `5=OFF`, 시험 주석도 "OFF". **"임계 미만은 버린다" 를 확인하는 시험이 없다** (반환 코드만 본다). 이유의 일부: `xpe_common` 자신은 spdlog 로 아무것도 기록하지 않는다 (`src/*.cpp` 의 `spdlog::info/debug/…` 호출 0건) — 필터링은 다른 모듈이나 시험이 spdlog 를 직접 부를 때만 관측된다 | `xpe_logging.cpp`, `xpe_common_api.h`, grep | 필터링 동작 |
| 024 `xpe_log_set_file` 실패 시 `XPE_ERR_FILE_IO`, 이전 목적지 유지 | 코드는 `XPE_ERR_IO_FAILED`(`XPE_ERR_FILE_IO` 는 어디에도 없는 이름). 이전 목적지 유지는 `CommonOom.ALogFileThatCannotBeOpenedLeavesThePreviousLogger` 가 확인한다 (CI 통과). `test_xpe_common.cpp` 의 `LogSetFileWithInvalidPathReturnsError` 는 `XPE_OK` 도 허용한다. 헤더는 "NULL 을 주면 stderr 로 되돌린다" 인데 코드는 `filePath == nullptr` 일 때 **null sink** 로거를 설치한다 (`xpe_null_revert`) — 되돌린 뒤의 출력 목적지가 stderr 인지 확인하는 시험이 없다 | 시험, `xpe_logging.cpp` | 문구의 코드 이름, NULL→stderr |
| 025 `xpe_log_flush` | `xpe_log_flush` 호출 뒤 파일 내용을 읽는 시험은 없다 (`CommonOom` 은 spdlog 의 `flush()` 를 직접 쓰고 `xpe_log_flush` 호출은 0건) | grep | "force-flush" 의 효과 |
| 026·027·028·028a | **본문이 없다.** 변경 이력에 "REQ-P0-026~028 교정, 028a 추가" 라고 적혀 있고 `tasks.md` 표에는 id 가 있지만 `spec.md` 에는 2.7 절 자체가 없다. 코드·시험이 이 id 를 인용한 곳도 없다 | `p0_req_task_census.txt` | 요구 텍스트 전체 |
| 029 독립 실행형 .NET 실행 파일이 P/Invoke 로 로드 | 구성됨 (`net8.0-windows`, `DllName`). 실행은 CI `dotnet-tests` 가 맡는다 (내용 미관측) | 소스 | — |
| 030 래퍼가 15개 함수를 선언, `Pack=8` | 16개 이름 전부 선언됨. 문구의 수 15 가 틀리다 | grep | 수 |
| 031 시작 시 `xpe_init`, 버전 표시, 종료 시 `xpe_shutdown` | `RealXpeCommonBackend.cs` 가 세 함수를 호출한다. 화면에 표시하는지는 열어 보지 않았다 | grep | 표시 |
| 032 8개 모듈 디렉터리 각각 최소 공유 라이브러리 | 7개 이름만 나열(문구가 "8개" 라고 하면서 7개를 적는다). 모든 디렉터리에 `CMakeLists.txt` 있음 | 목록 | 수 |
| 033 모듈마다 placeholder 버전 함수 | 7개 중 6개. **dicom 없음** | grep | — |

### 2.3 작업 없는 요구

이름으로 센 경우 (`p0_req_task_census.txt`):
- 본문이 있는 요구 30개 중 `tasks.md` 의 작업 행에 이름이 있는 것은 11개(001·005·006·007·008·009·029~033)다. **19개(002·003·004·010~025)는 "이미 구현됨" 으로 `Coverage Verification` 표에만 나온다.**
- 본문이 없는 026·027·028·028a 도 같은 표에만 나온다. 합치면 이름으로는 **23개 id 가 작업이 없다.**

행위로 센 경우:
- 표가 이들을 떠넘긴 작업(T-004: 내보내기 개수, T-002: ctest 발견·구조·커버리지)의 수락 기준은 010~025 의 어느 동작도 행사하지 않는다. 실제로 그 동작을 행사하는 것은 작업과 무관하게 있던 `test_xpe_common.cpp` 60개다.
- 작업이 있는 11개도 행위로는 어긋난다: 001 은 T-003·T-007 에 묶였으나 그 AC 는 C++17 과 CI 트리거여서 "단일 루트가 8개 타깃을 컴파일" 을 확인하지 않는다. 008·030 은 AC 가 틀린 수(18)를 요구한다. 006 은 85 % 게이트가 푸시에서 돌지 않는다.

### 2.4 관련 RTM 의 ✓ 근거

`docs/common/RTM-COMMON-001` (`evidence/rtm_common_001_name_census.txt`, 스크립트로 센 값):

| 항목 | 값 |
|---|---|
| FR/SAF/PERF 행 | 52 |
| 상태 칸에 ✓ 가 있는 행 | **52** |
| 인용한 시험·벤치마크 이름 (서로 다른 것) | 52 |
| 그중 `modules/common/tests` 의 gtest 이름(81개)에 있는 것 | **0** |
| 그중 `modules/common` 아래 어디서든 문자열로 나오는 것 | 0 |
| 인용한 구현 이름 `MemoryPool`·`JsonConfig`·`EventSystem`·`ErrorHandler`·`_xpe_error_context` 가 소스에 있는 것 | 0 (`ParameterValidator` 는 `xpe_common.cpp` 의 `TODO: Replace with ParameterValidator` 주석에만 나옴) |

RTM-COMMON-001 의 요구 id 는 `FR-CMN-*` 이며 SPEC-XPE-P0 의 `REQ-P0-*` 를 한 번도 인용하지 않는다. 즉 이 RTM 은 SRS-COMMON-001 이 상정한 설계(메모리 풀, JSON 설정, 이벤트 시스템)를 추적하는데, 구현된 공용 모듈은 그 설계가 아니다. ✓ 는 존재하지 않는 것을 가리키고 있다. 이 문서는 `check_spec_test_refs.py`(`.moai/specs/*/*.md` 만 검사)의 범위 밖이어서 CI 가 못 잡았다.

### 2.5 내보내기 개수 — 문서마다 다른 수

같은 모듈의 내보내기 수를 문서가 **15·16·18·20** 으로 적는다 (`p0_export_count_statements.txt`):
- 15: `spec.md` §1·§3.2·§5.2·§8.1·§8.3 일부, `export-verification.md` 본문, `xpe_common_api.h` 머리 주석의 REQ 인용
- 16: `spec.md` REQ-P0-008, §7 P0-05, §8.3, `export-verification.md` 수락 기준(체크됨), 헤더의 실제 목록, **실제 DLL**
- 18: `tasks.md` T-004·T-006, `progress.md`, `xpe_common_api.h`·`xpe_memory.h` 의 "18-function" 문구
- 20: `modules/common/docs/EXPORT_POLICY.md` ("20 symbols = 18 public + 2 test")

이슈 #111(닫힘)이 REQ-P0-008 을 16 으로 고쳤지만, 그 뒤에도 위 문구들이 남아 있다. `export-verification.md` 는 끝줄이 "Status: PENDING VERIFICATION" 인데 수락 기준 두 줄은 체크돼 있다.

### 2.6 헤더가 하지 않는 약속 (modules/common)

- `xpe_log_set_level`: "5=OFF". 코드는 5→`critical` 이다 (로그를 끄는 값이 아니다). 실행으로 확인하지는 않았고 소스와 헤더를 읽은 결론이다.
- `xpe_log_set_file`: "NULL 을 주면 stderr 로 되돌린다". 코드는 NULL 일 때 null sink 로거를 설치한다 (주석이 크래시 방지를 이유로 적는다). 출력 목적지가 stderr 로 돌아가는지는 실행으로 확인하지 못했다. 이 모듈은 스스로 기록하지 않아 외부에서 spdlog 를 불러야 관측된다.
- `xpe_common_api.h`·`xpe_memory.h`: "18-function total" 문구가 남아 있다 (헤더 스스로 "어느 수와도 맞지 않는다" 고 적었으나 문구는 그대로다).
- `xpe_get_pending_alert`·`xpe_shutdown` 은 헤더가 정확하다. SPEC 쪽이 낡았다 (§2.2 의 012·019·020).

### 2.7 빌드·CI 에서 빠진 것

- 정적 분석: 요구 §3.4 의 `cppcheck --std=c++17 0건`, `clang-tidy modernize/performance/bugprone 0건`, MISRA — `.github`·`CMakePresets.json`·`cmake`·`tools/ci` 어디에도 문자열이 없다.
- ASan: §3.2 "1000회 init/shutdown ASan 청정" — 시험 `MemoryLeakTestThousandCycles` 는 ctest 로 돌지만 새니타이저 구성은 프리셋·CI 에 없다.
- 내보내기 개수: CI 단계 없음.
- 커버리지: 푸시에서 안 돈다.
- 프리셋 컴파일: `default`·`release`·`ci` 를 빌드하는 잡 없음.
- 로그 필터링과 flush 효과: 파일 내용을 읽는 시험 없음 (§2.2 의 023·025).
- 빌드에서 빠진 시험 파일은 없다: `modules/common/tests` 의 gtest 81개 = CI common-build 81/81.

## 3. 이슈 추적

- BENCH-PRE 의 공백(판정선 시험·데이터·게이트 부재)과 P0 문서의 잔여 불일치(§2.5 수, REQ-026~028a 본문, tasks.md 상태, RTM-COMMON-001)는 모두 닫힌 이슈(#54·#111·#120) 뒤에 남은 상태다. 이 둘을 담는 열린 항목은 찾지 못했다 → **새 이슈 필요.** 커밋은 228 보고서와 같은 장르(명세-실태 대조)라서 `Refs #245` 로 묶는다.

## 4. 리더 질문: 헤더에 "metadata 를 읽는다" 고 남은 문장

사실 확인 (`evidence/header_metadata_sentences.txt`): `src` 에서 `metadata`/`meta` 를 통해 `kVp`·`SID_mm`·`acquisitionTime`·`pixelPitch_mm`·`mAs`·`bodyPart` 를 읽는 곳은 **0건**이다 (대조군: `meta->flags` 를 건드리는 줄은 8개 찾아진다). offset·gain·defect 의 `metadata` 인자는 `!metadata` 널 검사만 하고(`offset_correct.cpp:153`, `gain_correct.cpp:247`, `defect_correct.cpp:223`) 내용은 읽지 않는다.

`preprocess_api.h` 의 `@param metadata` 문장 중 **읽는다고 남은 것** (현재 줄 번호, 줄 번호는 편집 때마다 밀리므로 함수 이름이 주소다):

| 줄 | 함수 | 문장 | 판단 |
|---|---|---|---|
| 252 | `xpe_offset_correct` | "Image metadata including temperature and acquisition time" | **낡음.** 같은 블록 240~245줄이 "온도 필드 없음, acquisitionTime 읽지 않음" 이라 적는다 |
| 375 | `xpe_defect_correct` | "Image metadata for dose-dependent threshold" | **낡음.** 선량 의존 문턱은 구현에 없다 |
| 683 | `xpe_defect_detect_runtime` | "Image metadata for dose information" | **낡음.** 읽지 않는다 |
| 953 | `xpe_validate_readout_artifact` | "Image metadata (acquisition context)" | **낡음/모호.** 읽지 않는다 |
| 1519, 1640 | `xpe_verify_offset`, `xpe_verify_pipeline` | "Image metadata" | 주장은 없으나 사용 여부를 말하지 않는다 |
| 281, 788 | `xpe_gain_correct`, `xpe_ghost_correct` | "must be non-NULL, kVp/SID_mm do not change the result" / "acquisitionTime is NOT used" | 이미 정정됨 (229 M2/M2b) |

229 M2/M2b 가 고친 것은 offset·gain 의 서술 블록과 gain·ghost 의 `@param` 이다. offset 의 `@param`(252)과 결함 두 함수·readout 검증 함수의 `@param` 은 같은 철회에서 빠졌다. 고칠 목록으로 `fixes/preprocess_api_h_stale_metadata_sentences.txt` 에 두었다 (헤더는 내 소유이므로 다음 카드에서 한 번에 고칠 수 있다).

## 5. 결함 후보 (고치지 않음)

관측 수준을 함께 적는다.

| # | 후보 | 수준 |
|---|---|---|
| D1 | `xpe_log_set_level(5)`: 헤더 "5=OFF" / 코드 `critical` — "끄기" 를 기대한 호출자는 치명 로그를 계속 받는다 | 코드·헤더 읽기 (실행 미관측) |
| D2 | SPEC-XPE-P0 REQ-P0-012·016·019·020·022·011·014·024 의 문구가 코드 이름·시그니처·동작과 다르다 (반환 코드 이름 3종은 존재하지 않는 이름) | 코드·시험 관측 |
| D3 | `ShutdownReturnsNotInitializedWhenNotInit` — 이름이 코드를 약속하나 `SUCCEED()` 뿐 | 시험 본문 관측 |
| D4 | `LogSetFileWithInvalidPathReturnsError` 가 `XPE_OK` 도 통과시킨다 — 이름이 말하는 실패를 단언하지 못한다 (같은 동작은 `CommonOom` 이 단언) | 시험 본문 관측 |
| D5 | `test_xpe_common.cpp` 구역 제목 "(REQ-P0-022 thread safety)" 의 요구 번호 오인용 | grep |
| D6 | dicom 모듈에 버전 함수 없음 (REQ-P0-033) | grep |
| D7 | `xpe_free_image` 이중 해제가 `XPE_OK` (REQ-P0-016 은 `INVALID_PARAM`) — 의도된 동작이면 SPEC 을 고치는 쪽 | 시험 관측 |
| D8 | `RTM-COMMON-001` 52행 ✓ 의 근거 시험 52개가 전부 없다 | 스크립트 관측 |
| D9 | `vcpkg.json` 에 opencv4·eigen3 없음 (REQ-P0-003), Eigen 은 FetchContent 대체 | 파일 관측 |
| D10 | `PreprocessDegraded` 의 `BP03`~`BP05` 번호가 manifest 와 다르고, `BP02`·`BP03` 시험은 이름이 말하는 성질(identity 보존, 무동작)을 단언하지 않는다 (`NOT_INITIALIZED` 만 단언) | 시험 본문 관측 |
| D11 | `preprocess_api.h` 의 낡은 `@param metadata` 4곳 (§4) | 헤더·소스 관측 |
| D12 | REQ-BPRE-007 "BP-01~05 를 돌려 회귀를 막는다" 가 사실과 다르다 (BP-06~09 만) | 워크플로 관측 |
| D13 | `spec.md` 자체: 머리 v1.2.0 / 이력표 v1.1.0 / 끝줄 v1.1.0, deliverable 수 "12/12"(이력) vs 11개 표, REQ-P0-032 "8개" 에 7개 나열 | 문서 관측 |
| D14 | `xpe_log_set_file(NULL)`: 헤더 "stderr 로 되돌림" / 코드 null sink 설치 — 되돌린 뒤 로그가 어디에도 안 나갈 수 있다 | 코드·헤더 읽기 (실행 미관측) |
| D15 | 기록 수준 기본값: REQ-P0-011 "INFO" / 코드 `g_currentLevel = 0`(TRACE) — 첫 `xpe_log_set_file(경로)` 로 만든 파일 로거는 TRACE 로 시작한다 | 코드 읽기 (실행 미관측) |

## 6. 수정안 초안 (`fixes/*.txt`, 리더 소유 문서용)

- `spec_bench_pre_fix_draft.txt` — SPEC-BENCH-PRE 와 manifest 의 상태·REQ-BPRE-001/007·동결 표를 사실대로 고치는 문구 (두 길: "동결의 범위를 스모크로 선언" 과 "게이트를 실제로 만든다")
- `spec_xpe_p0_fix_draft.txt` — REQ 문구 정정표(코드 이름·시그니처·동작), 026~028a 본문 복원 제안, 수 통일, tasks.md 상태 정정
- `rtm_common_001_fix_draft.txt` — RTM-COMMON-001 의 ✓ 를 실제 시험 이름으로 다시 잇는 방법 (또는 비고 처리)
- `preprocess_api_h_stale_metadata_sentences.txt` — 헤더 4곳의 현행 문장과 제안 문장 (내 소유)

## 7. 미검증 (Gaps)

- `ci.yml` `dotnet-tests`·`gui-e2e-native` 잡의 로그는 열지 않았다. C# 쪽 요구(029~031)의 "실행" 증거는 잡이 success 였다는 사실뿐이다.
- 신선한 `xpe_common.dll` 은 로컬 `ci-preprocess` 빌드(2026-10-03 08:09)에서 읽었다. CI 가 만든 DLL 의 내보내기표는 읽지 못했다 (CI 에는 세는 단계가 없다). 로컬 `build/ci-common/bin/xpe_common.dll`(2026-09-18) 도 16개였으나 그 뒤 `modules/common` 이 바뀌어 사용하지 않았다.
- 커버리지: xpe_common 단독 수치는 관측하지 못했다. 마지막 수동 실행은 2026-09-12 이므로 현재 값이 아니다.
- `XPE_ERR_*` 불일치 중 D1(`5=OFF`)은 실행하지 않았다.
- BP-SIMD 의 1830건과 대응하는 시험을 찾지 못한 것은 문자열 검색(`1830`·`1800`, `SimdParityTest`)과 이름 목록으로만 확인했다. 연산별 동등성 시험이 1830건과 같은 범위인지는 판정하지 않았다.
- `benchmark/BP-06-09-post-benchmark-baseline.md` 와 post 쪽 BP 는 이 카드의 범위가 아니라 보지 않았다.

## 8. 잔여 위험

- 내 결론 "BP-01~05 판정선을 단언하는 시험이 없다" 는 `modules/preprocess` 의 `src`·`tests`·`include` 에서의 검색이다. 다른 모듈(`tests/e2e_post_pipeline` 등)에 같은 판정선을 재는 시험이 있다면 이 결론은 좁혀진다. 검색 범위를 문장에 적었다.
- CI 로그 두 건(main `f1f3e54d`, `483e8fd2`)은 한 시점이다. 워크플로가 바뀌면 §1.1 의 표는 낡는다.
