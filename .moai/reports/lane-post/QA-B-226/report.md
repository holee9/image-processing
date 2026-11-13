ACK 착수 2026-11-13 16:20 KST

16:32 리더 보강 반영 완료: 공개 API 주석 추가, underflow 관측·정책 근거 기록, 인계 목록에 헤더와 `underflow.txt` 추가. 커밋은 리더 담당으로 두었다.

Reviewer F226-1 반영: 거절 조건을 계수 절대값 `>FLT_MAX`로 명확히 한정했다. subnormal·−0 range 계수는 허용하며 추가 관측은 `underflow-subnormal.txt`에 남겼다. NLM은 실패 미재현 위험 목록으로만 유지했다.

# QA-B-226 — bilateral 역수 계수 범위 검사

## Claim

- `apply_bilateral`의 range·spatial 역수 계수를 double로 계산하고, **계수 절대값이 FLT_MAX를 넘는 작은 sigma**를 기존 `XPE_ERR_INVALID_INPUT`(-1)로 거절하도록 수정했다. 모든 부정확한 float 변환을 거절하는 정책이 아니며, 큰 sigma_range의 subnormal·−0 계수는 거절하지 않는다. 검사 이후에만 float로 내리고 필터를 실행한다.
- 거절되는 sigma에서 입력 바이트가 보존됨을 Debug·RelWithDebInfo 모두에서 확인했다.
- 정상 대표 입력 3종의 출력 SHA-256은 빌드별 전후 6/6 일치했다. 서로 다른 빌드 사이의 해시 일치를 주장하는 것은 아니다.
- 수정 전 실패하던 기존 Debug NonFinite 시험은 수정 후 통과했다. 단언·시험·입력·기대값은 바꾸지 않았다.
- 전체 enhance_basic 시험을 각각 1회 실행했다. Debug 189/194 통과(시간 예산 5건 실패), RelWithDebInfo 194/194 통과다. **Debug 전체 통과는 아니다.**
- 커밋 staging은 공용 `.git` 쓰기 권한 때문에 실패했다. 커밋 SHA는 없고 push도 하지 않았다. 카드의 대체 절차에 따라 리더 인계 목록을 아래에 적었다.

## 변경과 diff

동작 변경은 `modules/enhance_basic/src/noise_reduce.cpp` 한 파일에 한정했다. 기존 파일의 `<limits>`를 재사용했다. 16:32 리더 보강에 따라 공개 헤더 `modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h`의 거절 조건 주석 한 줄도 추가했다. 다른 모듈·CI·CMake 수정은 없다.

```diff
-    float ss2_inv            = 1.0f / (sigma_space * sigma_space);
-    float minus_half_sr2_inv = -0.5f / (sigma_range * sigma_range);
+    // QA-B-226: reject unrepresentable coefficients before any pixel is written.
+    const double spatialSigma = static_cast<double>(sigma_space);
+    const double rangeSigma = static_cast<double>(sigma_range);
+    const double spatialInverse = 1.0 / (spatialSigma * spatialSigma);
+    const double rangeInverse = -0.5 / (rangeSigma * rangeSigma);
+    const double maxFloat = static_cast<double>((std::numeric_limits<float>::max)());
+    if (spatialInverse > maxFloat || rangeInverse < -maxFloat) {
+        return XPE_ERR_INVALID_INPUT;
+    }
+    const float ss2_inv = static_cast<float>(spatialInverse);
+    const float minus_half_sr2_inv = static_cast<float>(rangeInverse);
```

검사는 기존 화소 쓰기 경로(`bilateral_band`)와 spatial LUT 생성보다 앞이다. 앞선 `image_peak` 검사는 입력을 읽지만 수정하지 않는다. 극소 sigma 전용 필터 경로는 추가하지 않았다.

## Evidence — 수정 전 실패

이번 카드의 수정 전 트리에서 기존 Debug 시험을 새로 실행했다. 외부 `spawnSync` timeout은 10초다. [before-failure.txt](before-failure.txt)에 명령·원문 전체를 보존했다.

