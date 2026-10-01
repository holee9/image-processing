# QA-A-04 — test_xpe_common.cpp 고아 트리 2개 내력 조사

- **지시**: lead — 내력 조사만. **삭제 금지.** 판정은 lead.
- **결론**: 유실되는 커버리지는 **사실상 없다.** 다만 assertion 2건은 빌드본에 없다.
  두 고아는 "죽은 것"이 아니라 **의도적 임시 비활성화**다.

## 1. 왜 빌드되지 않는가 — 죽은 게 아니라 꺼 둔 것

`tests/CMakeLists.txt:15-18` 원문:

```cmake
if(GTest_FOUND)
    add_subdirectory(common_smoke)
    # Temporarily disabled due to GTest conflicts
    # add_subdirectory(common)
    # add_subdirectory(common_unit)
```

주석이 이유를 명시한다 — **GTest 충돌로 임시 비활성화**. 참조가 없는 것은 삭제 대상이라는
뜻이 아니라 미해결 빌드 문제의 흔적이다. lead의 "참조 없음이 죽었다는 증거는 아니다"가 맞다.

추가로 `tests/CMakeLists.txt`의 `check` / `check_verbose` 타깃이 `DEPENDS test_xpe_common`을
걸고 있는데, 이 타깃은 현재 `modules/common/CMakeLists.txt:116`이 만든다. 즉 tests/ 트리는
자기가 못 만드는 타깃에 의존하는 상태로 남아 있다.

## 2. 언제 갈라졌나

```
modules/common/tests/test_xpe_common.cpp   추가 fbe3a64 2026-04-16   최종 f057d9e 2026-04-20   601줄
tests/common/test_xpe_common.cpp           추가 4dffef4 2026-04-16   최종 29d3cb2 2026-04-19   596줄
tests/common_unit/test_xpe_common.cpp      추가 baead62 2026-04-15   최종 29d3cb2 2026-04-19   491줄
```

`tests/common_unit`이 최초(4/15), 다음 날 `tests/common`과 `modules/common/tests`가 같은 날
갈라졌다(4/16). 두 고아는 4/19에 함께 멈췄고, 빌드본만 4/20까지 이어졌다.
즉 4/19~20 사이에 `modules/common/tests`로 일원화된 것으로 보인다.

## 3. 커버리지 유실 — 케이스명 대조

```
빌드됨 modules/common/tests : 53 케이스
고아  tests/common          : 52 케이스   빌드본에 없는 케이스 0건
고아  tests/common_unit     : 61 케이스   빌드본에 없는 케이스 61건 (픽스처명 전면 상이)
```

### 3-1. `tests/common` — 빌드본이 상위집합

케이스명 기준 고유 케이스 0건. 실제 diff는 21줄뿐이고, 방향은 대부분 **빌드본이 더 많다**:

- 빌드본에만 있는 테스트: `AllocImageForUint8Format` (11줄)
- 빌드본이 `[[maybe_unused]]` 정리 반영

**단, 고아에만 있는 assertion 2건**:

```cpp
tests/common/test_xpe_common.cpp:273-274
    void* dataPtr = buf.data;
    EXPECT_NE(dataPtr, nullptr);          // alloc 후 data 포인터 non-null 검사

tests/common/test_xpe_common.cpp:572-573
    EXPECT_EQ(xpe_get_pending_alert_count(), numAlerts);   // 동시성 테스트의 최종 카운트 검사
```

두 건은 빌드본에 없다. 케이스 자체는 양쪽에 있으나 검사 강도가 다르다.

### 3-2. `tests/common_unit` — 이름은 전부 다르나 개념은 중복, 레이아웃 4건만 고유

61건 중 57건(`XpeCommonFixture` 28, `XpeErrorString` 5, `XpeImageFixture` 15, `XpeLifecycle` 5,
`XpeThreadSafety` 1, `XpeVersion` 3)은 픽스처명만 다를 뿐 빌드본이 같은 대상을 덮는다
(Configure/Alert/ParamRange/Log/Version/Alloc·Copy·Free).

고유해 보이는 것은 `XpeStructLayout` 4건뿐인데, **빌드본보다 약하다**:

