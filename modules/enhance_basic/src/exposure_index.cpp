// SWU-2.10: Exposure Index / Deviation Index (IEC 62494-1)
// SPEC-XPE-P1B-ENH  REQ-ENH-023..030

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/enhance_basic/enhance_basic_internal.h"

#include <cmath>
#include <cstring>
#include <cstdio>

namespace {

// @MX:NOTE: S0_REFERENCE calibration constant per ALG-SPEC-001 baseline.
// @MX:SPEC: REQ-ENH-023
constexpr float S0_REFERENCE = 1000.0f;

// EIT lookup table: body part name -> Exposure Index Target value.
// Case-insensitive matching. Unknown/empty defaults to 200.0f.
struct EitEntry {
    const char* bodyPart;
    float       eit;
};

constexpr EitEntry kEitTable[] = {
    {"CHEST",   200.0f},
    {"HAND",    100.0f},
    {"FOOT",    100.0f},
    {"ABDOMEN", 250.0f},
    {"PELVIS",  250.0f},
    {"SPINE",   300.0f},
    {"SKULL",   320.0f},
};
constexpr int kEitTableSize = static_cast<int>(sizeof(kEitTable) / sizeof(kEitTable[0]));
constexpr float kDefaultEit = 200.0f;

// Case-insensitive string comparison (portable, no dependency on _stricmp).
static bool str_iequal(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a >= 'a' && *a <= 'z' ? static_cast<char>(*a - 32) : *a;
        char cb = *b >= 'a' && *b <= 'z' ? static_cast<char>(*b - 32) : *b;
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static float lookup_eit(const char* bodyPart) {
    if (!bodyPart || bodyPart[0] == '\0') return kDefaultEit;
    for (int i = 0; i < kEitTableSize; ++i) {
        if (str_iequal(bodyPart, kEitTable[i].bodyPart)) {
            return kEitTable[i].eit;
        }
    }
    return kDefaultEit;
}

} // anonymous namespace

extern "C" {

// @MX:ANCHOR: xpe_calc_exposure_index is the EI/DI computation entry point (IEC 62494-1).
// @MX:REASON: [AUTO] Public API boundary, called by C# orchestrator and pipeline. REQ-ENH-023..030.
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img,
                                              const XpeImageMetadata* meta,
                                              float* outEI,
                                              float* outDI)
{
    // REQ-ENH-027: NULL pointer validation
    if (!img || !meta || !outEI || !outDI) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-ENH-028: empty image check
    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    XpeErrorCode err = validate_float32_image(img);
    if (err != XPE_OK) return err;

    const float* px = const_float_pixels(img);
    const uint64_t n = static_cast<uint64_t>(img->width) * img->height;

    // Compute mean pixel value (REQ-ENH-023)
    double sum = 0.0;
    for (uint64_t i = 0; i < n; ++i) {
        sum += static_cast<double>(px[i]);
    }
    float mean = static_cast<float>(sum / static_cast<double>(n));

    // REQ-ENH-030: zero/negative mean indicates invalid detector data
    if (mean <= 0.0f) {
        *outEI = 0.0f;
        *outDI = 0.0f;
        return XPE_ERR_PROCESSING_FAILED;
    }

    // REQ-ENH-025: EIT lookup by body part
    float eit = lookup_eit(meta->bodyPart);

    // REQ-ENH-023: EI = EIT * (mean / S0_REFERENCE)
    float ei = eit * (mean / S0_REFERENCE);

    // REQ-ENH-024: DI = 10.0 * log10(EI / EIT)
    float di = 10.0f * std::log10(ei / eit);

    *outEI = ei;
    *outDI = di;

    // REQ-ENH-026: post WARNING alert if |DI| > 3.0
    if (di < -3.0f || di > 3.0f) {
        char alertMsg[256];
        std::snprintf(alertMsg, sizeof(alertMsg),
                      "Exposure deviation: DI=%.2f (outside [-3.0, +3.0] range)", di);
        xpe_test_inject_alert(alertMsg, XPE_ALERT_WARNING);
    }

    return XPE_OK;
}

} // extern "C"