```text
build/cov-local/bin/xpe_enhance_basic_tests.exe --gtest_filter=NonFinitePixels.NoiseReduceNeverMakesANonFinitePixelFromFiniteInputAndParameters
TIMEOUT_MS 10000 EXIT 1 ERROR undefined
D:\workspace-github\xpe-post\modules\enhance_basic\tests\test_nonfinite_pixels.cpp(222): error: Value of: std::isfinite(v)
  Actual: false
Expected: true
bilateral sigma_range = 9.9999996826552254e-21
```

## 거절 경계와 화소 보존

`F = FLT_MAX = 3.4028234663852886e38`일 때 double 계수 검사에서의 이론적 하한은 다음과 같다.

- range: `sqrt(0.5/F) = 3.8332336559477019e-20`.
- spatial: `sqrt(1/F) = 5.4210110239862428e-20`.

float 파라미터의 인접 값으로 실제 API를 호출했다. 아래 관측은 Debug·RelWithDebInfo에서 같았다. 오류 시 `memcmp`로 입력 전체의 바이트 동일성을 검사했다. [hashes-after-boundary.txt](hashes-after-boundary.txt)에 수치·종료 코드·`unchanged` 원문이 있다.

| 파라미터 | 시험 값 | API 결과 | 입력 바이트 동일 |
|---|---:|---:|---|
| sigma_range | 3.8332334761054374e-20 | -1 | 예 |
| sigma_range | **3.8332337992228642e-20** | 0 | 필터 실행 |
| sigma_range | 3.833234122340291e-20 | 0 | 필터 실행 |
| sigma_range | 9.9999996826552254e-21 | -1 | 예 |
| sigma_space | 5.4210105393100954e-20 | -1 | 예 |
| sigma_space | 5.4210108624275222e-20 | -1 | 예 |
| sigma_space | **5.4210115086623757e-20** | 0 | 예(허용된 극좁은 필터 결과) |
| sigma_space | 9.9999996826552254e-21 | -1 | 예 |

따라서 이번 toolchain에서의 허용 최소 float 값은 range `3.8332337992228642e-20`, spatial `5.4210115086623757e-20`이다. 바로 아래의 인접 float는 거절된다. 기존 `sigma_space <= 7.5` 상한은 유지했다. 기존 유한성·양수 검사도 유지했다. 이전에는 양수 검사만 통과했던 극소 범위에 계수 절대값 상한에 따른 sigma 하한이 추가된 것이다.

## 정상 범위 출력 해시 비교

기존 `build/cov-local`에 진단 프로그램 `qa-b-226-probe.cpp`를 두고 실제 빌드 DLL의 `xpe_noise_reduce`를 로드해 호출했다. 프로젝트 소스나 시험을 추가한 것은 아니다. 64×64 float32 입력, 고정 seed 17의 LCG를 사용했다.

| case | 입력 | sigma_space | sigma_range |
|---|---|---:|---:|
| 0 | LCG 상위 8비트 + `i % 13`의 램프/잡음 | 2 | 10 |
| 1 | 좌측 100, 우측 900의 평탄부·경계 | 3 | 50 |
| 2 | 500 기반 체커보드 40 + LCG 상위 4비트 | 5 | 100 |

각 case의 실제 필터 출력 16,384바이트를 해시했다. 세 경우 모두 API 성공이고 입력과 출력이 달라 실제 필터 실행을 확인했다. 각 빌드 49,152바이트를 수정 전·후에 받아 비교했으며, DLL 호출은 외부 10초 제한으로 실행했다. 원문: [hashes-before.txt](hashes-before.txt), [hashes-after-boundary.txt](hashes-after-boundary.txt).

| 빌드 | case | 수정 전 = 수정 후 SHA-256 |
|---|---:|---|
| Debug | 0 | `736276763cb0fd6f201d1f3f929f1b026939743930b1f84fc8f4a31edf39592a` |
| Debug | 1 | `3068369e4255a7ec3fd0b36ca7de668feffbc4098bad29bad2c6eaaa1c642b66` |
| Debug | 2 | `0b442f0f007edb61b419f245faa0294cd9e43e82e4b369b5928c9a63369c37c0` |
| RelWithDebInfo | 0 | `7335bf6a27b7e323424df49f810fed7d4b51cbb31955044c0f85917bcda1246e` |
| RelWithDebInfo | 1 | `aa98c72e45c75edf8799aec1456a206b4a4917341bc17b1d58b578aacc3819a9` |
| RelWithDebInfo | 2 | `15678121d09e61ca6cad2a92515128d32e525f9657523853c4a40c4ff603c880` |

