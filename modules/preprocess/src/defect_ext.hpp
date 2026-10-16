/**
 * @file defect_ext.hpp
 * @brief XCal DEFECT file, format version 2 (QA-A-254, SRS-CALIB-FUNC-039): the extension block, its checks, the reader and the writer
 *
 * A version-2 DEFECT file keeps the version-1 meaning of the defect plane (FUNC-003 pixel codes: 0 good, anything else defective) and adds, in ONE payload:
 *
 *     plane 0   defect map          width x height bytes   the pixel codes, exactly the version-1 payload
 *     plane 1   imaging area        width x height bytes   1 inside the imaging area, 0 outside (shielded or dummy rows and columns). A SEPARATE layer: it is not a defect code.
 *     extension block               ext_len bytes          line records, blob marks, the unconfirmed candidates (diagnostic appendix)
 *
 * so payload_len = 2 * width * height + ext_len, and the config block carries `"defect_ext_len": <ext_len>` (and the usual RLE pair when the payload is compressed; the raw length then
 * is the same sum). The header's version field is 2 and only a DEFECT file may carry it; every other type stays at 1. A reader built before version 2 refuses such a file at the
 * version check (XPE_ERR_CONFIG_INVALID) instead of reading plane 1 as something else; version-1 files are read as before by this reader.
 *
 * Extension block (little endian):
 *     header, 32 bytes:   "XDEX", ext_format u32 (= 1), n_lines u32, n_blobs u32, n_candidates u32, n_segments u32, reserved u32 (= 0), reserved u32 (= 0)
 *     n_lines      x 52 bytes (the confirmed line groups, in file order)
 *     n_candidates x 52 bytes (the same record, reason != 0, no segments: DIAGNOSTIC APPENDIX, NOT part of the defect layer)
 *     n_blobs      x 20 bytes (x0 y0 x1 y1 size: u32 each; bounding box with x1 and y1 inclusive)
 *     n_segments   x 12 bytes (line start end: u32 each; end exclusive)
 *   line record (13 x u32/f32): orientation, index, width, start, end, causes, flags, reason, correction, strength (f32), persistence (f32), segment_first, segment_count
 *   `start`/`end` of a record are the bounding range of its member stretches (metadata); the INDIVIDUAL stretches are the segments (their union is what the line mask paints).
 *   The segment table is consumed in order: the segments of line k begin where those of line k - 1 end.
 *
 * Every check a reader makes names its reason in the text of one XPE_ALERT_ERROR ("XPE_ERR_XCAL_<KIND>: ...") together with the error code, so a refused file says why.
 */

#ifndef XPE_DEFECT_EXT_HPP
#define XPE_DEFECT_EXT_HPP

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct XpeConfigDoc;   // xpe_preprocess_internal.h

#define XCAL_VERSION_DEFECT_EXT 2u   /* the header version of a DEFECT file with the extension */
#define XPE_DEFECT_EXT_FORMAT 1u     /* the version of the extension block inside it */

/** How a line is to be corrected (the field `correction` of a line record). */
#define XPE_DEFECT_LINE_CORRECT_UNSPECIFIED 0u
#define XPE_DEFECT_LINE_CORRECT_INTERPOLATE 1u /* from the adjacent lines (a line of one or two lines' width) */
#define XPE_DEFECT_LINE_CORRECT_REJECT      2u /* three or more adjacent lines: not a correction task but a panel rejection condition */

struct DefectExtSegment {
    uint32_t line{0};    ///< row (or column) number
    uint32_t start{0};   ///< first pixel along the line
    uint32_t end{0};     ///< one past the last pixel
};

struct DefectExtLine {
    uint32_t orientation{0};   ///< 0 row, 1 column (XPE_BPM_LINE_ROW / XPE_BPM_LINE_COLUMN)
    uint32_t index{0};         ///< row (or column) number of the first line of the group
    uint32_t width{1};         ///< adjacent lines in the group
    uint32_t start{0};         ///< bounding range along the line (metadata)
    uint32_t end{0};
    uint32_t causes{0};        ///< XPE_BPM_LINE_CAUSE_* bits
    uint32_t flags{0};         ///< XPE_BPM_LINE_* flag bits
    uint32_t reason{0};        ///< 0 for a confirmed group, XPE_BPM_LINE_REASON_* for a candidate
    uint32_t correction{0};    ///< XPE_DEFECT_LINE_CORRECT_*
    float    strength{0.0f};
    float    persistence{-1.0f};
    std::vector<DefectExtSegment> segments;   ///< the individual stretches (empty for a candidate)
};

struct DefectExtBlob {
    uint32_t x0{0}, y0{0}, x1{0}, y1{0};   ///< bounding box, x1 and y1 inclusive
    uint32_t size{0};                       ///< pixels
};

struct DefectExt {
    std::vector<DefectExtLine> lines;        ///< confirmed groups: part of the defect layer
    std::vector<DefectExtLine> candidates;   ///< unconfirmed candidates: diagnostic appendix, NOT in the defect layer
    std::vector<DefectExtBlob> blobs;        ///< blob marks of the defect plane
};

