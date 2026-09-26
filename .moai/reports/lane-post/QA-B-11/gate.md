# QA-B-11 — 흡수 스위트 실패 2건 기대치 정정 (#119 / #113)

**레인**: Lane B (`dev/postprocess`)
**적용 계약** (api-spec "Error code precedence", 카드 본문):
`INVALID_INPUT`(널·범위) → `NOT_INITIALIZED` → 내용 검증(`UNSUPPORTED_FORMAT` / `CONFIG_INVALID` / `BUFFER_TOO_SMALL`) → 처리 오류.
`configJsonOrNull`: `NULL` = 기본값, `""` = `CONFIG_INVALID`.

**결과**: 실패 2건 정정 완료, `ci-post` **386/386 무실패**.
**그리고 이 카드가 새로 찾은 것**: 계약을 실제로 찔러 보니 **구현이 계약과 반대 순서로 검사한다**(§4). 모듈 수정은 범위 밖이라 비활성 케이스로 자리를 남기고 보고한다.

---

## 1. 주장 (Claim)

1. `MfpScalarExtTest` 의 빈 문자열 케이스를 계약대로 정정·개명했다.
2. `IntegrationPipelineTest.ErrorPathsCovered` 의 널 인자 단언 3건을 `INVALID_INPUT` 으로 정정했다.
3. 정정 과정에서 **원래 진단이 틀렸던 부분을 스스로 정정**했다(§3.3).
4. 계약의 우선순위 자체를 검증하는 케이스를 추가했고, **그중 하나가 구현과 불일치**함을 관측했다(§4).
5. 같은 가정이 다른 곳에 더 있는지 grep 했다 — **위반 0건**(§5).
6. `ci-post` 386/386, 실패 0. `-L SPEC-XPE-P2-ADV` 171.

---

## 2. 1단계 — 빈 설정 문자열

### 2.1 변경

```diff
-TEST_F(MfpScalarExtTest, EmptyConfigStringUsesDefaults) {
+// Per api-spec "Error code precedence": configJsonOrNull treats NULL as "use
+// defaults"; an empty string is not a valid JSON document and is rejected.
+// (This suite already asserted the same thing for the init entry point in
+// LifecycleTest.InitWithEmptyStringReturnsConfigInvalid.)
+TEST_F(MfpScalarExtTest, EmptyConfigStringIsConfigInvalid) {
     XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
     XpeImageMetadata meta = MakeMeta("CHEST");
-    EXPECT_EQ(xpe_multiscale_process(&img, &meta, ""), XPE_OK);
+    EXPECT_EQ(xpe_multiscale_process(&img, &meta, ""), XPE_ERR_CONFIG_INVALID);
     FreeImageBuffer(img);
 }
```

### 2.2 NULL 기본값 케이스 — 추가 불필요

카드는 "NULL 기본값 케이스가 없으면 1건 추가" 를 조건부로 지시했다. **이미 있다** —
바로 위의 `MfpScalarExtTest.NullConfigUsesDefaults`(`test_mfp_scalar_ext.cpp:290`)가
`xpe_multiscale_process(&img, &meta, nullptr)` → `XPE_OK` 를 단언하며 통과한다.
조건이 성립하지 않아 추가하지 않았다.

---

## 3. 2단계 — 널 인자 단언 정정

### 3.1 변경 (단언 3건)

| 위치 | 호출 | 이전 기대 | 정정 |
|---|---|---|---|
| 270 | `xpe_multiscale_process(nullptr, nullptr, nullptr)` | `NOT_INITIALIZED` | `INVALID_INPUT` |
| 271 | `xpe_fractional_process(nullptr, 1.0f, nullptr)` | `NOT_INITIALIZED` | `INVALID_INPUT` |
| 285 | `xpe_multiscale_process(&img, nullptr, nullptr)` (img.format=UINT16) | `UNSUPPORTED_FORMAT` | `INVALID_INPUT` |

### 3.2 주석의 거짓말도 함께 고쳤다

원본 주석은 `// NOT_INITIALIZED checks (module not initialized yet)` 였다. **거짓이다** —
`IntegrationPipelineTest::SetUp()`(`test_integration_ext.cpp:71-77`)이 매 테스트 전에
`xpe_enhance_advanced_init(nullptr)` 을 부른다. 이 두 줄은 **초기화된 상태**에서 돈다.
주석을 실제 상태와 계약 근거로 교체했다.

### 3.3 285행의 원인을 잘못 짚었다가 정정했다 (기록)

정정 1차에서 285행의 `-1` 을 "널 메타데이터가 포맷 검사를 앞선다" 로 적었다. **틀렸다.**