```cpp
tests/common_unit — 런타임 검사
    EXPECT_GE(sizeof(XpeImageBuffer), 36u);
    EXPECT_GE(sizeof(XpeImageMetadata), 88u);
    EXPECT_EQ(offsetof(XpeImageBuffer, data) % 8, 0u);
    EXPECT_EQ(offsetof(XpeImageMetadata, acquisitionTime) % 8, 0u);

modules/common/include/xpe/common/xpe_types.h:128-142 — 컴파일 타임 강제 (이미 존재)
    static_assert(sizeof(XpeImageBuffer)   == 40u, ...);
    static_assert(sizeof(XpeImageMetadata) == 96u, ...);
    static_assert(sizeof(XpePixelFormat)   ==  4u, ...);
    static_assert(offsetof(XpeImageBuffer,   width)           ==  0u, ...);
    static_assert(offsetof(XpeImageBuffer,   height)          ==  4u, ...);
    static_assert(offsetof(XpeImageBuffer,   data)            == 24u, ...);
    static_assert(offsetof(XpeImageMetadata, bodyPart)        ==  0u, ...);
    static_assert(offsetof(XpeImageMetadata, acquisitionTime) == 80u, ...);
```

헤더는 정확값(`== 40`, `== 96`, 정확한 오프셋)을 **컴파일 타임에** 강제한다.
고아 테스트는 하한(`>= 36`, `>= 88`)과 8바이트 정렬만 런타임에 본다. 즉 헤더 쪽이
엄격히 더 강하고, 헤더를 include 하는 모든 TU에서 이미 검증되고 있다.
P/Invoke ABI 레이아웃 커버리지 유실은 없다.

## 4. 부수 발견 — QA-A-02 판단과 관련

`tests/common`은 `sscanf_s`를 이식성 가드로 감싸고 있었다:

```cpp
tests/common/test_xpe_common.cpp:368-372
    #ifdef _MSC_VER
        ... sscanf_s ...
    #else
        int parsed = sscanf(ver, "%d.%d.%d", &major, &minor, &patch);
    #endif
```

빌드본 `modules/common/tests/test_xpe_common.cpp:378`은 이 가드를 떼고 `sscanf_s`를
무조건 쓴다. QA-A-02에서 나도 가드 없이 `_s`를 택했고 근거는 "저장소가 MSVC 단일 타깃"이었다.
그 판단 자체는 유지하되, **과거에 이식성을 고려한 흔적이 있었다**는 사실은 기록해 둔다.
비MSVC 지원을 되살릴 계획이 있다면 QA-A-02 변경도 함께 재검토 대상이다.

## 5. 판정 재료 (lead 소관 — 본 카드는 삭제·수정 없음)

| 항목 | 사실 |
|---|---|
| 두 고아의 비활성화 사유 | GTest 충돌, 임시 (주석 명시) |
| `tests/common` 고유 커버리지 | assertion 2건 (data non-null, 동시성 최종 카운트) |
| `tests/common_unit` 고유 커버리지 | 없음 — 레이아웃 4건은 헤더 static_assert가 더 강하게 대체 |
| 삭제 시 실제 손실 | assertion 2건 + 이식성 가드 1곳의 이력 |
| 미해결 부채 | GTest 충돌 원인 자체는 규명되지 않음. `check` 타깃 의존 불일치 잔존 |

선택 가능한 방향(판정은 lead):
- (가) assertion 2건을 빌드본에 이식한 뒤 두 고아 제거 → 손실 0
- (나) GTest 충돌을 먼저 규명해 tests/ 트리 복구 → 부채 해소, 비용 미상
- (다) 현상 유지 → 세 사본 공존, 신규 인원 혼동 지속

## 미검증 (Gaps)

- **GTest 충돌의 실제 원인은 조사하지 않았다.** 주석의 진술을 그대로 인용했을 뿐,
  지금도 재현되는지 확인하지 않았다(재현하려면 CMakeLists 수정이 필요한데 본 카드 범위 밖).
- 케이스명·diff 기반 대조다. 같은 이름 아래 assertion 내용이 다른 경우는
  `tests/common`에 대해서만 확인했고, `tests/common_unit`(픽스처명 전면 상이)은
  개념 매핑 수준까지만 확인했다. 57건 각각의 assertion 강도는 대조하지 않았다.
- 두 고아가 4/19에 멈춘 커밋 `29d3cb2`의 의도는 커밋 메시지를 읽지 않았다.
