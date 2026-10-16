/**
 * @file defect_ext.cpp
 * @brief XCal DEFECT file, format version 2: extension block, checks, reader, writer (QA-A-254, SRS-CALIB-FUNC-039)
 *
 * See defect_ext.hpp for the layout. Nothing here reads a global or touches the calibration store: the loaders decide what to keep.
 */

#include "defect_ext.hpp"
#include "xcal_reader.hpp"
#include "xcal_writer.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace {

constexpr size_t kExtHeader = 32, kLineRecord = 52, kBlobRecord = 20, kSegmentRecord = 12;
constexpr uint32_t kMaxLines = 1u << 16, kMaxCandidates = 1u << 20, kMaxBlobs = 1u << 20, kMaxSegments = 1u << 22;
constexpr uint64_t kMaxExtLen = 256ull << 20;

void put32(std::vector<uint8_t>* o, uint32_t v) {
    for (int i = 0; i < 4; ++i) o->push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
}

void putf(std::vector<uint8_t>* o, float f) {
    uint32_t v = 0;
    std::memcpy(&v, &f, 4);
    put32(o, v);
}

uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

float getf(const uint8_t* p) {
    const uint32_t v = get32(p);
    float f = 0.0f;
    std::memcpy(&f, &v, 4);
    return f;
}

std::string fmt(const char* f, unsigned long long a = 0, unsigned long long b = 0, unsigned long long c = 0, unsigned long long d = 0) {
    // each %llu / %llx takes the next value; written with literal formats so that no format string is passed on as data
    const unsigned long long v[4] = {a, b, c, d};
    std::string              out;
    int                      k = 0;
    for (const char* p = f; *p != 0; ++p) {
        if (p[0] == '%' && p[1] == 'l' && p[2] == 'l' && (p[3] == 'u' || p[3] == 'x') && k < 4) {
            char t[32];
            if (p[3] == 'u') std::snprintf(t, sizeof t, "%llu", v[k]);
            else std::snprintf(t, sizeof t, "%llx", v[k]);
            ++k;
            out += t;
            p += 3;
        } else {
            out += *p;
        }
    }
    return out;
}

/** One line record against the geometry. @p candidate: an unconfirmed candidate (reason != 0, no segments). */
bool check_line(const DefectExtLine& L, uint32_t w, uint32_t h, bool candidate, const char* what, size_t k, std::string* why) {
    auto bad = [&](const std::string& m) {
        *why = std::string(what) + " " + std::to_string(k) + ": " + m;
        return false;
    };
    if (L.orientation > 1u) return bad(fmt("orientation %llu is neither 0 (row) nor 1 (column)", L.orientation));
    const uint64_t span = L.orientation == 0u ? h : w;     // how many lines of this orientation the frame has
    const uint64_t length = L.orientation == 0u ? w : h;   // how long one of them is
    if (L.width < 1u) return bad("width 0");
    if (static_cast<uint64_t>(L.index) + L.width > span)
        return bad(fmt("lines %llu..%llu are outside the frame (it has %llu lines of this orientation)", L.index, static_cast<unsigned long long>(L.index) + L.width - 1, span));
    if (!(L.start < L.end) || L.end > length) return bad(fmt("range %llu..%llu is not inside the line length %llu", L.start, L.end, length));
    if ((L.causes & ~0x0Fu) != 0u || (!candidate && L.causes == 0u)) return bad(fmt("causes 0x%llx are not a set of the known cause bits (and a confirmed line has at least one)", L.causes));
    if ((L.flags & ~0x07u) != 0u) return bad(fmt("flags 0x%llx contain bits that are not defined", L.flags));
    if (candidate ? (L.reason < 1u || L.reason > 15u) : (L.reason != 0u)) return bad(fmt("reason %llu does not fit (a confirmed line has 0, a candidate 1..15)", L.reason));
    if (L.correction > XPE_DEFECT_LINE_CORRECT_REJECT) return bad(fmt("correction method %llu is unknown", L.correction));
    if (!std::isfinite(L.strength) || L.strength < 0.0f) return bad("strength is not a finite non-negative number");
    if (!std::isfinite(L.persistence) || L.persistence < -1.0f || L.persistence > 1.0f) return bad("persistence is not a finite number in -1..1");
    if (candidate) {
        if (!L.segments.empty()) return bad("a candidate has no segments");
        return true;
    }
    if (L.segments.empty()) return bad("a confirmed line has at least one segment");
    for (size_t s = 0; s < L.segments.size(); ++s) {
        const DefectExtSegment& g = L.segments[s];
        if (g.line < L.index || static_cast<uint64_t>(g.line) >= static_cast<uint64_t>(L.index) + L.width)
            return bad(fmt("segment %llu is on line %llu, outside the group %llu..%llu", s, g.line, L.index, static_cast<unsigned long long>(L.index) + L.width - 1));
        if (!(g.start < g.end) || g.end > length || g.start < L.start || g.end > L.end) return bad(fmt("segment %llu range %llu..%llu is empty, outside the line, or outside the record's range", s, g.start, g.end));
        if (s > 0) {
            const DefectExtSegment& p = L.segments[s - 1];
            if (g.line < p.line || (g.line == p.line && g.start < p.end)) return bad(fmt("segment %llu is not after segment %llu (they must be ordered by line and start, and must not overlap)", s, s - 1));
        }
    }
    return true;
}