/** What a defect file holds, whatever its version. */
struct DefectXcal {
    uint32_t             format_version{1};
    uint32_t             width{0}, height{0};
    int64_t              created_epoch_ms{0}, expiry_epoch_ms{0};
    std::string          session_id;
    std::vector<uint8_t> map;            ///< plane 0
    std::vector<uint8_t> area;           ///< plane 1 (empty for a version-1 file: every pixel is inside)
    bool                 has_area{false};
    DefectExt            ext;
    bool                 has_ext{false};
    std::string          config_json;    ///< the config block as stored
};

/**
 * What the correction needs of a version-2 file, immutable and shared (QA-A-255): the imaging area (empty = every pixel is inside), the confirmed lines with their individual stretches and the
 * blob marks. The runtime store, the frame's snapshot and the cache entry hold the same object; a version-1 file has none.
 */
struct DefectLayers {
    uint32_t                   width{0}, height{0};
    std::vector<uint8_t>       area;      ///< width x height bytes, 1 inside the imaging area; empty when the file has no area layer
    size_t                     outside{0};   ///< pixels with area == 0
    std::vector<DefectExtLine> lines;
    std::vector<DefectExtBlob> blobs;
};

/** The layers of a file read with read_defect_xcal / defect_v2_split; null for a version-1 file. */
XPE_API std::shared_ptr<const DefectLayers> make_defect_layers(const DefectXcal& parts);

/** What a writer is given. `map` and `area` are width x height bytes; `ext` may be null (then the extension block is empty). */
struct DefectXcalWrite {
    uint32_t         width{0}, height{0};
    const uint8_t*   map{nullptr};
    const uint8_t*   area{nullptr};
    const DefectExt* ext{nullptr};
    int64_t          created_epoch_ms{0}, expiry_epoch_ms{0};
    std::string      session_id;
    std::string      extra_config_members;   ///< members to add to the config block, e.g. "\"method\":\"v2\"" (no braces); may be empty
    bool             compress{true};
};

/** Encode an extension block against the geometry; false with the reason when a record is outside it (nothing is written to @p out then). */
XPE_API bool defect_ext_encode(const DefectExt& ext, uint32_t width, uint32_t height, std::vector<uint8_t>* out, std::string* reason);

/** Decode and check an extension block: size, counts, every record inside the geometry, the segment table. false with the reason on any fault. */
XPE_API bool defect_ext_decode(const uint8_t* data, size_t size, uint32_t width, uint32_t height, DefectExt* out, std::string* reason);

/**
 * Push the one alert that tells why a file was refused: "XPE_ERR_XCAL_<kind>: <what>" at XPE_ALERT_ERROR. `kind` is one of VERSION, TRUNCATED, LAYOUT, AREA, EXTENSION.
 * Never throws.
 */
XPE_API void xcal_reject_reason(const char* kind, const std::string& what) noexcept;

/**
 * The layout rule of a version-2 DEFECT file, from the header and the config block: the config must give "defect_ext_len" as a bare unsigned integer, and the raw payload length
 * (the header's payload_len when uncompressed, xcal_raw_payload_len when compressed) must be 2 * width * height + that. XPE_OK, or XPE_ERR_CONFIG_INVALID with the reason pushed.
 */
XPE_API XpeErrorCode defect_v2_check_layout(const XCalFileHeader& hdr, const XpeConfigDoc& config, bool compressed, uint64_t raw_payload_len);

/**
 * Splits a decoded version-2 payload (plane 0, plane 1, extension block) and checks it: plane 1 holds 0 and 1 only, the extension block decodes inside the geometry. XPE_OK or
 * XPE_ERR_CONFIG_INVALID with the reason pushed.
 */
XPE_API XpeErrorCode defect_v2_split(const XCalFileHeader& hdr, const std::vector<uint8_t>& payload, DefectXcal* out);

/**
 * Pre-check of the length of a calibration file against its own header: a file shorter than the 152-byte header, or than 152 + config_json_len + payload_len, is TRUNCATED. Pushes the reason
 * ("XPE_ERR_XCAL_TRUNCATED: ... N bytes declared, M found") and returns false; true when the file is long enough or cannot be opened/judged (the reader then reports that itself).
 */
XPE_API bool xcal_file_length_ok(const char* path);

/** Reads a defect file of version 1 or 2 completely (map, area, extension). Version 1: area empty, has_area false, has_ext false. The errors of read_xcal_file plus those of the checks above. */
XPE_API XpeErrorCode read_defect_xcal(const char* path, bool check_expiry, DefectXcal* out);

/** Writes a version-2 defect file (atomic, compressed when smaller). XPE_ERR_INVALID_INPUT with the reason pushed when a record is outside the geometry or the area plane has a value other than 0 and 1. */
XPE_API XpeErrorCode write_defect_xcal(const char* path, const DefectXcalWrite& in);

#endif /* XPE_DEFECT_EXT_HPP */
