# QA-B-13 — ai 널 검사 선행 교정 + gsvg 널 핸들 판정 (#119)

**레인**: Lane B (`dev/postprocess`)
**계약(개정본)**: ① 필수 포인터 널 → `INVALID_INPUT` 이 무엇보다 먼저(선택 포인터 제외)
② 이후 `NOT_INITIALIZED` ↔ 내용 검증 순서는 구현 정의 ③ 처리 오류는 그 뒤

**결과**: ai 5개 진입점 교정, RED→GREEN. ai 108/108, 166/166. `ci-post` 387/387.
**gsvg 는 교정하지 않았다** — 의도된 설계이며 계약 1 의 예외 명시가 필요한 사안이다(§4).

---

## 1. 주장 (Claim)

1. `ai` 의 XPE_API 진입점 **10개를 전수 분류**하고, 위반 5개만 교정했다(§2).
2. RED(널 → `-6`) → GREEN(널 → `-1`)을 프로브로 관측했고, 초기화 가드는 살아 있다(§3).
3. `gsvg` 의 `-6` 은 **헤더에 명시되고 테스트 2건이 고정한 의도된 동작**이다 — 오류가 아니다(§4).
4. 카드가 요구한 **마커 개수 선검증**을 치환 전에 실행했고 로그로 남겼다(§5).
5. 재실측: ai 108/108, ci-ai 전체 166/166, ci-post 387/387.

---

## 2. 1단계 — ai 진입점 전수 분류

`modules/ai/src/ai.cpp` 의 `XPE_API` 진입점 10개를 모두 읽고 분류했다.
초기화 가드는 `checkInitialized()` 헬퍼 하나로 통일돼 있다(`ai.cpp:128`).

| 진입점 | 초기화 검사 | 필수 포인터 널 검사 | 판정 |
|---|---|---|---|
| `xpe_ai_version` | 없음 | 없음 | 대상 아님 |
| `xpe_ai_init` | 없음(자신이 초기화) | — | 대상 아님 |
| `xpe_ai_shutdown` | 없음 | — | 대상 아님 |
| `xpe_bodypart_recognize` | 먼저 | `checkNotNull(img)`, `checkNotNull(bodyPartOut)` | **교정** |
| `xpe_stitch_images` | 먼저 | `!parts \|\| partCount < 2 \|\| !stitchedOut` | **교정** |
| `xpe_stitch_estimate_size` | **없음** | 먼저 | 이미 준수 |
| `xpe_bone_suppress` | 먼저 | `checkNotNull(img)`, `checkNotNull(softTissueOut)` | **교정** |
| `xpe_dl_denoise` | 먼저 | `checkNotNull(img)`, `checkNotNull(meta)` | **교정** |
| `xpe_ai_get_model_card` | 먼저 | `!modelId \|\| !buf` | **교정** |
| `xpe_ai_set_fallback_mode` | 먼저 | 포인터 인자 없음 | 대상 아님 |

교정 5건. 변경은 널 검사 블록을 초기화 검사 **앞으로** 옮긴 것뿐이며, 조건식·반환값은 불변이다.
`validateImageBuffer()`(내용 검증)와 `bufSize < 1`(`BUFFER_TOO_SMALL`)은 계약 ②에 따라
구현 정의이므로 초기화 검사 뒤에 그대로 두었다.

### 2.1 `xpe_stitch_images` 의 범위 검사가 함께 앞으로 갔다

이 함수의 널 검사는 `if (!parts || partCount < 2 || !stitchedOut)` 한 문장이라,
옮기면 범위 조건(`partCount < 2`)도 같이 앞으로 간다. 계약 ①은 널 검사의 선행만
규정하고 다른 검사를 뒤로 밀라고 하지 않으므로 위반이 아니다. 조건을 쪼개면
"검사 의미 변경 금지" 에 더 가까워지므로 문장 단위로 옮겼다.

### 2.2 컴파일 불가 상태를 한 번 만들었다가 잡았다 (기록)

널 검사 블록을 그대로 앞으로 옮기자 `ec = checkNotNull(img);` 가
`XpeErrorCode ec = checkInitialized();` **앞**에 오면서 `ec` 를 선언 전에 쓰게 됐다.
diff 를 눈으로 확인하는 단계에서 잡아, 옮긴 블록의 첫 줄이 선언을 갖고 초기화 줄은
대입이 되도록 3개 함수를 수정했다. 빌드가 잡아 줬을 결함이지만, **블록 이동은
선언 위치를 옮긴다**는 점이 이 변환의 고유 위험이다.

---

## 3. 2단계 — RED → GREEN 관측

프로브는 어떤 init 도 호출하지 않는다(= 미초기화 상태). 귀속 분리를 위해
**같은 상태에서 유효 인자로도** 한 번 더 호출한다.

**교정 전** (`QA-B-12/_probe.log`):
```
ai xpe_dl_denoise(nullptr,nullptr,nullptr) = -6      <- 계약 위반
ai xpe_dl_denoise(&img,&meta,nullptr)      = -6
```

