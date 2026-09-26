# QA Gate — enhance_basic

- module: enhance_basic
- build: build/ci-post (Ninja, RelWithDebInfo, /WX=ON)
- branch: dev/postprocess
- sha: e67c125
- measured: 2026-08-28

| # | Gate item | Result | Evidence |
|---|---|---|---|
| 1 | dumpbin /dependents 횡단 의존성 | PASS | xpe_common.dll + MSVC/UCRT/KERNEL32만 의존. 타 xpe_* 모듈 의존 0건 |
| 2 | GTest 100% GREEN | PASS | 74/74 (100% tests passed, 0 failed) |
| 3 | 메모리 누수 1000 프레임 | GAP | 1000회 루프 테스트는 존재·통과하나 힙 사용량을 **측정하지 않음** (crash/corruption 테스트) |
| 4 | /WX 0 warning | PASS | CMakeCache XPE_WARNINGS_AS_ERRORS=ON, 빌드 로그 warning 0건 (모듈 6개 .cpp 전부 신규 컴파일됨) |
| 5 | P/Invoke ABI 심볼 수 일치 | FAIL | DLL export 8개 vs api-spec.md 문서 7개 — `xpe_enhance_basic_version` 누락 (delta +1) |
| 6 | CODEOWNERS 경계 | PASS(공허참) | CODEOWNERS에 `/modules/enhance_basic/ @holee9` 존재. 단 dev/postprocess는 main 대비 커밋 0건이라 경계 위반 여지 자체가 없음 |

## Evidence (verbatim)

### 1. 횡단 의존성

```
> dumpbin /nologo /dependents build\ci-post\bin\xpe_enhance_basic.dll

  Image has the following dependencies:

    xpe_common.dll
    MSVCP140.dll
    VCRUNTIME140.dll
    VCRUNTIME140_1.dll
    api-ms-win-crt-math-l1-1-0.dll
    api-ms-win-crt-runtime-l1-1-0.dll
    api-ms-win-crt-stdio-l1-1-0.dll
    api-ms-win-crt-heap-l1-1-0.dll
    KERNEL32.dll
```

허용된 `xpe_common.dll` 외에 `xpe_*` 모듈 의존 없음. → PASS

### 2. GTest

모듈 소속 테스트 식별: `ctest --show-only=json-v1`에서 실행 파일이
`bin/xpe_enhance_basic_tests.exe`인 항목만 추출 → 74건, 7개 suite
(ContrastEnhance, EdgeEnhance, Enh01LogNoise, EnhanceIntegration,
ExposureIndex, LogTransform, NoiseReduce).

```
> ctest --test-dir build\ci-post -R "^(ContrastEnhance|EdgeEnhance|Enh01LogNoise|EnhanceIntegration|ExposureIndex|LogTransform|NoiseReduce)\." --output-on-failure

100% tests passed, 0 tests failed out of 74

Total Test time (real) =   2.04 sec
```

전체 출력: `tests.log`

주의 — 정규식은 `\.` 앵커로 enhance_advanced의 동명 유사 suite
(`EdgeEnhancementTest.`, `ExposureIndexTest.`)를 배제한다.

### 3. 메모리 누수 1000 프레임

