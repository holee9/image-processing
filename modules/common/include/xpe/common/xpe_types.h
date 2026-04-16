/**
 * @file xpe_types.h
 * @brief Core image buffer and metadata types shared across all XPE modules.
 *
 * Defines the fundamental plain-old-data structures passed between every XPE
 * processing stage: pixel format selector, image buffer, image metadata, and
 * processing-stage completion flags.
 *
 * @note ABI contract: all structs use `#pragma pack(push, 8)` for deterministic
 *       layout under both MSVC and GCC, enabling safe P/Invoke from C#.
 * @note Thread-safety: these are plain-old-data types. Thread safety is the
 *       caller's responsibility — do not pass the same buffer to concurrent
 *       processing functions without external synchronization.
 * @note Maximum supported image size: 4096 x 4096 x float32 = 64 MB per buffer.
 *
 * @ingroup xpe_common
 */
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

/**
 * @defgroup xpe_common XPE Common
 * @brief Shared types, error codes, and utility functions used by all XPE modules.
 *
 * All XPE modules depend on this group. Types defined here form the stable ABI
 * boundary between the native DLLs and the C# orchestration host.
 * @{
 */

/* Pack = 8 for C#/C++ struct packing alignment */
#pragma pack(push, 8)

/**
 * @brief Pixel format selector for XpeImageBuffer.
 *
 * Determines the data type and interpretation of the pixel buffer.
 * All enhancement and display functions require @c XPE_PIXEL_FLOAT32.
 * Pre-processing input from the detector is @c XPE_PIXEL_UINT16.
 */
typedef enum XpePixelFormat {
    XPE_PIXEL_UINT16  = 0,  /**< 16-bit unsigned integer (raw detector output) */
    XPE_PIXEL_FLOAT32 = 1   /**< 32-bit IEEE 754 single-precision float (post-gain-correction domain) */
} XpePixelFormat;

/**
 * @brief Image pixel buffer with format and dimension metadata.
 *
 * Passed by pointer to all XPE processing functions. Pixel data is always
 * contiguous (no padding between rows): `stride == width * bytes_per_pixel`.
 *
 * Ownership semantics:
 * - Allocate via `xpe_alloc_image()` and free via `xpe_free_image()`.
 * - Never pass a stack-allocated or externally managed `data` pointer to
 *   functions that may reallocate (none currently do, but callers must not
 *   assume this).
 *
 * @note Maximum supported size: 4096 x 4096 x float32 = 64 MB.
 * @note `bitsStored` may be less than `bitsAllocated` for detector data where
 *       only the lower N bits carry meaningful values (e.g. 12-bit pixels in a
 *       16-bit allocation).
 * @ingroup xpe_common
 */
typedef struct XpeImageBuffer {
    uint32_t       width;         /**< Image width in pixels */
    uint32_t       height;        /**< Image height in pixels */
    uint32_t       bitsAllocated; /**< Bits allocated per pixel (16 for UINT16, 32 for FLOAT32) */
    uint32_t       bitsStored;    /**< Bits stored (meaningful data bits; may be <= bitsAllocated) */
    XpePixelFormat format;        /**< Pixel data type selector */
    void*          data;          /**< Pixel data pointer. Ownership: xpe_alloc_image / xpe_free_image */
    size_t         dataSize;      /**< Total byte size of data buffer */
} XpeImageBuffer;

/**
 * @brief Acquisition metadata accompanying an XpeImageBuffer.
 *
 * Carries the physical and clinical context of the acquisition. Used by
 * EI/DI calculation (IEC 62494-1), alert reporting, and DICOM I/O.
 *
 * All string fields are fixed-size C strings (not `std::string`) to maintain
 * a stable, zero-copy ABI across the DLL boundary.
 *
 * @note `flags` is a bitfield updated by each processing stage upon successful
 *       completion. Inspect individual bits using the @c XPE_FLAG_* defines.
 * @note `acquisitionTime` is a POSIX timestamp (seconds since Unix epoch,
 *       UTC). Zero is treated as "unknown" by pipeline stages that use it.
 * @ingroup xpe_common
 */
typedef struct XpeImageMetadata {
    char     bodyPart[64];    /**< Body part identifier (e.g. "CHEST", "HAND"). Used for EIT lookup in IEC 62494-1. */
    float    kVp;             /**< X-ray tube peak voltage in kV */
    float    mAs;             /**< Tube current-time product in mA*s */
    float    SID_mm;          /**< Source-to-Image Distance in mm */
    float    pixelPitch_mm;   /**< Detector pixel pitch in mm */
    uint64_t acquisitionTime; /**< POSIX timestamp of acquisition (seconds since Unix epoch, UTC). 0 = unknown. */
    uint32_t flags;           /**< Processing stage completion flags. See @ref XPE_FLAG_GHOST_CORRECTED and related defines. */
} XpeImageMetadata;

#pragma pack(pop)

/** @name Processing Stage Flags
 *  Bitfield values for XpeImageMetadata::flags.
 *
 *  Each bit is set by its corresponding processing stage upon successful
 *  completion. Callers may inspect these flags to determine which stages
 *  have already been applied and which remain pending.
 *
 *  Flags are additive: a stage sets its bit but must not clear others.
 *  @{
 */
#define XPE_FLAG_GHOST_CORRECTED         0x00000001u  /**< Ghost/lag correction applied (xpe_preprocess) */
#define XPE_FLAG_AI_PROCESSED            0x00000002u  /**< AI module processed (xpe_ai) */
#define XPE_FLAG_DEFECT_CORRECTED        0x00000004u  /**< Bad-pixel / defect map correction applied */
#define XPE_FLAG_GAIN_CORRECTED          0x00000008u  /**< Flat-field gain correction applied */
#define XPE_FLAG_READOUT_VALIDATED       0x00000010u  /**< Detector readout integrity validated */
#define XPE_FLAG_TEMP_COMPENSATED        0x00000020u  /**< Temperature drift compensation applied */
#define XPE_FLAG_NONLINEARITY_CORRECTED  0x00000040u  /**< Detector nonlinearity correction applied */
#define XPE_FLAG_BINNING_CORRECTED       0x00000080u  /**< Binning mode correction applied */
#define XPE_FLAG_AED_TRIGGERED           0x00000100u  /**< Automatic Exposure Detection (AED) triggered acquisition */
#define XPE_FLAG_COLLIMATION_DETECTED    0x00000200u  /**< Collimation region detected and masked */
#define XPE_FLAG_STITCHED                0x00000400u  /**< Image assembled from multiple detector panels (stitching) */
#define XPE_FLAG_BONE_SUPPRESSED         0x00000800u  /**< Bone suppression (AI premium stage) applied */
#define XPE_FLAG_GSVG_SKIPPED            0x00001000u  /**< GSVG stage was skipped (e.g. modality does not require it) */
/** @} */

/** @} */ /* end group xpe_common */

#ifdef __cplusplus
}
#endif

#endif /* XPE_TYPES_H */
