#ifndef XPE_TYPES_H
#define XPE_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
    #ifdef XPE_DLL_EXPORT
        #define XPE_API __declspec(dllexport)
    #else
        #define XPE_API __declspec(dllimport)
    #endif
#else
    #define XPE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Pack = 8 for C#/C++ struct packing alignment */
#pragma pack(push, 8)

typedef enum XpePixelFormat {
    XPE_PIXEL_UINT16  = 0,
    XPE_PIXEL_FLOAT32 = 1
} XpePixelFormat;

typedef struct XpeImageBuffer {
    uint32_t       width;
    uint32_t       height;
    uint32_t       bitsAllocated;
    uint32_t       bitsStored;
    XpePixelFormat format;
    void*          data;       /* Allocated via xpe_alloc_image, freed via xpe_free_image */
    size_t         dataSize;   /* Max: 4096*4096*4 = 64 MB */
} XpeImageBuffer;

typedef struct XpeImageMetadata {
    char     bodyPart[64];     /* Fixed-size C string, no std::string */
    float    kVp;
    float    mAs;
    float    SID_mm;
    float    pixelPitch_mm;
    uint64_t acquisitionTime;
    uint32_t flags;            /* Stable ABI bitfield, see XPE_FLAG_* defines below */
} XpeImageMetadata;

#pragma pack(pop)

/* Compile-time struct size verification for P/Invoke compatibility (T-005) */
#ifdef __cplusplus

// @MX:ANCHOR: [AUTO] P/Invoke struct size verification -- SPEC-XPE-P0
// @MX:REASON: Ensures C#/C++ ABI compatibility; prevents marshaling errors

/* XpeImageBuffer size verification (Pack=8) */
/* Layout: width(4) + height(4) + bitsAllocated(4) + bitsStored(4) + format(4) + data(8) + dataSize(8) = 36 bytes */
static_assert(sizeof(XpeImageBuffer) == 36, "XpeImageBuffer must be 36 bytes for P/Invoke Pack=8 compatibility");

/* XpeImageMetadata size verification (Pack=8) */
/* Layout: bodyPart(64) + kVp(4) + mAs(4) + SID_mm(4) + pixelPitch_mm(4) + acquisitionTime(8) + flags(4) = 92 bytes */
static_assert(sizeof(XpeImageMetadata) == 92, "XpeImageMetadata must be 92 bytes for P/Invoke Pack=8 compatibility");

/* XpePixelFormat size verification (enum is 4 bytes in MSVC with Pack=8) */
static_assert(sizeof(XpePixelFormat) == 4, "XpePixelFormat must be 4 bytes");

/* Verify no padding between struct members (critical for C# marshaling) */
static_assert(offsetof(XpeImageBuffer, width) == 0, "XpeImageBuffer.width offset must be 0");
static_assert(offsetof(XpeImageBuffer, height) == 4, "XpeImageBuffer.height offset must be 4");
static_assert(offsetof(XpeImageBuffer, data) == 20, "XpeImageBuffer.data offset must be 20");
static_assert(offsetof(XpeImageMetadata, bodyPart) == 0, "XpeImageMetadata.bodyPart offset must be 0");
static_assert(offsetof(XpeImageMetadata, acquisitionTime) == 80, "XpeImageMetadata.acquisitionTime offset must be 80");

#endif

/* Image metadata flags */
#define XPE_FLAG_GHOST_CORRECTED         0x00000001u
#define XPE_FLAG_AI_PROCESSED            0x00000002u
#define XPE_FLAG_DEFECT_CORRECTED        0x00000004u
#define XPE_FLAG_GAIN_CORRECTED          0x00000008u
#define XPE_FLAG_READOUT_VALIDATED       0x00000010u
#define XPE_FLAG_TEMP_COMPENSATED        0x00000020u
#define XPE_FLAG_NONLINEARITY_CORRECTED  0x00000040u
#define XPE_FLAG_BINNING_CORRECTED       0x00000080u
#define XPE_FLAG_AED_TRIGGERED           0x00000100u
#define XPE_FLAG_COLLIMATION_DETECTED    0x00000200u
#define XPE_FLAG_STITCHED                0x00000400u
#define XPE_FLAG_BONE_SUPPRESSED         0x00000800u
#define XPE_FLAG_GSVG_SKIPPED            0x00001000u

#ifdef __cplusplus
}
#endif

#endif /* XPE_TYPES_H */
