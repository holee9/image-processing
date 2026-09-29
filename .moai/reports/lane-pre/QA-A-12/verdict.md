# QA-A-12 — 보정 함수의 "캘리브레이션 없음" 동작 판정 (#117, 판정 카드)

**레인**: Lane A (`xpe-pre`, dev/preprocess)   **발행**: 2026-09-10   **Refs**: #117
**baseline**: HEAD `524e6a6` (origin/main 과 `0 1` — A-18 커밋만 앞섬)
**코드 변경**: **없음** (판정 결과가 "요구 없음"이므로 카드 2항의 "보고만" 경로)

## 1. 판정 (Claim)

**(c) 요구 없음.** 세 문서 어디에도 "맵 미로드 상태"의 동작 요구가 없다.
없는 이유가 핵심이다 — **SPEC 설계에는 그 상태가 존재할 수 없다.** 맵이 인자이기
때문이다. 맵이 없다는 것은 인자가 NULL 이라는 뜻이고, 그것은 상태가 아니라 입력 오류다.

부수 판정 하나: 현재 코드가 쓰는 `XPE_ERR_NOT_INITIALIZED` 는 **문서가 그 코드에 부여한
의미와 충돌한다.** 이것은 이 카드 범위에서 보고만 하고 고치지 않는다(카드 2항).

## 2. 증거 (Evidence) — 전수 인용

### 2.1 세 문서 모두 "맵을 인자로 받는" 시그니처를 요구한다

| 출처 | 인용 |
|---|---|
| `spec.md:131` (REQ-P1A-010) | **When** `xpe_offset_correct(img, offsetMap)` is called … subtract the per-pixel dark offset from `img` **in-place** |
| `spec.md:145` (REQ-P1A-011) | **When** `xpe_gain_correct(img, gainMap)` is called … multiply each pixel by the corresponding gain factor **in-place** |
| `spec.md:160` (REQ-P1A-012) | **When** `xpe_defect_correct(img, defectMap, configJsonOrNull)` is called … replace all defective pixel values (where **defectMap** is non-zero) |
| `acceptance.md:78` (AC-LC-004) | **When** `xpe_offset_correct(img, offsetMap)` is called |
| `api-spec.md:404` (§6.1) | `xpe_offset_correct(XpeImageBuffer* img, const XpeImageBuffer* offsetMap)` |
| `api-spec.md:417` (§6.2) | `xpe_gain_correct(XpeImageBuffer* img, const XpeImageBuffer* gainMap)` |
| `api-spec.md:431` (§6.3) | `xpe_defect_correct(XpeImageBuffer* img, const XpeImageBuffer* defectMap, const char* configJsonOrNull)` |

**구현**: `xpe_offset_correct(const XpeImageBuffer* input, XpeImageBuffer* output,
const XpeImageMetadata* metadata)` — 맵은 인자가 아니라 전역 `g_calib.*_map` 이다.
**세 문서 일곱 군데가 모두 같은 방향을 가리키고, 구현만 다르다.**

### 2.2 "맵 미로드" 요구는 어디에도 없다

`spec.md` / `acceptance.md` / `api-spec.md` 전문에서 `not loaded` · `calibration absent` ·
`미로드` · `pass-through` 검색 결과 **해당 상태를 다루는 문장 0건**.
§2.1 의 설계에서는 당연한 결과다 — 맵이 인자이므로 "안 실린 맵"이라는 상태가 없다.

### 2.3 `NOT_INITIALIZED` 의 문서상 의미는 모듈 초기화 하나뿐이다

| 출처 | 인용 |
|---|---|
| `spec.md:242` (REQ-P1A-020) | **While** the module is not initialized (`xpe_preprocess_init()` not called or after `xpe_preprocess_shutdown()`), all processing functions **shall** return `XPE_ERR_NOT_INITIALIZED` |
| `acceptance.md:70` (AC-LC-003) | Given … `xpe_preprocess_shutdown()` … **And** subsequent processing calls return XPE_ERR_NOT_INITIALIZED |
| `acceptance.md:81` (AC-LC-004) | Given the module is **NOT initialized** … Then the function returns XPE_ERR_NOT_INITIALIZED |
| `api-spec.md:112` | `#define XPE_ERR_NOT_INITIALIZED -6 /* xpe_init() not called or failed */` |

네 곳 모두 조건이 **모듈 초기화 여부**다. "캘리브레이션이 실렸는가"는 어디에도 없다.

현재 코드는 `xpe_preprocess_init()` 이 성공한 뒤 맵만 없을 때도 이 코드를 돌려준다
(`offset_correct.cpp:308` `if (!g_calib.offset_map) return XPE_ERR_NOT_INITIALIZED;`).
호출자는 "init 을 안 불렀다"와 "init 은 됐는데 캘리브레이션이 없다"를 **구별할 수 없다.**

### 2.4 exit 127 중단 — 원인 규명 (`a12-asan-crash.log`)

leader 요청대로 ASan 으로 1회 관측했다. **크래시는 구현이 아니라 테스트에 있다.**

```
==18716==ERROR: AddressSanitizer: attempting double-free on 0x12c7b98a2140
  #5 PipelineExTest_PipelineExWithState_Test::TestBody  test_pipeline_ex.cpp:209
     (std::vector<unsigned short> 소멸자)
freed by thread T0 here:
  #1 PipelineExTest_PipelineExWithState_Test::TestBody  test_pipeline_ex.cpp:207
     (std::free)
```

`test_pipeline_ex.cpp:206-207`:

```cpp
// Clean up: gain_correct allocated a new buffer
std::free(img.data);
```