## 16:32 보강 — 공개 API 주석과 큰 sigma의 underflow

리더 지시 `20261113-1626-post.md`와 카드 맨 위 보강 절을 읽고, 16:32 지시에 따라 공개 API의 `xpe_noise_reduce` 반환 조건에 다음 한 줄을 추가했다. 주석은 기존 헤더의 영문 형식에 맞췄다.

```text
bilateral: |1/sigma_space^2| or |0.5/sigma_range^2| > FLT_MAX (small sigma) returns XPE_ERR_INVALID_INPUT; build minima: sigma_range 3.8332338e-20, sigma_space 5.4210115e-20; subnormal/-0 range coefficients are not rejected;
```

**결정(F226-1):** 거절 대상은 double로 계산한 계수의 **절대값이 FLT_MAX를 넘는 작은 sigma**다. 큰 sigma_range에서 float 변환 결과가 subnormal 또는 −0이 되는 것은 거절하지 않는다. 따라서 “float로 표현할 수 없는 모든 계수를 거절”한다고 해석하면 안 된다. 유효 숫자 손실이나 0으로의 반올림까지 금지하는 정책이 아니다. 제품 동작이나 단언은 더 바꾸지 않았다.

**확인 사실:** `sigma_range=FLT_MAX=3.4028234663852886e38`에서 double range 계수는 `-4.3180847923030273e-78`이고, float 변환 결과는 `-0x0.0000000000000p+0`이다. 진단 프로그램은 `/EHsc /std:c++17 /utf-8 /O2 /MD`로 빌드했고, 실제 Debug·Rel DLL을 각각 외부 10초 제한으로 호출했다. 위 정상 입력 3종과 sigma_space 2·3·5에서 **양쪽 빌드 모두 rc=0, 전체 출력 화소 finite=1**을 확인했다. 정확한 명령·계수·각 입력 결과는 [underflow.txt](underflow.txt)에 있다. 이 원문은 총 6회의 API 호출이며 기존 전체 시험을 재실행한 것은 아니다.

**F226-1 추가 확인 사실:** 같은 진단 프로그램에 `sigma_range=1.0e20f`를 추가했다. 실제 값 `1.0000000200408773e20`에서 double 계수는 `-4.9999997995912324e-41`, float 변환은 `-0x1.16c2000000000p-134`이고 `finite=1, zero=0, subnormal=1`이었다. 이 값과 FLT_MAX의 −0 경우를 각각 3종 입력·두 빌드에서 호출한 추가 12회 모두 `rc=0, finite=1`이었다. [underflow-subnormal.txt](underflow-subnormal.txt)에 컴파일/실행 명령과 원문을 보존했다. 이는 대표 입력에서 두 경우를 거절하지 않고 유한 출력을 내는 사실이며, subnormal을 중간 연산이 끝날 때까지 보존한다거나 원래 double 식과 출력이 정확히 같다는 증거는 아니다.

**수학적 해석과 추정:** 유한한 차이 값에 계수 −0을 적용한 range 지수는 0이고 `exp(0)=1`이므로, 이때 계산된 필터는 range 구분을 하지 않는 공간 가중 필터로 해석할 수 있다. subnormal 계수는 크기가 매우 작은 비영 가중 인자를 나타내지만 중간 연산의 반올림·실행 환경의 subnormal 처리에 따라 영향이 사라질 수도 있다. 이것은 코드·수식에 따른 해석이며 모든 허용 입력에서의 실제 오차 측정은 아니다. 원래 double 비영 계수 식과 항상 같은 결과라고 주장하지 않는다. 위 대표 입력 관측 밖에서 정확성·컴파일러 재배치·subnormal 처리 모드까지 전수 검증하지 않았다. 기존 유한 입력/누적 범위 검사는 유지한다.