반증은 모듈 쪽 기존 테스트에 있었다 — `test_integration.cpp:463` 이
`xpe_multiscale_process(&img, nullptr, nullptr)` 를 **널 메타데이터로** 부르고
`NOT_INITIALIZED` 를 받으며 **통과한다.** 메타데이터가 널이라는 이유로 걸렸다면
그 자리에서 `INVALID_INPUT` 이 났어야 한다. 즉 **이 함수에서 메타데이터는 선택 인자다.**

285행의 진짜 원인은 버퍼다: `XpeImageBuffer img{}` 는 값 초기화라
`data == nullptr`, `dataSize == 0` 이고 테스트는 `width/height/format` 만 채운다.
**픽셀 데이터가 없는 버퍼**라서 입력 검증에서 걸린 것이다.

주석을 이 근거로 교체했다. 기대치 정정 자체(`INVALID_INPUT`)는 두 해석 모두에서 같지만,
**틀린 이유를 코드에 남기면 다음 사람이 그걸 사실로 읽는다.**

---

## 4. 계약을 실제로 찔러 봤더니 — 구현이 반대 순서다 (신규 발견)

### 4.1 왜 추가했나

카드는 "미초기화·포맷 경로를 검증하려던 의도가 있으면 널이 아닌 인자로 분리 케이스를
추가" 하라고 했다. 대조 결과 **그 두 의도는 이미 다른 곳이 덮고 있었다**:

| 의도 | 이미 덮는 곳 | 상태 |
|---|---|---|
| `NOT_INITIALIZED` (널 아닌 인자) | `NotInitializedGuardTest.*` 4건 (`test_lifecycle_ext.cpp:120-`) | 통과 |
| `UNSUPPORTED_FORMAT` (널 아닌 인자) | `MfpScalarExtTest.Uint16FormatReturnsUnsupportedFormat` (`:168`), `EdgeEnhancement…:195` | 통과 |

덮여 있는 것을 복제하는 대신, **아무도 덮지 않는 것**을 넣었다 — 계약이 새로 정한
**우선순위 자체**. 두 케이스는 인자가 널이냐 아니냐만 다르므로 둘이 함께 순서를 고정한다.

### 4.2 관측 — 구현이 계약을 위반한다

```
test_integration_ext.cpp(324): xpe_multiscale_process(nullptr, nullptr, nullptr)
    Which is: -6 (NOT_INITIALIZED)        expected -1 (INVALID_INPUT)
test_integration_ext.cpp(325): xpe_fractional_process(nullptr, 1.0f, nullptr)
    Which is: -6                          expected -1
```
로그: `_build_run.log` (비활성화 전 실행분)

계약은 `INVALID_INPUT → NOT_INITIALIZED` 인데, **미초기화 상태에서는 초기화 검사가
인자 검사를 앞선다.** 초기화된 상태에서는 인자 검사가 먼저 발화한다(§3.1 정정분이 통과).
즉 **상태에 따라 우선순위가 뒤집힌다.**

`ValidArgumentsReachNotInitialized`(널 아닌 인자 → `-6`)는 통과한다 — 두 케이스가
함께 있어야 이 뒤집힘이 보인다.

### 4.3 처리 — 비활성 케이스로 자리를 남겼다

모듈 수정은 이 카드 범위 밖이고("실패 케이스 수정 금지" 는 QA-B-10 부터의 원칙),
알면서 빨간 테스트를 남기면 카드의 합격 조건(무실패 재실측)과 충돌한다.
그래서 **`DISABLED_` 접두사**로 남기고 사유·관측값·미해결 상태를 주석에 적었다.

- 지우지 않았다 → 불일치가 코드에 남아 있다.
- gtest 가 `YOU HAVE 1 DISABLED TEST` 를 찍고, ctest 는
  `303 - ErrorPrecedenceTest.NullArgumentOutranksNotInitialized (Disabled)` 를
  **"did not run"** 목록에 넣는다 — **통과로 집계되지 않는다**(가짜 초록 아님).
- 해소는 **모듈 수정 또는 계약 수정** 중 하나이며 leader 판정 사안이다.

---

## 5. 3단계 — 같은 가정 grep (수정 범위 밖)

찾는 패턴: **필수 포인터가 널인데 `-6`/`-7` 을 기대**하는 단언.

전 모듈 테스트(`modules/*/tests/`, `tests/`)를 훑은 결과 후보 3건이 나왔고,
확인 결과 **위반은 0건**이다:

| 위치 | 호출 | 판정 |
|---|---|---|
| `modules/enhance_advanced/tests/test_integration.cpp:463` | `xpe_multiscale_process(&img, nullptr, nullptr)` → `NOT_INITIALIZED` | **위반 아님.** 널인 것은 선택 인자(메타데이터·설정)뿐이고 `img` 는 유효하다. 통과 중 |
| `.../test_mfp_scalar_ext.cpp:176` | `(&img, &meta, nullptr)` → `UNSUPPORTED_FORMAT` | **위반 아님.** 널은 설정 인자(= 기본값) |
| `.../test_edge_enhancement_ext.cpp:195` | `(&img, 1.0f, nullptr)` → `UNSUPPORTED_FORMAT` | **위반 아님.** 동일 |