테스트는 존재하며 통과했다(Test #126).

```
> ctest ... -R "EnhanceIntegration\."
68/74 Test #126: EnhanceIntegration.NoHeapLeak_1000Iterations ....   Passed    0.01 sec
```

그러나 소스를 읽어보면 누수를 **측정하지 않는다**:

```cpp
// modules/enhance_basic/tests/test_enhance_integration.cpp:212
TEST(EnhanceIntegration, NoHeapLeak_1000Iterations) {
    auto img = make_f32(64, 64, 500.0f);

    for (int i = 0; i < 1000; i++) {
        float* px = static_cast<float*>(img.data);
        std::fill(px, px + 64 * 64, 500.0f);

        XpeErrorCode rc = xpe_log_transform(&img, 1000.0f);
        ASSERT_EQ(XPE_OK, rc) << "Iteration " << i << " failed";
    }

    // If we got here without crash/corruption, no double-free or heap corruption
    free_img(img);
}
```

세 가지 이유로 게이트 항목을 충족하지 못한다:

1. 힙/RSS 델타에 대한 assertion이 전혀 없다 — 통과 조건은 "크래시하지 않음"뿐이다.
2. 8개 export 중 `xpe_log_transform` 하나만 1000회 반복한다. 나머지 7개 함수의 반복 생명주기는 검증되지 않는다.
3. 이미지가 64×64이고 버퍼를 루프 밖에서 1회만 할당한다. "1000 프레임"이라 부를 만한 프레임 단위 할당/해제 사이클이 아니다(0.01초 소요가 이를 뒷받침한다).

따라서 "1000 프레임 누수 없음"은 **미검증**으로 기록한다.
누수 테스트를 새로 작성하지는 않았다(측정 전용 과제이므로 지시대로 보류).

참고: 피크 메모리를 실제로 측정하는 테스트는 별도 실행 파일에 존재한다
(`FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB`, `test_e2e_post_pipeline.exe`).
이는 enhance_basic 모듈 단독 게이트 증거가 아니라 파이프라인 E2E 증거다.

### 4. /WX 0 warning

```
> grep -i "XPE_WARNINGS_AS_ERRORS" build/ci-post/CMakeCache.txt
XPE_WARNINGS_AS_ERRORS:BOOL=ON

> grep -ci warning .moai/reports/lane-post/QA-B-01/_build_post.log
0
```

이 로그에서 모듈 소스가 실제로 신규 컴파일되었다(캐시 히트가 아니다):

```
[3/89] Building CXX object modules\enhance_basic\...\src\exposure_index.cpp.obj
[4/89] Building CXX object modules\enhance_basic\...\src\enhance_basic.cpp.obj
[5/89] Building CXX object modules\enhance_basic\...\src\log_transform.cpp.obj
[6/89] Building CXX object modules\enhance_basic\...\src\noise_reduce.cpp.obj
[7/89] Building CXX object modules\enhance_basic\...\src\edge_enhance.cpp.obj
[8/89] Building CXX object modules\enhance_basic\...\src\contrast_enhance.cpp.obj
[77/89] Linking CXX shared library bin\xpe_enhance_basic.dll
```

DLL 타깃 기준 PASS.

caveat — 테스트 타깃은 /WX를 명시적으로 해제한다:

```cmake
# modules/enhance_basic/CMakeLists.txt:127
target_compile_options(xpe_enhance_basic_tests PRIVATE /W4 /WX-)
```

즉 `/WX` 0-warning 보장은 DLL 소스에만 적용되고 테스트 코드에는 적용되지 않는다.
전체 로그 warning이 0건이므로 실제 테스트 경고도 0건이지만, 이는 게이트가 강제한
결과가 아니라 관측된 사실이다.

### 5. P/Invoke ABI 심볼 수

DLL 실측 export — 8개:

```
> dumpbin /nologo /exports build\ci-post\bin\xpe_enhance_basic.dll

           8 number of functions
           8 number of names

    ordinal hint RVA      name
          1    0 0000118B xpe_calc_exposure_index
          2    1 00001398 xpe_contrast_enhance
          3    2 00001483 xpe_edge_enhance
          4    3 00001190 xpe_enhance_basic_version
          5    4 00001488 xpe_log_inverse
          6    5 000012D0 xpe_log_transform
          7    6 000011D6 xpe_noise_estimate_sigma
          8    7 00001258 xpe_noise_reduce
```

공개 헤더 선언 — 8개(DLL과 일치):

```
> grep -rn "XPE_API" modules/enhance_basic/include/
enhance_basic_api.h:83  xpe_enhance_basic_version
enhance_basic_api.h:99  xpe_log_transform
enhance_basic_api.h:110 xpe_log_inverse
enhance_basic_api.h:126 xpe_noise_reduce
enhance_basic_api.h:137 xpe_noise_estimate_sigma
enhance_basic_api.h:153 xpe_contrast_enhance
enhance_basic_api.h:170 xpe_edge_enhance
enhance_basic_api.h:189 xpe_calc_exposure_index
```

문서 — 7개:

```
> sed -n '155,160p' docs/project/api-spec.md
| DLL | Exported Functions | Notes |
|-----|--------------------|----|
| xpe_common.dll | 15 | ...
| xpe_preprocess.dll | 18 | no change |
| xpe_enhance_basic.dll | 7 | includes `xpe_calc_exposure_index` moved from enhance_advanced |
```

`docs/project/api-spec.md` §7 하위 절도 7.1~7.7 총 7개이며
`xpe_enhance_basic_version`에 대응하는 절이 없다. 문서 내 `_version` 함수 절은
`5.3 xpe_version`(common)과 `12.3 gsvg_version`(gsvg)뿐이라, 모듈별 version 함수를
카운트에서 일괄 제외하는 규약도 아니다(common·gsvg는 각자 카운트에 포함되어 있다).

→ **MISMATCH, delta +1**: `xpe_enhance_basic_version`이 DLL·헤더에는 있으나
api-spec.md §4 카운트(7)와 §7 함수 명세 양쪽에서 누락. 문서 총계 79도
1 과소 계상 가능성이 있다(다른 모듈은 미검증 — Gaps 참조).

`enhance_basic_internal.h:30`의 `xpe_test_inject_alert`는 이 delta와 무관하다.
주석이 명시하듯 `xpe_common.dll`에서 export되는 심볼을 내부 사용 목적으로
재선언한 것이며, enhance_basic DLL export 목록에는 나타나지 않는다(확인함).

### 6. CODEOWNERS 경계

```
> cat CODEOWNERS
# Lane B: Postprocessing (Claude)
# Branch: dev/postprocess | Worktree: xpe-post
/modules/enhance_basic/    @holee9
...
```

모듈 경로 소유 엔트리 확인됨.

레인 변경 범위:

```
> git diff --stat main...HEAD -- modules/enhance_basic
(빈 출력)

> git diff --stat main...HEAD
(빈 출력)

> git rev-list --count --left-right main...HEAD
2	0        # main만 2개 앞섬, HEAD 고유 커밋 0개

> git status --short
(빈 출력)
```

dev/postprocess는 main 대비 **자체 커밋이 0건**이고 워킹트리도 깨끗하다.
따라서 "자기 경로만 건드렸다"는 조건은 참이지만 **공허하게 참**이다 —
아직 아무것도 건드리지 않았으므로 경계 규율이 시험된 적이 없다.

## Gaps (미검증)

- **1000 프레임 메모리 누수**: 실제 힙/RSS 측정이 이루어진 적이 없다. 기존 테스트는
  크래시 부재만 확인한다. 8개 export 중 7개는 반복 생명주기 검증이 전무하다.
- **문서 총계 79의 정확성**: enhance_basic에서 +1 delta를 확인했으나, 나머지 7개 모듈의
  export 수 대 문서 수 대조는 이번 과제 범위 밖이라 수행하지 않았다. 총계가 80이어야
  하는지는 미검증이다.
- **P/Invoke 실제 호출 검증**: 심볼 *개수*와 *이름* 일치만 대조했다. C# 측 DllImport
  선언의 시그니처(파라미터 타입·호출 규약·구조체 정렬)와 DLL ABI의 일치 여부는
  확인하지 않았다. `EnhanceIntegration.StructSizes_PInvokeCompatible`가 통과하지만
  이는 C++ 측 sizeof 검사이지 C# 바인딩 대조가 아니다.
- **CODEOWNERS 경계의 실효성**: 커밋 0건이라 규율이 시험되지 않았다.
- **테스트 타깃 /WX**: `/WX-`로 해제되어 있어 향후 테스트 코드 경고는 빌드를 깨뜨리지
  않는다. 이번 로그가 0건인 것은 관측 사실일 뿐 게이트의 보장이 아니다.
- **커버리지 수치**: 이번 과제에 포함되지 않아 측정하지 않았다.

## Residual risk

- **item 3이 가장 큰 위험이다.** 게이트 표의 "메모리 누수 1000 프레임"을 기존 테스트
  이름(`NoHeapLeak_1000Iterations`)만 보고 PASS 판정하면 거짓 통과가 된다. 이름이
  주장하는 바와 테스트가 검증하는 바가 다르다. 실제 누수가 있어도 이 테스트는
  초록으로 남는다.
- **item 5는 코드 결함이 아니라 문서 결함일 개연성이 높다.** `xpe_enhance_basic_version`은
  다른 모듈과 동일한 version 함수 패턴이므로 DLL이 옳고 api-spec.md가 갱신 누락일
  가능성이 크다. 다만 어느 쪽이 정본인지는 판단하지 않았다 — 소스 수정 금지 과제이며,
  정본 결정은 문서 소유자의 몫이다.
- **측정 시점 구속**: 모든 증거는 sha e67c125, build/ci-post 스냅샷 기준이다.
  재빌드·리베이스 이후에는 재측정이 필요하다.
- **성능 테스트의 환경 의존성**: `Performance_3072x3072_Within50ms` 등 시간 예산
  테스트가 다수 포함되어 있다. 이번 실행은 통과했으나 부하가 걸린 머신에서는
  간헐 실패할 수 있다(코드가 아니라 머신을 측정하는 테스트다).