spatial 계수의 underflow는 현재 허용 범위에서 발생하지 않는다. 기존 상한 `sigma_space<=7.5`로 인해 `1/sigma_space^2 >= 1/56.25 ≈ 0.01777777778`이며, 더 큰 spatial sigma는 기존 파라미터 검사에서 거절된다. 따라서 이번 큰 sigma/0 계수 관측의 대상은 range 계수다.

| 보강 확인 항목 | 상태 | 근거 |
|---|---|---|
| 일반 sigma, 대표 입력 3종 | 확인함 | 빌드별 전후 해시 6/6 일치 |
| range/spatial 허용 최소값 바로 안·밖 | 확인함 | 양쪽 DLL의 인접 float 호출 결과 |
| 거절 시 버퍼 전체 불변 | 확인함 | 양쪽 빌드 `memcmp`, `unchanged=1` |
| 큰 range sigma에서 0 계수 허용과 유한 출력 | 확인함 | `underflow.txt`, 3입력×2빌드 |
| subnormal range 계수 허용과 유한 출력 | 확인함 | `underflow-subnormal.txt`, 3입력×2빌드 |
| 허용 입력 전 범위에서 0 계수의 근사 정확성 | 미확인 | 대표 입력 관측이며 전수 검증 아님 |
| 원격 CI/coverage 재확인 | 미확인 | 리더 담당 |

이번 보강은 제품 동작을 추가 변경하지 않은 API 주석·보고서 변경이다. 아래 전체 시험 결과는 앞서 수행한 실행의 결과를 그대로 유지한다.

## 빌드·시험 결과

두 구성에서 타깃 빌드 exit 0을 확인했다. [build.txt](build.txt)에 명령과 출력이 있다.

```text
cmake --build D:/workspace-github/xpe-post/build/cov-local --target xpe_enhance_basic_tests --parallel 2
cmake --build D:/workspace-github/xpe-post/build/ci-post --target xpe_enhance_basic_tests --parallel 2
```

기존 Debug 대상 시험의 수정 후 결과는 [target-after.txt](target-after.txt)에 있다.

```text
TIMEOUT_MS 10000 EXIT 0 ERROR undefined
[       OK ] NonFinitePixels.NoiseReduceNeverMakesANonFinitePixelFromFiniteInputAndParameters (97 ms)
[  PASSED  ] 1 test.
```

enhance_basic 전체 시험은 필터 없이 각 실행 파일을 **각각 1회** 실행했다. 작업 디렉터리는 두 경우 모두 `D:/workspace-github/xpe-post/modules/enhance_basic`이고, 외부 Node `spawnSync` timeout은 300,000ms다. 모두 제한 전에 종료됐다. 별도 재실행으로 실패를 덮지 않았다.

```text
QA-B-226 DEBUG FULL COMMAND D:/workspace-github/xpe-post/build/cov-local/bin/xpe_enhance_basic_tests.exe CWD D:/workspace-github/xpe-post/modules/enhance_basic TIMEOUT_MS 300000 ELAPSED_MS 32578 EXIT 1 ERROR undefined
[==========] Running 194 tests from 21 test suites.
[==========] 194 tests from 21 test suites ran. (32562 ms total)
[  PASSED  ] 189 tests.
[  FAILED  ] 5 tests, listed below:
[  FAILED  ] LogTransform.Performance_3072x3072_Within15ms
[  FAILED  ] LogTransform.InversePerformance_3072x3072_Within15ms
[  FAILED  ] ContrastEnhance.Performance_3072x3072_Within50ms
[  FAILED  ] EdgeEnhance.Performance_3072x3072_Within20ms
[  FAILED  ] EnhanceIntegration.FullPipeline_3072x3072_Within200ms

QA-B-226 REL FULL COMMAND D:/workspace-github/xpe-post/build/ci-post/bin/xpe_enhance_basic_tests.exe CWD D:/workspace-github/xpe-post/modules/enhance_basic TIMEOUT_MS 300000 ELAPSED_MS 4551 EXIT 0 ERROR undefined
[==========] Running 194 tests from 21 test suites.
[==========] 194 tests from 21 test suites ran. (4529 ms total)
[  PASSED  ] 194 tests.
```

