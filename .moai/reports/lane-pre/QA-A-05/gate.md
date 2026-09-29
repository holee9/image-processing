# QA-A-05 — 고아 테스트 고유 assertion 2건 이식

- **브랜치**: `dev/preprocess` · **워크트리**: `D:/workspace-github/xpe-pre`
- **지시**: lead — 고아 `tests/common` 의 고유 assertion 2건을 빌드되는
  `modules/common/tests/test_xpe_common.cpp` 로 이식. 원본 삭제 금지. 깨지면 완화 말고 보고.
- **결과**: 1건 이식 완료. **1건은 이식 거부** — 명세와 모순되는 잘못된 assertion으로 판명.
  ctest 345/345 무회귀.

## 이식 방식 — 기존 케이스 흡수 (새 케이스 아님)

두 assertion 모두 **동일 이름의 케이스가 빌드본에 이미 존재**하고, 셋업·본문 구조가 같으며
차이는 assertion 유무뿐이었다.

```
tests/common                      modules/common/tests
FreeImageReleasesMemory      →    FreeImageReleasesMemory      (동일, assertion 2줄만 없음)
ConcurrentAlertQueueAccess   →    ConcurrentAlertQueueAccess   (동일, assertion 1줄만 없음)
```

새 케이스로 추가하면 같은 대상을 두 케이스가 중복 검증하게 되고, 케이스명이 하나 더 늘어
`tests/common` 과의 대조표(QA-A-04)가 무의미해진다. **기존 케이스 흡수**를 택했다 —
assertion 은 새로운 행위를 검증하는 것이 아니라 기존 케이스의 검사 강도를 높이는 것이다.

## 이식 1 — data non-null (완료)

```diff
 TEST_F(XpeCommonTest, FreeImageReleasesMemory) {
     XpeImageBuffer buf;
     xpe_alloc_image(100, 100, XPE_PIXEL_UINT16, &buf);

+    void* dataPtr = buf.data;
+    EXPECT_NE(dataPtr, nullptr);
     EXPECT_EQ(xpe_free_image(&buf), XPE_OK);
```

원문(`tests/common/test_xpe_common.cpp:273-274`) 그대로. 통과 확인:

```
 28/346 Test #28: XpeCommonTest.FreeImageReleasesMemory ... Passed  0.01 sec
```

이 검사가 채우는 공백: 기존 케이스는 free **이후** `buf.data == nullptr` 만 봤다.
alloc 이 애초에 실패해 `data` 가 null 이었어도 그 사후 검사는 통과한다. 이 assertion 이
alloc 성공을 먼저 확인해 그 구멍을 막는다.

## 이식 2 — 동시성 최종 카운트 (**이식 거부**)

원문(`tests/common/test_xpe_common.cpp:572-573`):

```cpp
EXPECT_EQ(xpe_get_pending_alert_count(), numAlerts);   // numAlerts = 100
```

### 그대로 이식해 실측한 결과 — 결정적 실패

```
52/346 Test #52: XpeCommonTest.ConcurrentAlertQueueAccess ...***Failed  0.02 sec

modules\common\tests\test_xpe_common.cpp(581): error: Expected equality of these values:
  xpe_get_pending_alert_count()
    Which is: 64
  numAlerts
    Which is: 100
```

### 원인 — 명세된 링 버퍼 상한과 모순된다

```cpp
modules/common/src/xpe_common.cpp:45-46
    // Alert queue (bounded ring -- max 64 entries)
    static constexpr std::size_t kAlertQueueMax = 64;

modules/common/src/xpe_common.cpp:84-86
    if (g_alertQueue.size() >= kAlertQueueMax) {
        g_alertQueue.pop_front();      // 가장 오래된 항목 폐기
    }
```

100개를 주입하면 큐에는 정의상 64개만 남는다. 소비자 스레드는
`xpe_get_pending_alert(0, ...)` 로 읽기만 하고 소비하지 않으므로(비파괴 읽기) 카운트가
줄지도 않는다. 따라서 `count == 100` 은 **어떤 실행에서도 성립할 수 없다** —
경합 조건이 아니라 결정적 불일치다.

### 이 assertion 은 작성 시점부터 틀려 있었다

```
kAlertQueueMax = 64 도입   baead62  2026-04-15
고아 파일 최종 커밋        29d3cb2  2026-04-19
```

상한이 고아 파일보다 **먼저** 들어왔다. 즉 나중에 생긴 제약이 기존 테스트를 깨뜨린 것이
아니라, 이미 상한이 있는 상태에서 잘못된 기대가 작성된 것이다.
빌드본 이력에도 이 assertion 은 존재한 적이 없다:

```
git log -S "xpe_get_pending_alert_count(), numAlerts" -- modules/common/tests/...  → 결과 없음
```

`tests/common` 이 비활성화된 채 방치된 이유의 일부일 가능성이 있다.

### 링 버퍼는 명세된 설계다 — 구현 결함이 아니다

```
docs/project/api-spec.md:285
  "Returns the number of unread alerts queued in the internal alert ring buffer."
  SRS: SRS-ALERT-001, SRS-ALERT-002

docs/project/sprint-plan.md:418  SPRINT-P0-05
  "Implement the alert ring buffer (Alert Queue) for warnings, errors, and diagnostic events."
```

문서가 링 버퍼를 명시한다. 따라서 불일치의 원인은 구현이 아니라 assertion 쪽이다.

### 판단 — 완화가 아니라 거부

lead 지시는 "통과시키려고 완화하지 말 것"이었다. 다음 두 가지는 모두 **완화**에 해당해
택하지 않았다.

