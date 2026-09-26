# QA-B-14 — gsvg 널 핸들 정렬 + ai 우선순위 회귀 가드 (#119 마무리)

**레인**: Lane B (`dev/postprocess`)
**판정 근거(카드)**: 계약 ①의 목적은 호스트 쪽 예측 가능성. "초기화 안 됨" 은 널 핸들이 아니라
shutdown 이후 사용이다. 리포지토리 내 일관성(dicom) 우선 → gsvg 를 정렬.

**결과**: gsvg RED→GREEN, ai 회귀 가드 2건 추가(**실효성 프로브로 확인**).
ai 108→**110**, ci-ai 166→**168**, gsvg 26/26, `ci-post` **387/387**.

---

## 1. 주장 (Claim)

1. gsvg 널 핸들 → `INVALID_INPUT` 로 정렬했다. 소스 1곳 + 헤더 + 테스트 2건(§2).
2. RED(테스트 기대치 선변경 → 실패) → GREEN 을 관측했다(§2.2, §2.4).
3. **정렬 후 gsvg 에는 `NOT_INITIALIZED` 반환 경로가 하나도 남지 않는다** — use-after-shutdown 은
   코드 경로가 아니라 dangling pointer 다(§3). 표로 보고한다.
4. ai 에 우선순위 회귀 가드 2건을 추가했고, **순서를 되돌리면 실제로 실패함을 관측**했다(§4).
5. 재실측: ai 110/110, ci-ai 168/168, gsvg 26/26, ci-post 387/387.

---

## 2. gsvg 정렬

### 2.1 대상 전수 확인 (치환 전 검증 — QA-B-12 규칙)

`modules/gsvg/src/gsvg.cpp` 의 핸들·널 관련 가드 전수:

| 행 | 코드 | 처리 |
|---|---|---|
| 183 | `if (!handleOut) return XPE_ERR_INVALID_INPUT;` | `xpe_gsvg_init` 의 out 파라미터 — **이미 준수**, 무변경 |
| 204 | `if (!handle) return XPE_ERR_NOT_INITIALIZED;` | **교정 대상 (유일)** |
| 231 | `if (handle == nullptr) return XPE_OK;` | `xpe_gsvg_shutdown` 의 문서화된 no-op — 무변경 |

즉 `NOT_INITIALIZED` 를 내는 곳은 **204행 하나뿐**이다. 테스트 쪽도 파일당
`XPE_ERR_NOT_INITIALIZED` 가 정확히 1회임을 치환 전에 확인했다:

```
pre-verification: 2 files, anchor=1 and NOT_INITIALIZED=1 each
```

### 2.2 RED — 기대치를 먼저 뒤집었다

| 파일 | 변경 |
|---|---|
| `tests/test_gsvg_abi_smoke.cpp:197,208` | 주석 + 기대치 `NOT_INITIALIZED` → `INVALID_INPUT` |
| `tests/test_gsvg_degraded.cpp:200,206` | 테스트명 `…_ReturnsNotInitialized` → `…_ReturnsInvalidInput` + 기대치 |

```
    Which is: -6
[  FAILED  ] GsvgDegradedMode.ProcessWithNullHandle_ReturnsInvalidInput
    Which is: -6
[  FAILED  ] GsvgAbiSmoke.ProcessRejectsNullHandle
```
로그: `_red.log`

테스트명을 함께 바꾼 이유는 QA-B-12 에서 세운 원칙과 같다 — **이름이 검증 대상을 말한다.**
기대치만 바꾸고 `_ReturnsNotInitialized` 를 남기면 이름이 거짓이 된다.

### 2.3 소스와 헤더

```diff
-    if (!handle) return XPE_ERR_NOT_INITIALIZED;
+    // A NULL handle is a NULL required pointer, so it is INVALID_INPUT — the
+    // same code dicom returns for a NULL handle (dicom.cpp:56,67) and what the
+    // api-spec precedence contract requires (#119). "Not initialised" would be
+    // use-after-shutdown, which is a dangling pointer here, not a NULL one.
+    if (!handle) return XPE_ERR_INVALID_INPUT;
```

