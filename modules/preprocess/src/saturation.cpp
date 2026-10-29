/**
 * @file saturation.cpp
 * @brief Detector saturation level (QA-A-296, Refs #278). See saturation.hpp.
 */

#include "saturation.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "defect_ext.hpp"
#include <mutex>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <memory>

// NOLINTBEGIN(bugprone-easily-swappable-parameters): width and height are the frame size and rowStep the row stride of the sample (1 = every row); the call sites pass (img->width, img->height, 16u) and (width, height, 1u)
void xpe_sat_histogram(const uint16_t* raw, uint32_t width, uint32_t height, uint32_t rowStep,
                       const uint8_t* exclude, uint32_t* hist, uint64_t* counted) noexcept
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (raw == nullptr || hist == nullptr || width == 0 || height == 0) return;
    if (rowStep == 0) rowStep = 1;
    uint64_t n = 0;
    for (uint32_t y = 0; y < height; y += rowStep) {
        const uint16_t* row = raw + static_cast<size_t>(y) * width;
        if (exclude == nullptr) {
            for (uint32_t x = 0; x < width; ++x) ++hist[row[x]];
            n += width;
        } else {
            const uint8_t* ex = exclude + static_cast<size_t>(y) * width;
            for (uint32_t x = 0; x < width; ++x) {
                if (ex[x] != 0) continue;
                ++hist[row[x]];
                ++n;
            }
        }
    }
    if (counted) *counted += n;
}

bool xpe_sat_find_pile(const uint32_t* hist, uint64_t total, const XpeSatParams& p,
                       uint32_t* level, uint64_t* count) noexcept
{
    if (hist == nullptr || total == 0) return false;
    const double tot = static_cast<double>(total);
    uint64_t above = 0;   // pixels strictly above v
    for (int64_t v = static_cast<int64_t>(XPE_SAT_BINS) - 1; v >= 65; --v) {
        const uint32_t c = hist[v];
        if (c == 0) continue;
        if (static_cast<double>(above) > p.maxAboveFrac * tot) return false;   // a pile-up sits at the top
        if (static_cast<double>(c) < p.minCountFrac * tot) { above += c; continue; }   // too small to be a pile: no need for the median
        // median of the 64 bins below v (mean of the two middle values, as numpy.median does)
        uint32_t below[64];
        for (int k = 0; k < 64; ++k) below[k] = hist[v - 64 + k];
        std::sort(below, below + 64);
        const double base = 0.5 * (static_cast<double>(below[31]) + static_cast<double>(below[32]));
        if (static_cast<double>(c) >= p.minCountFrac * tot &&
            static_cast<double>(c) >= p.minRatio * std::max(base, 1.0)) {
            if (level) *level = static_cast<uint32_t>(v);
            if (count) *count = c;
            return true;
        }
        above += c;
    }
    return false;
}

float xpe_sat_output_value(uint32_t level, float medianGain) noexcept
{
    if (level == 0 || !(medianGain > 0.0f) || !std::isfinite(medianGain)) return 0.0f;
    return static_cast<float>(level) / medianGain;
}

float xpe_sat_gain_median(const float* gain, size_t n, const uint32_t* excluded, size_t excludedCount)
{
    if (gain == nullptr || n == 0) return 0.0f;
    // the population: every value of the plane except those at the (ascending) excluded indices
    std::unique_ptr<float[]> copy(new float[n]);
    size_t m = 0, e = 0;
    for (size_t i = 0; i < n; ++i) {
        while (e < excludedCount && excluded[e] < i) ++e;
        if (e < excludedCount && excluded[e] == i) continue;
        copy[m++] = gain[i];
    }
    if (m == 0) return 0.0f;
    std::nth_element(copy.get(), copy.get() + m / 2, copy.get() + m);
    return copy[m / 2];
}

namespace {
bool isIntegerInRange(const XpeConfigDoc& doc, const char* key, double lo, double hi, double* out)
{
    double v = 0.0;
    if (!doc.getNumber(key, &v)) return false;
    if (!(v >= lo && v <= hi) || v != std::floor(v)) return false;
    *out = v;
    return true;
}

// UTC ISO 8601, "YYYY-MM-DDThh:mm:ssZ" (20 characters)
bool isUtcStamp(const std::string& t)
{
    if (t.size() != 20) return false;
    for (size_t i = 0; i < t.size(); ++i) {
        const char c = t[i];
        const bool digit = (c >= '0' && c <= '9');
        if (i == 4 || i == 7) { if (c != '-') return false; }
        else if (i == 10) { if (c != 'T') return false; }
        else if (i == 13 || i == 16) { if (c != ':') return false; }
        else if (i == 19) { if (c != 'Z') return false; }
        else if (!digit) return false;
    }
    return true;
}
}  // namespace

