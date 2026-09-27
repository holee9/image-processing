# QA-A-01 — Lane A (common + preprocess) QA 게이트 실측

- **레인**: Lane A (`xpe-pre`)
- **워크트리**: `D:/workspace-github/xpe-pre`
- **브랜치**: `dev/preprocess` @ `e67c125`
- **기준**: `.moai/project/lane-sessions.md` §2 (dev-plan §4.1 게이트 6항목)
- **측정일**: 2026-08-28
- **판정 주체**: 본 문서는 **실측 기록**이며 합격 판정을 주장하지 않는다 (§8 L1 규약).

## 빌드 환경

시스템 PATH에 cmake/ninja/MSVC가 없어 VS 2022 Professional 내장 도구를 직접 지정했다.
Git Bash에서 `cmd.exe`로 넘길 때 PATH에 Windows 기본 경로가 없으면 `vcvars64.bat`이
`vswhere.exe`를 못 찾아 실패한다(rc.exe/mt.exe 누락 → 컴파일러 테스트 실패). PATH 초기화 후 정상.

```
PATH=C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem
call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
cmake --preset ci-preprocess
cmake --build build/ci-preprocess
```

결과: `CONFIGURE_EXIT=0`, `BUILD_EXIT=0` (원문: `build.log`)
산출물: `build/ci-preprocess/bin/{xpe_common.dll, xpe_preprocess.dll, test_xpe_common.exe, xpe_preprocess_tests.exe}`

---

## 게이트 6항목 실측 결과

| # | 항목 | 실측 | 근거 |
|---|---|---|---|
| 1 | `dumpbin /dependents` 횡단 의존성 없음 | 충족 | `abi.log` |
| 2 | Google Test 해당 모듈 100% GREEN | 충족(단서 있음) | `tests.log` |
| 3 | 메모리 누수 1000프레임 PASS | 충족 | `tests.log` |
| 4 | static analysis 0 warning (`/WX`) | **미충족** | `build.log` |
| 5 | P/Invoke ABI 심볼 수 문서 일치 | **불일치 1건** | `abi.log` |
| 6 | CODEOWNERS 경계 준수 | 충족(공집합) | `git diff main...HEAD` |

---

### 1. 횡단 의존성 — 충족

`dumpbin /dependents` 원문은 `abi.log`.

- `xpe_common.dll` → `spdlog.dll`, MSVCP140/VCRUNTIME140/api-ms-win-crt-*, KERNEL32
- `xpe_preprocess.dll` → MSVCP140/VCRUNTIME140/api-ms-win-crt-*, KERNEL32

Lane B/C 모듈(`enhance_basic`, `enhance_advanced`, `ai`, `display`, `dicom`, `gsvg`, `clients`, `gui`)
DLL은 어느 쪽에도 나타나지 않는다. 횡단 의존성 없음.

**관찰(게이트 항목 아님)**: `xpe_preprocess.dll`의 import 목록에 `xpe_common.dll`이 없다.
preprocess가 common을 헤더/정적 경로로만 쓰거나 런타임 로딩하는 구조로 보인다.
설계 의도와 일치하는지 확인이 필요하다 — 본 레인 단독 판단 사항이 아니라 판정을 유보한다.

### 2. Google Test — 충족(단서 있음)

```
ctest --output-on-failure      (build/ci-preprocess)
100% tests passed, 0 tests failed out of 345
Total Test time (real) = 20.19 sec
CTEST_EXIT=0
```

실패 0건. 다만 **미실행 14건**이 있다(원문 `tests.log` 말미):
- Skipped 13건 — `DegradedMode.BP06~BP10` 5건(타 모듈 부재로 인한 정상 스킵),
  `PreprocessCalibrationTest.*` / `CalibSaveTest.*` 8건(XCal 픽스처 의존)
- Disabled 1건 — `Integration.PipelinePerformance3072x3072`

"100% GREEN"은 실행된 345건 기준이다. 미실행 14건은 이 빌드 구성에서 검증되지 않았다.