bool validate_ext(const DefectExt& e, uint32_t w, uint32_t h, std::string* why) {
    if (e.lines.size() > kMaxLines || e.candidates.size() > kMaxCandidates || e.blobs.size() > kMaxBlobs) {
        *why = "more records than the format allows";
        return false;
    }
    uint64_t segs = 0;
    for (size_t k = 0; k < e.lines.size(); ++k) {
        if (!check_line(e.lines[k], w, h, false, "line", k, why)) return false;
        segs += e.lines[k].segments.size();
    }
    for (size_t k = 0; k < e.candidates.size(); ++k)
        if (!check_line(e.candidates[k], w, h, true, "candidate", k, why)) return false;
    if (segs > kMaxSegments) {
        *why = "more segments than the format allows";
        return false;
    }
    for (size_t k = 0; k < e.blobs.size(); ++k) {
        const DefectExtBlob& b = e.blobs[k];
        auto bad = [&](const std::string& m) {
            *why = "blob " + std::to_string(k) + ": " + m;
            return false;
        };
        if (b.x0 > b.x1 || b.y0 > b.y1 || b.x1 >= w || b.y1 >= h) return bad(fmt("box %llu,%llu..%llu,%llu is not inside the frame", b.x0, b.y0, b.x1, b.y1));
        const uint64_t area = static_cast<uint64_t>(b.x1 - b.x0 + 1) * (b.y1 - b.y0 + 1);
        if (b.size < 1u || b.size > area) return bad(fmt("size %llu does not fit a box of %llu pixels", b.size, area));
    }
    return true;
}

void put_line(std::vector<uint8_t>* o, const DefectExtLine& L, uint32_t segFirst) {
    put32(o, L.orientation);
    put32(o, L.index);
    put32(o, L.width);
    put32(o, L.start);
    put32(o, L.end);
    put32(o, L.causes);
    put32(o, L.flags);
    put32(o, L.reason);
    put32(o, L.correction);
    putf(o, L.strength);
    putf(o, L.persistence);
    put32(o, segFirst);
    put32(o, static_cast<uint32_t>(L.segments.size()));
}

} // namespace

void xcal_reject_reason(const char* kind, const std::string& what) noexcept {
    try {
        const std::string msg = std::string("XPE_ERR_XCAL_") + kind + ": " + what;
        xpe_alert_push(msg.c_str(), XPE_ALERT_ERROR);
    } catch (...) {
        // [no-throw-boundary] advisory: lost under memory pressure
    }
}