헤더(`gsvg_api.h`)는 카드가 지시한 "INVALID_INPUT if handle is NULL" 로 바꾸면
바로 위 줄과 **같은 `@return XPE_ERR_INVALID_INPUT` 이 두 번** 나온다. 정보를 잃지 않으면서
중복을 없애려고 한 줄로 합쳤다:

```diff
- * @return XPE_ERR_INVALID_INPUT on NULL pointer or non-positive dimension.
- * @return XPE_ERR_NOT_INITIALIZED if handle is NULL.
+ * @return XPE_ERR_INVALID_INPUT on a NULL pointer -- including a NULL handle,
+ *         which is a NULL required pointer like any other -- or on a
+ *         non-positive dimension.
```

카드 문구를 그대로 옮기지 않은 유일한 지점이며, 의미는 동일하고 핸들을 명시적으로 언급한다.

### 2.4 GREEN

```
ctest -R "Gsvg"  ->  100% tests passed, 0 tests failed out of 26
```
(`_verify.log`)

---

## 3. use-after-shutdown 경로 — 존재하지 않는다 (카드 요구 표)

카드: "use-after-shutdown 경로가 따로 있으면 그것이 NOT_INITIALIZED 를 유지하는지 확인해 표로".

| 항목 | 관측 |
|---|---|
| `xpe_gsvg_shutdown` 구현 | `if (handle == nullptr) return XPE_OK; delete static_cast<GsvgHandle*>(handle); return XPE_OK;` (`gsvg.cpp:229-234`) |
| 핸들 유효성 표식(magic/generation) | **없음.** `struct GsvgHandle { bool vignette_enabled; bool grid_enabled; };` (`gsvg.cpp:38-41`) — 두 개의 bool뿐 |
| shutdown 후 호출자 포인터 | 값 전달이라 호출자 쪽 포인터는 그대로 남는다. 라이브러리가 널로 만들 수 없다 |
| 결과 | **use-after-shutdown 은 dangling pointer 역참조(UB)** — 검출되지 않고, 어떤 오류코드도 반환하지 않는다 |
| 정렬 후 gsvg 의 `NOT_INITIALIZED` 반환 경로 | **0곳** (`grep NOT_INITIALIZED` → 소스·헤더·테스트 전부 무출력) |

즉 카드의 전제("초기화 안 됨은 shutdown 이후 사용")는 개념적으로는 맞지만,
**gsvg 에는 그 경우를 감지하는 코드가 없다.** 정렬의 결과로 이 모듈에서
`NOT_INITIALIZED` 는 완전히 사라진다 — 이것이 의도된 최종 상태인지 확인이 필요하면
별도 판정 사안이다(§7).

---

## 4. ai 회귀 가드 — 실효성까지 확인했다

### 4.1 추가한 것

`tests/ai_tests/test_ai_fallback.cpp` 끝에 `AiErrorPrecedenceTest` 픽스처(SetUp 에서
`xpe_ai_shutdown()`, TearDown 에서 복구 — `NotInitializedGuardTest` 와 같은 방식)와 2건:

| 케이스 | 호출 | 기대 |
|---|---|---|
| `NullArgumentOutranksNotInitialized` | `xpe_dl_denoise(nullptr,nullptr,nullptr)` | `INVALID_INPUT` |
| `ValidArgumentsReachNotInitialized` | `xpe_dl_denoise(&img,&meta,nullptr)` | `NOT_INITIALIZED` |

두 값이 **갈려야** 통과한다. 순서를 되돌리면 둘 다 `-6`, 초기화 가드를 없애면 둘 다 `-1` 이므로
어느 쪽 회귀도 잡힌다.

### 4.2 가드가 실제로 무는지 관측했다

통과하는 테스트는 그 자체로 "잡아낸다" 를 증명하지 않는다(QA-B-05·B-06 에서 세운 원칙).
`xpe_dl_denoise` 의 검사 순서를 **일시적으로 교정 전 상태로 되돌려** 실행했다:

```
    Which is: -6
[  FAILED  ] AiErrorPrecedenceTest.NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.ValidArgumentsReachNotInitialized
===RUN_EXIT=1===
```
로그: `_guard_probe.log`

되돌린 소스는 원복했고 `git diff modules/ai/src/ai.cpp` 는 빈 출력이다
(이 카드는 ai 소스를 바꾸지 않는다 — 테스트만 추가).

### 4.3 남는 공백