### 3. 메모리 누수 1000프레임 — 충족

```
 53/346 XpeCommonTest.MemoryLeakTestThousandCycles ............ Passed 0.01 sec
125/346 XpePreprocessEndurance.NoMemoryLeakAfter1000Frames .... Passed 0.33 sec
172/346 EnduranceTest.ThousandCycles_NocrashAllOk ............. Passed 0.78 sec
173/346 EnduranceTest.ThousandCycles_MemoryGrowthUnderOneMB ... Passed 0.77 sec
```

1000프레임 지구력 테스트 4건 전부 PASS. `MemoryGrowthUnderOneMB`가 증가량 상한을 실측 검증한다.

**단서**: `test_xpe_common.cpp:585` 주석은 "ASan 오류가 없으면 누수 없음"을 전제하나,
이 빌드는 RelWithDebInfo이며 ASan이 켜져 있지 않다. 즉 이 테스트는 크래시/증가량은
잡지만 ASan 수준의 누수 탐지는 하지 않는다.

### 4. static analysis 0 warning (`/WX`) — 미충족

빌드 경고 **3건**:

```
modules\preprocess\tests\test_ghost_correct.cpp(35):            warning C4244 (double → uint64_t 데이터 손실)
modules\preprocess\tests\test_xpe_sha256.cpp(28):               warning C4996 ('sscanf' unsafe)
modules\preprocess\tests\test_xpe_preprocess_correction.cpp(67): warning C4996 ('strncpy' unsafe)
```

3건 모두 테스트 소스이며 **프로덕션 소스 경고는 0건**이다.

그러나 게이트가 요구하는 "`/WX` 빌드"는 성립하지 않는다. 프리셋이
`XPE_WARNINGS_AS_ERRORS=ON`을 주지만, 두 모듈 모두 타깃 단위에서 `/WX-`로 명시적 opt-out 한다:

```
modules/common/CMakeLists.txt:82       target_compile_options(xpe_common PRIVATE /W4 /WX-)
modules/common/CMakeLists.txt:129      target_compile_options(test_xpe_common PRIVATE /W4 /WX-)
modules/preprocess/CMakeLists.txt:71   target_compile_options(xpe_preprocess PRIVATE /W4 /arch:AVX2)   # /WX 없음
modules/preprocess/CMakeLists.txt:199  target_compile_options(xpe_preprocess_tests PRIVATE /W4 /WX-)
```

`cmake/CompilerWarnings.cmake`는 MSVC에서 `/WX`를 전역 적용하지 않고 모듈에 위임하는데,
Lane A 두 모듈 중 어느 쪽도 `if(XPE_WARNINGS_AS_ERRORS)`로 `/WX`를 걸지 않는다.
결과적으로 `XPE_WARNINGS_AS_ERRORS`는 Lane A 범위에서 **동작하지 않는 옵션**이다.

즉 게이트는 두 가지로 갈린다.
- "경고 0건"으로 읽으면: 프로덕션 충족 / 테스트 3건 미충족
- "`/WX`로 기계가 강제"로 읽으면: 강제 자체가 없으므로 미충족

### 5. ABI 심볼 수 — 불일치 1건

| DLL | export 함수 | 공개 헤더 `XPE_API` 선언 | 결과 |
|---|---|---|---|
| `xpe_preprocess.dll` | 45 | `xpe/preprocess_api.h` 45 | 일치 |
| `xpe_common.dll` | 16 | `xpe/common/*.h` 15 | **+1 초과** |

초과 심볼: **`xpe_test_inject_alert`**

`grep -rn "xpe_test_inject_alert" modules/common/include/` → 결과 없음.
공개 헤더에 선언되지 않은 테스트 주입용 심볼이 프로덕션 DLL에서 export 되고 있다.
IEC 62304 Class B 기준에서 릴리스 바이너리의 테스트 훅 노출은 검토 대상이다.
제거/격리 여부는 본 레인 단독 결정 사항이 아니므로 lead 판정에 올린다.