bool defect_ext_encode(const DefectExt& ext, uint32_t width, uint32_t height, std::vector<uint8_t>* out, std::string* reason) {
    std::string why;
    if (!validate_ext(ext, width, height, &why)) {
        if (reason) *reason = why;
        return false;
    }
    std::vector<uint8_t> o;
    uint64_t segs = 0;
    for (const DefectExtLine& L : ext.lines) segs += L.segments.size();
    o.reserve(kExtHeader + kLineRecord * (ext.lines.size() + ext.candidates.size()) + kBlobRecord * ext.blobs.size() + kSegmentRecord * segs);
    o.push_back('X'); o.push_back('D'); o.push_back('E'); o.push_back('X');
    put32(&o, XPE_DEFECT_EXT_FORMAT);
    put32(&o, static_cast<uint32_t>(ext.lines.size()));
    put32(&o, static_cast<uint32_t>(ext.blobs.size()));
    put32(&o, static_cast<uint32_t>(ext.candidates.size()));
    put32(&o, static_cast<uint32_t>(segs));
    put32(&o, 0);
    put32(&o, 0);
    uint32_t next = 0;
    for (const DefectExtLine& L : ext.lines) {
        put_line(&o, L, next);
        next += static_cast<uint32_t>(L.segments.size());
    }
    for (const DefectExtLine& L : ext.candidates) put_line(&o, L, 0);
    for (const DefectExtBlob& b : ext.blobs) {
        put32(&o, b.x0); put32(&o, b.y0); put32(&o, b.x1); put32(&o, b.y1); put32(&o, b.size);
    }
    for (const DefectExtLine& L : ext.lines)
        for (const DefectExtSegment& g : L.segments) {
            put32(&o, g.line); put32(&o, g.start); put32(&o, g.end);
        }
    *out = std::move(o);
    return true;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): width before height, as everywhere in the format; one call site
bool defect_ext_decode(const uint8_t* data, size_t size, uint32_t width, uint32_t height, DefectExt* out, std::string* reason) {
// NOLINTEND(bugprone-easily-swappable-parameters)
    auto fail = [&](const std::string& m) {
        if (reason) *reason = m;
        return false;
    };
    if (size < kExtHeader) return fail(fmt("extension block is %llu bytes, shorter than its 32-byte header (truncated)", size));
    if (std::memcmp(data, "XDEX", 4) != 0) return fail("extension block does not start with XDEX");
    if (get32(data + 4) != XPE_DEFECT_EXT_FORMAT) return fail(fmt("extension block format %llu is not supported (this reader reads format 1)", get32(data + 4)));
    const uint32_t nl = get32(data + 8), nb = get32(data + 12), nc = get32(data + 16), ns = get32(data + 20);
    if (get32(data + 24) != 0u || get32(data + 28) != 0u) return fail("reserved fields of the extension header are not zero");
    if (nl > kMaxLines || nc > kMaxCandidates || nb > kMaxBlobs || ns > kMaxSegments) return fail(fmt("record counts (%llu lines, %llu blobs, %llu candidates, %llu segments) exceed the format limits", nl, nb, nc, ns));
    const uint64_t want = kExtHeader + kLineRecord * (static_cast<uint64_t>(nl) + nc) + kBlobRecord * static_cast<uint64_t>(nb) + kSegmentRecord * static_cast<uint64_t>(ns);
    if (want != size) return fail(fmt("extension block is %llu bytes but its header declares %llu (truncated or padded)", size, want));
    DefectExt e;
    const uint8_t* p = data + kExtHeader;
    uint32_t expectFirst = 0;
    auto read_line = [&](DefectExtLine* L, bool candidate, size_t k) {
        L->orientation = get32(p); L->index = get32(p + 4); L->width = get32(p + 8); L->start = get32(p + 12); L->end = get32(p + 16);
        L->causes = get32(p + 20); L->flags = get32(p + 24); L->reason = get32(p + 28); L->correction = get32(p + 32);
        L->strength = getf(p + 36); L->persistence = getf(p + 40);
        const uint32_t first = get32(p + 44), count = get32(p + 48);
        p += kLineRecord;
        if (candidate) {
            if (count != 0u) return fail(fmt("candidate %llu declares segments", k));
            return true;
        }
        if (first != expectFirst || static_cast<uint64_t>(first) + count > ns)
            return fail(fmt("line %llu: its segments %llu..+%llu are not the next ones of the table (it has %llu)", k, first, count, ns));
        expectFirst += count;
        L->segments.resize(count);   // filled below, once the table has been reached
        return true;
    };
    e.lines.resize(nl);
    for (size_t k = 0; k < nl; ++k)
        if (!read_line(&e.lines[k], false, k)) return false;
    if (expectFirst != ns) return fail(fmt("segment table has %llu entries but the lines use %llu", ns, expectFirst));
    e.candidates.resize(nc);
    for (size_t k = 0; k < nc; ++k)
        if (!read_line(&e.candidates[k], true, k)) return false;
    e.blobs.resize(nb);
    for (DefectExtBlob& b : e.blobs) {
        b.x0 = get32(p); b.y0 = get32(p + 4); b.x1 = get32(p + 8); b.y1 = get32(p + 12); b.size = get32(p + 16);
        p += kBlobRecord;
    }
    for (DefectExtLine& L : e.lines)
        for (DefectExtSegment& g : L.segments) {
            g.line = get32(p); g.start = get32(p + 4); g.end = get32(p + 8);
            p += kSegmentRecord;
        }
    std::string why;
    if (!validate_ext(e, width, height, &why)) return fail(why);
    *out = std::move(e);
    return true;
}