- `EXPECT_LE(count, numAlerts)` 로 약화 → 무엇도 검증하지 못하는 assertion 이 된다
- `EXPECT_EQ(count, 64)` 로 상수 고정 → 테스트가 export 되지 않은 내부 상수에 결합된다

대신 **이식 자체를 거부**했다. 명세와 모순되는 기대를 살아있는 테스트에 넣으면
빌드가 상시 red 가 되고, 그것은 손실 방지가 아니라 손실 확대다.
QA-A-04 에서 "손실 = assertion 2건" 이라고 보고했으나, 실측 결과 **2건 중 1건은
애초에 가치가 없는 잘못된 기대**였다. 실제 손실은 1건이다.

## lead 판정 요청

원본 트리 처분(별도 카드)에서 이 사실을 반영해야 한다. 추가로 판단이 필요한 것:

**알림 유실이 Class B 에서 허용되는가.** 65번째 알림이 도착하면 가장 오래된 알림이
조용히 사라진다. 링 버퍼는 명세된 설계이나, 명세가 **오버플로 시 동작**(가장 오래된 것
폐기, 호출자 통지 없음)까지 규정하는지는 확인하지 못했다. 운영자에게 보여야 할 경고가
소리 없이 사라지는 경로이므로 SRS 확인 대상으로 올린다. Lane A 단독 판단 사항이 아니다.

## 증거

```
cmake --preset ci-preprocess && cmake --build build/ci-preprocess
BUILD_EXIT=0   warning C####: 0건

ctest --output-on-failure
100% tests passed, 0 tests failed out of 345   CTEST_EXIT=0
 28/346 XpeCommonTest.FreeImageReleasesMemory ... Passed      ← 이식된 assertion 실행 확인
```

원문: `build.log`(최종), `tests.log`(최종). 실패 재현 원문은 본 문서 상단에 인용했다.

## 지키지 않은 것 (지시대로)

- `tests/common`, `tests/common_unit` 삭제하지 않음
- `tests/CMakeLists.txt:17,18` 주석 처리 그대로 둠
- `/WX` 켜지 않음 (#106 — 3레인 합동 카드 소관)

## 미검증 (Gaps)

- **이식성 가드 이력만 기록, 코드 이식 안 함** (지시대로).
  `tests/common:368-372` 은 `sscanf_s` 를 `#ifdef _MSC_VER` 로 감쌌고 빌드본은 뗐다.
- 실패 재현은 1회 실행이다. 결정적 불일치(64 vs 100)라 반복 실행으로 달라질 여지는
  없다고 판단했으나 반복 측정은 하지 않았다.
- 오버플로 시 "가장 오래된 것 폐기" 정책이 SRS-ALERT-001/002 본문에 규정돼 있는지는
  `api-spec.md` 요약만 확인했고 SRS 원문은 대조하지 않았다.

---

# 판정 수령 (2026-08-31, lead)

**QA-A-05 PASS.** 이식 거부 판정 채택 — "약화도 상수 고정도 하지 않고 거부한 것이 정답".
QA-A-04 자기 정정(손실 2건 → 1건)도 접수됐다.

## 본 문서 상단 "lead 판정 요청" 항목의 처리 결과

**알림 유실이 Class B 에서 허용되나 → lead 처리 완료. 이슈 #110.**

lead 가 SRS 를 대조한 결과, 본 문서가 "확인하지 못했다"고 남긴 부분의 답이 나왔다:
**오버플로 정책이 SRS·SDD·api-spec 어디에도 규정돼 있지 않다.** 링 버퍼라는 사실만
문서화돼 있고, 초과 시 무엇을 버리는지·호출자에게 알리는지는 명세가 없다.

추가로 lead 가 확인한 것 — SRS-ALERT-001 은 불량화소 보정 실패 시 팝업 표시를 요구하고,
SRS-ALERT-002 는 `XPE-SDD-002:991` 에서 HAZ-006 에 연결된다. 즉 **요구된 통지가 전달되지
않는 경로가 존재하는데 그것이 위험분석에 반영됐는지 확인되지 않았다.**

정책이 무엇이어야 하는지는 요구사항·위험분석 영역이므로 코드로 정하지 않는다.
**Lane A 는 이 건으로 코드를 건드리지 않는다.**

## QA-A-03 에서 올린 REQ-P0-008 건 → 이슈 #111

lead 확인 결과 본 레인이 보고한 것보다 드리프트가 넓었다:

```
.moai/specs/.../spec.md:52              exactly 15
.moai/specs/.../spec.md:225             18 API   ← 같은 REQ 를 근거로 "✅ DONE"
export-verification:63                  18
research.md:91                          15 (충족 확인)
실측                                     16
```

같은 요구사항 ID 로 15 와 18 이 **동시에 충족 확인**돼 있고 실제는 16 이다.
현재의 16 은 근거 있는 숫자라고 판정됐다 — QA-A-03 이 헤더 선언을 추가해 16==16 을
만들었고, 그 심볼은 `enhance_basic/src/exposure_index.cpp:116` 이 실제로 호출한다.

문서 정렬은 `.moai/specs/` 가 main 소유라 lead 가 수행한다. **Lane A 는 손대지 않는다.**

## 푸시 — 하지 않는다

레인 브랜치는 로컬 유지가 기본. 지시 없이 push 하지 않는다.
origin 반영 시점은 lead 가 사용자와 정한다.

커밋 3건(`564c38e`, `17779d1`, `21a282d`)은 이 워크트리에만 존재하는 유일본이다.
`origin/dev/preprocess` 는 없다 — 워크트리를 제거하면 작업이 사라진다.