### 6. CODEOWNERS 경계 — 충족(공집합)

```
git merge-base main HEAD  → e67c1250b9b9ace92cafc378a1cdf55adaa809eb
git log --oneline main..HEAD  → (없음)
git diff --name-only main...HEAD  → (없음)
git status --short  → (clean)
```

`dev/preprocess`는 아직 `main` 대비 커밋이 없다. 경계 침범이 **발생할 수 없는 상태**이며,
"준수"는 아직 아무 변경도 없다는 뜻이지 경계 규율이 검증되었다는 뜻이 아니다.
(`origin/main`은 로컬 `main`보다 크게 뒤처져 있어 원격 기준 비교는 의미가 없다.)

---

## 미검증 (Gaps)

- 미실행 테스트 14건(Skipped 13 / Disabled 1) — 이 빌드 구성에서 검증되지 않음
- ASan 빌드 미수행 — 누수 탐지는 증가량 임계 기반에 한정
- `Integration.PipelinePerformance3072x3072` Disabled — 3072×3072 성능은 main 통합 E2E 소관
- 정적 분석 도구(clang-tidy / MSVC `/analyze`) 미실행 — 게이트의 "static analysis"를
  컴파일러 경고로 해석해 측정했다. 별도 도구를 의도한 것이라면 재측정이 필요하다.

## 잔여 위험

- `xpe_preprocess.dll`이 `xpe_common.dll`을 import 하지 않는 점이 설계 의도와 다를 경우,
  게이트 1 "횡단 의존성 없음" 충족은 의도치 않은 결합 구조의 부산물일 수 있다.
- `/WX` opt-out 상태에서는 앞으로 유입되는 경고가 빌드를 깨지 않는다. 현재 경고 3건은
  누적의 시작일 수 있다.

## 실행 명령 원문 위치

| 파일 | 내용 |
|---|---|
| `build.log` | configure + build 원문 (경고 3건 포함) |
| `tests.log` | `ctest --output-on-failure` 원문 346줄 |
| `abi.log` | `dumpbin /dependents` + `/exports` 원문 (4블록) |

---

# 부록 — 후속 조사 (lead 판정 대기 중 수행, 읽기 전용)

lead 회신 전 결정 비용을 줄이기 위한 추가 실측. 소스 수정 없음.

## A-1. `/WX` 활성화 시 실제 비용 — 3건, 각 1줄

```
modules/preprocess/tests/test_ghost_correct.cpp:35
    meta.acquisitionTime = 0.0;                          // C4244 double → uint64_t

modules/preprocess/tests/test_xpe_sha256.cpp:28
    std::sscanf(hex + 2 * i, "%02x", &byte);             // C4996 sscanf

modules/preprocess/tests/test_xpe_preprocess_correction.cpp:67
    std::strncpy(meta->bodyPart, "CHEST", sizeof(meta->bodyPart) - 1);   // C4996 strncpy
```

세 건 모두 테스트 코드의 단일 라인이며 국소 수정으로 해소된다
(캐스트 명시 / `sscanf_s` / `strncpy_s`, 또는 해당 TU 한정 `_CRT_SECURE_NO_WARNINGS`).
`/WX` 활성화의 비용은 이 3줄이 전부이고 프로덕션 소스는 손댈 필요가 없다.

## A-2. `xpe_test_inject_alert` — 테스트 훅이 아니라 **레인 간 실사용 심볼**

당초 "테스트 주입 훅"으로 보고했으나 오판이었다. Lane B 프로덕션 코드가 이 심볼을 호출한다.

```
modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h:27
    /* xpe_test_inject_alert is exported from xpe_common.dll but not declared ... */
modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_internal.h:30
    extern "C" XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity);

modules/enhance_basic/src/exposure_index.cpp:116
    xpe_test_inject_alert(alertMsg, XPE_ALERT_WARNING);      ← 프로덕션 호출
```

