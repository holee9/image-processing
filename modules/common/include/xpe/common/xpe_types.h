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
