---
name: xpe-testing
description: "XPE Google Test 작성 패턴. C ABI 경계 테스트, 픽셀 정확도(Scalar vs SIMD), 스테이트풀 생명주기, 메모리 누수, 재현성. tests/ 디렉토리 작업 시 트리거. '테스트 작성', 'Google Test', 'ABI 검증', '픽셀 정확도', '벤치마크 테스트' 요청에 반드시 사용."
---

# XPE Testing

Google Test를 사용한 XPE 모듈 테스트. "존재 확인"이 아닌 경계면 교차 비교를 핵심으로 한다.

## 테스트 디렉토리 구조

```
tests/
├── CMakeLists.txt                    # 테스트 스위트 등록
├── common_smoke/                     # 기존 smoke 테스트
├── {module}_tests/
│   ├── CMakeLists.txt
│   ├── test_{module}_abi.cpp         # C ABI 경계 테스트
│   ├── test_{module}_algorithm.cpp   # 알고리즘 정확도 테스트
│   └── test_{module}_lifecycle.cpp   # 생명주기 테스트 (스테이트풀)
└── golden_data/                      # 기준 출력 파일
    └── {module}/
```

## 테스트 CMakeLists.txt 패턴

```cmake
add_executable(test_{module}
    test_{module}_abi.cpp
    test_{module}_algorithm.cpp
    test_{module}_lifecycle.cpp
)

target_link_libraries(test_{module}
    PRIVATE xpe_{module}
    PRIVATE xpe_common
    PRIVATE GTest::gtest_main
)

gtest_discover_tests(test_{module})
```

## C ABI 경계 테스트 (필수)

```cpp
#include <gtest/gtest.h>
#include "xpe/{module}/{module}_api.h"

// 1. null 입력 테스트
TEST({Module}Abi, NullHandleReturnsError) {
    EXPECT_EQ(xpe_{module}_process(nullptr, nullptr, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

// 2. null 버퍼 테스트
TEST({Module}Abi, NullInputBufferReturnsError) {
    auto h = xpe_{module}_create(nullptr);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(xpe_{module}_process(h, nullptr, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
    xpe_{module}_destroy(h);
}

// 3. 잘못된 크기
TEST({Module}Abi, ZeroSizeBufferReturnsError) {
    XpeImageBuffer buf{0, 0, 16, 12, XPE_PIXEL_UINT16, nullptr, 0};
    auto h = xpe_{module}_create(nullptr);
    EXPECT_EQ(xpe_{module}_process(h, &buf, &buf, nullptr),
              XPE_ERR_INVALID_INPUT);
    xpe_{module}_destroy(h);
}

// 4. 잘못된 JSON config
TEST({Module}Abi, InvalidJsonConfig) {
    auto h = xpe_{module}_create("{invalid json}");
    // 잘못된 JSON이어도 생성은 성공 (기본값 사용)
    EXPECT_NE(h, nullptr);
    xpe_{module}_destroy(h);
}
```

## 스테이트풀 생명주기 테스트

```cpp
// Create → Process 여러 번 → Destroy
TEST({Module}Lifecycle, CreateProcessDestroy) {
    auto h = xpe_{module}_create(R"({"param":1.0})");
    ASSERT_NE(h, nullptr);

    // 유효 버퍼 준비
    constexpr uint32_t W = 64, H = 64;
    std::vector<uint16_t> in_data(W * H, 1000);
    std::vector<uint16_t> out_data(W * H, 0);
    XpeImageBuffer in_buf{W, H, 16, 12, XPE_PIXEL_UINT16, in_data.data(), W*H*2};
    XpeImageBuffer out_buf{W, H, 16, 12, XPE_PIXEL_UINT16, out_data.data(), W*H*2};

    for (int i = 0; i < 10; ++i)
        EXPECT_EQ(xpe_{module}_process(h, &in_buf, &out_buf, nullptr), XPE_OK);

    xpe_{module}_destroy(h);
}

// Double-destroy 안전성 (crash 없어야 함)
TEST({Module}Lifecycle, DoubleDestroyIsSafe) {
    auto h = xpe_{module}_create(nullptr);
    xpe_{module}_destroy(h);
    xpe_{module}_destroy(h);  // crash 없어야 함
    SUCCEED();
}
```

## Scalar vs SIMD Parity 테스트

```cpp
TEST({Module}Algorithm, ScalarSIMDParity) {
    constexpr uint32_t W = 512, H = 512;
    std::vector<uint16_t> src(W*H), ref_out(W*H), opt_out(W*H);
    // 테스트 데이터 생성 (시드 고정)
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 4095);
    std::generate(src.begin(), src.end(), [&]{ return dist(rng); });

    // Scalar reference
    run_scalar_{module}(src.data(), ref_out.data(), W, H);
    // SIMD optimized
    run_simd_{module}(src.data(), opt_out.data(), W, H);

    // 픽셀 단위 정확히 일치 (또는 최대 1 ULP)
    for (uint32_t i = 0; i < W*H; ++i)
        EXPECT_EQ(ref_out[i], opt_out[i]) << "at pixel " << i;
}
```

## 재현성 테스트

```cpp
TEST({Module}Algorithm, DeterministicOutput) {
    constexpr uint32_t W = 256, H = 256;
    std::vector<uint16_t> src(W*H, 2000);

    auto run = [&]() {
        std::vector<uint16_t> out(W*H);
        auto h = xpe_{module}_create(nullptr);
        XpeImageBuffer in_buf{W, H, 16, 12, XPE_PIXEL_UINT16, src.data(), W*H*2};
        XpeImageBuffer out_buf{W, H, 16, 12, XPE_PIXEL_UINT16, out.data(), W*H*2};
        xpe_{module}_process(h, &in_buf, &out_buf, nullptr);
        xpe_{module}_destroy(h);
        return out;
    };

    auto first = run();
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(first, run()) << "non-deterministic at run " << i+2;
}
```

## vcpkg GTest 연동

```cmake
# 루트 CMakeLists.txt에서
find_package(GTest CONFIG REQUIRED)
enable_testing()

# 각 테스트 모듈
include(GoogleTest)
gtest_discover_tests(test_{module}
    PROPERTIES TIMEOUT 30
)
```
