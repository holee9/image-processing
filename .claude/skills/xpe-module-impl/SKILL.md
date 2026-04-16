---
name: xpe-module-impl
description: "XPE C++ 모듈 구현 패턴. xpe_common 의존, C ABI Pack=8, extern C, XPE_API 매크로, init/process/destroy 생명주기, nlohmann/json config, spdlog 로깅. modules/ 디렉토리 작업 시 반드시 이 스킬을 사용할 것. '모듈 구현', '헤더 작성', 'CMakeLists 추가', 'DLL 만들어줘' 요청에도 트리거."
---

# XPE Module Implementation

XPE 모듈(xpe_preprocess, enhance_basic, enhance_advanced, ai, display, dicom, gsvg) 구현 패턴.

## 모듈 레이어 규칙

```
Layer 0: xpe_common (유일한 공통 기반)
Layer 1: preprocess, enhance_basic, enhance_advanced, ai, display, dicom
Layer 1-G: gsvg (독립, xpe_common 미사용, FFTW3 전용)
```

**Lateral dependency 금지:** Layer 1 모듈 간 include 절대 금지. xpe_common만 허용.

## 디렉토리 구조 (모듈당)

```
modules/{name}/
├── include/xpe/{name}/
│   └── {name}_api.h      # 공개 API (C ABI)
├── src/
│   └── {name}.cpp        # 구현
└── CMakeLists.txt
```

## 헤더 패턴 (C ABI)

```cpp
#ifndef XPE_{NAME}_API_H
#define XPE_{NAME}_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 8)

// 스테이트풀 모듈 핸들 (불투명 포인터)
typedef struct Xpe{Name}Ctx* Xpe{Name}Handle;

// 핵심 API
XPE_API Xpe{Name}Handle xpe_{name}_create(const char* configJsonOrNull);
XPE_API XpeErrorCode    xpe_{name}_process(Xpe{Name}Handle h,
                                            const XpeImageBuffer* in,
                                            XpeImageBuffer* out,
                                            const XpeImageMetadata* meta);
XPE_API void            xpe_{name}_destroy(Xpe{Name}Handle h);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
#endif
```

## CMakeLists.txt 패턴

```cmake
add_library(xpe_{name} SHARED
    src/{name}.cpp
)

target_include_directories(xpe_{name}
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xpe_{name}
    PRIVATE xpe_common
    PRIVATE spdlog::spdlog
    PRIVATE nlohmann_json::nlohmann_json
)

set_target_properties(xpe_{name} PROPERTIES
    OUTPUT_NAME "xpe_{name}"
    VERSION ${PROJECT_VERSION}
)
```

## 구현 패턴

### 핸들 패턴 (스테이트풀)
```cpp
#include "xpe/{name}/{name}_api.h"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <mutex>
#include <new>

struct Xpe{Name}Ctx {
    std::mutex mtx;
    // 내부 상태 (헤더에 노출 안 함)
};

XPE_API Xpe{Name}Handle xpe_{name}_create(const char* configJson) {
    auto* ctx = new(std::nothrow) Xpe{Name}Ctx{};
    if (!ctx) return nullptr;

    if (configJson) {
        auto cfg = nlohmann::json::parse(configJson, nullptr, false);
        if (!cfg.is_discarded()) {
            // 파라미터 파싱
        }
    }

    spdlog::info("xpe_{name}: created");
    return reinterpret_cast<Xpe{Name}Handle>(ctx);
}

XPE_API XpeErrorCode xpe_{name}_process(Xpe{Name}Handle h,
                                         const XpeImageBuffer* in,
                                         XpeImageBuffer* out,
                                         const XpeImageMetadata* meta) {
    if (!h || !in || !out) return XPE_ERR_INVALID_INPUT;
    auto* ctx = reinterpret_cast<Xpe{Name}Ctx*>(h);
    std::lock_guard<std::mutex> lock(ctx->mtx);

    // 처리 로직
    return XPE_OK;
}

XPE_API void xpe_{name}_destroy(Xpe{Name}Handle h) {
    if (!h) return;  // null-safe
    delete reinterpret_cast<Xpe{Name}Ctx*>(h);
}
```

## 메모리 관리 규칙

- **출력 버퍼 할당**: `xpe_alloc_image()`로 할당 후 out에 할당. caller가 `xpe_free_image()`로 해제.
- **입력 버퍼**: 읽기 전용. 절대 수정 금지.
- **내부 버퍼**: 모듈 내부에서 `std::vector<float>`로 관리 (RAII).

## GSVG 전용 규칙

gsvg 모듈은 xpe_common을 사용하지 않고 독립 패키지다:
- FFTW3 동적 링크 (`fftw3f`, GPL 준수)
- 자체 타입 정의 (`GsvgImage`, `GsvgResult`)
- xpe_* 함수명 대신 `gsvg_*` 함수명 사용
