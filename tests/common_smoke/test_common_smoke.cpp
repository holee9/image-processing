#include "xpe/common/xpe_common_api.h"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }

    return true;
}

}  // namespace

int main() {
    bool ok = true;

    ok &= Expect(xpe_init(nullptr) == XPE_OK, "xpe_init should accept a null config");
    ok &= Expect(xpe_version() != nullptr, "xpe_version should return a non-null pointer");
    ok &= Expect(std::strlen(xpe_version()) > 0, "xpe_version should return a non-empty string");
    ok &= Expect(std::strcmp(xpe_error_string(XPE_ERR_INVALID_INPUT), "Invalid input parameter") == 0,
                 "xpe_error_string should map known error codes");

    float minValue = 0.0f;
    float maxValue = 0.0f;
    float defaultValue = 0.0f;
    ok &= Expect(xpe_get_param_range("CHEST", "windowWidth", &minValue, &maxValue, &defaultValue) == XPE_OK,
                 "xpe_get_param_range should return a default range");
    ok &= Expect(minValue <= defaultValue && defaultValue <= maxValue,
                 "xpe_get_param_range should return an ordered range");

    XpeImageBuffer source {};
    XpeImageBuffer target {};
    ok &= Expect(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &source) == XPE_OK,
                 "xpe_alloc_image should allocate the source image");
    ok &= Expect(source.data != nullptr, "xpe_alloc_image should allocate a backing buffer");
    ok &= Expect(source.dataSize == 32, "4x4 uint16 image should consume 32 bytes");

    auto* sourcePixels = static_cast<std::uint16_t*>(source.data);
    for (std::size_t index = 0; index < 16; ++index) {
        sourcePixels[index] = static_cast<std::uint16_t>(index * 3);
    }

    ok &= Expect(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &target) == XPE_OK,
                 "xpe_alloc_image should allocate the destination image");
    ok &= Expect(xpe_copy_image(&source, &target) == XPE_OK,
                 "xpe_copy_image should copy the image payload");
    ok &= Expect(std::memcmp(source.data, target.data, source.dataSize) == 0,
                 "xpe_copy_image should preserve the image contents");
    ok &= Expect(target.width == source.width && target.height == source.height,
                 "xpe_copy_image should preserve the image dimensions");

    ok &= Expect(xpe_get_pending_alert_count() == 0, "freshly initialized runtime should not expose alerts");

    ok &= Expect(xpe_free_image(&source) == XPE_OK, "xpe_free_image should release the source buffer");
    ok &= Expect(xpe_free_image(&target) == XPE_OK, "xpe_free_image should release the target buffer");

    xpe_shutdown();

    if (!ok) {
        return 1;
    }

    std::cout << "xpe_common smoke test passed\n";
    return 0;
}