XpeErrorCode defect_v2_check_layout(const XCalFileHeader& hdr, const XpeConfigDoc& config, bool compressed, uint64_t raw_payload_len) {
    const XpeConfigEntry* e = config.find("defect_ext_len");
    unsigned long long extLen = 0;
    bool ok = e != nullptr && e->scalar && !e->quoted && !e->text.empty() && e->text.size() <= 12;
    if (ok)
        for (const char ch : e->text) {
            if (ch < '0' || ch > '9') { ok = false; break; }
            extLen = extLen * 10 + static_cast<unsigned>(ch - '0');
        }
    if (!ok) {
        xcal_reject_reason("LAYOUT", "a version-2 defect file must give defect_ext_len (a bare unsigned integer) in its config block");
        return XPE_ERR_CONFIG_INVALID;
    }
    if (extLen > kMaxExtLen) {
        xcal_reject_reason("LAYOUT", fmt("defect_ext_len %llu exceeds the 256 MiB the format allows", extLen));
        return XPE_ERR_CONFIG_INVALID;
    }
    const unsigned long long planes = 2ull * hdr.width * hdr.height;
    const unsigned long long actual = compressed ? raw_payload_len : hdr.payload_len;
    if (actual != planes + extLen) {
        xcal_reject_reason("LAYOUT", fmt("the payload is %llu bytes but 2 x width x height (%llu) + defect_ext_len (%llu) = %llu", actual, planes, extLen, planes + extLen));
        return XPE_ERR_CONFIG_INVALID;
    }
    return XPE_OK;
}