분모는 빌드별 활성 시험 194개이며 skip/비활성 시험은 없다. 전체 원문은 [debug-full.txt](debug-full.txt), [release-full.txt](release-full.txt)에 있다. 큰 출력은 context-mode에 색인됐으므로 두 실행의 청크 전체를 순서대로 읽어 보존했다. CRLF를 LF로 정규화하고 청크 경계를 단락으로 연결했으며, 새 증거의 줄 끝 공백은 제거했다. 시험 자체를 다시 실행한 것은 아니다.

Debug 실패 5건은 시간 예산 단언이다. 수정 전 전체 Debug 실행은 이 카드에서 하지 않았으므로 “기존 실패”나 “이번 수정이 유발한 실패” 어느 쪽으로도 인과를 단정하지 않는다. 단언·필터·입력을 바꾸지 않았고, 카드 범위 밖의 성능 수정을 하지 않았다.

## 유사 역수 식 검색 — 목록만

`rg`가 기본 PATH/확인 경로에 없어 읽기 전용 조사 에이전트가 Node 파일 순회·정규식 검색으로 대체했다. 해당 위치는 수정 전 기준이다.

| 위치 | 식 | 처리 |
|---|---|---|
| `noise_reduce.cpp:239` | `1.0f / (sigma_space * sigma_space)` | 같은 bilateral 함수이므로 이번 범위에 포함 |
| `noise_reduce.cpp:240` | `-0.5f / (sigma_range * sigma_range)` | 이번 결함 위치 |
| `noise_reduce.cpp:291`(수정 전 282) | `1.0f / (h_param * h_param)` | NLM, 같은 형식의 위험 후보이나 실패는 재현되지 않음. 목록만 기록·수정 없음 |
| `edge_enhance.cpp:124` | `1.0f / (2.0f * sigma * sigma)` | USM, 목록만 기록·수정 없음 |

USM의 sigma는 radius이고 `edge_enhance.cpp:84`에서 유한성과 `[0.5,10]` 범위를 검사한다. 식이 비슷하다는 이유만으로 같은 극소 sigma 결함이라고 판정하지 않았다.

NLM의 제곱 역수에도 같은 유형의 수치 위험이 있지만 NLM 실패는 이번에 재현되지 않았다. 기존 NonFinite 시험은 수정 후 통과했다. 이 관측만으로 NLM의 전체 입력 범위가 안전하다고 결론내리지 않으며, 리더 지시대로 위험 목록만 남기고 NLM은 수정하지 않았다.

## 텍스트 검증과 커밋 차단

`tools/ci/Test-TrackedTextFiles.ps1`을 읽고 같은 규칙을 후보 파일에 적용했다. 스크립트는 tracked 파일을 순회하므로, 새 미추적 증거까지 검사하기 위해 보강 전 소스·보고서·증거 9개를 명시적으로 순회했다. UTF-8로 읽어 충돌 표시, 줄 끝 공백, 소스의 금지 제어문자를 검사했다. 카드 요구보다 넓게 Markdown에도 줄 끝 공백 검사를 적용했다. 원래 규칙처럼 `.txt`는 제어문자 검사에서 제외했다. 보강 후에는 헤더와 underflow 증거를 포함한 11개를 다시 검사했다.

```text
TEXT_RULE_FILES 9 FINDINGS 0 []
git diff --check -- modules/enhance_basic/src/noise_reduce.cpp
EXIT 0
output: (empty)
```

staging 직전 HEAD `0c5b9b7b`, 브랜치 `dev/postprocess`, 기존 staged 경로 없음과 이번 명시적 파일 상태를 다시 확인했다. 명시적 pathspec으로 시도했으나 실패했다.

```text
git add -- modules/enhance_basic/src/noise_reduce.cpp
exit_code: 128
fatal: Unable to create 'D:/workspace-github/image-processing/.git/worktrees/xpe-post/index.lock': Permission denied
```

