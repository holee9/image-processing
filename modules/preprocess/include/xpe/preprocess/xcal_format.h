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

/** @brief XCal calibration data type selector. */
typedef enum XCalType {
    XCAL_TYPE_OFFSET     = 0,  /* Dark/offset map (FLOAT32 payload) */
    XCAL_TYPE_GAIN       = 1,  /* Gain/flat-field map (FLOAT32 payload) */
    XCAL_TYPE_DEFECT     = 2,  /* Defect pixel map (UINT8_MASK payload) */
    XCAL_TYPE_GAIN_POLY  = 3,  /* Gain polynomial coefficients (FLOAT32 payload) */
    /*
     * Nonlinearity lookup table (UINT16 payload), SRS-CALIB-FUNC-006-EXT 6a.
     * QA-A-110 (#186). Appended at the end so no existing type renumbers.
     *
     * The payload is `width * height` uint16 entries read as ONE flat table:
     * index = raw ADU, value = linearized ADU. The geometry fields carry the
     * entry count because the validator's payload_len check (check 8) is
     * width * height * bpp, and XCAL_MAX_DIM is 4096 -- so a 65536-entry table
     * cannot be (65536, 1). The two sizes 6a allows are stored as:
     *
     *     4096 entries  -> width 4096, height 1
     *     65536 entries -> width 4096, height 16
     *
     * A reader built before this type rejects such a file with
     * XPE_ERR_CONFIG_INVALID (both the plain and the compressed path compare
     * hdr.type against the highest type they know), which is the intended
     * behaviour: an old binary refuses a file it cannot interpret rather than
     * reading it as something else.
     */
    XCAL_TYPE_NONLIN_LUT = 4
} XCalType;

/** @brief XCal payload pixel format codes. */
typedef enum XCalPixelFormat {
    XCAL_FMT_UINT16    = 0,  /* 16-bit unsigned integer */
    XCAL_FMT_FLOAT32   = 1,  /* 32-bit IEEE 754 float */
    XCAL_FMT_UINT8_MASK = 2  /* 8-bit boolean mask (defect map) */
} XCalPixelFormat;

/** @brief XCal v1 file magic identifier ("XCAL", 4 bytes, no NUL terminator). */
#define XCAL_MAGIC "XCAL"
/** @brief XCal file format version number (= 1). */
#define XCAL_VERSION 1u

/** @brief Maximum allowed image dimension per axis in pixels (prevents runaway allocation). */
#define XCAL_MAX_DIM 4096u

/** @brief Maximum config JSON blob length in bytes (sanity cap: 1 MB). */
#define XCAL_MAX_CONFIG_JSON_LEN (1024u * 1024u)

/** @brief No payload compression (default). */
#define XCAL_COMPRESSION_NONE 0u
/** @brief Run-Length Encoding payload compression (defect maps only). */
#define XCAL_COMPRESSION_RLE  1u

/*
 * Compression metadata convention (stored inside config_json):
 *
 * When payload is compressed, the config_json MUST contain a JSON object
 * with at minimum: {"xcal_compression":1,"xcal_raw_payload_len":NNN}
 *
 * - "xcal_compression": integer, compression method (XCAL_COMPRESSION_RLE = 1)
 * - "xcal_raw_payload_len": integer, original uncompressed payload size in bytes
 *
 * This approach preserves the 152-byte header layout for backward compatibility.
 * Uncompressed files have no compression metadata in config_json.
 *
 * Reading rule (QA-A-209b): the config block is ONE valid JSON object, read to its stored length; the two keys are
 * TOP-LEVEL members and each is a bare non-negative integer. A key of that name inside a nested object is not
 * metadata. A member name given twice at the top level -- any name -- refuses the block. The pair is all or nothing:
 * neither key means "uncompressed"; ONE key alone, a value that is not a bare unsigned integer, or a block that is not
 * a JSON object (white space only included) is XPE_ERR_CONFIG_INVALID. A block of length 0 is a file without a config.
 *
 * Files of the older writer (QA-A-209c, narrowed by QA-A-209d). The writer of 90c1b6b1 and earlier compressed DEFECT
 * maps and nothing else (method 1, RLE), and carried the pair only for those. With a non-empty caller config it cut the
 * caller's last '}' and appended the pair with its own closing brace plus another one, so the stored block ENDS "}}":
 * {"mode":"x","xcal_compression":1,"xcal_raw_payload_len":N}} (shape A: at least one caller member, then the comma, then
 * the pair); for a caller object with no members it made {,"xcal_compression":1,"xcal_raw_payload_len":N}} (shape B; a
 * white-space-only caller object "{ }" kept its white space: "{ ,..."). With NO caller config the block was always a
 * valid single-brace object. Those two shapes -- and only those, and only in a DEFECT file whose method is 1 -- are
 * repaired in memory when the strict parse fails (drop the extra '}', and for shape B the comma), then parsed strictly
 * again, so the duplicate / nested / pair rules apply to the repaired text. The pair alone with a second brace
 * ({"xcal_compression":1,"xcal_raw_payload_len":N}}), the same tails in a file of another type, and a method other than 1
 * are not the old writer's work and stay refused. The file is accepted with an XPE_ALERT_WARNING whose text begins
 * "XPE_WARN_XCAL_LEGACY_CONFIG:" and says to regenerate the file; the text is built before any output argument of the
 * reader is touched, so an allocation failure leaves them all as they were. The stored bytes are not changed, and the
 * SHA-256 covers them (config || payload), so the repair does not weaken tamper detection: a file whose config bytes
 * were altered fails the hash and is refused without the alert. Any other malformed block stays refused.
 */

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
static_assert(sizeof(XCalFileHeader) == 152u,
    "XCalFileHeader must be exactly 152 bytes (pack=1 layout)");
static_assert(offsetof(XCalFileHeader, version)          ==  4u, "version offset");
static_assert(offsetof(XCalFileHeader, type)             ==  8u, "type offset");
static_assert(offsetof(XCalFileHeader, pixel_format)     == 12u, "pixel_format offset");
static_assert(offsetof(XCalFileHeader, width)            == 16u, "width offset");
static_assert(offsetof(XCalFileHeader, height)           == 20u, "height offset");
static_assert(offsetof(XCalFileHeader, created_epoch_ms) == 24u, "created_epoch_ms offset");
static_assert(offsetof(XCalFileHeader, expiry_epoch_ms)  == 32u, "expiry_epoch_ms offset");
static_assert(offsetof(XCalFileHeader, session_id)       == 40u, "session_id offset");
static_assert(offsetof(XCalFileHeader, config_json_len)  == 104u, "config_json_len offset");
static_assert(offsetof(XCalFileHeader, payload_len)      == 112u, "payload_len offset");
static_assert(offsetof(XCalFileHeader, sha256)           == 120u, "sha256 offset");
#endif /* __cplusplus */

#endif /* XPE_XCAL_FORMAT_H */
