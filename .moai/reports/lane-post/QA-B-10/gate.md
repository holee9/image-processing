# QA-B-10 — `tests/enhance_advanced_tests` 103건 흡수 (#113 2단계)

**레인**: Lane B (`dev/postprocess`)
**결과**: 7파일 이동·등록 완료. **169건 실행(66 + 103, 기대치 일치), 167 통과 / 2 실패.**
실패 2건은 지시대로 **고치지 않고 분류만** 했다(§4).

---

## 1. 주장 (Claim)

1. 7개 파일을 `modules/enhance_advanced/tests/` 로 이름 충돌 없이 이동했다(`_ext` 접미사).
2. gtest 픽스처 충돌 4건을 사본 쪽 개명으로 해소했다. **단언은 바꾸지 않았다.**
3. 모듈 등록에 7소스를 추가하고 라벨을 **이스케이프 형식**으로 넣었다 — 3개 전부 적용됨(관측).
4. 실행 결과 **169건 = 66 + 103**, 기대치와 정확히 일치. 167 통과 / 2 실패.
5. 실패 2건은 **모듈 결함이 아니라 테스트 기대치 문제**로 분류된다(§4, 각각 반증 증거 포함).
6. 옛 디렉터리를 삭제했다. `ci-post` generate 통과.

---

## 2. 1단계 — 이동과 충돌 해소

### 2.1 파일 이동 (`git mv`, 7건)

| 원본 | 이동 후 |
|---|---|
| `tests/enhance_advanced_tests/test_api_header.cpp` | `modules/enhance_advanced/tests/test_api_header_ext.cpp` |
| `…/test_lifecycle.cpp` | `…/test_lifecycle_ext.cpp` |
| `…/test_mfp_scalar.cpp` | `…/test_mfp_scalar_ext.cpp` |
| `…/test_collimation_detect.cpp` | `…/test_collimation_detect_ext.cpp` |
| `…/test_edge_enhancement.cpp` | `…/test_edge_enhancement_ext.cpp` |
| `…/test_exposure_index.cpp` | `…/test_exposure_index_ext.cpp` |
| `…/test_integration.cpp` | `…/test_integration_ext.cpp` |

`git status` 가 7건 모두 `R`(rename)로 인식한다 — 이력이 끊기지 않는다.

### 2.2 픽스처 충돌 4건 — 사본 쪽 개명

이동 전 양쪽 픽스처를 대조했다. 4개가 겹쳤다:

| 겹친 이름 | 개명 (사본 쪽) | 치환 식별자 수 |
|---|---|---|
| `CollimationDetectTest` | `CollimationDetectExtTest` | 15 |
| `EdgeEnhancementTest` | `EdgeEnhancementExtTest` | 22 |
| `ExposureIndexTest` | `ExposureIndexExtTest` | 16 |
| `MfpScalarTest` | `MfpScalarExtTest` | 19 |

개명은 **필수**다. gtest 는 스위트를 이름 문자열로 등록하므로, 같은 이름에 서로 다른
픽스처 클래스가 붙으면 "All tests in the same test suite must use the same test fixture class"
로 런타임에 죽는다. 익명 네임스페이스로 C++ ODR 을 피해도 이 규칙은 남는다.

겹치지 않은 4개(`ApiHeaderTest`, `LifecycleTest`, `NotInitializedGuardTest`,
`IntegrationPipelineTest`)는 **그대로 뒀다** — 카드 지시는 "충돌하면 개명" 이고,
불필요한 개명은 diff 를 키울 뿐이다.

개명 후 재대조 결과 모듈 쪽 7개 픽스처
(`EnhanceAdvancedApiHeaderTest`, `EnhanceAdvancedLifecycleTest`, `CollimationDetectTest`,
`EdgeEnhancementTest`, `ExposureIndexTest`, `IntegrationTest`, `MfpScalarTest`)와
겹치는 이름이 없다.

### 2.3 헬퍼 충돌 — 없음 (확인함)