커밋 SHA는 없다. 다른 경로·도구로 공용 `.git` 쓰기를 우회하지 않았고 push도 실행하지 않았다. 리더가 대신 커밋할 파일 목록:

1. `modules/enhance_basic/src/noise_reduce.cpp`
2. `.moai/reports/lane-post/QA-B-226/report.md`
3. `.moai/reports/lane-post/QA-B-226/before-failure.txt`
4. `.moai/reports/lane-post/QA-B-226/hashes-before.txt`
5. `.moai/reports/lane-post/QA-B-226/hashes-after-boundary.txt`
6. `.moai/reports/lane-post/QA-B-226/build.txt`
7. `.moai/reports/lane-post/QA-B-226/target-after.txt`
8. `.moai/reports/lane-post/QA-B-226/debug-full.txt`
9. `.moai/reports/lane-post/QA-B-226/release-full.txt`
10. `modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h`
11. `.moai/reports/lane-post/QA-B-226/underflow.txt`
12. `.moai/reports/lane-post/QA-B-226/underflow-subnormal.txt`

```text
QA-B-226: bilateral 역수 계수의 float 범위 검사

Refs #284
```

## Baseline-attribution / Gaps / Residual-risk

- 이번 카드 측정 트리는 `D:/workspace-github/xpe-post`, `dev/postprocess`, HEAD `0c5b9b7b`다. 시작 시 enhance_basic 소스 변경은 없었다. 수정 전 실패·해시는 이번 카드에서 직접 확보했고, 수정 후 결과도 같은 트리에서 새로 관측했다.
- MSVC 14.44.35207/Windows SDK 10.0.26100.0, Debug `build/cov-local`과 RelWithDebInfo `build/ci-post`를 사용했다. 두 구성 모두 `/arch:AVX2 /fp:fast`를 유지했다.
- 정상 해시 불변은 위 대표 입력 3종에 대한 빌드별 비교다. 모든 정상 sigma/모든 입력/타 toolchain의 바이트 동일성을 측정한 것은 아니다.
- Debug 전체는 시간 예산 시험 5건 실패 상태다. 이를 전체 통과로 해석하면 안 된다. 리더는 16:32 지시에서 coverage 실행의 시간 시험 제외 정책에 따라 이 5건을 이번 수용 판정에서 제외했다. 실제 측정 결과 자체는 위 원문대로 남겼다.
- API 지원 범위가 리더 정책에 따라 줄어든다. 보강 지시에 따라 공개 API 헤더에 새 거절 조건과 이번 빌드의 최소값을 반영했다. 다른 toolchain의 경계값까지 측정한 것은 아니다.
- 원격 CI/커버리지 재실행은 하지 않았다. 카드의 커밋 단계는 권한 차단으로 리더에게 남았다.
- 기존 `build/cov-local`을 재사용했다. 새 임시 폴더를 만들지 않았고 진단 probe cpp/exe는 이 폴더에 유지하여 리더/Reviewer가 다시 볼 수 있게 했다. staging 대상에는 넣지 않는다. 이전 폴더 삭제 정책 거절을 우회하지 않았다.

보강 후 최종 검증(2026-11-13 16:37 KST):

```text
TEXT_RULE_FILES 11 FINDINGS 0 []
UNDERFLOW_FINITE_CASES 6
git diff --check -- modules/enhance_basic/src/noise_reduce.cpp modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h
EXIT 0, STDOUT "", STDERR ""
git diff --stat -- modules/enhance_basic
 enhance_basic_api.h | 1 +
 noise_reduce.cpp    | 13 +++++++++++--
 2 files changed, 12 insertions(+), 2 deletions(-)
dev/postprocess
0c5b9b7b
```

F226-1 보강 후 재검사(2026-11-13 16:41 KST): `TEXT_RULE_FILES 12 FINDINGS 0 []`, 추가 underflow 호출 12/12 성공·유한 출력, 소스/헤더 `git diff --check` exit 0. 브랜치 `dev/postprocess`, HEAD `0c5b9b7b`를 다시 확인했다. 동작 코드는 이 보강에서 바꾸지 않았고 커밋은 리더 담당으로 유지한다.