bool xpe_sat_record_from_config(const XpeConfigDoc& doc, XpeSatRecord* out) noexcept
{
    try {
        XpeSatRecord rec;
        bool any = false;
        for (const XpeConfigEntry& e : doc.entries) {
            if (e.key.rfind("saturation_", 0) != 0) continue;
            any = true;
            static const char* const kKnown[] = {"saturation_status", "saturation_level", "saturation_method", "saturation_min_count_frac",
                                                 "saturation_min_ratio", "saturation_max_above_frac", "saturation_frames_used",
                                                 "saturation_frames_total", "saturation_decided_at"};
            bool known = false;
            for (const char* k : kKnown) known = known || (e.key == k);
            if (!known) return false;   // an unknown saturation_* key
        }
        if (!any) { *out = rec; return true; }   // a file written before QA-A-296: no record

        std::string status, method, when;
        bool quoted = false;
        if (!doc.getString("saturation_status", &status, &quoted) || !quoted) return false;
        if (status != "determined" && status != "undetermined") return false;
        if (!doc.getString("saturation_method", &method, &quoted) || !quoted || method.empty()) return false;
        if (!doc.getString("saturation_decided_at", &when, &quoted) || !quoted || !isUtcStamp(when)) return false;
        double v = 0.0;
        if (!doc.getNumber("saturation_min_count_frac", &v) || !(v > 0.0 && v <= 1.0)) return false;
        if (!doc.getNumber("saturation_min_ratio", &v) || !(v >= 1.0) || !std::isfinite(v)) return false;
        if (!doc.getNumber("saturation_max_above_frac", &v) || !(v >= 0.0 && v <= 1.0)) return false;
        double total = 0.0, used = 0.0;
        if (!isIntegerInRange(doc, "saturation_frames_total", 1.0, 1e9, &total)) return false;
        if (!isIntegerInRange(doc, "saturation_frames_used", 0.0, 1e9, &used) || used > total) return false;
        const bool hasLevel = doc.find("saturation_level") != nullptr;
        if (status == "undetermined") {
            if (hasLevel) return false;   // state contradiction
            rec.known = true;
            *out = rec;
            return true;
        }
        double lv = 0.0;
        if (!hasLevel || !isIntegerInRange(doc, "saturation_level", 1.0, 65535.0, &lv) || used < 1.0) return false;
        rec.known = true;
        rec.determined = true;
        rec.level = static_cast<uint32_t>(lv);
        *out = rec;
        return true;
    } catch (...) {
        return false;
    }
}