이동한 7파일 중 5개는 헬퍼를 익명 네임스페이스로 감싼다
(`MakeConstantImage`, `FreeImageBuffer`, `MakeMeta` 등이 파일마다 중복 정의돼 있어
감싸지 않았다면 링크가 깨졌을 것이다). 감싸지 않은 2개(`test_api_header`,
`test_lifecycle`)는 자유 함수를 하나도 정의하지 않는다 — 픽스처 클래스 선언뿐이다.

모듈 쪽 7파일은 익명 네임스페이스를 전혀 쓰지 않지만, 사본 쪽이 감싸고 있으므로
교차 충돌이 없다. **빌드가 이를 확증한다**(§3, 링크 성공).

**단언 내용은 한 줄도 바꾸지 않았다.** 치환은 픽스처 식별자에 한정된다.

---

## 3. 2단계 — 등록과 라벨

### 3.1 소스 등록

`modules/enhance_advanced/CMakeLists.txt` 의 `add_executable(test_${MODULE_NAME} …)` 에
7소스를 추가하고, 왜 두 벌이 공존하는지 주석으로 남겼다.

### 3.2 라벨 — 이스케이프 형식 (QA-B-09 교훈 적용)

원본 `tests/enhance_advanced_tests/CMakeLists.txt:57-60` 값을 확인해 그대로 옮기되,
세미콜론은 이스케이프했다:

```cmake
gtest_discover_tests(test_${MODULE_NAME}
    PROPERTIES
        LABELS "unit\;enhance_advanced\;SPEC-XPE-P2-ADV"
        TIMEOUT 300
)
```

관측(`_verify.log`):

```
All Labels:
  SPEC-XPE-P2-ADV
  enhance_advanced
  unit

ctest -N -L "SPEC-XPE-P2-ADV"  ->  Total Tests: 169
```

3개 전부 적용됐고 169건 전체가 SPEC 라벨을 갖는다. 이스케이프하지 않았다면
QA-B-09 의 ai 사례처럼 `unit` 하나만 남았을 것이다.

### 3.3 빌드

```
===BUILD_EXIT=0===
warning / error : 0건
```
로그: `_build.log`

이 타깃은 QA-B-06 에서 경고-오류화(`/WX`)로 전환된 상태다. 즉 **한 번도 빌드된 적 없던
103건이 `/WX` 를 켠 채로 경고 0으로 통과했다** — 흡수가 새 경고를 들여오지 않았다.

---

## 4. 3단계 — 실행 결과와 실패 분류

### 4.1 카운트

```
[==========] 169 tests from 15 test suites ran. (2665 ms total)
[  PASSED  ] 167 tests.
[  FAILED  ] 2 tests
```
로그: `_run.log`

**169 = 66(모듈) + 103(흡수)** — 카드의 기대치와 정확히 일치한다. 손실·중복 0건.

### 4.2 실패 2건 — 분류 (수정하지 않음)

#### 실패 1: `MfpScalarExtTest.EmptyConfigStringUsesDefaults`

```
test_mfp_scalar_ext.cpp(300): error: Expected equality of these values:
  xpe_multiscale_process(&img, &meta, "")
    Which is: -4        (XPE_ERR_CONFIG_INVALID)
  0                     (XPE_OK)
```

**분류: 테스트 기대치 오류.** 근거 두 가지 —

1. **헤더 계약과 어긋난다.** 파라미터명이 `configJsonOrNull` 이다
   (`xpe_enhance_advanced_api.h:50`, "Optional JSON configuration"). "설정 없음" 의
   센티널은 `NULL` 이지 `""` 가 아니다. 빈 문자열은 유효한 JSON 이 아니므로
   `XPE_ERR_CONFIG_INVALID` 는 계약에 부합한다.
2. **같은 스위트가 스스로를 반박한다.** 흡수한 `test_lifecycle_ext.cpp:39-43` 이
   `LifecycleTest.InitWithEmptyStringReturnsConfigInvalid` 로 `""` → `CONFIG_INVALID`
   를 단언하며, 주석까지 `// Empty string (not NULL) is invalid per implementation`
   라고 적혀 있다. 그리고 이 테스트는 **통과한다.**
   즉 사본 스위트 안에서 `""` 의 의미가 두 갈래로 갈려 있다.