XpeErrorCode defect_v2_split(const XCalFileHeader& hdr, const std::vector<uint8_t>& payload, DefectXcal* out) {
    try {
        const size_t px = static_cast<size_t>(hdr.width) * hdr.height;
        if (payload.size() < 2 * px) {
            xcal_reject_reason("LAYOUT", fmt("the payload is %llu bytes, less than the two planes of %llu bytes", payload.size(), 2 * px));
            return XPE_ERR_CONFIG_INVALID;
        }
        const uint8_t* area = payload.data() + px;
        for (size_t i = 0; i < px; ++i)
            if (area[i] > 1u) {
                xcal_reject_reason("AREA", fmt("the imaging-area plane has the value %llu at pixel %llu (it holds 0 outside and 1 inside only)", area[i], i));
                return XPE_ERR_CONFIG_INVALID;
            }
        DefectExt ext;
        const size_t extLen = payload.size() - 2 * px;
        bool hasExt = false;
        if (extLen > 0) {
            std::string why;
            if (!defect_ext_decode(payload.data() + 2 * px, extLen, hdr.width, hdr.height, &ext, &why)) {
                xcal_reject_reason("EXTENSION", why);
                return XPE_ERR_CONFIG_INVALID;
            }
            hasExt = true;
        }
        out->map.assign(payload.begin(), payload.begin() + static_cast<std::ptrdiff_t>(px));
        out->area.assign(area, area + px);
        out->has_area = true;
        out->ext = std::move(ext);
        out->has_ext = hasExt;
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

bool xcal_file_length_ok(const char* path) {
    try {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f.is_open()) return true;
        const unsigned long long size = static_cast<unsigned long long>(f.tellg());
        if (size < sizeof(XCalFileHeader)) {
            xcal_reject_reason("TRUNCATED", fmt("the file is %llu bytes, shorter than the %llu-byte header", size, sizeof(XCalFileHeader)));
            return false;
        }
        XCalFileHeader hdr;
        f.seekg(0);
        f.read(reinterpret_cast<char*>(&hdr), sizeof hdr);
        if (!f) return true;
        const unsigned long long declared = sizeof(XCalFileHeader) + hdr.config_json_len + hdr.payload_len;
        if (std::memcmp(hdr.magic, XCAL_MAGIC, 4) == 0 && size < declared) {
            xcal_reject_reason("TRUNCATED", fmt("the file is %llu bytes but its header declares %llu (config %llu + payload %llu + the header): it ends early", size, declared, hdr.config_json_len, hdr.payload_len));
            return false;
        }
        return true;
    } catch (...) {
        return true;
    }
}

XpeErrorCode read_defect_xcal(const char* path, bool check_expiry, DefectXcal* out) {
    try {
        if (path == nullptr || out == nullptr) return XPE_ERR_INVALID_INPUT;
        if (!xcal_file_length_ok(path)) return XPE_ERR_IO_FAILED;
        XCalFileHeader hdr;
        std::vector<uint8_t> config, payload;
        const XpeErrorCode rc = read_xcal_file(path, hdr, config, payload, check_expiry, XCAL_TYPE_DEFECT, nullptr, nullptr);
        if (rc != XPE_OK) return rc;
        DefectXcal d;
        d.format_version = hdr.version;
        d.width = hdr.width;
        d.height = hdr.height;
        d.created_epoch_ms = hdr.created_epoch_ms;
        d.expiry_epoch_ms = hdr.expiry_epoch_ms;
        d.session_id.assign(hdr.session_id, strnlen(hdr.session_id, sizeof hdr.session_id));
        d.config_json.assign(config.begin(), config.end());
        const size_t px = static_cast<size_t>(hdr.width) * hdr.height;
        if (hdr.version == XCAL_VERSION) {
            if (payload.size() != px) {
                xcal_reject_reason("LAYOUT", fmt("the payload is %llu bytes, not width x height = %llu", payload.size(), px));
                return XPE_ERR_CONFIG_INVALID;
            }
            d.map = std::move(payload);
        } else {
            const XpeErrorCode src = defect_v2_split(hdr, payload, &d);
            if (src != XPE_OK) return src;
        }
        *out = std::move(d);
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XpeErrorCode write_defect_xcal(const char* path, const DefectXcalWrite& in) {
    try {
        if (path == nullptr || in.map == nullptr || in.width == 0 || in.height == 0 || in.width > XCAL_MAX_DIM || in.height > XCAL_MAX_DIM) return XPE_ERR_INVALID_INPUT;
        const size_t px = static_cast<size_t>(in.width) * in.height;
        std::vector<uint8_t> payload;
        std::vector<uint8_t> extBytes;
        DefectExt none;
        const DefectExt& ext = in.ext ? *in.ext : none;
        {
            std::string why;
            if (!defect_ext_encode(ext, in.width, in.height, &extBytes, &why)) {
                xcal_reject_reason("EXTENSION", why);
                return XPE_ERR_INVALID_INPUT;
            }
        }
        const bool emptyExt = ext.lines.empty() && ext.candidates.empty() && ext.blobs.empty();
        if (emptyExt) extBytes.clear();   // no records: no block at all
        payload.reserve(2 * px + extBytes.size());
        payload.insert(payload.end(), in.map, in.map + px);
        if (in.area != nullptr) {
            for (size_t i = 0; i < px; ++i)
                if (in.area[i] > 1u) {
                    xcal_reject_reason("AREA", fmt("the imaging-area plane has the value %llu at pixel %llu (it holds 0 outside and 1 inside only)", in.area[i], i));
                    return XPE_ERR_INVALID_INPUT;
                }
            payload.insert(payload.end(), in.area, in.area + px);
        } else {
            payload.insert(payload.end(), px, 1u);
        }
        payload.insert(payload.end(), extBytes.begin(), extBytes.end());
        std::string cfg = "{\"defect_format\":2,\"defect_ext_len\":" + std::to_string(extBytes.size());
        if (!in.extra_config_members.empty()) cfg += "," + in.extra_config_members;
        cfg += "}";
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION_DEFECT_EXT;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = in.width;
        hdr.height = in.height;
        hdr.created_epoch_ms = in.created_epoch_ms;
        hdr.expiry_epoch_ms = in.expiry_epoch_ms;
        std::memcpy(hdr.session_id, in.session_id.data(), std::min<size_t>(in.session_id.size(), sizeof hdr.session_id - 1));
        return write_xcal_file_ex(path, hdr, reinterpret_cast<const uint8_t*>(cfg.data()), cfg.size(), payload.data(), payload.size(), in.compress);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