테스트는 **구 API 의 소유권 이전 계약**을 전제한다 — 옛 `xpe_gain_correct` 는 `img.data` 를
새로 malloc 한 float 버퍼로 갈아끼웠고, 그래서 테스트가 free 할 책임이 있었다. 현재 구현은
그렇게 하지 않으므로 `img.data` 는 여전히 `std::vector<uint16_t>` 의 버퍼이고, 테스트가
free 한 뒤 벡터 소멸자가 한 번 더 free 한다.

**#117 과 별개 결함이 아니라 같은 갈래의 증상**이다: 구 설계(맵 인자 + 소유권 이전)를
전제한 테스트가 신 설계(전역 맵 + 호출자 소유 출력 버퍼) 위에서 도는 것.
수정처는 테스트이며 A-20 등록 카드에서 함께 처리하면 된다.

### 2.5 `xpe_calib_state_load` 미충전 관측 (A-15 이관 1번)

`pipeline.cpp:326-342` 는 `xpe_calib_load_offset/gain/defect_map` 을 불러 **전역 `g_calib`
에만** 싣고 `cs->offsetMap` / `gainMap` / `defectMap` 은 채우지 않는다. 소스 주석도
*"Get defect map from calibState (other maps use g_calib)"* 라고 적어 defect 만 구조체를
쓴다고 밝힌다. 즉 미충전은 버그라기보다 **§2.1 갈라짐이 구조체 계약에까지 이어진 결과**다.
문서에 `XpeCalibrationState` 의 충전 계약이 없으므로 여기서도 "요구 없음"이다.

## 3. baseline 귀속

문서 인용은 HEAD `524e6a6` 트리의 `.moai/specs/SPEC-XPE-P1A/{spec,acceptance}.md` 와
`docs/project/api-spec.md`. ASan 관측은 `build/asan-a17` 트리에서 `test_pipeline_ex.cpp` 를
**임시로** 등록해 1회 실행한 뒤 등록을 원복했다(`git status` 클린 확인).

## 4. 카드 3항 — "initialized, calibration absent" 를 구별해 표현할 수 있는가

**지금은 불가능하고, 구별이 필요하다는 것이 이 판정의 실질적 결론이다.**

두 갈래가 있고 어느 쪽이든 leader/SRS 결정이 필요하다:

- **A. SPEC 으로 수렴** — 구현을 문서에 맞춰 맵을 인자로 되돌린다. 그러면 "미로드" 상태
  자체가 사라지고 NULL 맵은 `XPE_ERR_INVALID_INPUT` 이 된다(문서의 오류 코드 목록과 일치).
  파급: 보정 3함수 시그니처, `pipeline.cpp`, `calibration_cache`, 호출자 전부. 큼.
- **B. 구현으로 수렴** — 전역 캘리브레이션 설계를 정본으로 인정하고 문서를 고친다.
  그러면 "미로드"는 실재하는 상태가 되므로 **전용 코드가 필요하다.**
  `XPE_ERR_NOT_INITIALIZED` 재사용은 §2.3 정의와 충돌하므로 부적절하다.
  `XPE_ERR_CALIBRATION_EXPIRED`(-5) 는 만료 전용이라 맞지 않고, 남는 선택지는
  `XPE_ERR_CONFIG_INVALID`(-4) 재사용 또는 신규 코드다. 신규 코드는 ABI 표면 변경이라
  `xpe_error.h` · api-spec · 모든 모듈의 오류 매핑을 건드린다.

**의견**: 어느 쪽을 고르든, 지금처럼 두 상태가 같은 코드로 보고되는 상태는 유지하면 안 된다.
호출자(특히 C# 호스트)가 "캘리브레이션을 실어라"와 "init 을 불러라"를 구분할 수 없고,
이는 운영자에게 잘못된 조치를 유도한다. B 를 고른다면 신규 코드가 정직하다 — 기존 코드
재사용은 §2.3 과 같은 종류의 의미 중첩을 하나 더 만드는 일이다.

## 5. Gaps (미검증) / 잔여 위험

**Gaps**

- **SRS 원문을 읽지 않았다.** `spec.md` 가 인용하는 `SRS-CALIB-001~004`, `SRS-INIT-003` 의
  실제 문서를 확인하지 않았다. SRS 에 미로드 요구가 있다면 이 판정("요구 없음")은 뒤집힌다.
- `xpe_offset_correct` 외 다른 진입점의 `NOT_INITIALIZED` 사용처를 전수 조사하지 않았다.
  `offset_correct.cpp:308` 한 곳만 인용했다.
- **C# 쪽 기대치를 확인하지 않았다**(카드가 변경을 금지했고 Lane C 소유). 호스트가 현재
  동작에 의존하고 있다면 A/B 어느 쪽이든 Lane C 카드가 필요하다.
- ASan 관측은 `PipelineExWithState` 1건이다. 등록 보류된 다른 3파일에 같은 소유권 전제가
  더 있는지는 확인하지 않았다.

**잔여 위험**

- 이 판정은 **문서를 정본으로 놓고 읽은 결과**다. 만약 설계 변경이 의도적이고 문서 갱신만
  누락된 것이라면, "구현이 SPEC 과 갈라졌다"가 아니라 "문서가 낡았다"가 맞는 서술이다.
  둘을 가르는 근거는 이 트리 안에 없다 — 결정 이력을 아는 사람의 판단이 필요하다.
- 판정을 미루는 동안 테스트 4파일(44 케이스)은 계속 실행되지 않고, A-17 의 gain/defect
  가드도 미실행 상태로 남는다.