void xpe_sat_record_append_json(std::string* json, bool determined, uint32_t level, const XpeSatParams& p,
                                uint32_t framesUsed, uint32_t framesTotal)
{
    char buf[512];
    char when[32] = "unknown";
    const std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    if (gmtime_s(&tmv, &t) == 0) std::strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", &tmv);
#else
    if (gmtime_r(&t, &tmv) != nullptr) std::strftime(when, sizeof(when), "%Y-%m-%dT%H:%M:%SZ", &tmv);
#endif
    int len;
    if (determined) {
        len = std::snprintf(buf, sizeof(buf),
            ",\"saturation_status\":\"determined\",\"saturation_level\":%u,"
            "\"saturation_method\":\"histogram_top_pile/1\","
            "\"saturation_min_count_frac\":%.9g,\"saturation_min_ratio\":%.9g,\"saturation_max_above_frac\":%.9g,"
            "\"saturation_frames_used\":%u,\"saturation_frames_total\":%u,\"saturation_decided_at\":\"%s\"",
            static_cast<unsigned>(level), p.minCountFrac, p.minRatio, p.maxAboveFrac,
            static_cast<unsigned>(framesUsed), static_cast<unsigned>(framesTotal), when);
    } else {
        len = std::snprintf(buf, sizeof(buf),
            ",\"saturation_status\":\"undetermined\","
            "\"saturation_method\":\"histogram_top_pile/1\","
            "\"saturation_min_count_frac\":%.9g,\"saturation_min_ratio\":%.9g,\"saturation_max_above_frac\":%.9g,"
            "\"saturation_frames_used\":%u,\"saturation_frames_total\":%u,\"saturation_decided_at\":\"%s\"",
            p.minCountFrac, p.minRatio, p.maxAboveFrac,
            static_cast<unsigned>(framesUsed), static_cast<unsigned>(framesTotal), when);
    }
    if (len > 0) json->append(buf, static_cast<size_t>(std::min<int>(len, static_cast<int>(sizeof(buf)) - 1)));
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters): n is the pixel count of the range, level the raw saturation value and sOut the output value of a saturated pixel; the one call site passes (e - b, level, sOut)
void xpe_sat_apply(float* out, const uint16_t* raw, size_t n, uint32_t level, float sOut) noexcept
// NOLINTEND(bugprone-easily-swappable-parameters)
{
    const uint16_t lv = static_cast<uint16_t>(level);
    for (size_t i = 0; i < n; ++i) {
        const float v = out[i];
        out[i] = (raw[i] >= lv) ? sOut : (v > sOut ? sOut : v);
    }
}

namespace {
struct MedianCache {
    std::mutex m;
    std::weak_ptr<float[]> gain;
    std::weak_ptr<uint8_t[]> defect;
    std::weak_ptr<const DefectLayers> layers;
    bool hadDefect{false};
    bool hadLayers{false};
    bool valid{false};
    float median{0.0f};
};
MedianCache g_medianCache;

bool sameMaps(const MedianCache& c, const CalibSnapshot& s)
{
    if (!c.valid) return false;
    if (c.gain.lock().get() != s.gain_map.get() || s.gain_map.get() == nullptr) return false;
    if (c.hadDefect != (s.defect_map != nullptr) || c.hadLayers != (s.defect_layers != nullptr)) return false;
    if (c.hadDefect && c.defect.lock().get() != s.defect_map.get()) return false;
    if (c.hadLayers && c.layers.lock().get() != s.defect_layers.get()) return false;
    return true;
}
}  // namespace

float xpe_sat_median_for(const CalibSnapshot& calib)
{
    if (!calib.gain_map) return 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_medianCache.m);
        if (sameMaps(g_medianCache, calib)) return g_medianCache.median;
    }
    const size_t n = static_cast<size_t>(calib.gain_width) * calib.gain_height;
    const uint8_t* bpm = (calib.defect_map && calib.defect_width == calib.gain_width && calib.defect_height == calib.gain_height)
                             ? calib.defect_map.get() : nullptr;
    const DefectLayers* layers = (calib.defect_layers && calib.defect_layers->width == calib.gain_width &&
                                  calib.defect_layers->height == calib.gain_height && !calib.defect_layers->area.empty())
                                     ? calib.defect_layers.get() : nullptr;
    const float* g = calib.gain_map.get();
    const uint32_t* inv = calib.gain_defect_idx.get();
    const size_t invCount = inv ? calib.gain_defect_count : 0;
    std::unique_ptr<float[]> pop(new float[n]);
    size_t m = 0, e = 0;
    for (size_t i = 0; i < n; ++i) {
        while (e < invCount && inv[e] < i) ++e;
        if (e < invCount && inv[e] == i) continue;          // classified defective by the gain calibration
        if (bpm && bpm[i] != 0) continue;                   // in the defect map
        if (layers && layers->area[i] == 0) continue;       // outside the imaging area
        pop[m++] = g[i];
    }
    float median = 0.0f;
    if (m > 0) {
        std::nth_element(pop.get(), pop.get() + m / 2, pop.get() + m);
        median = pop[m / 2];
    }
    std::lock_guard<std::mutex> lock(g_medianCache.m);
    g_medianCache.gain = calib.gain_map;
    g_medianCache.defect = calib.defect_map;
    g_medianCache.layers = calib.defect_layers;
    g_medianCache.hadDefect = (calib.defect_map != nullptr);
    g_medianCache.hadLayers = (calib.defect_layers != nullptr);
    g_medianCache.median = median;
    g_medianCache.valid = true;
    return median;
}
