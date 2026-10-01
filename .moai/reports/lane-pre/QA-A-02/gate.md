# QA-A-02 — 컴파일러 경고 3건 해소

- **브랜치**: `dev/preprocess` · **워크트리**: `D:/workspace-github/xpe-pre`
- **지시**: lead 판정 [A] — 경고 3건만 해소, `/WX` 활성화는 하지 말 것
- **결과**: 경고 3 → **0**, 테스트 345/345 유지

## 수정 (3파일 3줄)

```diff
modules/preprocess/tests/test_ghost_correct.cpp:35
-        meta.acquisitionTime = 0.0;
+        meta.acquisitionTime = 0;
```
`XpeImageMetadata.acquisitionTime`은 `uint64_t`(xpe_types.h:114)인데 double 리터럴을
대입해 C4244가 났다. 캐스트를 씌우는 대신 정수 리터럴로 바꿨다 — 같은 파일군의
`test_xpe_preprocess_correction.cpp:71`이 이미 `= 0`을 쓰고 있어 표기도 일치한다.

```diff
modules/preprocess/tests/test_xpe_sha256.cpp:28
-        std::sscanf(hex + 2 * i, "%02x", &byte);
+        sscanf_s(hex + 2 * i, "%02x", &byte);
```

```diff
modules/preprocess/tests/test_xpe_preprocess_correction.cpp:67
-    std::strncpy(meta->bodyPart, "CHEST", sizeof(meta->bodyPart) - 1);
+    strncpy_s(meta->bodyPart, sizeof(meta->bodyPart), "CHEST", _TRUNCATE);
```

## `_s` 계열을 고른 근거

TU 한정 `_CRT_SECURE_NO_WARNINGS` 매크로가 아니라 `_s` 계열을 택했다. 코드베이스에
이미 확립된 관례이기 때문이다(가드 없이 그대로 사용 중):

```
modules/common/src/xpe_common.cpp:274            strncpy_s(msg, msgLen, ...)
modules/common/tests/test_xpe_common.cpp:378     sscanf_s(ver, "%d.%d.%d", ...)
modules/enhance_advanced/tests/test_integration.cpp:61,126,185,290,376,498,568
                                                 strncpy_s(meta.bodyPart, sizeof(...), "CHEST", _TRUNCATE)
```

이식성 검토: `_s` 계열은 MSVC 확장이라 비MSVC 빌드를 깬다. 다만 기존 사용처 어디에도
`_MSC_VER` 가드가 없고, `.github/workflows/*.yml` 전 잡이 `windows-2025`다. 즉 이 저장소는
MSVC 단일 타깃이며 `_s` 사용이 새로 위험을 들이지 않는다. 경고 억제 매크로는 해당 TU의
다른 경고까지 함께 가려 버리므로 택하지 않았다.

`%02x`(`unsigned int*`)는 `sscanf_s`에서 크기 인자를 요구하지 않는다(`%s`/`%c`만 요구).

## 증거

```
cmake --preset ci-preprocess && cmake --build build/ci-preprocess
CONFIGURE_EXIT=0  BUILD_EXIT=0
grep -ciE "warning C[0-9]" build.log  →  0        (QA-A-01 시점 3)

ctest --output-on-failure
100% tests passed, 0 tests failed out of 345
CTEST_EXIT=0
```

원문: `build.log`, `tests.log`

## 미검증 (Gaps)

- `/WX`는 lead 지시대로 켜지 않았다. 따라서 앞으로 유입되는 경고를 빌드가 막지 못하는
  상태는 그대로다. 3레인 합동 카드 소관.
- 미실행 14건(Skipped 13 / Disabled 1)은 QA-A-01과 동일하게 그대로다.