정의는 `modules/common/src/xpe_common.cpp:305`. 그 외 호출은 전부 common 테스트다.

즉 현재 상태는 **"공개 헤더에 없는 심볼을 Lane B가 자체 extern 선언으로 끌어다 쓰는 것"**이며,
Lane B 헤더의 주석이 그 사실을 이미 알고 적어 두었다. 이는 미선언 심볼 노출 문제인 동시에
문서화되지 않은 레인 간 ABI 결합이다.

**따라서 단순 제거는 Lane B 빌드를 깬다.** 선택지는 셋으로 갈린다.

| 방향 | 내용 | 영향 |
|---|---|---|
| (a) 공식화 | alert 주입을 공개 API로 승격해 `xpe_common_api.h`에 선언 | 헤더 15 → 16, export와 일치. Lane B 자체 extern 제거 |
| (b) 대체 후 제거 | Lane B가 쓸 정식 alert API를 따로 제공하고 이 심볼은 내부화 | Lane B 코드 변경 필요, 조율 대상 |
| (c) 현상 유지 | 게이트 5 불일치를 알려진 예외로 기록 | IEC 62304 Class B 추적성 부담 잔존 |

Lane A 단독 결정 불가. `lane-sessions.md` §3 "Lane ↔ Lane ABI 변경 통보" 대상이며
Lane B 합의가 선행되어야 한다.

## A-3. `xpe_preprocess.dll`이 `xpe_common.dll`을 import 하지 않는 이유 — 규명됨

CMake 선언은 정상이다.

```
modules/preprocess/CMakeLists.txt:64-65
    target_link_libraries(xpe_preprocess PUBLIC xpe_common)
```

`modules/preprocess/src/` 전체에서 common 공개 함수(`xpe_init`, `xpe_alloc_image`,
`xpe_log_*`, `xpe_error_string` 등) 호출은 **0건**이다. 참조가 없으니 링커가 import
엔트리를 만들지 않았을 뿐, 설정 오류가 아니다.

앞서 "게이트 1 충족이 의도치 않은 결합 구조의 부산물일 수 있다"는 유보는 철회한다.
횡단 의존성 없음은 실제로 충족이다. 다만 preprocess가 common의 런타임 기능을
전혀 쓰지 않는다는 사실은 설계 의도 대비 확인할 가치가 있다(예: 로깅/에러 문자열을
공통 경로로 태우려던 의도가 있었다면 미구현일 수 있다). 이 판단은 lead 소관으로 남긴다.

## A-4. `test_xpe_common.cpp` 3중 중복 (Lane A 소유 정리 대상)

```
modules/common/tests/test_xpe_common.cpp   601줄   ← modules/common/CMakeLists.txt:116 에서 빌드
tests/common_unit/test_xpe_common.cpp      491줄   ← tests/common_unit/CMakeLists.txt:2 에서 빌드
tests/common/test_xpe_common.cpp           596줄   ← 어느 CMakeLists 에서도 참조되지 않음 (고아)
```

`tests/common/`은 빌드에 들어가지 않는다. 세 파일이 서로 조금씩 다르며(601/596/491줄)
동일 테스트의 분기 사본으로 보인다. 게이트 2의 "100% GREEN"은 빌드에 포함된 사본
기준이고, 고아 사본의 내용은 검증되지 않는다.

Lane C의 `clients`/`gui` 중복(§5 이슈 #1)과 성격이 같은 문제이며 경로상 Lane A 소유다.
정리 방향(고아 삭제 / 통합 / 의도적 분기 여부)은 lead 판정을 받아 진행한다.

## 부록 판정 요약

- A-1: `/WX` 활성화 비용은 테스트 3줄. 프로덕션 무영향
- A-2: **게이트 5 불일치는 Lane B와의 미문서화 ABI 결합. 단독 제거 불가**
- A-3: 게이트 1은 진성 충족. 앞선 유보 철회
- A-4: 신규 발견 — 빌드되지 않는 테스트 사본 1건 (Lane A 소유)