모듈 동작을 바꿀 근거는 없다. 고치려면 이 케이스의 기대치를 `CONFIG_INVALID` 로
바꾸는 것이 맞아 보이나 **이 카드에서는 손대지 않았다.**

#### 실패 2: `IntegrationPipelineTest.ErrorPathsCovered` (단언 3건)

```
test_integration_ext.cpp(270): xpe_multiscale_process(nullptr,nullptr,nullptr)
    Which is: -1 (INVALID_INPUT)   expected -6 (NOT_INITIALIZED)
test_integration_ext.cpp(271): xpe_fractional_process(nullptr,1.0f,nullptr)
    Which is: -1                   expected -6
test_integration_ext.cpp(285): xpe_multiscale_process(&img,nullptr,nullptr)  // img.format=UINT16
    Which is: -1                   expected -7 (UNSUPPORTED_FORMAT)
```

**분류: 테스트의 오류코드 우선순위 가정 오류.** 세 단언 모두 "널 인자" 를 준 채
다른 코드를 기대한다. 모듈은 **인자 검사 → 초기화 검사 → 포맷 검사** 순으로 보고,
널이 있으면 그 자리에서 `INVALID_INPUT` 을 낸다.

**"초기화 가드가 없다" 는 해석은 반증된다.** 같은 흡수분의
`test_lifecycle_ext.cpp:120-131` `NotInitializedGuardTest.*` 4건은 **널이 아닌 인자**로
미초기화 상태를 찔러 `-6` 을 받고 **전부 통과한다**:

```
[  OK ] NotInitializedGuardTest.MultiscaleProcessReturnsNotInitialized
[  OK ] NotInitializedGuardTest.FractionalProcessReturnsNotInitialized
[  OK ] NotInitializedGuardTest.DetectCollimationReturnsNotInitialized
[  OK ] NotInitializedGuardTest.CalcExposureIndexReturnsNotInitialized
```

초기화 가드도 포맷 검사도 존재한다. 다른 것은 **어느 검사가 먼저 발화하느냐** 뿐이다.

다만 이것을 "테스트가 틀렸다" 로 닫는 것이 항상 옳지는 않다 — "널 포인터보다
미초기화가 더 상위 오류인가" 는 API 계약 설계 문제다. 헤더는 우선순위를 명시하지
않는다. **모듈 결함으로 볼 여지는 낮지만, 계약 명시가 없다는 점은 남는다** —
후속 판단 대상으로 올린다.

### 4.3 두 실패 모두 흡수분에서 나왔다

기존 모듈 66건은 전부 통과한다(`ctest` 실패 목록이 `*ExtTest` /
`IntegrationPipelineTest` 두 건뿐). 흡수가 기존 스위트를 깨뜨리지 않았다.

---

## 5. 4·5단계 — 삭제와 검증

`git rm -r tests/enhance_advanced_tests` (이동 후 남은 `CMakeLists.txt` 1건 포함).
`tests/` 아래에 `enhance_advanced_tests/` 가 더 이상 없다.

루트 `CMakeLists.txt` 와 `tests/CMakeLists.txt` 는 **건드리지 않았다**(카드 금지 사항).
`tests/CMakeLists.txt` 는 이 워크트리에 아직 남아 있고 `add_subdirectory(enhance_advanced_tests)`
를 참조하지만, main 이 그 파일을 폐기했다고 들었으므로 레인에서 손대지 않는다.
**merge 하지 않은 이 워크트리에서 집계를 켜면 QA-B-07 §5 와 같은 종류의 오류가 난다** —
main 쪽 처리를 전제로 한다.

검증(`_verify.log`):

| 항목 | 결과 |
|---|---|
| `cmake --preset ci-post` | `Configuring done`, `CFG_EXIT=0` |
| `ctest --print-labels` | `SPEC-XPE-P2-ADV`, `enhance_advanced`, `unit` (3개) |
| `ctest -N -L "SPEC-XPE-P2-ADV"` | **169** |
| `ctest` 전체 (ci-post) | **383 passed / 2 failed out of 385**, `ALL_EXIT=8` |

