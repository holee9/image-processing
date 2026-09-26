# QA-B-09 — `tests/enhance_advanced_tests` 처분 + SPEC 라벨 이식 (#113 2단계)

**레인**: Lane B (`dev/postprocess`)
**결론 요약**:
- **2단계(디렉터리 삭제)는 수행하지 않았다** — 카드의 삭제 조건이 성립하지 않는다(§2).
- **3단계(ai 라벨 이식)는 수행했고, 이식 과정에서 원본의 결함을 하나 발견해 고쳤다**(§3).

---

## 1. 주장 (Claim)

1. `tests/enhance_advanced_tests/` 는 `modules/enhance_advanced/tests/` 의 **사본이 아니다.**
   같은 파일명 7쌍이지만 내용이 모두 다르고, **테스트 케이스 이름이 한 건도 겹치지 않는다.**
2. 사본 쪽이 103건, 모듈 쪽이 66건이며 **모듈은 상위집합이 아니다** → 카드가 정한
   삭제 조건("동일하거나 모듈이 상위집합") 불성립 → **삭제 보류, leader 판정 요청.**
3. 7개 파일은 현재 헤더로 **전부 컴파일된다** — 낡아서 못 쓰는 코드가 아니다.
4. `modules/ai` 등록에 `LABELS` / `TIMEOUT` 을 이식했고, **원본 그대로 옮겼다면 라벨 3개 중
   1개만 적용됐을 결함**을 발견해 고쳤다(관측).
5. `ci-post` generate 통과, ai 108/108 · enhance_advanced 66건 · ci-post 282/282 재실측.

---

## 2. 1단계 — 7쌍 대조. "사본" 전제가 틀렸다

### 2.1 파일 단위 diff

| 파일 | `tests/enhance_advanced_tests/` | `modules/enhance_advanced/tests/` | 판정 |
|---|---|---|---|
| `test_api_header.cpp` | 144 L | 54 L | 다름 |
| `test_collimation_detect.cpp` | 304 L | 435 L | 다름 |
| `test_edge_enhancement.cpp` | 362 L | 741 L | 다름 |
| `test_exposure_index.cpp` | 319 L | 610 L | 다름 |
| `test_integration.cpp` | 294 L | 708 L | 다름 |
| `test_lifecycle.cpp` | 183 L | 125 L | 다름 |
| `test_mfp_scalar.cpp` | 389 L | 323 L | 다름 |

**7쌍 모두 다르고, 3쌍은 사본 쪽이 더 크다.** 로그: `_diff_summary.txt`.

### 2.2 테스트 케이스 이름 대조 — 겹침 0건

라인 수는 판정 근거가 못 되므로 `TEST` / `TEST_F` 선언을 뽑아 집합으로 비교했다.

| 파일 | 사본 케이스 | 모듈 케이스 | 이름 겹침 |
|---|---|---|---|
| `test_api_header` | 12 | 3 | **0** |
| `test_collimation_detect` | 14 | 12 | **0** |
| `test_edge_enhancement` | 21 | 11 | **0** |
| `test_exposure_index` | 15 | 9 | **0** |
| `test_integration` | 8 | 10 | **0** |
| `test_lifecycle` | 15 | 8 | **0** |
| `test_mfp_scalar` | 18 | 13 | **0** |
| **합계** | **103** | **66** | **0** |

픽스처명이 달라서(`ApiHeaderTest` vs `EnhanceAdvancedApiHeaderTest`) 이름이 안 맞는 것일
수 있으므로, 픽스처를 떼고 **메서드명만 소문자로 정규화해 재비교**했다 — 그래도 **겹침 0건**이다.

두 트리는 "원본과 사본" 이 아니라 **같은 모듈을 대상으로 독립적으로 작성된 별개의 스위트**다.
전체 목록: `_testcase_inventory.txt`.

### 2.3 사본만 가진 것 (leader 판정용 — 이식하지 않았다)

이름이 하나도 안 겹치므로 "사본만 가진 케이스" 는 형식상 103건 전부다. 그중 모듈 쪽에
**대응 개념 자체가 안 보이는** 것을 성격별로 추린다:

- **ABI 레이아웃 단언** (`test_api_header`, 사본 12건 중) —
  `XpeImageBufferSizeIs40Bytes`, `XpeImageMetadataSizeIs96Bytes`,
  `XpeImageBufferFieldOffsets`, `XpeImageMetadataFieldOffsets`, `XpePixelFormatSizeIs4Bytes`.
  모듈 쪽 3건은 `ApiFunctionsDeclared` / `CommonTypesUsable` / `FunctionPointersExist` 로
  선언 존재만 본다. **구조체 크기·오프셋 고정은 P/Invoke 경계 보증인데 모듈 스위트에 없다.**
- **미초기화 가드 개별 검증** (`test_lifecycle`) — 사본은
  `NotInitializedGuardTest.{MultiscaleProcess,FractionalProcess,DetectCollimation,CalcExposureIndex}ReturnsNotInitialized`
  4개 함수를 각각 확인한다. 모듈은 `ProcessingBeforeInit` 1건으로 묶는다.