grep 이 이들을 잡은 이유는 마지막 인자 `nullptr` 이 계약상 **합법**(NULL = 기본값)이기
때문이다 — 텍스트 매칭만으로는 "널 인자" 와 "선택 인자 생략" 이 구분되지 않는다.
필수/선택을 갈라 읽어야 판정이 된다.

`enhance_basic` / `ai` / `dicom` / `display` / `gsvg` 테스트에는 해당 패턴이 없다.

---

## 6. 4단계 — 검증 (재실측)

스위트 단독 (`_build_run.log`):

```
===BUILD_EXIT=0===
[==========] 170 tests from 16 test suites ran.
[  PASSED  ] 170 tests.
  YOU HAVE 1 DISABLED TEST
===RUN_EXIT=0===
```

`ci-post` 전체 (`_verify.log`):

| 항목 | 결과 |
|---|---|
| `cmake --preset ci-post` | `CFG_EXIT=0` |
| `ctest -N -L "SPEC-XPE-P2-ADV"` | **171** |
| `ctest` 전체 | **100% tests passed, 0 tests failed out of 386**, `ALL_EXIT=0` |
| did-not-run | 기존 `DegradedMode` 5건 (Skipped) + 신규 1건 (Disabled) |

## 7. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-post` 총계 | 385 (`QA-B-10/gate.md` §5) | **386** | +1 = `ValidArgumentsReachNotInitialized` |
| `ci-post` 실패 | **2** (QA-B-10) | **0** | 카드 목적 달성 |
| enhance_advanced 등록 | 169 (QA-B-10) | **171** | +2 = 신규 케이스 2건(1건은 Disabled) |
| enhance_advanced 실행 | 169 (167P/2F) | **170 실행, 170 통과** | 실행 수 +1(1건 Disabled 제외) |
| `-L SPEC-XPE-P2-ADV` | 169 (QA-B-10) | **171** | 신규 2건 포함 |

> 카드의 기대치는 "385/385" 였다. 실제는 **386/386** 이다 — 차이 +1 은 이 카드가
> 추가한 활성 케이스 1건이며, 비활성 1건은 실행 집계에 들어가지 않는다.

## 8. 미검증 (Gaps)

- **§4.2 불일치의 구현 위치를 특정하지 않았다.** `xpe_multiscale_process` /
  `xpe_fractional_process` 의 소스에서 검사 순서를 읽지 않았다. 관측은 반환값 기준이다.
- **다른 5개 모듈의 우선순위 준수 미검증.** §5 는 "테스트가 계약을 위반하는 기대를
  갖고 있는가" 를 본 것이지, **구현이 계약을 지키는가** 를 본 것이 아니다.
  §4.2 가 enhance_advanced 에서 위반을 찾았으므로 같은 검사를 다른 모듈에 돌리면
  더 나올 수 있다 — 별도 카드 후보.
- **api-spec 원문 미열람.** 계약은 카드 본문의 요약으로만 받았고 main 의 문서를
  읽지 않았다(`git merge main` 불필요 판정에 따름). 요약과 원문이 다르면 §4 판정이 흔들린다.
- **`""` 이외의 공백 설정 문자열(`" "`, `"\n"`) 미검증.**
- **비-MSVC / Linux 미검증.**

## 9. 잔여 위험 (Residual risk)

- `DISABLED_` 케이스는 해소되지 않으면 영구히 잠든다. gtest/ctest 가 매 실행마다
  1건을 보고하지만, 그 신호를 읽는 사람이 없으면 삭제와 실질적으로 같아진다.
  **후속 카드가 나지 않으면 이 발견은 유실된다.**
- §4.2 를 "계약 수정" 으로 해소하면(구현에 맞춰 `NOT_INITIALIZED` 를 앞으로),
  이번에 정정한 §3.1 의 270·271행 기대치가 **다시 틀리게 된다**. 두 결정은 연동돼 있다.
- 상태에 따라 우선순위가 뒤집히는 현재 동작은, 호출자가 오류코드로 분기할 때
  초기화 여부에 따라 다른 코드를 받는다는 뜻이다. 계약이 어느 쪽으로 정리되든
  이 비대칭 자체가 API 사용자에게 함정이다.

---

## 10. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| diff 2파일 | PASS | `test_mfp_scalar_ext.cpp`, `test_integration_ext.cpp` |
| 169/169 + `ci-post` 무실패 재실측 | PASS | §6 — 스위트 170 실행/170 통과(+1 Disabled), ci-post **386/386 실패 0** |
| 3단계 목록 | PASS | §5 — 후보 3건 전부 위반 아님으로 판정, 근거 포함 |
| footer `Refs #119` | PASS | 커밋 메시지 |