실패 2건은 §4.2 의 그 두 건이며, ctest 목록에도 라벨이 함께 찍힌다:

```
238 - MfpScalarExtTest.EmptyConfigStringUsesDefaults (Failed) SPEC-XPE-P2-ADV enhance_advanced unit
302 - IntegrationPipelineTest.ErrorPathsCovered (Failed) SPEC-XPE-P2-ADV enhance_advanced unit
```

---

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` ctest 총계 | 282 (`QA-B-09/gate.md` §4) | **385** | +103 = 흡수분 정확히 일치 |
| `ci-post` 실패 | 0 | **2** | 신규 — 결함 발견(§4.2), 회귀 아님 |
| enhance_advanced 케이스 | 66 (`QA-B-09/gate.md` §4) | **169** | +103 |
| enhance_advanced 실패 | 0 (66건 기준) | **0** (기존 66건 기준) | 동일 (§4.3) |
| SPEC-XPE-P2-ADV 라벨 | 0 (라벨 없음) | **169** | 신규 |
| `ci-post` generate | 통과 | 통과 | 동일 |

> `ALL_EXIT=8` 은 ctest 가 실패 테스트를 보고한 정상 종료 코드다. 빌드는 성공했다
> (`BUILD_EXIT=0`).

## 7. 미검증 (Gaps)

- **실패 2건의 근본 원인을 코드로 추적하지 않았다.** 분류 근거는 헤더 계약, 같은
  스위트의 상반된 통과 케이스, 관측된 반환값이다. `xpe_multiscale_process` 구현의
  검사 순서를 소스로 확인하지는 않았다 — 카드가 "분류만" 을 지시했다.
- **오류코드 우선순위의 정본 계약 미확인.** 헤더에 명시가 없고 SPEC 문서
  (`SPEC-XPE-P2-ADV`)를 뒤지지 않았다. §4.2 의 "테스트 기대치 오류" 분류는
  이 확인 없이는 잠정이다.
- **흡수분과 모듈 66건의 의미 중복도 여전히 미정량.** 이름 겹침 0은 확정이나
  같은 것을 다른 이름으로 두 번 검증하는 부분이 얼마인지는 모른다. 169건이 도는
  비용(2.7초)은 문제가 아니므로 이번에는 다루지 않았다.
- **`ci-ai` / `ci-dicom` 미재실행.** 이 카드는 enhance_advanced 만 건드리므로 영향이
  없다고 판단했고, 관측하지 않았다.
- **`TIMEOUT 300` 적정성 미검증.** 원본 값을 그대로 옮겼다. 실측 최장 케이스는
  전체 169건이 2.7초이므로 여유가 크다.
- **비-MSVC / Linux 미검증.**

## 8. 잔여 위험 (Residual risk)

- **`ci-post` 가 이제 빨간불이다.** 실패 2건은 "발견" 이지만, 고쳐지기 전까지
  CI 게이트가 이 브랜치에서 실패한다. 후속 카드 전까지 이 상태가 유지된다는 점을
  통합 시점에 고려해야 한다.
- `_ext` 접미사와 `*ExtTest` 픽스처는 "두 벌이 공존한다" 는 사실을 이름에 남긴다.
  나중에 중복을 정리하면 이 접미사도 함께 정리 대상이 된다.
- §4.2 실패 2 를 "테스트가 틀렸다" 로 닫고 기대치만 고치면, 오류코드 우선순위가
  계약으로 명시되지 않은 채 굳는다. 명시가 먼저다.

---

## 9. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 이동 diff | PASS | §2.1 (7건 `R` rename), §2.2 개명표 |
| 실행 결과 (통과/실패 목록 + 분류) | PASS | §4.1 169=66+103, §4.2 실패 2건 분류 + 반증 증거 |
| 라벨 `-L` 카운트 | PASS | §3.2 / §5 — 라벨 3개, `-L SPEC-XPE-P2-ADV` = 169 |
| ctest 재실측 | PASS | §5 385건, §6 baseline 대조 |
| footer `Refs #113` | PASS | 커밋 메시지 |