- **설정 파싱 실패 경로** (`test_lifecycle`) — 사본의
  `InitWithMalformedJsonReturnsConfigInvalid`, `InitWithEmptyStringReturnsConfigInvalid`,
  `DoubleShutdownDoesNotCrash`, `VersionStringIsStable`.

반대로 모듈 쪽 `test_integration`(10) 이 사본(8)보다 많은 등, **양방향으로 서로 없는 것이 있다.**

카드 지시대로 **이식하지 않았고 삭제도 하지 않았다.**

### 2.4 사본이 지금도 유효한 코드인가 — 컴파일된다

"어차피 낡아서 못 쓰는 코드" 라면 판단이 쉬워지므로 확인했다. 7개 소스를 현재 헤더
(`modules/enhance_advanced/include`, `modules/common/include`, gtest include)로
`cl /c /W4 /std:c++17` 컴파일했다:

```
===EXIT_test_api_header=0===          ===EXIT_test_exposure_index=0===
===EXIT_test_collimation_detect=0===  ===EXIT_test_integration=0===
===EXIT_test_edge_enhancement=0===    ===EXIT_test_lifecycle=0===
                                      ===EXIT_test_mfp_scalar=0===
```
로그: `_compile_probe.log`. **7건 전부 성공.** API 드리프트가 없다.

즉 지금 디렉터리를 삭제하면 **컴파일 가능한 테스트 케이스 103건이 사라진다.**
그중 얼마가 실제로 통과하는지는 미검증이다(§5).

### 2.5 판정 요청

카드의 조건문은 "동일하거나 모듈이 상위집합이면 삭제" 다. 둘 다 아니므로 삭제하지 않았다.
선택지는 leader 몫이다 — (a) 그대로 삭제(103건 포기), (b) 모듈에 없는 케이스를 모듈
스위트로 이식한 뒤 삭제, (c) 유지하고 타깃명 충돌만 별도 해소.
**(b) 를 고르면 별도 카드가 필요하다** — 픽스처·헬퍼가 달라 단순 복사로 끝나지 않는다.

---

## 3. 3단계 — `modules/ai` 라벨 이식과, 그 과정에서 발견한 결함

QA-B-07 에서 삭제한 `tests/ai_tests/CMakeLists.txt:57-63` 이 갖고 있던
`LABELS "unit;ai;SPEC-XPE-P3-AI"` / `TIMEOUT 30` 을 라이브 등록으로 옮겼다.

### 3.1 그대로 옮겼더니 라벨 3개 중 1개만 적용됐다

원본 문법을 그대로 이식하고 재빌드한 뒤 확인한 결과:

```
ctest --print-labels
All Labels:
  unit                 <- 이것뿐

ctest -N -L "SPEC-XPE-P3-AI"  ->  Total Tests: 0
```

생성된 디스커버리 파일을 열어 보니 원인이 보였다:

```
set_tests_properties([=[AiAbi.VersionReturnsNonNull]=] PROPERTIES ... LABELS unit ai SPEC-XPE-P3-AI TIMEOUT 30)
```

`"unit;ai;SPEC-XPE-P3-AI"` 가 CMake 리스트로 **펼쳐져** 인자 3개가 됐고, `LABELS` 에는
첫 원소만 묶였다. 나머지 둘은 `set_tests_properties` 의 다음 인자로 흘러갔다.

**이 결함은 원본 파일이 한 번도 빌드된 적이 없어서 드러난 적이 없다** — QA-B-07 에서
확인한 대로 `tests/ai_tests/CMakeLists.txt` 는 어떤 구성에서도 읽히지 않았다.
"원본에 있으니 옳다" 로 그대로 옮겼다면 라벨 3개 중 1개만 붙은 채 "이식 완료" 가 됐을 것이다.

### 3.2 수정과 검증

세미콜론을 이스케이프했다 (`LABELS "unit\;ai\;SPEC-XPE-P3-AI"`) — 사유를 주석으로 남겼다.

```
ctest --print-labels
All Labels:
  SPEC-XPE-P3-AI
  ai
  unit

ctest -N -L "SPEC-XPE-P3-AI"  ->  Total Tests: 108
```
로그: `_relabel.log`. 108건 전부가 SPEC 라벨을 갖는다.

### 3.3 `enhance_advanced` 라벨은 이식하지 않았다

카드에서 이 이식은 **디렉터리 삭제와 짝**이다("사본만 가진 CTest 속성이 있으면 … 이식").
삭제를 보류했으므로 사본의 `LABELS "unit;enhance_advanced;SPEC-XPE-P2-ADV"` / `TIMEOUT 300`
(`tests/enhance_advanced_tests/CMakeLists.txt:57-60`)도 이식하지 않았다 —
지금 이식하면 삭제하지 않은 파일과 중복 정의가 된다. §2.5 판정 후 처리할 사안이다.