**교정 후** (`_ai_green.log`):
```
ai xpe_dl_denoise(nullptr,nullptr,nullptr) = -1      <- INVALID_INPUT (계약 ①)
ai xpe_dl_denoise(&img,&meta,nullptr)       = -6      <- NOT_INITIALIZED (가드 살아 있음)
```

두 값이 갈린다는 것이 핵심이다. 둘 다 `-1` 이 되었다면 초기화 가드를 죽인 것이고,
둘 다 `-6` 이면 교정이 안 된 것이다.

테스트: `ctest -R "Ai|AI"` → **108 passed / 0 failed** (`_ai_green.log`).

---

## 4. 3단계 — gsvg 널 핸들 판정: **의도된 설계. 교정하지 않았다**

### 4.1 검사 지점

```cpp
// modules/gsvg/src/gsvg.cpp:204
XpeErrorCode xpe_gsvg_process(void* handle, const uint16_t* src, uint16_t* dst,
                              int width, int height, const float* gainMap)
{
    if (!handle) return XPE_ERR_NOT_INITIALIZED;      // <- 여기
    if (!src || !dst) return XPE_ERR_INVALID_INPUT;
    if (width <= 0 || height <= 0) return XPE_ERR_INVALID_INPUT;
```

### 4.2 의도된 설계라는 근거 3가지

**(1) 헤더가 두 코드를 나누어 명시한다** (`gsvg_api.h:88-90`):
```
 * @return XPE_ERR_INVALID_INPUT on NULL pointer or non-positive dimension.
 * @return XPE_ERR_NOT_INITIALIZED if handle is NULL.
```
같은 함수에서 "널 포인터는 INVALID_INPUT" 과 "핸들이 널이면 NOT_INITIALIZED" 를
**따로** 적었다. 빠뜨린 것이 아니라 구분한 것이다.

**(2) 테스트 2건이 고정하고 있다**:
```
modules/gsvg/tests/test_gsvg_abi_smoke.cpp:197  // Documented to return NOT_INITIALIZED.
modules/gsvg/tests/test_gsvg_abi_smoke.cpp:208  EXPECT_EQ(..., XPE_ERR_NOT_INITIALIZED)
modules/gsvg/tests/test_gsvg_degraded.cpp:206   EXPECT_EQ(..., XPE_ERR_NOT_INITIALIZED)
```

**(3) 모듈에 전역 초기화 상태가 없다.** `xpe_gsvg_init(void** handleOut, …)` 이 핸들을
만들고 `xpe_gsvg_shutdown(handle)` 이 해제한다. 다른 모듈이 전역 플래그로 표현하는
초기화 상태를 gsvg 는 **핸들 자체로** 표현한다. 널 핸들 = 초기화 안 됨(또는 이미 해제됨).
같은 파일에서 `xpe_gsvg_init` 의 out 파라미터 널은 `INVALID_INPUT` 이다(`gsvg.cpp:183`) —
즉 저자는 "일반 널 포인터" 와 "핸들 널" 을 의식적으로 다르게 다뤘다.

### 4.3 그러나 리포지토리 안에서 일관되지 않다

같은 핸들 패턴을 쓰는 `dicom` 은 **반대**다:
```
modules/dicom/src/dicom.cpp:56  if (!handle || !outImg)  return XPE_ERR_INVALID_INPUT;
modules/dicom/src/dicom.cpp:67  if (!handle || !outMeta) return XPE_ERR_INVALID_INPUT;
```
`xpe_dicom_open(const char*, XpeDicomHandle**)` 가 핸들을 만드는 동일 구조인데,
널 핸들을 `INVALID_INPUT` 으로 처리하고 헤더에 `NOT_INITIALIZED` 언급이 없다.

**두 핸들 기반 모듈이 서로 다르다.** 이것이 판정이 필요한 실체다.

### 4.4 보고 (교정 여부는 판정 사안)

- gsvg 를 계약 ①에 맞추면(`-1`) **헤더 문구와 테스트 2건을 함께 고쳐야** 하며,
  "핸들 널은 초기화 상태" 라는 설계 의도를 폐기하는 결정이다.
- 유지하면 **계약 ①의 예외로 "핸들 인자는 초기화 상태의 표현이므로 제외" 를 명시**해야
  한다. 그 경우 `dicom` 이 반대로 어긋나므로 그쪽을 맞추는 후속이 필요하다.
- 어느 쪽이든 **모듈 단위가 아니라 계약 단위 결정**이라 손대지 않았다.

---

## 5. 마커 개수 선검증 (QA-B-12 사고 대응 규칙)

QA-B-12 에서 일괄 치환이 `@MX:ANCHOR` 주석을 가드로 오인해 코드를 함수 밖으로
옮긴 사고가 있었다. 이번에는 **치환 전에** 검증을 돌리고 로그로 남겼다:

```
pre-verification:
  xpe_bodypart_recognize    anchor=1 init=1 null=1 order=init-first
  xpe_stitch_images         anchor=1 init=1 null=1 order=init-first
  xpe_bone_suppress         anchor=1 init=1 null=1 order=init-first
  xpe_dl_denoise            anchor=1 init=1 null=1 order=init-first
  xpe_ai_get_model_card     anchor=1 init=1 null=1 order=init-first
```

검증 항목 4가지 — 각각 하나라도 어긋나면 치환을 중단한다:
1. **앵커 유일성**: 함수 시그니처가 파일 전체에서 1회만 등장
2. **함수 범위 한정**: 치환 대상을 시그니처 ~ 다음 `XPE_API` 사이로 자름 (파일 전역 검색 금지)
3. **블록 유일성**: 그 범위 안에서 대상 블록이 정확히 1회
4. **사전 상태 확인**: 실제로 교정이 필요한 순서인지(이미 준수면 건너뜀)

**레인 gate 유형으로 올릴 규칙**: *마커/패턴 기반 일괄 치환은 치환 전에 (a) 개수와
(b) 유일성을 검증하고 그 결과를 증거로 남긴다. 마커가 주석에도 쓰이는 식별자면
함수 범위로 한정한다.* QA-B-12 의 사고는 (a)만 했어도 잡혔다(020=3 vs 022=3).

---

## 6. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-ai -R "Ai\|AI"` | 108 (`QA-B-12`, `QA-B-09/gate.md` §4) | **108 passed / 0 failed** | 동일 |
| `ci-ai` 전체 | 166 (`QA-B-09/gate.md` §4) | **166 passed / 0 failed** | 동일 |
| `ci-post` 전체 | 387 (`QA-B-12/gate.md` §6) | **387 passed / 0 failed** | 동일 |
| ai 널 인자 반환 | -6 (`QA-B-12/_probe.log`) | **-1** | 교정됨 |
| ai 유효 인자 반환 | -6 | **-6** | 초기화 가드 보존 |

`ci-post` 는 `BUILD_AI=OFF` 라 이번 변경의 영향권 밖이지만, gsvg 판정 과정에서 소스를
읽었으므로 회귀가 없음을 확인차 돌렸다.

## 7. 미검증 (Gaps)

- **`ai` 는 `xpe_dl_denoise` 하나만 실행 프로브했다.** 나머지 4개 교정 함수
  (`bodypart_recognize`, `stitch_images`, `bone_suppress`, `get_model_card`)는
  소스 대조와 108건 무회귀로만 확인했고 반환값을 직접 관측하지 않았다.
- **`ai` 테스트가 교정된 순서를 고정하지 않는다.** 108건 중 "미초기화 + 널 → -1" 을
  단언하는 케이스가 없다. 즉 이번 교정은 **회귀 방지 장치가 없다** — 되돌아가도
  테스트는 통과한다. enhance_advanced 는 `ErrorPrecedenceTest` 가 고정하고 있는 것과 대조된다.
  회귀 방지 케이스 추가는 이 카드 범위 밖이나, 남겨두면 재발한다.
- **`gsvg` 는 소스·헤더·테스트 대조만 했고 교정 프로브를 돌리지 않았다**(교정 대상이 아니므로).
- **`ai` 의 IPC 경로(`ai_ipc_bridge.cpp`, `NOT_INIT=1 INVALID=8`)는 보지 않았다.**
  XPE_API 진입점이 아니라 내부 구현이라 카드 범위 밖으로 두었으나, 같은 유형의
  순서 문제가 있을 수 있다.
- **비-MSVC / Linux 미검증.** ai 는 stub 빌드(`XPE_AI_STUB_BUILD=ON`)로만 확인했다 —
  ONNX 실경로는 이 레인에서 계속 미검증이다.

## 8. 잔여 위험 (Residual risk)

- §7 의 "회귀 방지 장치 없음" 이 가장 큰 위험이다. ai 의 5개 함수는 다음 리팩터에서
  조용히 원래 순서로 돌아갈 수 있다.
- gsvg 판정이 "계약 예외 명시" 로 가면 `dicom` 이 어긋난 채 남는다. 반대로 가면
  gsvg 헤더·테스트 3곳을 고쳐야 한다. **어느 쪽이든 후속 작업이 남는다.**
- `xpe_stitch_images` 는 범위 검사(`partCount < 2`)가 초기화 검사보다 앞으로 갔다(§2.1).
  계약상 문제는 없지만, 같은 모듈 안에서 함수마다 범위 검사 위치가 달라졌다.

---

## 9. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| ai RED→GREEN + 재실측 | PASS | §3 (-6 → -1, 유효 인자 -6 유지), 108/108 · 166/166 |
| gsvg 소스 근거 판정 | PASS | §4 — `gsvg.cpp:204` 특정 + 근거 3건 + dicom 반례, 교정 보류 |
| footer `Refs #119` | PASS | 커밋 메시지 |
