/**
 * @file xcal_format.h
 * @brief XCal v1 calibration file format definition (SPEC-XPE-P1A SUP-01)
 *
 * XCal v1 canonical 152-byte fixed header layout (0x98 = 152 variable-data start):
 *
 * Offset  Size  Field              Type
 * 0x00    4     magic[4]           "XCAL"
 * 0x04    4     version            uint32_t LE = 1
 * 0x08    4     type               uint32_t LE: 0=OFFSET, 1=GAIN, 2=DEFECT
 * 0x0C    4     pixel_format       uint32_t LE: 0=UINT16, 1=FLOAT32, 2=UINT8_MASK
 * 0x10    4     width              uint32_t LE
 * 0x14    4     height             uint32_t LE
 * 0x18    8     created_epoch_ms   int64_t LE
 * 0x20    8     expiry_epoch_ms    int64_t LE (0 = never expires)
 * 0x28    64    session_id         char[64] (UTF-8, NUL-padded)
 * 0x68    8     config_json_len    uint64_t LE
 * 0x70    8     payload_len        uint64_t LE
 * 0x78    32    sha256[32]         uint8_t[32]
 * 0x98    ---   config_json bytes, then payload bytes (variable)
 *
 * SHA-256 covers (config_json || payload). Header is excluded from hash.
 * File total = 136 + config_json_len + payload_len exactly.
 *
 * REQ-P1A-014, REQ-P1A-015, REQ-P1A-016 (SUP-01)
 * REQ-P1A-002: Pack=1 for file I/O struct; Pack=8 for in-memory ABI
 */

#ifndef XPE_XCAL_FORMAT_H
#define XPE_XCAL_FORMAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* XCal calibration type codes */
typedef enum XCalType {
    XCAL_TYPE_OFFSET = 0,  /* Dark/offset map (FLOAT32 payload) */
    XCAL_TYPE_GAIN   = 1,  /* Gain/flat-field map (FLOAT32 payload) */
    XCAL_TYPE_DEFECT = 2   /* Defect pixel map (UINT8_MASK payload) */
} XCalType;

/* XCal payload pixel format codes */
typedef enum XCalPixelFormat {
    XCAL_FMT_UINT16    = 0,  /* 16-bit unsigned integer */
    XCAL_FMT_FLOAT32   = 1,  /* 32-bit IEEE 754 float */
    XCAL_FMT_UINT8_MASK = 2  /* 8-bit boolean mask (defect map) */
} XCalPixelFormat;

/* XCal v1 magic string (4 bytes, no NUL terminator in file) */
#define XCAL_MAGIC "XCAL"
#define XCAL_VERSION 1u

/* Maximum allowed dimensions per axis (prevents runaway allocation) */
#define XCAL_MAX_DIM 4096u

/* Maximum config JSON length (sanity cap: 1 MB) */
#define XCAL_MAX_CONFIG_JSON_LEN (1024u * 1024u)

/* @MX:ANCHOR: [AUTO] XCalFileHeader -- canonical file format contract
 * @MX:REASON: SHA-256 coverage and pack=1 struct layout are invariant;
 *             any change breaks all existing XCal files on disk.
 * @MX:SPEC: SPEC-XPE-P1A SUP-01
 */

/* Pack=1 for exact binary file I/O (no padding between fields) */
#pragma pack(push, 1)

/**
 * @brief XCal v1 fixed 136-byte file header.
 *
 * MUST be written/read with pack=1 to ensure byte-exact file layout.
 * In-memory use (for P/Invoke) should copy fields individually or use
 * the provided accessor functions.
 */
typedef struct XCalFileHeader {
    char     magic[4];           /**< File magic: "XCAL" (no NUL) */
    uint32_t version;            /**< Format version = 1 */
    uint32_t type;               /**< XCalType: 0=OFFSET, 1=GAIN, 2=DEFECT */
    uint32_t pixel_format;       /**< XCalPixelFormat */
    uint32_t width;              /**< Image width in pixels [1..4096] */
    uint32_t height;             /**< Image height in pixels [1..4096] */
    int64_t  created_epoch_ms;   /**< Creation timestamp (ms since Unix epoch) */
    int64_t  expiry_epoch_ms;    /**< Expiry timestamp (0 = never expires) */
    char     session_id[64];     /**< Session identifier (UTF-8, NUL-padded) */
    uint64_t config_json_len;    /**< Length of config JSON blob in bytes */
    uint64_t payload_len;        /**< Length of pixel payload in bytes */
    uint8_t  sha256[32];         /**< SHA-256 of (config_json || payload) */
} XCalFileHeader;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

/* Compile-time size assertions
 *
 * XCal v1 canonical layout (pack=1, 152 bytes total):
 * magic[4](4) + version(4) + type(4) + pixel_format(4) + width(4) + height(4) = 24 bytes
 * created_epoch_ms(8) + expiry_epoch_ms(8) = 16 bytes  -> cumulative 40
 * session_id[64](64) = 64 bytes            -> cumulative 104
 * config_json_len(8) + payload_len(8)      -> cumulative 120
 * sha256[32](32)                           -> cumulative 152
 * Variable data starts at offset 0x98 = 152.
 */
#ifdef __cplusplus
static_assert(sizeof(XCalFileHeader) == 152,
    "XCalFileHeader must be exactly 152 bytes (pack=1 layout)");
static_assert(offsetof(XCalFileHeader, version)          ==  4, "version offset");
static_assert(offsetof(XCalFileHeader, type)             ==  8, "type offset");
static_assert(offsetof(XCalFileHeader, pixel_format)     == 12, "pixel_format offset");
static_assert(offsetof(XCalFileHeader, width)            == 16, "width offset");
static_assert(offsetof(XCalFileHeader, height)           == 20, "height offset");
static_assert(offsetof(XCalFileHeader, created_epoch_ms) == 24, "created_epoch_ms offset");
static_assert(offsetof(XCalFileHeader, expiry_epoch_ms)  == 32, "expiry_epoch_ms offset");
static_assert(offsetof(XCalFileHeader, session_id)       == 40, "session_id offset");
static_assert(offsetof(XCalFileHeader, config_json_len)  == 104, "config_json_len offset");
static_assert(offsetof(XCalFileHeader, payload_len)      == 112, "payload_len offset");
static_assert(offsetof(XCalFileHeader, sha256)           == 120, "sha256 offset");
#endif /* __cplusplus */

#endif /* XPE_XCAL_FORMAT_H */