**다만 §3.1 의 결함은 이 파일에도 동일하게 존재한다**(같은 세미콜론 문법). 나중에
이식할 때 그대로 복사하면 같은 방식으로 라벨이 유실된다 — 판정 시 함께 고려할 것.

---

## 4. 4단계 — 검증 (재실측)

로그: `_verify.log`

| 항목 | 결과 |
|---|---|
| `cmake --preset ci-post` | `POST_CFG_EXIT=0` |
| `cmake --build build\ci-ai` | `AI_BUILD_EXIT=0` |
| `ctest -R "Ai\|AI"` (ci-ai) | **108 passed / 0 failed**, `AI_TEST_EXIT=0` |
| `ctest -N -L "SPEC-XPE-P3-AI"` | **Total Tests: 108** |
| `ctest -R "EnhanceAdvanced"` (ci-post) | 12 passed / 0 failed, `ENH_EXIT=0` |
| `test_xpe_enhance_advanced.exe --gtest_list_tests` | **66 케이스** |
| `ctest` 전체 (ci-post) | **282 passed / 0 failed**, `POST_ALL_EXIT=0` |

> `-R "EnhanceAdvanced"` 가 12건인 것은 회귀가 아니다. 모듈 스위트 66건 중 픽스처명이
> `EnhanceAdvanced` 로 시작하는 것만 정규식에 걸린다. 실제 케이스 수는 gtest 열거로
> 66건임을 확인했고, 이 값이 §2.2 표의 모듈 합계 66 과 일치한다 — 추출이 정확했다는 교차 확인이다.

## 5. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-ai -R "Ai\|AI"` | 108 (`QA-B-07/gate.md` §4) | **108 / 0 failed** | 동일 |
| `ci-post` 전체 | 282 (`QA-B-08/gate.md` §2.4) | **282 / 0 failed** | 동일 |
| `ci-post` generate | 통과 (QA-B-08) | 통과 | 동일 |
| ai SPEC 라벨 부착 | 0 (이식 전, `_verify.log` 1차) | **108** | 신규 |

## 6. 미검증 (Gaps)

- **사본 103건이 실제로 통과하는지 미검증.** 확인한 것은 **컴파일까지**다(§2.4).
  링크·실행은 하지 않았다 — 타깃명이 모듈 등록과 중복이라 정상 경로로는 빌드할 수 없고,
  우회 타깃을 만드는 것은 카드 범위 밖이다. "103건의 커버리지가 있다" 가 아니라
  **"103건의 컴파일 가능한 케이스가 있다"** 까지만 주장한다.
- **사본과 모듈의 의미 중복도 미정량.** 이름 겹침 0건은 확정했으나, 다른 이름으로 같은
  것을 검증하는 케이스가 얼마나 되는지는 사람이 읽어야 한다. §2.3 은 그중 눈에 띄는
  비중복 영역을 추린 것이지 전수 분류가 아니다.
- **`TIMEOUT 30` 의 적정성 미검증.** 원본 값을 그대로 썼다. ai 테스트 108건이 현재
  모두 그보다 훨씬 빨리 끝나지만, 값 자체를 실측으로 정한 것은 아니다.
- **`enhance_advanced` 라벨 미이식** (§3.3) — 의도된 보류.
- **`tests/` 의 나머지(`common_smoke`) 미조사.** 카드 범위 밖.
- **비-MSVC / Linux 미검증.**

## 7. 잔여 위험 (Residual risk)

- §2.4 의 컴파일 성공이 "이 테스트들이 지금 통과한다" 로 읽히면 과대 해석이다. 삭제 판정을
  내릴 때 이 구분이 결정적일 수 있다 — 통과 여부를 알려면 별도 실행 카드가 필요하다.
- 라벨을 붙였으므로 앞으로 `ctest -L` 로 SPEC 단위 선택 실행이 가능해진다. 반대로
  `TIMEOUT 30` 이 이제 실효 상한이 되어, 느린 머신에서 IPC 계열 테스트가 시간 초과로
  실패할 여지가 생긴다(현재 관측된 사례는 없다).
- §3.1 결함은 리포지토리의 다른 `LABELS "a;b;c"` 표기에도 잠재한다. 확인된 잔존 위치는
  `tests/enhance_advanced_tests/CMakeLists.txt:59` 한 곳이며, 그 파일은 빌드되지 않아
  현재 무해하다.

---

## 8. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| diff 대조표 | PASS | §2.1 파일 단위, §2.2 케이스 단위 |
| 삭제 후 generate 통과 | N/A — **삭제하지 않음** | §2.5 조건 불성립. generate 통과는 §4 에서 별도 확인 |
| ctest 카운트 재실측 (인용 금지) | PASS | §4 — 108 / 66 / 282 전부 이번 실행에서 측정 |
| footer `Refs #113` | PASS | 커밋 메시지 |