가드는 `xpe_dl_denoise` **하나**만 고정한다. QA-B-13 이 교정한 나머지 4개
(`bodypart_recognize`, `stitch_images`, `bone_suppress`, `get_model_card`)는 여전히
회귀 방지 장치가 없다. 카드가 "1건 추가" 를 지시했으므로 범위를 지켰고, 공백은 §7 에 남긴다.

---

## 5. 재실측

| 대상 | 결과 | 로그 |
|---|---|---|
| `ctest -R "Ai\|AI"` (ci-ai) | **110 passed / 0 failed** | `_verify.log` |
| `ctest` 전체 (ci-ai) | **168 passed / 0 failed**, `AI_ALL_EXIT=0` | `_verify.log` |
| `ctest -R "Gsvg"` (ci-post) | **26 passed / 0 failed**, `GSVG_EXIT=0` | `_verify.log` |
| `ctest` 전체 (ci-post) | **387 passed / 0 failed**, `POST_ALL_EXIT=0` | `_verify.log` |

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-ai -R "Ai\|AI"` | 108 (`QA-B-13/gate.md` §6) | **110** | +2 = 신규 회귀 가드 |
| `ci-ai` 전체 | 166 (`QA-B-13/gate.md` §6) | **168** | +2 = 동일 |
| `ci-post` 전체 | 387 (`QA-B-13/gate.md` §6) | **387** | 동일 (gsvg 는 기대치만 변경, 개수 불변) |
| gsvg 널 핸들 반환 | -6 (`QA-B-12/_probe.log`) | **-1** | 정렬됨 |
| gsvg `NOT_INITIALIZED` 경로 수 | 1 (`gsvg.cpp:204`) | **0** | §3 |

## 7. 미검증 (Gaps)

- **ai 회귀 가드는 `xpe_dl_denoise` 만 고정한다**(§4.3). 나머지 4개 교정 함수는
  여전히 순서를 되돌려도 테스트가 통과한다.
- **gsvg 정렬 후 프로브를 다시 돌리지 않았다.** `ctest -R Gsvg` 26/26 과 RED→GREEN 으로
  확인했고, `QA-B-12/_probe.log` 형식의 외부 프로브 재실행은 하지 않았다.
- **use-after-shutdown 은 관측하지 않았다**(§3). dangling pointer 역참조는 UB 라
  실행 관측 자체가 부적절하다고 판단해 소스 구조로만 확정했다.
- **`gsvg` 에서 `NOT_INITIALIZED` 가 완전히 사라진 것이 의도된 최종 상태인지 미확인.**
  헤더에서도 그 코드가 사라졌으므로, 호스트가 이 모듈에서 `-6` 을 받는 경우는 이제 없다.
- **dicom 쪽은 손대지 않았다**(카드 지시). dicom 헤더에 널 핸들 반환코드 문서화가
  없다는 점은 QA-B-13 에서 관측한 그대로 남아 있다.
- **비-MSVC / Linux 미검증.** ai 는 stub 빌드로만 확인(ONNX 실경로 계속 미검증).

## 8. 잔여 위험 (Residual risk)

- gsvg 의 널 핸들과 "정상 널 포인터" 가 이제 같은 코드를 낸다. 호스트가 둘을 구분해
  진단 메시지를 내고 있었다면 그 구분이 사라진다 — 헤더 문서화가 바뀌었으므로
  계약상으로는 정당하지만, 기존 호출자에게는 **행위 변경**이다.
- §3 대로 gsvg 는 이제 어떤 상황에서도 `NOT_INITIALIZED` 를 내지 않는다. 초기화 실수를
  코드로 구분하려던 설계 의도는 폐기됐고, 대신 dicom 과 동일해졌다.
- §4.3 의 공백 때문에, 나머지 4개 함수는 다음 리팩터에서 조용히 되돌아갈 수 있다.

---

## 9. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| RED→GREEN 로그 (gsvg·ai) | PASS | gsvg §2.2 `_red.log` → §2.4 26/26 · ai §4.2 `_guard_probe.log`(역방향 프로브) → §4.1 통과 |
| 헤더 diff | PASS | §2.3 |
| 재실측 카운트 | PASS | §5 / §6 (110 · 168 · 26 · 387) |
| footer `Refs #119` | PASS | 커밋 메시지 |
