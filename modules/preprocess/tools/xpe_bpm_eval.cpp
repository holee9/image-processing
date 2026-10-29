/**
 * @file xpe_bpm_eval.cpp
 * @brief Defect-map evaluation tool (QA-A-251, #273): plants defects whose positions are known, scores a defect map against them.
 *
 * There is no ground-truth defect map (AAPM TG-150 4.3.8), so a map is judged by how well it finds defects that were planted. The method (injection spec,
 * scoring definitions) is fixed in .moai/reports/lane-pre/QA-A-251/evidence/30_method_fixed_before_measurement.md and was committed before this tool.
 * The tool knows nothing about how a map was made: it takes a map (defect code per pixel, 0 = good) and judges it.
 *
 *   xpe_bpm_eval inject --out DIR --dark F... --bright F... [--eval F...] [--exclude-mask F]... [--seed 251] [--width 3072] [--height 3072]
 *                       [--pixels-per-level 64] [--blocks-per-level 16] [--margin 32] [--line-gap 24]
 *       Plants the defects of the spec in every given frame (the same places in all of them) and writes DIR/frames/<name> (the planted frames),
 *       DIR/truth.tsv (what the scorer reads), DIR/truth.json (spec, options, input hashes) and DIR/truth_ids.u16 (item number per pixel, 65535 = shared).
 *       --dark: generation dark frames (the pixel dark level D is their mean). --bright: bright generation frames. --eval: bright evaluation frames.
 *       --exclude-mask: a map (raw uint8/uint16, or a defect.xcal) whose defects, and 3 pixels around them, stay free of planted defects.
 *       Response items (QA-A-252d, SRS FUNC-022): a response r below 0.45 is a defect, 0.45..1.5 is normal (class control; 0.45 itself is normal). Two models:
 *       gain_loss (signal and noise both x r, like dead and slope) and efficiency_loss (r times the detected quanta: the signal x r and the quantum noise variance x r),
 *       levels 0.3, 0.4, 0.44, 0.46, 0.6, 0.7 for pixels and lines. The label of these items is the RESPONSE axis only (the planted response r). QA-A-252e chose the
 *       pair 0.44 / 0.46 AFTER seeing the result at the exact 0.45 (a development experiment, not a pre-registered design); a true response of exactly 0.45 is normal, the
 *       decision of a detector at that value fluctuates with the noise of its estimate (about 0.0007, an estimate from earlier data, not re-measured).
 *       The NOISE axis is measured, not derived: see noise-cost below. dead at 0.5 and slope 0.75..1.25 are normal now. The quantum gain kappa (noise variance per signal unit) is
 *       estimated from the generation bright frames and written to truth.json.
 *   xpe_bpm_eval noise-cost --truth DIR --dark F... --verify F... --out FILE [--limit 2] [--raw-frames 1] [--sample-margin 32]
 *       The noise cost of every response item (gain_loss, efficiency_loss), measured on verification frames (planted frames of ONE dose that no detector run uses): per pixel
 *       the temporal variance (row and column means of every frame taken out, nu = frames - 1) divided by the squared measured response and by the local expectation
 *       (median of the unplanted pixels 4..12 pixels away, divided by the median of chi-square_nu / nu). The item cost is the mean over its pixels with a chi-square
 *       interval of nu x pixels degrees of freedom, widened by the error of the expectation. Verdict: defect when the interval lies at or above the limit, normal when it lies
 *       below, undetermined when it contains it. The model formula 1/r is NOT used.
 *       Exposure (QA-A-252g): the net signal (frame minus the dark level) of every verification frame is multiplied by S_ref / S_k, S_k the median of its net signal over the unplanted
 *       interior pixels (a defect cannot pull it), S_ref the mean of the S_k; then the deviation about the mean of the scaled frames, then the row and column means of every frame out.
 *       One scale per frame from about 10^6 pixels: the per-pixel degrees of freedom stay frames - 1. --raw-frames 1 switches the scaling off (the labels of QA-A-252f).
 *       Interval: the degrees of freedom of an item are the smaller of pixels x nu and the effective ones from blocks of 64 consecutive pixels (variance of the block means); the labels
 *       are PROVISIONAL (status column) until the spatial correlation is checked against an independent replicate.
 *   xpe_bpm_eval defect-xcal --in F [--map-out F] [--area-out F] [--lines-out F] [--v1-out F]
 *   xpe_bpm_eval correct-time --set DIR --frame F --width W --height H [--repeat N]
 *       What a defect.xcal of format version 1 or 2 holds, and the layers written out (QA-A-254).
 *   xpe_bpm_eval score --truth DIR --map F [--noise-truth FILE] [--overlap composite] [--overlap-truth FILE] [--overlap-out FILE]
 *   xpe_bpm_eval chain --truth DIR --items a+b+c [--dark D] [--signal S]
 *       The response of the injection chain (QA-A-255; the arithmetic of the overlap measurement)
 *       --overlap-truth (QA-A-252j): the overlapped pixels are classified from the MEASUREMENT of noise-cost --overlap-out (noise label, chain response with the real offset and dose, clipping)
 *       --noise-truth: the file of noise-cost. An item is a defect when either axis says so; normal when both say so; undetermined (counted as neither a hit nor a false alarm,
 *       only its number is reported) when the response axis says normal and the noise verdict is undetermined. [--clean-map F] [--half-a F --half-b F] [--area MASK.u8] [--out metrics.json]
 *       --area: the imaging-area mask (1 inside, 0 outside, as written by map-v2 --area): flags outside it are not counted in any map.
 *       Detection rate per kind and level, false alarms, control false detections, split-half Jaccard.
 *   xpe_bpm_eval correct --truth DIR --clean-set DIR --inj-set DIR --clean-eval F... --inj-eval F... [--margins L,T,R,B] [--out metrics.json]
 *       With --margins the line index is also measured under the imaging-area contract (QA-A-265): only where the part's lines, its two reference lines and its extent cut to the area
 *       lie inside the area; the rest is counted as not measurable with its reason. The old figures stay as they were; the new ones are printed and written as "area_contract".
 *       Correction error against local noise and the line index D_n (TG-150 4.3.9) on evaluation frames, through the application's final pipeline.
 *   xpe_bpm_eval map-v2 --out MASK.u8 [--features F.u8] [--report R.json] --dark F... --bright F... [--keep-first-dark] [--skip-first-dark 0|1] [--margins L,T,R,B] [--area OUT.u8] [--target-fa 1] [--min-dark N] [--min-bright N]
 *       The second-generation detector xpe_bpm_generate_v2 (QA-A-252): the map, the per-pixel feature bits and a JSON report of the thresholds.
 *   xpe_bpm_eval lines --out LINES.u8 [--tsv F] [--report R.json] --dark F... --bright F... [--margins L,T,R,B] [--keep-first-dark] [--skip-first-dark 0|1] [--fa N] [--min-segment N] [--tolerance-db D]
 *       The line detector xpe_bpm_find_lines (QA-A-253): defective rows and columns (full, partial, double, crossing) as a mask and a table; exit code 20 when three or
 *       more adjacent lines are found (panel rejection).
 *   xpe_bpm_eval merge --in A.u8 --in B.u8 ... --out C.u8
 *       The union of 8-bit maps (a pixel keeps its first non-zero value).
 *   xpe_bpm_eval blobs --map F [--width W --height H] [--out blobs.tsv]
 *       The blobs (8-neighbourhood groups of defective pixels) of a map with their sizes.
 *   xpe_bpm_eval map-v1 --out MASK.u8 --dark F... --bright F... [--min-dark N] [--min-bright N] [--width W] [--height H]
 *       The module's xpe_bpm_generate with default settings (only the minimum frame counts can be lowered), the mask written as raw uint8.
 *
 * Exit codes: 0 ok, 2 usage, 3 input unreadable or wrong size, 4 module error, 5 placement impossible (frame too small for the spec).
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe_sha256.hpp"
#include "xpe_bpm_eval_stats.hpp"
#include "defect_ext.hpp"
#include "xcal_writer.hpp"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_common_api.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint16_t kSharedId = 65535;
constexpr double   kNonlinearRef = 30000.0;   // signal at which the nonlinearity term c is defined (spec 2.1)
constexpr double kResponseDefectBelow = 0.45;   // SRS FUNC-022 (provisional, user decision 2026-10-16): a response below 45 % of the local median is a defect
constexpr double kResponseDefectAbove = 1.5;    // ... or above 150 %
constexpr double kRatioSlack = 1e-6;            // the same slack as the detector (xpe_bpm_v2.cpp kRatioSlack): exactly 0.45 and 1.5 are normal

// ------------------------------------------------------------------------------------------------------------- arguments
struct Args {
    std::string                                      cmd;
    std::map<std::string, std::vector<std::string>> opt;
    bool has(const std::string& k) const { return opt.count(k) != 0; }
    std::string str(const std::string& k, const std::string& def = "") const {
        auto it = opt.find(k);
        return it == opt.end() || it->second.empty() ? def : it->second.back();
    }
    long long num(const std::string& k, long long def) const {
        auto it = opt.find(k);
        return it == opt.end() || it->second.empty() ? def : std::strtoll(it->second.back().c_str(), nullptr, 10);
    }
    std::vector<std::string> list(const std::string& k) const {
        auto it = opt.find(k);
        return it == opt.end() ? std::vector<std::string>() : it->second;
    }
};

bool parseArgs(int argc, char** argv, Args* a) {
    if (argc < 2) return false;
    a->cmd = argv[1];
    for (int i = 2; i < argc; ++i) {
        const std::string k = argv[i];
        if (k == "--keep-first-dark") {   // QA-A-260: a flag without a value, like the generator's option of the same name
            a->opt["keep-first-dark"].push_back("1");
            continue;
        }
        if (k.size() < 3 || k[0] != '-' || k[1] != '-' || i + 1 >= argc) {
            std::fprintf(stderr, "bad argument: %s\n", k.c_str());
            return false;
        }
        a->opt[k.substr(2)].push_back(argv[++i]);
    }
    return true;
}

void usage() {
    std::fprintf(stderr,
                 "usage: xpe_bpm_eval inject|score|correct|map-v1 ...   (see the header of tools/xpe_bpm_eval.cpp)\n");
}

// ------------------------------------------------------------------------------------------------------------- files
bool readRaw16(const std::string& path, size_t count, std::vector<uint16_t>* out) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size != count * sizeof(uint16_t)) {
        std::fprintf(stderr, "%s: %s (expected %zu bytes)\n", path.c_str(), ec ? "cannot open" : "wrong size", count * sizeof(uint16_t));
        return false;
    }
    out->resize(count);
    std::ifstream f(path, std::ios::binary);
    f.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(count * sizeof(uint16_t)));
    return static_cast<bool>(f);
}

bool writeBytes(const std::string& path, const void* data, size_t bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(static_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    f.flush();
    return static_cast<bool>(f);
}

bool readAllBytes(const std::string& path, std::vector<uint8_t>* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out->assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

std::string sha256Hex(const std::string& path) {
    std::vector<uint8_t> bytes;
    if (!readAllBytes(path, &bytes)) return {};
    const auto digest = compute_sha256(bytes.data(), bytes.size());
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (uint8_t b : digest) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

/**
 * A defect map as flags (1 = defective): raw uint8 (N bytes), raw uint16 (2N bytes), or a defect.xcal of either format version. QA-A-261 (Codex #230 R230-2): a defect.xcal is read by the
 * module's own reader (read_defect_xcal), so a version-2 file with its imaging area and line/blob extension gives its defect plane; the earlier "last N bytes" guess only fitted version 1.
 */
bool readMap(const std::string& path, size_t n, std::vector<uint8_t>* flags) {
    std::vector<uint8_t> bytes;
    flags->assign(n, 0);
    const bool xcal = path.size() > 5 && path.compare(path.size() - 5, 5, ".xcal") == 0;
    if (xcal) {
        DefectXcal d;
        xpe_clear_alerts();
        const XpeErrorCode rc = read_defect_xcal(path.c_str(), false, &d);
        xpe_clear_alerts();
        if (rc != XPE_OK) {
            std::fprintf(stderr, "%s: not a readable defect.xcal (error code %d)\n", path.c_str(), static_cast<int>(rc));
            return false;
        }
        if (d.map.size() != n) {
            std::fprintf(stderr, "%s: the defect.xcal holds %zu pixels, expected %zu\n", path.c_str(), d.map.size(), n);
            return false;
        }
        for (size_t i = 0; i < n; ++i) (*flags)[i] = d.map[i] != 0 ? 1 : 0;
        return true;
    }
    if (!readAllBytes(path, &bytes)) {
        std::fprintf(stderr, "%s: cannot open\n", path.c_str());
        return false;
    }
    if (bytes.size() == n) {
        for (size_t i = 0; i < n; ++i) (*flags)[i] = bytes[i] != 0 ? 1 : 0;
    } else if (bytes.size() == 2 * n) {
        for (size_t i = 0; i < n; ++i) (*flags)[i] = (bytes[2 * i] | bytes[2 * i + 1]) != 0 ? 1 : 0;
    } else {
        std::fprintf(stderr, "%s: %zu bytes; expected %zu (uint8), %zu (uint16) or a defect.xcal\n", path.c_str(), bytes.size(), n, 2 * n);
        return false;
    }
    return true;
}

std::string baseName(const std::string& path) { return fs::path(path).filename().string(); }

// ------------------------------------------------------------------------------------------------------------- random
uint64_t splitmix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

// ------------------------------------------------------------------------------------------------------------- items
using xpe_bpm_eval::Rect;

struct Item {
    int               id{0};
    std::string       cls;     // pixel, block, line, control (after the noise axis: also undetermined)
    std::string       cls0;    // the class of truth.tsv, the injection class (QA-A-255: an overlap reads this, never the promotions of cls)
    std::string       kind;    // dead, hot, noisy, slope, nonlinear, fixed_band, gain_band, varying_band
    std::string       shape;   // pixel, block<k>, row_full, col_full, row_partial, col_partial, row_pair, col_pair, cross
    double            level{0};
    std::vector<Rect> rects;
};

struct Options {
    uint32_t w{3072}, h{3072};
    uint64_t seed{251};
    int      pixelsPerLevel{64}, blocksPerLevel{16}, margin{32}, lineGap{24}, exclHalo{3};
};

std::string fmtLevel(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return b;
}

/** The class of an item that changes the response to r: a defect class below 45 % (or above 150 %), control inside (0.45 itself is inside). */
std::string responseClass(const char* defectClass, double r) {
    return (r < kResponseDefectBelow - kRatioSlack || r > kResponseDefectAbove + kRatioSlack) ? defectClass : "control";
}

std::string groupKey(const Item& it) { return it.cls + "/" + it.kind + "/" + it.shape + "/" + fmtLevel(it.level); }

/** Calls @p fn(index) for every pixel of the item once (a pixel covered by two rectangles of one item is visited once). */
template <class F>
void forItemPixels(const Item& it, uint32_t w, F fn) {
    for (size_t r = 0; r < it.rects.size(); ++r) {
        const Rect& q = it.rects[r];
        for (int y = q.y; y < q.y + q.h; ++y) {
            for (int x = q.x; x < q.x + q.w; ++x) {
                bool earlier = false;
                for (size_t e = 0; e < r && !earlier; ++e) {
                    const Rect& p = it.rects[e];
                    earlier = x >= p.x && x < p.x + p.w && y >= p.y && y < p.y + p.h;
                }
                if (!earlier) fn(static_cast<size_t>(y) * w + static_cast<size_t>(x));
            }
        }
    }
}

// ------------------------------------------------------------------------------------------------------------- inject
struct Interval {
    int a, b;   // inclusive
};

bool gapOk(const std::vector<Interval>& occ, int a, int b, int gap) {
    for (const Interval& o : occ)
        if (a <= o.b + gap && b >= o.a - gap) return false;
    return true;
}

std::vector<uint8_t> dilate(const std::vector<uint8_t>& m, uint32_t w, uint32_t h, int r) {
    std::vector<uint8_t> tmp(m.size(), 0), out(m.size(), 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            if (!m[static_cast<size_t>(y) * w + x]) continue;
            const int x0 = std::max<int>(0, static_cast<int>(x) - r), x1 = std::min<int>(static_cast<int>(w) - 1, static_cast<int>(x) + r);
            for (int xx = x0; xx <= x1; ++xx) tmp[static_cast<size_t>(y) * w + static_cast<size_t>(xx)] = 1;
        }
    }
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            if (!tmp[static_cast<size_t>(y) * w + x]) continue;
            const int y0 = std::max<int>(0, static_cast<int>(y) - r), y1 = std::min<int>(static_cast<int>(h) - 1, static_cast<int>(y) + r);
            for (int yy = y0; yy <= y1; ++yy) out[static_cast<size_t>(yy) * w + x] = 1;
        }
    }
    return out;
}

bool rectFree(const std::vector<uint8_t>& excl, uint32_t w, const Rect& q) {
    for (int y = q.y; y < q.y + q.h; ++y)
        for (int x = q.x; x < q.x + q.w; ++x)
            if (excl[static_cast<size_t>(y) * w + static_cast<size_t>(x)]) return false;
    return true;
}

/** Spec 2.5: builds the item list. Returns false when the frame is too small for the spec. */
bool placeItems(const Options& o, const std::vector<uint8_t>& exclD, std::vector<Item>* items) {
    const int W = static_cast<int>(o.w), H = static_cast<int>(o.h);
    std::mt19937_64 rng(splitmix(o.seed));
    auto uni = [&](int lo, int hi) { return lo + static_cast<int>(rng() % static_cast<uint64_t>(hi - lo + 1)); };   // inclusive

    std::vector<Interval> rowOcc, colOcc;
    std::vector<uint8_t>  rowHas(static_cast<size_t>(H), 0), colHas(static_cast<size_t>(W), 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (exclD[static_cast<size_t>(y) * o.w + static_cast<size_t>(x)]) { rowHas[static_cast<size_t>(y)] = 1; colHas[static_cast<size_t>(x)] = 1; }

    const int partialRow = W / 4, partialCol = H / 4;
    auto pickLine = [&](bool row, int k, bool full, Rect* out) -> bool {
        std::vector<Interval>& occ = row ? rowOcc : colOcc;
        const std::vector<uint8_t>& has = row ? rowHas : colHas;
        const int span = row ? H : W, along = row ? W : H, partial = row ? partialRow : partialCol;
        for (int attempt = 0; attempt < 40000; ++attempt) {
            const int i = uni(o.margin, span - o.margin - k);
            if (!gapOk(occ, i, i + k - 1, o.lineGap)) continue;
            Rect q{};
            if (full) {
                bool free = true;
                for (int t = 0; t < k && free; ++t) free = !has[static_cast<size_t>(i + t)];
                if (!free) continue;
                q = row ? Rect{0, i, along, k} : Rect{i, 0, k, along};
            } else {
                const int s = uni(o.margin, along - o.margin - partial);
                q = row ? Rect{s, i, partial, k} : Rect{i, s, k, partial};
                if (!rectFree(exclD, o.w, q)) continue;
            }
            occ.push_back({i, i + k - 1});
            *out = q;
            return true;
        }
        return false;
    };

    int nextId = 1;
    auto addLine = [&](const std::string& cls, const std::string& kind, double level, const std::string& shape) -> bool {
        Item it;
        it.id = nextId;
        it.cls = cls;
        it.kind = kind;
        it.level = level;
        it.shape = shape;
        Rect q{};
        if (shape == "cross") {
            Rect r{}, c{};
            if (!pickLine(true, 1, true, &r) || !pickLine(false, 1, true, &c)) return false;
            it.rects = {r, c};
        } else {
            const bool row = shape.compare(0, 3, "row") == 0;
            const bool full = shape.find("full") != std::string::npos;
            const int  k = shape.find("pair") != std::string::npos ? 2 : 1;
            if (!pickLine(row, k, full, &q)) return false;
            it.rects = {q};
        }
        items->push_back(it);
        ++nextId;
        return true;
    };

    const char* const shapes6[6] = {"row_full", "col_full", "row_partial", "col_partial", "row_pair", "col_pair"};
    const std::pair<const char*, std::vector<double>> lineModels[3] = {{"dead", {1.0, 0.5}}, {"nonlinear", {0.4, 0.2}}, {"noisy", {4.0, 2.0}}};
    for (const auto& m : lineModels)
        for (double lv : m.second)
            for (const char* sh : shapes6)
                if (!addLine(std::string(m.first) == "dead" ? responseClass("line", 1.0 - lv) : "line", m.first, lv, sh)) return false;
    for (const auto& cr : std::vector<std::pair<const char*, double>>{{"dead", 1.0}, {"nonlinear", 0.4}, {"noisy", 4.0}})
        if (!addLine("line", cr.first, cr.second, "cross")) return false;
    // QA-A-252d: response lines of the two models, at the response r (the level is r itself)
    for (const char* kind : {"gain_loss", "efficiency_loss"})
        for (double r : {0.3, 0.4, 0.44, 0.46, 0.6, 0.7})
            for (const char* sh : shapes6)
                if (!addLine(responseClass("line", r), kind, r, sh)) return false;
    const std::pair<const char*, std::vector<double>> controls[3] = {{"fixed_band", {100.0, 30.0}}, {"gain_band", {0.8, 1.2}}, {"varying_band", {20.0, 8.0}}};
    for (const auto& c : controls)
        for (double lv : c.second)
            for (const char* sh : {"row_full", "col_full"})
                if (!addLine("control", c.first, lv, sh)) return false;

    // pixel and block defects keep 4 pixels away from every line's row and column numbers (full extent) and 8 from each other
    std::vector<uint8_t> rowBlock(static_cast<size_t>(H), 0), colBlock(static_cast<size_t>(W), 0);
    // a row-type rectangle blocks only its row numbers, a column-type one only its column numbers (a partial line too, over the full extent)
    for (const Item& it : *items) {
        for (const Rect& q : it.rects) {
            const bool rowType = q.w > q.h;
            if (rowType)
                for (int y = std::max(0, q.y - 3); y <= std::min(H - 1, q.y + q.h + 2); ++y) rowBlock[static_cast<size_t>(y)] = 1;
            else
                for (int x = std::max(0, q.x - 3); x <= std::min(W - 1, q.x + q.w + 2); ++x) colBlock[static_cast<size_t>(x)] = 1;
        }
    }
    std::vector<Rect> placed;
    auto placeSmall = [&](const std::string& cls, const std::string& kind, double level, const std::string& shape, int s) -> bool {
        for (int attempt = 0; attempt < 200000; ++attempt) {
            const Rect q{uni(o.margin, W - o.margin - s), uni(o.margin, H - o.margin - s), s, s};
            bool ok = true;
            for (int y = q.y; y < q.y + s && ok; ++y) ok = !rowBlock[static_cast<size_t>(y)];
            for (int x = q.x; x < q.x + s && ok; ++x) ok = !colBlock[static_cast<size_t>(x)];
            if (!ok || !rectFree(exclD, o.w, q)) continue;
            for (const Rect& p : placed) {
                const int dx = std::max(q.x - (p.x + p.w), p.x - (q.x + q.w)), dy = std::max(q.y - (p.y + p.h), p.y - (q.y + q.h));
                if (std::max(dx, dy) < 8) { ok = false; break; }
            }
            if (!ok) continue;
            Item it;
            it.id = nextId++;
            it.cls = cls;
            it.kind = kind;
            it.level = level;
            it.shape = shape;
            it.rects = {q};
            items->push_back(it);
            placed.push_back(q);
            return true;
        }
        return false;
    };
    for (int size : {2, 3, 4, 8})
        for (double a : {1.0, 0.5})
            for (int n = 0; n < o.blocksPerLevel; ++n)
                if (!placeSmall(responseClass("block", 1.0 - a), "dead", a, "block" + std::to_string(size), size)) return false;
    const std::pair<const char*, std::vector<double>> pix[7] = {
        {"dead", {1.0, 0.5}}, {"hot", {200.0, 100.0, 50.0, 25.0}}, {"noisy", {4.0, 2.0, 1.0}}, {"slope", {0.75, 0.9, 1.1, 1.25}}, {"nonlinear", {0.4, 0.2}},
        {"gain_loss", {0.3, 0.4, 0.44, 0.46, 0.6, 0.7}}, {"efficiency_loss", {0.3, 0.4, 0.44, 0.46, 0.6, 0.7}}};
    for (const auto& p : pix) {
        const std::string kind = p.first;
        for (double lv : p.second) {
            // response items are judged by their response: dead has the response 1 - level, slope and the loss models have the level itself
            const std::string cls = kind == "dead" ? responseClass("pixel", 1.0 - lv) : (kind == "slope" || kind == "gain_loss" || kind == "efficiency_loss") ? responseClass("pixel", lv) : "pixel";
            for (int n = 0; n < o.pixelsPerLevel; ++n)
                if (!placeSmall(cls, kind, lv, "pixel", 1)) return false;
        }
    }
    return true;
}

/** The quantum gain fit with its groups, for truth.json. */
struct QuantumGainResult {
    double      kappa{0.25}, se{0}, v0{0}, v0Se{0};
    bool        measured{false};
    std::string source;
    struct Group {
        double signal, variance, dof, pixels, kappaGroup;
        int    frames;
    };
    std::vector<Group> groups;
};

std::string quantumGainJson(const QuantumGainResult& q) {
    char b[512];
    std::snprintf(b, sizeof b, "\"quantum_gain\": %.6f,\n  \"quantum_gain_se\": %.6f,\n  \"readout_variance\": %.4f,\n  \"readout_variance_se\": %.4f,\n  \"quantum_gain_measured\": %s,\n  \"quantum_gain_source\": \"%s\",\n  \"quantum_gain_groups\": [", q.kappa, q.se, q.v0, q.v0Se, q.measured ? "true" : "false", q.source.c_str());
    std::string j = b;
    for (size_t i = 0; i < q.groups.size(); ++i) {
        const auto& g = q.groups[i];
        std::snprintf(b, sizeof b, "%s{\"signal\": %.2f, \"frames\": %d, \"dof\": %.0f, \"pixels\": %.0f, \"variance\": %.4f, \"kappa_group\": %.6f}", i ? ", " : "", g.signal, g.frames, g.dof, g.pixels, g.variance, g.kappaGroup);
        j += b;
    }
    return j + "]";
}

/**
 * The quantum gain kappa (variance per signal unit, in pixel values) and the readout variance v0 from the generation bright frames (QA-A-252f, Codex #213 finding 2).
 * Frames of nearly the same signal form a group. Per pixel the unbiased sample variance about the group mean (frames scaled to the group mean, the row mean of every frame
 * taken out) has m - 1 degrees of freedom; its MEDIAN over the pixels is too small by the median of chi-square / nu (0.693 for three frames), so it is divided by that factor.
 * The group variance is regressed on the group signal (variance = v0 + kappa x signal, weighted by the inverse variance of each group variance); the standard error widens with
 * the scatter about the line. With one group only, v0 is taken from the dark noise (sigma_t squared) and the estimate says so. Without any group of two frames the default 0.25 is
 * used and marked as an unconfirmed model.
 */
QuantumGainResult estimateQuantumGain(const std::vector<std::string>& brightFiles, size_t n, uint32_t w, uint32_t h, const std::vector<double>& D, double sigmaT) {
    QuantumGainResult res;
    res.source = "default 0.25 (unconfirmed model: no dose group of two frames among the generation frames)";
    std::vector<std::vector<uint16_t>> fr(brightFiles.size());
    std::vector<double>                sig(brightFiles.size(), 0.0);
    const uint32_t x0 = w / 10, x1 = w - w / 10, y0 = h / 10, y1 = h - h / 10;
    for (size_t k = 0; k < brightFiles.size(); ++k) {
        if (!readRaw16(brightFiles[k], n, &fr[k])) return res;
        double acc = 0.0;
        for (uint32_t y = y0; y < y1; y += 3)
            for (uint32_t x = x0; x < x1; x += 3) acc += fr[k][static_cast<size_t>(y) * w + x] - D[static_cast<size_t>(y) * w + x];
        sig[k] = acc / (static_cast<double>((y1 - y0 + 2) / 3) * static_cast<double>((x1 - x0 + 2) / 3));
    }
    std::vector<size_t> order(brightFiles.size());
    for (size_t k = 0; k < order.size(); ++k) order[k] = k;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return sig[a] < sig[b]; });
    std::vector<std::vector<size_t>> groups;
    for (size_t t = 0; t < order.size(); ++t) {
        if (t == 0 || !(sig[order[t]] > 0.0) || sig[order[t]] / sig[order[t - 1]] > 1.25) groups.emplace_back();
        groups.back().push_back(order[t]);
    }
    std::vector<xpe_bpm_eval::QuantumGainGroup> fit;
    for (const auto& g : groups) {
        if (g.size() < 2 || !(sig[g[0]] > 100.0)) continue;
        double gm = 0.0;
        for (size_t k : g) gm += sig[k];
        gm /= static_cast<double>(g.size());
        std::vector<std::vector<double>> rowMean(g.size(), std::vector<double>(h, 0.0));
        for (size_t j = 0; j < g.size(); ++j) {
            const double scale = gm / sig[g[j]];
            for (uint32_t y = 0; y < h; ++y) {
                double acc = 0.0;
                for (uint32_t x = 0; x < w; ++x) acc += (fr[g[j]][static_cast<size_t>(y) * w + x] - D[static_cast<size_t>(y) * w + x]) * scale;
                rowMean[j][y] = acc / w;
            }
        }
        std::vector<double> vars, nets;
        for (uint32_t y = y0; y < y1; y += 7)
            for (uint32_t x = x0; x < x1; x += 7) {
                const size_t        i = static_cast<size_t>(y) * w + x;
                std::vector<double> v(g.size());
                double              m = 0.0, net = 0.0;
                for (size_t j = 0; j < g.size(); ++j) {
                    const double scaled = (fr[g[j]][i] - D[i]) * (gm / sig[g[j]]);
                    v[j] = scaled - rowMean[j][y];
                    m += v[j];
                    net += scaled;
                }
                m /= static_cast<double>(g.size());
                net /= static_cast<double>(g.size());
                double var = 0.0;
                for (double e : v) var += (e - m) * (e - m);
                var /= static_cast<double>(g.size() - 1);
                if (net > 100.0) {
                    vars.push_back(var);
                    nets.push_back(net);
                }
            }
        if (vars.empty()) continue;
        const double nu = static_cast<double>(g.size() - 1);
        xpe_bpm_eval::QuantumGainGroup q;
        q.variance = xpe_bpm_eval::medianOf(vars) / xpe_bpm_eval::chiSquareMedianOverNu(nu);
        q.signal = xpe_bpm_eval::medianOf(nets);
        q.dof = nu;
        q.pixels = static_cast<double>(vars.size());
        fit.push_back(q);
        res.groups.push_back({q.signal, q.variance, q.dof, q.pixels, 0.0, static_cast<int>(g.size())});
    }
    if (fit.empty()) {
        res.groups.clear();
        return res;
    }
    const xpe_bpm_eval::QuantumGainFit f = xpe_bpm_eval::fitQuantumGain(fit);
    if (f.ok) {
        res.kappa = f.kappa;
        res.se = f.kappaSe;
        res.v0 = f.v0;
        res.v0Se = f.v0Se;
        res.measured = true;
        res.source = "regression of the group variance on the group signal over " + std::to_string(f.groups) + " dose groups (median variance corrected for the frame-count bias)";
    } else {
        const auto& q = fit.back();   // the largest signal: the quantum part dominates
        res.v0 = sigmaT * sigmaT;
        res.kappa = (q.variance - res.v0) / q.signal;
        res.se = q.variance / q.signal * std::sqrt(2.0 / (q.dof * q.pixels));
        res.measured = res.kappa > 0.0;
        res.source = "one dose group; the readout variance is taken from the dark noise (sigma_t squared)";
        if (!res.measured) {
            res.kappa = 0.25;
            res.source = "default 0.25 (unconfirmed model: the single-group estimate was not positive)";
        }
    }
    for (auto& g : res.groups) g.kappaGroup = (g.variance - res.v0) / g.signal;
    return res;
}

int cmdInject(const Args& a) {
    Options o;
    o.w = static_cast<uint32_t>(a.num("width", 3072));
    o.h = static_cast<uint32_t>(a.num("height", 3072));
    o.seed = static_cast<uint64_t>(a.num("seed", 251));
    o.pixelsPerLevel = static_cast<int>(a.num("pixels-per-level", 64));
    o.blocksPerLevel = static_cast<int>(a.num("blocks-per-level", 16));
    o.margin = static_cast<int>(a.num("margin", 32));
    o.lineGap = static_cast<int>(a.num("line-gap", 24));
    const std::string out = a.str("out");
    const std::vector<std::string> darkFiles = a.list("dark"), brightFiles = a.list("bright"), evalFiles = a.list("eval");
    if (out.empty() || darkFiles.size() < 3 || o.w < 64 || o.h < 64) {
        std::fprintf(stderr, "inject needs --out, at least 3 --dark frames and a frame of at least 64 x 64\n");
        return 2;
    }
    {
        std::set<std::string> names;
        for (const std::vector<std::string>* group : {&darkFiles, &brightFiles, &evalFiles})
            for (const std::string& f : *group)
                if (!names.insert(baseName(f)).second) {
                    std::fprintf(stderr, "two inputs have the file name %s: the planted frames are written under that name, one would overwrite the other\n", baseName(f).c_str());
                    return 2;
                }
    }
    const size_t n = static_cast<size_t>(o.w) * o.h;

    // the pixel dark level D and the temporal noise sigma_t, from the generation darks as they are before anything is planted
    std::vector<double> D(n, 0.0);
    std::vector<std::vector<uint16_t>> darks(darkFiles.size());
    for (size_t k = 0; k < darkFiles.size(); ++k) {
        if (!readRaw16(darkFiles[k], n, &darks[k])) return 3;
        for (size_t i = 0; i < n; ++i) D[i] += darks[k][i];
    }
    for (size_t i = 0; i < n; ++i) D[i] /= static_cast<double>(darks.size());
    std::vector<float> var(n, 0.0f);
    {
        std::vector<double> rowMean(o.h);
        for (size_t k = 0; k < darks.size(); ++k) {
            for (uint32_t y = 0; y < o.h; ++y) {
                double s = 0;
                for (uint32_t x = 0; x < o.w; ++x) s += darks[k][static_cast<size_t>(y) * o.w + x] - D[static_cast<size_t>(y) * o.w + x];
                rowMean[y] = s / static_cast<double>(o.w);
            }
            for (uint32_t y = 0; y < o.h; ++y)
                for (uint32_t x = 0; x < o.w; ++x) {
                    const size_t i = static_cast<size_t>(y) * o.w + x;
                    const double e = darks[k][i] - D[i] - rowMean[y];
                    var[i] += static_cast<float>(e * e);
                }
        }
    }
    for (size_t i = 0; i < n; ++i) var[i] = std::sqrt(var[i] / static_cast<float>(darks.size() - 1));
    std::nth_element(var.begin(), var.begin() + static_cast<std::ptrdiff_t>(n / 2), var.end());
    const double sigmaT = var[n / 2];
    var.clear();
    var.shrink_to_fit();
    const QuantumGainResult qg = estimateQuantumGain(brightFiles, n, o.w, o.h, D, sigmaT);
    const double            kappa = qg.kappa;

    // positions
    std::vector<uint8_t> excl(n, 0);
    std::vector<std::string> exclFiles = a.list("exclude-mask");
    for (const std::string& f : exclFiles) {
        std::vector<uint8_t> m;
        if (!readMap(f, n, &m)) return 3;
        for (size_t i = 0; i < n; ++i) excl[i] = static_cast<uint8_t>(excl[i] | m[i]);
    }
    const std::vector<uint8_t> exclD = dilate(excl, o.w, o.h, o.exclHalo);
    std::vector<Item> items;
    if (!placeItems(o, exclD, &items)) {
        std::fprintf(stderr, "the spec does not fit a %u x %u frame with these options (margin %d, line gap %d)\n", o.w, o.h, o.margin, o.lineGap);
        return 5;
    }

    // item number per pixel (65535 where items overlap)
    std::vector<uint8_t>  cover(n, 0);
    std::vector<uint16_t> ids(n, 0);
    for (const Item& it : items) forItemPixels(it, o.w, [&](size_t i) { if (cover[i] < 255) ++cover[i]; });
    for (const Item& it : items)
        forItemPixels(it, o.w, [&](size_t i) { ids[i] = cover[i] > 1 ? kSharedId : static_cast<uint16_t>(it.id); });

    std::error_code ec;
    fs::create_directories(fs::path(out) / "frames", ec);
    auto plant = [&](const std::string& file, bool bright) -> bool {
        std::vector<uint16_t> frame;
        if (!readRaw16(file, n, &frame)) return false;
        std::mt19937_64 rng(splitmix(o.seed ^ fnv1a(baseName(file))));
        std::normal_distribution<double> unit(0.0, 1.0);
        std::vector<double> common(items.size() + 1, 0.0);   // the per-frame value of each varying band
        for (const Item& it : items)
            if (it.kind == "varying_band") common[static_cast<size_t>(it.id)] = it.level * unit(rng);
        for (const Item& it : items) {
            forItemPixels(it, o.w, [&](size_t i) {
                double v = frame[i];
                const double d = D[i], nn = v - d;
                if (it.kind == "dead") {
                    if (bright) v = d + (1.0 - it.level) * nn;
                } else if (it.kind == "hot" || it.kind == "fixed_band") {
                    v += it.level;
                } else if (it.kind == "noisy") {
                    if (it.cls == "line" || it.cls == "pixel") v += it.level * sigmaT * unit(rng);
                } else if (it.kind == "slope" || it.kind == "gain_band" || it.kind == "gain_loss") {
                    if (bright) v = d + it.level * nn;
                } else if (it.kind == "efficiency_loss") {
                    // r times the detected quanta: the signal x r and the quantum noise variance x r. The frame already holds the noise of all its quanta (variance kappa * nn plus
                    // the readout variance), so after x r it has r^2 * (kappa * nn + readout); the thinning adds kappa * r * (1 - r) * nn (binomial thinning of the quanta, Gaussian
                    // approximation). The readout noise is therefore scaled by r^2, which a real loss of detection efficiency (before the readout) would not do (QA-A-252f: stated)
                    if (bright) v = d + it.level * nn + std::sqrt(kappa * it.level * (1.0 - it.level) * std::max(nn, 0.0)) * unit(rng);
                } else if (it.kind == "nonlinear") {
                    if (bright) v = d + nn * (1.0 - it.level * nn / kNonlinearRef);
                } else if (it.kind == "varying_band") {
                    v += common[static_cast<size_t>(it.id)];
                }
                v = std::floor(v + 0.5);
                frame[i] = static_cast<uint16_t>(std::min(65535.0, std::max(0.0, v)));
            });
        }
        return writeBytes((fs::path(out) / "frames" / baseName(file)).string(), frame.data(), frame.size() * sizeof(uint16_t));
    };
    for (const std::string& f : darkFiles)
        if (!plant(f, false)) return 3;
    for (const std::string& f : brightFiles)
        if (!plant(f, true)) return 3;
    for (const std::string& f : evalFiles)
        if (!plant(f, true)) return 3;

    // truth
    {
        std::string t = "#xpe_bpm_eval truth 1\nwidth\t" + std::to_string(o.w) + "\nheight\t" + std::to_string(o.h) + "\nseed\t" + std::to_string(o.seed) + "\n";
        char sb[64];
        std::snprintf(sb, sizeof sb, "sigma_t\t%.6f\n", sigmaT);
        t += sb;
        t += "items\t" + std::to_string(items.size()) + "\n";
        for (const Item& it : items) {
            t += std::to_string(it.id) + "\t" + it.cls + "\t" + it.kind + "\t" + it.shape + "\t" + fmtLevel(it.level) + "\t";
            for (size_t r = 0; r < it.rects.size(); ++r) {
                const Rect& q = it.rects[r];
                t += std::to_string(q.x) + "," + std::to_string(q.y) + "," + std::to_string(q.w) + "," + std::to_string(q.h) + (r + 1 < it.rects.size() ? ";" : "");
            }
            t += "\n";
        }
        if (!writeBytes((fs::path(out) / "truth.tsv").string(), t.data(), t.size())) return 3;
    }
    if (!writeBytes((fs::path(out) / "truth_ids.u16").string(), ids.data(), ids.size() * sizeof(uint16_t))) return 3;
    {
        std::map<std::string, int> perClass;
        for (const Item& it : items) ++perClass[it.cls];
        char sb[64], kb[64];
        std::snprintf(sb, sizeof sb, "%.6f", sigmaT);
        std::snprintf(kb, sizeof kb, "%.6f", kappa);
        std::string j = "{\n  \"tool\": \"xpe_bpm_eval\",\n  \"spec_version\": 2,\n  \"seed\": " + std::to_string(o.seed) + ",\n  \"width\": " + std::to_string(o.w) +
                        ",\n  \"height\": " + std::to_string(o.h) + ",\n  \"sigma_t\": " + sb + ",\n  " + quantumGainJson(qg) + ",\n  \"options\": {\"pixels_per_level\": " + std::to_string(o.pixelsPerLevel) +
                        ", \"blocks_per_level\": " + std::to_string(o.blocksPerLevel) + ", \"margin\": " + std::to_string(o.margin) + ", \"line_gap\": " + std::to_string(o.lineGap) +
                        ", \"exclusion_halo\": " + std::to_string(o.exclHalo) + "},\n  \"items\": {";
        size_t c = 0;
        for (const auto& kv : perClass) j += "\"" + kv.first + "\": " + std::to_string(kv.second) + (++c < perClass.size() ? ", " : "");
        j += "},\n  \"inputs\": [\n";
        std::vector<std::pair<std::string, std::string>> in;
        for (const std::string& f : darkFiles) in.push_back({"dark", f});
        for (const std::string& f : brightFiles) in.push_back({"bright", f});
        for (const std::string& f : evalFiles) in.push_back({"eval", f});
        for (const std::string& f : exclFiles) in.push_back({"exclude_mask", f});
        for (size_t i = 0; i < in.size(); ++i)
            j += "    {\"role\": \"" + in[i].first + "\", \"file\": \"" + baseName(in[i].second) + "\", \"sha256\": \"" + sha256Hex(in[i].second) + "\"}" + (i + 1 < in.size() ? ",\n" : "\n");
        j += "  ]\n}\n";
        if (!writeBytes((fs::path(out) / "truth.json").string(), j.data(), j.size())) return 3;
    }
    std::printf("planted %zu items (%zu pixels with an item) in %zu dark, %zu bright and %zu evaluation frames; sigma_t %.4f; quantum gain %.4f (%s); truth in %s\n", items.size(),
                static_cast<size_t>(std::count_if(ids.begin(), ids.end(), [](uint16_t v) { return v != 0; })), darkFiles.size(), brightFiles.size(), evalFiles.size(), sigmaT, kappa, qg.source.c_str(), out.c_str());
    return 0;
}

// ------------------------------------------------------------------------------------------------------------- truth reading
struct Truth {
    uint32_t          w{0}, h{0};
    double            sigmaT{0};
    std::vector<Item> items;
    std::vector<uint16_t> ids;
    std::vector<uint8_t>  truthAny;   // any item at the pixel (shared included)
    std::vector<uint8_t>  truthReal;  // a planted TRUE defect at the pixel (control bands are not defects)
};

bool loadTruth(const std::string& dir, Truth* t) {
    std::ifstream f((fs::path(dir) / "truth.tsv").string());
    if (!f) {
        std::fprintf(stderr, "%s: no truth.tsv\n", dir.c_str());
        return false;
    }
    std::string line;
    size_t expect = 0;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c;
        size_t s = 0;
        for (size_t p = 0; p <= line.size(); ++p)
            if (p == line.size() || line[p] == '\t') { c.push_back(line.substr(s, p - s)); s = p + 1; }
        if (c[0] == "width") t->w = static_cast<uint32_t>(std::strtoul(c[1].c_str(), nullptr, 10));
        else if (c[0] == "height") t->h = static_cast<uint32_t>(std::strtoul(c[1].c_str(), nullptr, 10));
        else if (c[0] == "sigma_t") t->sigmaT = std::strtod(c[1].c_str(), nullptr);
        else if (c[0] == "items") expect = static_cast<size_t>(std::strtoull(c[1].c_str(), nullptr, 10));
        else if (c[0] == "seed") continue;
        else if (c.size() >= 6) {
            Item it;
            it.id = std::atoi(c[0].c_str());
            it.cls = c[1];
            it.cls0 = c[1];
            it.kind = c[2];
            it.shape = c[3];
            it.level = std::strtod(c[4].c_str(), nullptr);
            size_t b = 0;
            const std::string& r = c[5];
            while (b < r.size()) {
                size_t e = r.find(';', b);
                if (e == std::string::npos) e = r.size();
                int v[4] = {0, 0, 0, 0};
                size_t from = b;
                for (int part = 0; part < 4; ++part) {
                    size_t comma = r.find(',', from);
                    if (comma == std::string::npos || comma > e) comma = e;
                    if (comma <= from && part < 3) return false;
                    v[part] = std::atoi(r.substr(from, comma - from).c_str());
                    from = comma + 1;
                }
                it.rects.push_back(Rect{v[0], v[1], v[2], v[3]});
                b = e + 1;
            }
            t->items.push_back(it);
        }
    }
    if (t->w == 0 || t->h == 0 || t->items.size() != expect) {
        std::fprintf(stderr, "%s: truth.tsv is incomplete\n", dir.c_str());
        return false;
    }
    const size_t n = static_cast<size_t>(t->w) * t->h;
    std::vector<uint8_t> bytes;
    if (!readAllBytes((fs::path(dir) / "truth_ids.u16").string(), &bytes) || bytes.size() != 2 * n) {
        std::fprintf(stderr, "%s: truth_ids.u16 missing or wrong size\n", dir.c_str());
        return false;
    }
    t->ids.resize(n);
    std::memcpy(t->ids.data(), bytes.data(), 2 * n);
    t->truthAny.assign(n, 0);
    for (size_t i = 0; i < n; ++i) t->truthAny[i] = t->ids[i] != 0 ? 1 : 0;
    t->truthReal.assign(n, 0);
    for (const Item& it : t->items) {
        if (it.cls == "control") continue;
        forItemPixels(it, t->w, [&](size_t i) { t->truthReal[i] = 1; });
    }
    return true;
}

// ------------------------------------------------------------------------------------------------------------- noise axis (QA-A-252f)
struct NoiseRecord {
    std::string label;   // defect, normal, undetermined
    double      cost{0}, lo{0}, hi{0};
    size_t      used{0};   // QA-A-255: the pixels the label was decided on
};

/** The file written by noise-cost: id, kind, shape, level, pixels, dof, cost, lo, hi, label (tab separated, one header line). */
bool loadNoiseTruth(const std::string& path, std::map<int, NoiseRecord>* out) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "%s: cannot read the noise truth\n", path.c_str());
        return false;
    }
    std::string line;
    bool        first = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (first) {
            first = false;
            continue;
        }
        std::vector<std::string> c;
        size_t s0 = 0;
        for (size_t q = 0; q <= line.size(); ++q)
            if (q == line.size() || line[q] == '\t') { c.push_back(line.substr(s0, q - s0)); s0 = q + 1; }
        if (c.size() < 10) return false;
        NoiseRecord r;
        r.cost = std::strtod(c[6].c_str(), nullptr);
        r.lo = std::strtod(c[7].c_str(), nullptr);
        r.hi = std::strtod(c[8].c_str(), nullptr);
        r.label = c[9];
        r.used = static_cast<size_t>(std::strtoull(c[4].c_str(), nullptr, 10));
        (*out)[std::atoi(c[0].c_str())] = r;
    }
    return true;
}

/** A measured overlap (the rows of noise-cost --overlap-out): label of the noise cost, the chain response with the real offset and dose, clipping. */
struct OverlapMeasure {
    std::string label;
    double      cost{0}, response{1.0};
    bool        clipped{false};
    size_t      used{0};   // QA-A-255: the pixels the noise label was decided on
    size_t      responseUsed{0};      // the pixels the chain response was computed on
    bool        responseValid{false}; // a measured response exists (responseUsed > 0)
    std::string responseUnused;       // why not, when it does not
    std::string costUnused;           // why the noise cost was not used, when used == 0
};

/** The rows of noise-cost --overlap-out that describe one distinct overlap: items (a+b+c), ..., label, response_model, clipped. The pooled rows (items '*') are for the report only. */
bool loadOverlapMeasure(const std::string& path, std::map<std::string, OverlapMeasure>* out) {
    std::ifstream f(path);
    if (!f) {
        std::fprintf(stderr, "%s: cannot read the overlap measurement\n", path.c_str());
        return false;
    }
    std::string line;
    bool        first = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (first) {
            first = false;
            continue;
        }
        std::vector<std::string> c;
        size_t s0 = 0;
        for (size_t q = 0; q <= line.size(); ++q)
            if (q == line.size() || line[q] == '\t') { c.push_back(line.substr(s0, q - s0)); s0 = q + 1; }
        if (c.size() < 18) {
            std::fprintf(stderr, "%s: the overlap measurement has %zu columns; it lacks response_valid and the unused reasons (noise-cost of QA-A-255 writes 18): regenerate it\n", path.c_str(), c.size());
            return false;
        }
        if (c[0] == "*") continue;
        OverlapMeasure m;
        m.responseUsed = static_cast<size_t>(std::strtoull(c[13].c_str(), nullptr, 10));
        m.responseValid = c[14] == "1";
        m.responseUnused = c[15];
        m.costUnused = c[16];
        m.cost = std::strtod(c[5].c_str(), nullptr);
        m.used = static_cast<size_t>(std::strtoull(c[3].c_str(), nullptr, 10));
        m.label = c[8];
        m.response = std::strtod(c[9].c_str(), nullptr);
        m.clipped = c[10] == "1";
        (*out)[c[0]] = m;
    }
    return true;
}

std::string defectClassOfShape(const std::string& shape) { return shape == "pixel" ? "pixel" : (shape.compare(0, 5, "block") == 0 ? "block" : "line"); }

enum Verdict : uint8_t { kVerdictNormal = 1, kVerdictDefect = 2, kVerdictUndetermined = 3 };

/** The response axis of a ground-truth record: a value, whether it is a valid measurement or definition, where it comes from and, when it is not valid, why it is not used. */
struct ResponseAxis {
    double      value{1.0};
    bool        valid{false};
    bool        defect{false};   ///< meaningful only when valid
    std::string source;
    std::string unused;          ///< the reason when !valid
};

/** The noise axis: whether the item or overlap was measured at all, the label, whether the label is a decision, the pixels that decided it. */
struct NoiseAxis {
    bool        applicable{false};
    bool        valid{false};    ///< the label is defect or normal (not undetermined, not missing)
    std::string label;
    size_t      usedPixels{0};
};

/**
 * THE rule (QA-A-255): the only place where the two axes become a class. Defect when a valid response says defect or a valid noise label says defect; undetermined when no axis says defect and
 * the response is not valid or the noise axis (when it applies) is not a decision; normal when everything that applies says normal.
 */
Verdict finalVerdict(const ResponseAxis& r, const NoiseAxis& n) {
    if ((r.valid && r.defect) || (n.applicable && n.valid && n.label == "defect")) return kVerdictDefect;
    if (!r.valid || (n.applicable && !(n.valid && n.label == "normal"))) return kVerdictUndetermined;
    return kVerdictNormal;
}

/**
 * The two axes for the ITEMS (QA-A-252f): the response axis is the injection class of truth.tsv (a response below 45 % or above 150 % is a defect), the noise axis is the measured verdict.
 * Both go through finalVerdict. Returns the numbers of items promoted to defect by the noise axis and made undetermined.
 */
void applyNoiseAxis(Truth* t, const std::map<int, NoiseRecord>& noise, int* promoted, int* undetermined, std::vector<uint8_t>* undetMask) {
    *promoted = 0;
    *undetermined = 0;
    const size_t n = static_cast<size_t>(t->w) * t->h;
    undetMask->assign(n, 0);
    for (Item& it : t->items) {
        const auto f = noise.find(it.id);
        if (f == noise.end()) continue;
        ResponseAxis r;
        r.valid = true;
        r.defect = it.cls0 != "control";
        r.source = "injection class";
        NoiseAxis nz;
        nz.applicable = true;
        nz.label = f->second.label;
        nz.usedPixels = f->second.used;
        nz.valid = f->second.used > 0 && (f->second.label == "defect" || f->second.label == "normal");
        const Verdict v = finalVerdict(r, nz);
        if (it.cls0 != "control") continue;   // the response axis already says defect: nothing to promote
        if (v == kVerdictDefect) {
            it.cls = defectClassOfShape(it.shape);
            ++*promoted;
        } else if (v == kVerdictUndetermined) {
            it.cls = "undetermined";
            ++*undetermined;
        }
    }
    t->truthReal.assign(n, 0);
    for (const Item& it : t->items) {
        if (it.cls == "control") continue;
        if (it.cls == "undetermined") {
            forItemPixels(it, t->w, [&](size_t i) { (*undetMask)[i] = 1; });
            continue;
        }
        forItemPixels(it, t->w, [&](size_t i) { t->truthReal[i] = 1; });
    }
}

/** One distinct overlap: the set of items that cover a pixel (in injection order, at most kMaxOverlapItems) with its ground-truth record. */
constexpr size_t kMaxOverlapItems = 8;

struct OverlapGroup {
    std::vector<int> ids;
    size_t           pixels{0};
    ResponseAxis     response;
    NoiseAxis        noise;
    bool             clipped{false}, unmeasured{false}, intrinsic{false};
    uint8_t          inherited{0};   ///< DIAGNOSTIC: the class the earlier rule gave (the classes of the covering items inherited): 1 normal, 2 defect, 3 undetermined
    uint8_t          after{0};       ///< the class of finalVerdict
    size_t           flagged{0};
    size_t           firstPixel{0};
};

std::string overlapKey(const std::vector<int>& ids) {
    std::string k;
    for (size_t j = 0; j < ids.size(); ++j) k += (j ? "+" : "") + std::to_string(ids[j]);
    return k;
}

struct OverlapReport {
    size_t pixels{0}, defect{0}, undetermined{0}, normal{0}, notInherited{0};
    size_t inheritedDefect{0}, inheritedUndetermined{0}, inheritedNormal{0};
    size_t noiseDefectPx{0}, noiseUndetPx{0}, unmeasuredPx{0}, clippedPx{0}, intrinsicPx{0};
    size_t transition[4][4]{};        // [inherited][after]
    std::vector<OverlapGroup> groups;
};

/**
 * The items that cover each pixel covered twice or more, in the order of the item list (the injection order). Refuses (returns false, sets @p err) a pixel covered by more than kMaxOverlapItems.
 */
bool collectOverlaps(const Truth& t, std::map<size_t, std::vector<int>>* multi, std::string* err) {
    const size_t           n = static_cast<size_t>(t.w) * t.h;
    std::vector<uint8_t>   count(n, 0);
    for (const Item& it : t.items)
        forItemPixels(it, t.w, [&](size_t i) {
            if (count[i] < 255) ++count[i];
        });
    for (const Item& it : t.items)
        forItemPixels(it, t.w, [&](size_t i) {
            if (count[i] >= 2) (*multi)[i].push_back(it.id);
        });
    for (const auto& kv : *multi)
        if (kv.second.size() > kMaxOverlapItems) {
            if (err) *err = "pixel (" + std::to_string(kv.first % t.w) + "," + std::to_string(kv.first / t.w) + ") is covered by " + std::to_string(kv.second.size()) + " items; at most " +
                            std::to_string(kMaxOverlapItems) + " are handled";
            return false;
        }
    return true;
}

/**
 * The response a pixel covered by @p chain (the items in injection order) shows: the injection applies each item to the frame value, rounds to an integer and clips to 0..65535; dark frames
 * receive the additive items only. Computed without noise from the dark level @p d and the net bright signal @p s (the nominal pixel: 1000 and 10000; the measured one: the pixel's own dark
 * level and the local net signal of the verification frames). @p clipped is set when a value left 0..65535 before it was clipped; the response is read as net(bright) / s.
 */
double chainResponseAt(const std::vector<const Item*>& chain, double d, double s, bool* clipped) {
    double vb = d + s, vd = d;
    if (clipped) *clipped = false;
    auto fix = [&](double v) {
        const double r = std::floor(v + 0.5);
        if (clipped && (r < 0.0 || r > 65535.0)) *clipped = true;
        return std::min(65535.0, std::max(0.0, r));
    };
    for (const Item* it : chain) {
        const std::string& k = it->kind;
        if (k == "dead") {
            vb = d + (1.0 - it->level) * (vb - d);
        } else if (k == "hot" || k == "fixed_band") {
            vb += it->level;
            vd += it->level;
        } else if (k == "slope" || k == "gain_band" || k == "gain_loss" || k == "efficiency_loss") {
            vb = d + it->level * (vb - d);
        } else if (k == "nonlinear") {
            const double nn = vb - d;
            vb = d + nn * (1.0 - it->level * nn / kNonlinearRef);
        }   // noisy, varying_band: noise terms, left to the noise axis
        vb = fix(vb);
        vd = fix(vd);
    }
    return (vb - vd) / s;
}

double chainResponse(const std::vector<const Item*>& chain) { return chainResponseAt(chain, 1000.0, 10000.0, nullptr); }

/**
 * Composite truth of the overlapped pixels (QA-A-254, the decision on QA-A-252h, rewritten against the real branches after Codex #216): see the header of the patch that made it. `overlapClass`
 * receives per pixel 0 (not overlapped), 1 normal, 2 defect, 3 undetermined; `overlapGroup` the index of the distinct overlap in rep->groups (valid where the class is not 0). The truth masks
 * are updated: a composite defect is a true defect (truthReal), an undetermined pixel joins the undetermined zone.
 */
bool applyOverlapComposite(Truth* t, std::vector<uint8_t>* undetMask, std::vector<uint8_t>* overlapClass, std::vector<uint32_t>* overlapGroup, OverlapReport* rep,
                           const std::map<std::string, OverlapMeasure>* measured = nullptr) {
    const size_t n = static_cast<size_t>(t->w) * t->h;
    std::map<size_t, std::vector<int>> multi;
    std::string                        err;
    if (!collectOverlaps(*t, &multi, &err)) {
        std::fprintf(stderr, "overlap composite: %s\n", err.c_str());
        return false;
    }
    std::map<int, const Item*> byId;
    for (const Item& it : t->items) byId[it.id] = &it;
    overlapClass->assign(n, 0);
    overlapGroup->assign(n, 0);
    *rep = OverlapReport{};
    std::map<std::string, uint32_t> index;
    for (const auto& kv : multi) {
        const size_t i = kv.first;
        const std::vector<int>& ids = kv.second;
        const std::string key = overlapKey(ids);
        auto f = index.find(key);
        if (f == index.end()) {
            OverlapGroup g;
            g.ids = ids;
            g.firstPixel = i;
            bool inheritedDefect = false, inheritedUndet = false;
            std::vector<const Item*> chain;
            for (int id : ids) {
                const Item* it = byId[id];
                chain.push_back(it);
                if (it->cls == "undetermined") inheritedUndet = true;
                else if (it->cls != "control") inheritedDefect = true;
                if (it->cls0 != "control" && (it->kind == "hot" || it->kind == "noisy" || it->kind == "nonlinear")) g.intrinsic = true;
            }
            g.inherited = inheritedDefect ? 2 : (inheritedUndet ? 3 : 1);
            // the record of the overlap: its OWN measurement, nothing of the covering items' single verdicts
            g.noise.applicable = true;
            g.noise.label = "none";
            const OverlapMeasure* m = nullptr;
            if (measured) {
                const auto mf = measured->find(key);
                if (mf != measured->end()) m = &mf->second;
            }
            if (!measured) {
                g.response.unused = "no overlap measurement given (--overlap-truth)";
                g.unmeasured = true;
            } else if (!m) {
                g.response.unused = "this overlap is not in the overlap measurement";
                g.unmeasured = true;
            } else {
                g.response.value = m->response;
                g.response.source = "measured chain (real dark level, local signal of the verification frames)";
                g.clipped = m->clipped;
                if (!m->responseValid) {
                    g.response.unused = m->responseUnused.empty() || m->responseUnused == "-" ? "the measurement holds no valid response" : m->responseUnused;
                } else if (m->clipped) {
                    g.response.unused = "the chain leaves 0..65535";
                } else {
                    g.response.valid = true;
                    g.response.defect = m->response < kResponseDefectBelow - kRatioSlack || m->response > kResponseDefectAbove + kRatioSlack;
                }
                g.noise.label = m->label;
                g.noise.usedPixels = m->used;
                g.noise.valid = m->used > 0 && (m->label == "defect" || m->label == "normal");
            }
            if (g.intrinsic) {   // hot, noisy and nonlinear items are defects by the injection, not by a level: the response axis, source named
                g.response.valid = true;
                g.response.defect = true;
                g.response.source += (g.response.source.empty() ? "" : " + ") + std::string("injection class of a hot/noisy/nonlinear item");
                g.response.unused.clear();
            }
            g.after = finalVerdict(g.response, g.noise);
            f = index.emplace(key, static_cast<uint32_t>(rep->groups.size())).first;
            rep->groups.push_back(g);
        }
        OverlapGroup& g = rep->groups[f->second];
        if (g.noise.valid && g.noise.label == "defect") ++rep->noiseDefectPx;
        if (!g.noise.valid) ++rep->noiseUndetPx;
        if (g.unmeasured) ++rep->unmeasuredPx;
        if (g.clipped) ++rep->clippedPx;
        if (g.intrinsic) ++rep->intrinsicPx;
        ++g.pixels;
        (*overlapGroup)[i] = f->second;
        (*overlapClass)[i] = g.after;
        ++rep->pixels;
        ++rep->transition[g.inherited][g.after];
        if (g.inherited == 2) ++rep->inheritedDefect; else if (g.inherited == 3) ++rep->inheritedUndetermined; else ++rep->inheritedNormal;
        // the masks at an overlap are the FINAL class, assigned (not added to what the covering items left there: a defect item's pixels were all set in truthReal, an undetermined item's in undetMask)
        t->truthReal[i] = g.after == kVerdictDefect ? 1 : 0;
        (*undetMask)[i] = g.after == kVerdictUndetermined ? 1 : 0;
        if (g.after == kVerdictDefect) {
            ++rep->defect;
            if (g.inherited != 2) ++rep->notInherited;
        } else if (g.after == kVerdictUndetermined) {
            ++rep->undetermined;
        } else {
            ++rep->normal;
        }
    }
    return true;
}

// ------------------------------------------------------------------------------------------------------------- score
struct GroupStat {
    std::string key;
    int         items{0}, detected{0}, allFlagged{0};
    double      recallSum{0};
};

using xpe_bpm_eval::medianOf;
using xpe_bpm_eval::percentileOf;

int cmdScore(const Args& a) {
    Truth t;
    if (!a.has("truth") || !a.has("map") || !loadTruth(a.str("truth"), &t)) {
        usage();
        return 2;
    }
    const size_t n = static_cast<size_t>(t.w) * t.h;
    std::vector<uint8_t> undetMask(n, 0);
    int                  noisePromoted = 0, noiseUndetermined = 0;
    const bool           haveNoise = a.has("noise-truth");
    if (haveNoise) {
        std::map<int, NoiseRecord> noise;
        if (!loadNoiseTruth(a.str("noise-truth"), &noise)) return 3;
        applyNoiseAxis(&t, noise, &noisePromoted, &noiseUndetermined, &undetMask);
        std::printf("== noise axis (%s, PROVISIONAL measured labels): %zu items measured; %d response-normal items are defects by the noise axis; %d are undetermined (counted as neither a hit nor a false alarm)\n",
                    a.str("noise-truth").c_str(), noise.size(), noisePromoted, noiseUndetermined);
    }
    std::vector<uint8_t>  overlapClass;
    std::vector<uint32_t> overlapGroup;
    OverlapReport         overlap;
    const bool            compositeOverlap = a.str("overlap") == "composite";
    std::map<std::string, OverlapMeasure> overlapMeasured;
    const bool                            haveOverlapTruth = compositeOverlap && a.has("overlap-truth");
    if (haveOverlapTruth && !loadOverlapMeasure(a.str("overlap-truth"), &overlapMeasured)) return 3;
    if (compositeOverlap && !applyOverlapComposite(&t, &undetMask, &overlapClass, &overlapGroup, &overlap, haveOverlapTruth ? &overlapMeasured : nullptr)) return 3;
    std::vector<uint8_t> map, clean;
    if (!readMap(a.str("map"), n, &map)) return 3;
    const bool haveClean = a.has("clean-map");
    if (haveClean && !readMap(a.str("clean-map"), n, &clean)) return 3;
    std::vector<uint8_t> area;
    size_t               outOfArea = 0;
    if (a.has("area")) {
        if (!readAllBytes(a.str("area"), &area) || area.size() != n) {
            std::fprintf(stderr, "%s: not an imaging-area mask of %zu pixels\n", a.str("area").c_str(), n);
            return 3;
        }
        for (size_t i = 0; i < n; ++i) outOfArea += area[i] == 0;
    }
    auto applyArea = [&](std::vector<uint8_t>* m) {
        if (area.empty()) return;
        for (size_t i = 0; i < n; ++i)
            if (area[i] == 0) (*m)[i] = 0;
    };
    applyArea(&map);
    if (haveClean) applyArea(&clean);
    if (!area.empty()) std::printf("imaging area mask: %zu pixels outside; flags there are not counted\n", outOfArea);

    // exclusive pixel counts and flagged counts per item
    std::vector<size_t> total(t.items.size() + 2, 0), flagged(t.items.size() + 2, 0), flaggedClean(t.items.size() + 2, 0);
    for (size_t i = 0; i < n; ++i) {
        const uint16_t id = t.ids[i];
        if (id == 0 || id == kSharedId) continue;
        ++total[id];
        if (map[i]) ++flagged[id];
        if (haveClean && clean[i]) ++flaggedClean[id];
    }
    std::vector<GroupStat> groups;
    std::map<std::string, size_t> gIndex;
    int controlItems = 0, controlFalse = 0;
    std::string controlRows;
    struct CtrlGroup {
        int    items{0}, falseHits{0};
        double fracSum{0}, cleanSum{0};
    };
    std::map<std::string, CtrlGroup> ctrlGroups;   // pixel and block controls (hundreds of items): one row per kind and level
    std::map<std::string, CtrlGroup> undetGroups;  // items whose noise verdict is undetermined: one row per class, kind, shape and level (hits are not false detections)
    for (const Item& it : t.items) {
        const size_t tot = total[static_cast<size_t>(it.id)];
        const double frac = tot ? static_cast<double>(flagged[static_cast<size_t>(it.id)]) / static_cast<double>(tot) : 0.0;
        if (it.cls == "undetermined") {
            CtrlGroup& ug = undetGroups[groupKey(it)];
            ++ug.items;
            ug.falseHits += frac >= 0.5 ? 1 : 0;   // here: items with at least half of the pixels flagged (a count, not an error)
            ug.fracSum += frac;
            continue;
        }
        if (it.cls == "control") {
            ++controlItems;
            const bool hit = frac >= 0.5;
            controlFalse += hit ? 1 : 0;
            const double fc = tot ? static_cast<double>(flaggedClean[static_cast<size_t>(it.id)]) / static_cast<double>(tot) : 0.0;
            if (it.shape == "pixel" || it.shape.compare(0, 5, "block") == 0) {
                CtrlGroup& cg = ctrlGroups[groupKey(it)];
                ++cg.items;
                cg.falseHits += hit ? 1 : 0;
                cg.fracSum += frac;
                cg.cleanSum += fc;
                continue;
            }
            char b[200];
            std::snprintf(b, sizeof b, "  %-14s %-9s level %-5s flagged %6zu of %6zu = %.4f   clean map %.4f   %s\n", it.kind.c_str(), it.shape.c_str(), fmtLevel(it.level).c_str(),
                          flagged[static_cast<size_t>(it.id)], tot, frac, fc, hit ? "FALSE DETECTION" : "ok");
            controlRows += b;
            continue;
        }
        const std::string key = groupKey(it);
        if (!gIndex.count(key)) { gIndex[key] = groups.size(); groups.push_back({key}); }
        GroupStat& g = groups[gIndex[key]];
        ++g.items;
        g.recallSum += frac;
        if (frac >= 0.5) ++g.detected;
        if (tot && flagged[static_cast<size_t>(it.id)] == tot) ++g.allFlagged;
    }

    std::printf("== detection (map %s)\n", a.str("map").c_str());
    std::printf("  %-34s %6s %9s %9s %12s %14s\n", "class/kind/shape/level", "items", "detected", "rate", "all pixels", "mean recall");
    for (const GroupStat& g : groups)
        std::printf("  %-34s %6d %9d %9.4f %12d %14.4f\n", g.key.c_str(), g.items, g.detected, g.items ? static_cast<double>(g.detected) / g.items : 0.0, g.allFlagged,
                    g.items ? g.recallSum / g.items : 0.0);

    // false alarms (QA-A-251b): the zone left out is the planted TRUE defects widened by 2 pixels. The control bands are not defects: a flag on a band that the
    // clean map did not have is a new false alarm, however much of the band is flagged.
    std::vector<uint8_t> zone = t.truthReal;   // the zone left out: true defects and the items whose verdict is undetermined (a flag there is neither a hit nor a false alarm)
    for (size_t i = 0; i < n; ++i)
        if (undetMask[i]) zone[i] = 1;
    const std::vector<uint8_t> wide = dilate(zone, t.w, t.h, 2);
    std::vector<uint8_t> controlId(t.items.size() + 2, 0);
    for (const Item& it : t.items) controlId[static_cast<size_t>(it.id)] = it.cls == "control" ? 1 : 0;
    size_t nOut = 0, nOutClean = 0, nNew = 0, nNewControl = 0, nFlag = 0, nClean = 0;
    for (size_t i = 0; i < n; ++i) {
        nFlag += map[i];
        if (haveClean) nClean += clean[i];
        if (wide[i]) continue;
        nOut += map[i];
        if (haveClean) {
            nOutClean += clean[i];
            if (map[i] && !clean[i]) {
                ++nNew;
                const uint16_t id = t.ids[i];
                if (id != 0 && id != kSharedId && controlId[id]) ++nNewControl;
            }
        }
    }
    std::printf("== false alarms\n  flagged pixels in the map: %zu; outside the true defects (widened by 2): %zu\n", nFlag, nOut);
    size_t ovFlag[4] = {0, 0, 0, 0}, ovNew[4] = {0, 0, 0, 0}, ovFlagBefore[4] = {0, 0, 0, 0};
    size_t ovTruthReal = 0, ovUndetZone = 0;
    if (compositeOverlap) {
        for (size_t i = 0; i < n; ++i)
            if (overlapClass[i] != 0 && map[i]) {
                ++ovFlag[overlapClass[i]];
                ++overlap.groups[overlapGroup[i]].flagged;
                if (haveClean && !clean[i]) ++ovNew[overlapClass[i]];
            }
        for (const OverlapGroup& g : overlap.groups) ovFlagBefore[g.inherited] += g.flagged;
        for (size_t i = 0; i < n; ++i)
            if (overlapClass[i] != 0) {
                ovTruthReal += t.truthReal[i];
                ovUndetZone += undetMask[i];
            }
        std::printf("== overlapped pixels (covered by two or more planted items: %zu distinct overlaps; ONE rule for every one of them)\n"
                    "  rule (one function for items and overlaps): defect when a valid response (the overlap's own measured chain, or a hot/noisy/nonlinear item by injection) or a valid noise label says defect; undetermined when no axis says defect and the response is not valid or the noise label is not a decision; else normal. The classes of the covering items are NOT inherited\n"
                    "  pixels %zu: defect %zu (of them %zu not defect by the inheriting rule), undetermined %zu, normal %zu\n"
                    "  class by the INHERITING rule (diagnostic): defect %zu, undetermined %zu, normal %zu; change of class (inherited -> now): normal->defect %zu, undetermined->defect %zu, other %zu\n"
                    "  performance on the crossings alone: flagged in defect %zu of %zu (recall %.4f), in undetermined %zu of %zu, in normal %zu of %zu (false alarms %.4f; new against the clean map: %zu)\n"
                    "  by the earlier rule the same flags would read: in defect %zu, in undetermined %zu, in normal %zu\n",
                    overlap.groups.size(), overlap.pixels, overlap.defect, overlap.notInherited, overlap.undetermined, overlap.normal, overlap.inheritedDefect, overlap.inheritedUndetermined,
                    overlap.inheritedNormal, overlap.transition[1][2], overlap.transition[3][2], overlap.pixels - overlap.transition[1][2] - overlap.transition[3][2] - overlap.transition[2][2] - overlap.transition[3][3] - overlap.transition[1][1],
                    ovFlag[2], overlap.defect, overlap.defect ? static_cast<double>(ovFlag[2]) / static_cast<double>(overlap.defect) : 0.0, ovFlag[3], overlap.undetermined, ovFlag[1], overlap.normal,
                    overlap.normal ? static_cast<double>(ovFlag[1]) / static_cast<double>(overlap.normal) : 0.0, ovNew[1], ovFlagBefore[2], ovFlagBefore[3], ovFlagBefore[1]);
        std::printf("  masks at the overlaps (assigned from the final class): true-defect mask %zu pixels, undetermined zone %zu pixels\n", ovTruthReal, ovUndetZone);
        std::printf("  response: %zu pixels have no valid measured response (not measured %zu, chain outside 0..65535 %zu; these are not judged by the nominal chain); %zu pixels are defect by the injection class of a hot/noisy/nonlinear item\n"
                    "  noise: pixels whose overlap noise label is defect %zu, not a decision (undetermined or missing) %zu%s\n",
                    overlap.unmeasuredPx + overlap.clippedPx, overlap.unmeasuredPx, overlap.clippedPx, overlap.intrinsicPx, overlap.noiseDefectPx, overlap.noiseUndetPx,
                    haveOverlapTruth ? (" (measurement: " + a.str("overlap-truth") + ", " + std::to_string(overlapMeasured.size()) + " groups read)").c_str() : " (no measurement given)");
        if (a.has("overlap-out")) {
            std::string tsv = "items\tkinds\tpixels\tresponse\tresponse_valid\tresponse_source\tresponse_unused\tnoise_label\tnoise_valid\tnoise_used\tinherited\tfinal\tflagged\tfirst_pixel_x\tfirst_pixel_y\n";
            static const char* const cn[4] = {"-", "normal", "defect", "undetermined"};
            for (const OverlapGroup& g : overlap.groups) {
                std::string kinds;
                for (size_t k = 0; k < g.ids.size(); ++k)
                    for (const Item& it : t.items)
                        if (it.id == g.ids[k]) kinds += (k ? " + " : "") + it.cls + "/" + it.kind + "/" + it.shape + "/" + fmtLevel(it.level);
                char b[1800];
                std::snprintf(b, sizeof b, "%s\t%s\t%zu\t%.4f\t%d\t%s\t%s\t%s\t%d\t%zu\t%s\t%s\t%zu\t%zu\t%zu\n", overlapKey(g.ids).c_str(), kinds.c_str(), g.pixels, g.response.value, g.response.valid ? 1 : 0,
                              g.response.source.empty() ? "-" : g.response.source.c_str(), g.response.unused.empty() ? "-" : g.response.unused.c_str(), g.noise.label.c_str(), g.noise.valid ? 1 : 0, g.noise.usedPixels,
                              cn[g.inherited], cn[g.after], g.flagged, g.firstPixel % t.w, g.firstPixel / t.w);
                tsv += b;
            }
            if (!writeBytes(a.str("overlap-out"), tsv.data(), tsv.size())) return 3;
        }
    }
    if (haveClean)
        std::printf("  clean map (no planting): flagged %zu; outside the true defects: %zu\n  NEW false alarms (flagged here, not in the clean map, outside the true defects): %zu = %.3f per million pixels\n"
                    "    of which on control bands: %zu\n", nClean, nOutClean, nNew, 1e6 * static_cast<double>(nNew) / static_cast<double>(n), nNewControl);
    for (const auto& kv : ctrlGroups) {
        char b[260];
        std::snprintf(b, sizeof b, "  %-34s %5d items  false detections %5d  mean flagged fraction %.4f   clean map %.4f\n", kv.first.c_str(), kv.second.items, kv.second.falseHits,
                      kv.second.fracSum / kv.second.items, kv.second.cleanSum / kv.second.items);
        controlRows += b;
    }
    std::printf("== control bands (%d items; false detection = at least half of the band flagged)\n%s  false detections: %d of %d\n", controlItems, controlRows.c_str(), controlFalse,
                controlItems);
    size_t undetItems = 0;
    for (const auto& kv : undetGroups) undetItems += static_cast<size_t>(kv.second.items);
    if (haveNoise) {
        std::printf("== undetermined items (%zu; counted neither as detections nor as false detections)\n", undetItems);
        for (const auto& kv : undetGroups)
            std::printf("  %-44s %5d items  with at least half flagged %5d  mean flagged fraction %.4f\n", kv.first.c_str(), kv.second.items, kv.second.falseHits, kv.second.fracSum / kv.second.items);
    }

    double jAll = -1, jIn = -1, jOut = -1;
    if (a.has("half-a") && a.has("half-b")) {
        std::vector<uint8_t> ha, hb;
        if (!readMap(a.str("half-a"), n, &ha) || !readMap(a.str("half-b"), n, &hb)) return 3;
        applyArea(&ha);
        applyArea(&hb);
        size_t iAll = 0, uAll = 0, iIn = 0, uIn = 0, iOut = 0, uOut = 0, cA = 0, cB = 0;
        for (size_t i = 0; i < n; ++i) {
            cA += ha[i];
            cB += hb[i];
            const bool both = ha[i] && hb[i], either = ha[i] || hb[i];
            iAll += both;
            uAll += either;
            if (t.truthAny[i]) { iIn += both; uIn += either; }
            else { iOut += both; uOut += either; }
        }
        auto ratio = [](size_t i, size_t u) { return u ? static_cast<double>(i) / static_cast<double>(u) : 1.0; };
        jAll = ratio(iAll, uAll);
        jIn = ratio(iIn, uIn);
        jOut = ratio(iOut, uOut);
        std::printf("== split-half agreement (Jaccard)\n  half A flags %zu, half B flags %zu\n  all pixels %.4f (%zu / %zu); inside the truth %.4f (%zu / %zu); outside the truth %.4f (%zu / %zu)\n", cA, cB, jAll,
                    iAll, uAll, jIn, iIn, uIn, jOut, iOut, uOut);
    }

    if (a.has("out")) {
        std::string j = "{\n  \"map\": \"" + baseName(a.str("map")) + "\",\n  \"detection\": [\n";
        for (size_t i = 0; i < groups.size(); ++i) {
            char b[300];
            std::snprintf(b, sizeof b, "    {\"group\": \"%s\", \"items\": %d, \"detected\": %d, \"all_pixels_flagged\": %d, \"mean_recall\": %.6f}%s\n", groups[i].key.c_str(), groups[i].items,
                          groups[i].detected, groups[i].allFlagged, groups[i].items ? groups[i].recallSum / groups[i].items : 0.0, i + 1 < groups.size() ? "," : "");
            j += b;
        }
        char tail[700];
        std::snprintf(tail, sizeof tail,
                      "  ],\n  \"flagged\": %zu,\n  \"outside_truth\": %zu,\n  \"clean_flagged\": %zu,\n  \"clean_outside_truth\": %zu,\n  \"new_false_alarms\": %zu,\n  \"new_false_alarms_in_controls\": %zu,\n  \"control_items\": %d,\n"
                      "  \"control_false_detections\": %d,\n  \"jaccard_all\": %.6f,\n  \"jaccard_inside_truth\": %.6f,\n  \"jaccard_outside_truth\": %.6f",
                      nFlag, nOut, nClean, nOutClean, nNew, nNewControl, controlItems, controlFalse, jAll, jIn, jOut);
        j += tail;
        j += ",\n  \"control_groups\": [\n";   // pixel and block controls summed per kind and level (the line controls are in the console table)
        size_t cgn = 0;
        for (const auto& kv : ctrlGroups) {
            char b[300];
            std::snprintf(b, sizeof b, "    {\"group\": \"%s\", \"items\": %d, \"false_detections\": %d, \"mean_flagged_fraction\": %.6f}%s\n", kv.first.c_str(), kv.second.items, kv.second.falseHits,
                          kv.second.fracSum / kv.second.items, ++cgn < ctrlGroups.size() ? "," : "");
            j += b;
        }
        j += "  ],\n  \"undetermined_items\": " + std::to_string(undetItems) + ",\n  \"noise_axis_promoted_to_defect\": " + std::to_string(noisePromoted) + ",\n  \"undetermined_groups\": [\n";
        size_t ugn = 0;
        for (const auto& kv : undetGroups) {
            char b[300];
            std::snprintf(b, sizeof b, "    {\"group\": \"%s\", \"items\": %d, \"at_least_half_flagged\": %d, \"mean_flagged_fraction\": %.6f}%s\n", kv.first.c_str(), kv.second.items, kv.second.falseHits,
                          kv.second.fracSum / kv.second.items, ++ugn < undetGroups.size() ? "," : "");
            j += b;
        }
        j += "  ]";
        if (compositeOverlap) {
            char ob[420];
            std::snprintf(ob, sizeof ob,
                          ",\n  \"overlap_pixels\": %zu,\n  \"overlap_defect\": %zu,\n  \"overlap_defect_not_inherited\": %zu,\n  \"overlap_undetermined\": %zu,\n  \"overlap_normal\": %zu,\n"
                          "  \"overlap_flagged_in_defect\": %zu,\n  \"overlap_flagged_in_undetermined\": %zu,\n  \"overlap_flagged_in_normal\": %zu,\n  \"overlap_new_false_alarms\": %zu",
                          overlap.pixels, overlap.defect, overlap.notInherited, overlap.undetermined, overlap.normal, ovFlag[2], ovFlag[3], ovFlag[1], ovNew[1]);
            j += ob;
            {
                char tb[200];
                std::snprintf(tb, sizeof tb, ",\n  \"overlap_truth_real_pixels\": %zu,\n  \"overlap_undetermined_zone_pixels\": %zu", ovTruthReal, ovUndetZone);
                j += tb;
            }
            if (haveOverlapTruth) {
                char mb[300];
                std::snprintf(mb, sizeof mb, ",\n  \"overlap_noise_defect_pixels\": %zu,\n  \"overlap_noise_undetermined_pixels\": %zu,\n  \"overlap_unmeasured_pixels\": %zu,\n  \"overlap_clipped_pixels\": %zu",
                              overlap.noiseDefectPx, overlap.noiseUndetPx, overlap.unmeasuredPx, overlap.clippedPx);
                j += mb;
            }
        }
        j += "\n}\n";
        if (!writeBytes(a.str("out"), j.data(), j.size())) return 3;
    }
    return 0;
}

// ------------------------------------------------------------------------------------------------------------- noise cost
/**
 * noise-cost (QA-A-252f): see the header of this file. The verification frames are planted frames of ONE dose; the temporal deviation about their mean has the row and the
 * column mean of every frame taken out (as the detector does), nu = frames - 1. The expectation a pixel is compared with is the variance-signal curve measured on the unplanted
 * pixels of the same frames (equal-count bins of the mean net signal; the median variance of a bin divided by the median of chi-square / nu), read at the pixel's local signal
 * (the median of the unplanted pixels 4..12 away). That curve is a measurement, not the model formula kappa * signal + readout.
 */
int cmdNoiseCost(const Args& a) {
    Truth t;
    const std::vector<std::string> darkFiles = a.list("dark"), verFiles = a.list("verify");
    if (!a.has("truth") || !a.has("out") || darkFiles.size() < 3 || verFiles.size() < 2 || !loadTruth(a.str("truth"), &t)) {
        std::fprintf(stderr, "noise-cost needs --truth DIR, --out FILE, at least 3 --dark and at least 2 --verify frames (planted, of one dose)\n");
        return 2;
    }
    const uint32_t w = t.w, h = t.h;
    const size_t   n = static_cast<size_t>(w) * h;
    const double   limit = a.has("limit") ? std::strtod(a.str("limit").c_str(), nullptr) : 2.0;
    const double   nu = static_cast<double>(verFiles.size() - 1);
    std::vector<double> D(n, 0.0);
    for (const std::string& f : darkFiles) {
        std::vector<uint16_t> fr;
        if (!readRaw16(f, n, &fr)) return 3;
        for (size_t i = 0; i < n; ++i) D[i] += fr[i];
    }
    for (size_t i = 0; i < n; ++i) D[i] /= static_cast<double>(darkFiles.size());
    std::vector<std::vector<float>> net(verFiles.size());
    std::vector<float>              mean(n, 0.0f);
    for (size_t k = 0; k < verFiles.size(); ++k) {
        std::vector<uint16_t> fr;
        if (!readRaw16(verFiles[k], n, &fr)) return 3;
        net[k].resize(n);
        for (size_t i = 0; i < n; ++i) net[k][i] = static_cast<float>(fr[i] - D[i]);
    }
    std::vector<uint8_t> occupied(n, 0);
    for (size_t i = 0; i < n; ++i) occupied[i] = t.ids[i] != 0 ? 1 : 0;
    const std::vector<uint8_t> occ = dilate(occupied, w, h, 3);
    {   // exposure: one scale per frame from the global dose (median of the net signal over the unplanted interior pixels)
        std::vector<double> dose(verFiles.size(), 1.0);
        const uint32_t margin = static_cast<uint32_t>(a.num("sample-margin", 32));   // the frame border left out of the dose sample (QA-A-254: lets a test leave the sample empty)
        for (size_t k = 0; k < verFiles.size(); ++k) {
            std::vector<float> sample;
            for (uint32_t y = margin; y + margin < h; y += 5)
                for (uint32_t x = margin; x + margin < w; x += 5) {
                    const size_t i = static_cast<size_t>(y) * w + x;
                    if (!occ[i]) sample.push_back(net[k][i]);
                }
            if (sample.empty()) {   // QA-A-252h: checked before nth_element and before the scale divides by the dose
                std::fprintf(stderr, "noise-cost: no unplanted interior pixel to estimate the dose of verification frame %zu from\n", k);
                return 3;
            }
            std::nth_element(sample.begin(), sample.begin() + static_cast<std::ptrdiff_t>(sample.size() / 2), sample.end());
            dose[k] = static_cast<double>(sample[sample.size() / 2]);
            if (!(dose[k] > 0.0) || !std::isfinite(dose[k])) {
                std::fprintf(stderr, "noise-cost: the median net signal of verification frame %zu is %g (not positive): the dose is not defined\n", k, dose[k]);
                return 3;
            }
        }
        double ref = 0.0;
        for (double v : dose) ref += v / static_cast<double>(dose.size());
        const bool raw = a.has("raw-frames");
        std::printf("== exposure of the verification frames (global dose = median net signal of the unplanted pixels)%s\n", raw ? "; --raw-frames: NOT scaled" : "");
        for (size_t k = 0; k < verFiles.size(); ++k) {
            const double f = raw ? 1.0 : ref / dose[k];
            std::printf("  frame %zu dose %.2f scale %.5f\n", k, dose[k], f);
            for (size_t i = 0; i < n; ++i) net[k][i] = static_cast<float>(static_cast<double>(net[k][i]) * f);
        }
    }
    for (size_t k = 0; k < verFiles.size(); ++k)
        for (size_t i = 0; i < n; ++i) mean[i] += net[k][i] / static_cast<float>(verFiles.size());
    std::vector<float> s2(n, 0.0f);   // the unbiased temporal variance of every pixel, nu degrees of freedom
    {
        std::vector<double> rm(h), cm(w);
        for (size_t k = 0; k < verFiles.size(); ++k) {
            for (uint32_t y = 0; y < h; ++y) {
                double acc = 0.0;
                for (uint32_t x = 0; x < w; ++x) acc += net[k][static_cast<size_t>(y) * w + x] - mean[static_cast<size_t>(y) * w + x];
                rm[y] = acc / w;
            }
            std::fill(cm.begin(), cm.end(), 0.0);
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) cm[x] += net[k][static_cast<size_t>(y) * w + x] - mean[static_cast<size_t>(y) * w + x] - rm[y];
            for (uint32_t x = 0; x < w; ++x) cm[x] /= h;
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) {
                    const size_t i = static_cast<size_t>(y) * w + x;
                    const double e = net[k][i] - mean[i] - rm[y] - cm[x];
                    s2[i] += static_cast<float>(e * e / nu);
                }
        }
    }
    net.clear();
    net.shrink_to_fit();
    const double medFactor = xpe_bpm_eval::chiSquareMedianOverNu(nu);

    // the measured variance-signal curve of the unplanted pixels
    struct Sample {
        float mean, var;
    };
    std::vector<Sample> cal;
    const uint32_t stride = n > 4000000 ? 2 : 1;
    for (uint32_t y = 32; y + 32 < h; y += stride)
        for (uint32_t x = 32; x + 32 < w; x += stride) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (!occ[i] && mean[i] > 100.0f) cal.push_back({mean[i], s2[i]});
        }
    const size_t nBins = 12;
    if (cal.size() < nBins * 200) {
        std::fprintf(stderr, "noise-cost: only %zu unplanted pixels with a signal to calibrate the expectation on\n", cal.size());
        return 3;
    }
    std::sort(cal.begin(), cal.end(), [](const Sample& p, const Sample& q) { return p.mean < q.mean; });
    std::vector<double> binMean, binVar;
    const size_t        per = cal.size() / nBins;
    for (size_t b = 0; b < nBins; ++b) {
        std::vector<double> m, v;
        for (size_t k = b * per; k < (b + 1) * per; ++k) {
            m.push_back(cal[k].mean);
            v.push_back(cal[k].var);
        }
        binMean.push_back(xpe_bpm_eval::medianOf(m));
        binVar.push_back(xpe_bpm_eval::medianOf(v) / medFactor);
    }
    const double expectSe = xpe_bpm_eval::medianRelativeSe(nu, static_cast<double>(per));
    auto expectation = [&](double mu) {
        if (mu <= binMean.front()) return binVar.front();
        if (mu >= binMean.back()) return binVar.back();
        const size_t k = static_cast<size_t>(std::upper_bound(binMean.begin(), binMean.end(), mu) - binMean.begin());
        const double f = (mu - binMean[k - 1]) / (binMean[k] - binMean[k - 1]);
        return binVar[k - 1] + f * (binVar[k] - binVar[k - 1]);
    };
    std::printf("== variance-signal curve of the unplanted pixels (%zu pixels in %zu bins of %zu; relative standard error of a bin %.3f)\n", cal.size(), nBins, per, expectSe);
    std::printf("  signal %.0f -> variance %.2f   signal %.0f -> variance %.2f   signal %.0f -> variance %.2f\n", binMean[nBins / 8], binVar[nBins / 8], binMean[nBins / 2], binVar[nBins / 2],
                binMean[nBins - 1 - nBins / 8], binVar[nBins - 1 - nBins / 8]);

    std::vector<std::pair<int, int>> ring;   // the unplanted pixels 4..12 away (Chebyshev distance): the local signal
    for (int dy = -12; dy <= 12; ++dy)
        for (int dx = -12; dx <= 12; ++dx)
            if (std::max(std::abs(dx), std::abs(dy)) >= 4) ring.push_back({dx, dy});
    std::string out = "id\tkind\tshape\tlevel\tpixels\tdof\tcost\tlo\thi\tlabel\tdof_independent\tlag1\tstatus\n";
    struct Sum {
        int                 items{0}, defect{0}, normal{0}, undetermined{0};
        std::vector<double> costs;
    };
    std::map<std::string, Sum> sums;
    for (const Item& it : t.items) {
        if (it.kind != "gain_loss" && it.kind != "efficiency_loss") continue;
        double              sumC = 0.0;
        size_t              npx = 0;
        std::vector<double> pc;   // the cost of every pixel of the item, in the order along the item
        forItemPixels(it, w, [&](size_t i) {
            if (t.ids[i] == kSharedId) return;   // shared with another item: its value is not this item's alone
            const int          x = static_cast<int>(i % w), y = static_cast<int>(i / w);
            std::vector<float> vm;
            vm.reserve(ring.size());
            for (const auto& o : ring) {
                const int xx = x + o.first, yy = y + o.second;
                if (xx < 0 || yy < 0 || xx >= static_cast<int>(w) || yy >= static_cast<int>(h)) continue;
                const size_t j = static_cast<size_t>(yy) * w + static_cast<size_t>(xx);
                if (!occ[j]) vm.push_back(mean[j]);
            }
            if (vm.size() < 60) return;
            std::nth_element(vm.begin(), vm.begin() + static_cast<std::ptrdiff_t>(vm.size() / 2), vm.end());
            const double localMean = static_cast<double>(vm[vm.size() / 2]);
            const double g = static_cast<double>(mean[i]) / localMean;
            const double expect = expectation(localMean);
            if (!(localMean > 100.0) || !(g > 0.05) || !(expect > 0.0)) return;
            const double c = (static_cast<double>(s2[i]) / (g * g)) / expect;
            sumC += c;
            pc.push_back(c);
            ++npx;
        });
        xpe_bpm_eval::NoiseVerdict v;
        double                     dofUsed = nu * static_cast<double>(npx), dofIndependent = dofUsed, lag1 = 0.0;
        if (npx > 0) {
            const xpe_bpm_eval::BlockDof bd = xpe_bpm_eval::blockEffectiveDof(pc, 64);
            lag1 = bd.lag1;
            if (bd.dofEff > 0.0) dofUsed = std::min(dofUsed, bd.dofEff);
            v = xpe_bpm_eval::classifyNoiseCost(sumC / static_cast<double>(npx), dofUsed, expectSe, limit);
        }
        const char* label = !npx ? "undetermined" : (v.label == xpe_bpm_eval::NoiseLabel::Defect ? "defect" : (v.label == xpe_bpm_eval::NoiseLabel::Normal ? "normal" : "undetermined"));
        char b[300];
        std::snprintf(b, sizeof b, "%d\t%s\t%s\t%s\t%zu\t%.0f\t%.4f\t%.4f\t%.4f\t%s\t%.0f\t%.3f\tprovisional\n", it.id, it.kind.c_str(), it.shape.c_str(), fmtLevel(it.level).c_str(), npx, dofUsed, v.cost, v.lo, v.hi,
                      label, dofIndependent, lag1);
        out += b;
        Sum& sm = sums[it.kind + "/" + it.shape + "/" + fmtLevel(it.level)];
        ++sm.items;
        if (std::string(label) == "undetermined") ++sm.undetermined;
        else if (std::string(label) == "defect") ++sm.defect;
        else ++sm.normal;
        if (npx) sm.costs.push_back(v.cost);
    }
    if (a.has("overlap-out")) {   // QA-A-252j: the overlapped pixels, measured as the items are (they are NOT left out as shared)
        std::map<size_t, std::vector<int>> multi;
        std::string                        err;
        if (!collectOverlaps(t, &multi, &err)) {
            std::fprintf(stderr, "noise-cost: %s\n", err.c_str());
            return 3;
        }
        std::map<int, const Item*> byId;
        for (const Item& it : t.items) byId[it.id] = &it;
        struct Acc {
            std::vector<int> ids;
            size_t           px{0}, used{0}, nResp{0};
            double           sumC{0}, sumR{0};
            bool             clipped{false};
        };
        std::map<std::string, Acc> groups;
        for (const auto& kv : multi) {
            const size_t i = kv.first;
            Acc&         ac = groups[overlapKey(kv.second)];
            ac.ids = kv.second;
            ++ac.px;
            const int          x = static_cast<int>(i % w), y = static_cast<int>(i / w);
            std::vector<float> vm;
            vm.reserve(ring.size());
            for (const auto& o : ring) {
                const int xx = x + o.first, yy = y + o.second;
                if (xx < 0 || yy < 0 || xx >= static_cast<int>(w) || yy >= static_cast<int>(h)) continue;
                const size_t j = static_cast<size_t>(yy) * w + static_cast<size_t>(xx);
                if (!occ[j]) vm.push_back(mean[j]);
            }
            if (vm.size() < 60) continue;
            std::nth_element(vm.begin(), vm.begin() + static_cast<std::ptrdiff_t>(vm.size() / 2), vm.end());
            const double localMean = static_cast<double>(vm[vm.size() / 2]);
            if (!(localMean > 100.0)) continue;
            std::vector<const Item*> chain;
            for (int id : kv.second) chain.push_back(byId[id]);
            bool clip = false;
            ac.sumR += chainResponseAt(chain, D[i], localMean, &clip);   // the REAL dark level of the pixel and the REAL local net signal of the verification frames
            ++ac.nResp;
            ac.clipped = ac.clipped || clip;
            const double g = static_cast<double>(mean[i]) / localMean;
            const double expect = expectation(localMean);
            if (!(g > 0.05) || !(expect > 0.0)) continue;
            ac.sumC += (static_cast<double>(s2[i]) / (g * g)) / expect;
            ++ac.used;
        }
        auto labelOf = [](const xpe_bpm_eval::NoiseVerdict& v, size_t used) {
            return !used ? "undetermined" : (v.label == xpe_bpm_eval::NoiseLabel::Defect ? "defect" : (v.label == xpe_bpm_eval::NoiseLabel::Normal ? "normal" : "undetermined"));
        };
        auto idealOf = [&](const std::vector<int>& ids, char* buf, size_t cap) {   // the ideal-model noise cost: 1 / (product of the efficiencies); the gain factors do not change it
            double prod = 1.0;
            for (int id : ids) {
                const Item* it = byId[id];
                if (it->kind == "efficiency_loss") prod *= it->level;
                else if (it->kind != "gain_loss") { std::snprintf(buf, cap, "-"); return; }
            }
            std::snprintf(buf, cap, "%.4f", 1.0 / prod);
        };
        auto kindsOf = [&](const std::vector<int>& ids, bool withShape) {
            std::string s;
            for (size_t k = 0; k < ids.size(); ++k) {
                const Item* it = byId[ids[k]];
                s += (k ? " + " : "") + it->kind + (withShape ? "/" + it->shape : std::string()) + "@" + fmtLevel(it->level);
            }
            return s;
        };
        std::string ovOut = "items\tkinds\tpixels\tused\tdof\tcost\tlo\thi\tlabel\tresponse_model\tclipped\tideal_cost\trow\tresponse_used\tresponse_valid\tresponse_unused\tcost_unused\tresponse_nominal_diag\n";
        struct Pool {
            std::vector<int> ids;
            size_t           groups{0}, px{0}, used{0};
            double           sumC{0};
        };
        std::map<std::string, Pool> pools;
        size_t                      nDef = 0, nNor = 0, nUnd = 0, nClip = 0, nPx = 0;
        for (const auto& kv : groups) {
            const Acc&                 ac = kv.second;
            const double               cost = ac.used ? ac.sumC / static_cast<double>(ac.used) : 0.0;
            xpe_bpm_eval::NoiseVerdict v;
            if (ac.used) v = xpe_bpm_eval::classifyNoiseCost(cost, nu * static_cast<double>(ac.used), expectSe, limit);
            const char* label = labelOf(v, ac.used);
            char        ideal[32];
            idealOf(ac.ids, ideal, sizeof ideal);
            const double nominal = chainResponse([&] { std::vector<const Item*> c; for (int id : ac.ids) c.push_back(byId[id]); return c; }());
            const bool   respValid = ac.nResp > 0;
            char b[900];
            std::snprintf(b, sizeof b, "%s\t%s\t%zu\t%zu\t%.0f\t%.4f\t%.4f\t%.4f\t%s\t%.4f\t%d\t%s\tgroup\t%zu\t%d\t%s\t%s\t%.4f\n", kv.first.c_str(), kindsOf(ac.ids, true).c_str(), ac.px, ac.used,
                          nu * static_cast<double>(ac.used), v.cost, v.lo, v.hi, label, respValid ? ac.sumR / static_cast<double>(ac.nResp) : 0.0, ac.clipped ? 1 : 0, ideal, ac.nResp, respValid ? 1 : 0,
                          respValid ? "-" : "no pixel with a local signal above 100 and 60 reference pixels", ac.used ? "-" : "no pixel with a valid cost (local signal, response above 0.05, expectation)", nominal);
            ovOut += b;
            nPx += ac.px;
            nClip += ac.clipped ? 1 : 0;
            if (std::string(label) == "defect") ++nDef; else if (std::string(label) == "normal") ++nNor; else ++nUnd;
            std::vector<std::string> parts;
            for (int id : ac.ids) parts.push_back(byId[id]->kind + "@" + fmtLevel(byId[id]->level));
            std::sort(parts.begin(), parts.end());
            std::string pk;
            for (size_t k = 0; k < parts.size(); ++k) pk += (k ? " + " : "") + parts[k];
            Pool& pl = pools[pk];
            pl.ids = ac.ids;
            ++pl.groups;
            pl.px += ac.px;
            pl.used += ac.used;
            pl.sumC += ac.sumC;
        }
        std::printf("== overlapped pixels, measured (%zu distinct overlaps, %zu pixels): defect %zu, normal %zu, undetermined %zu groups; %zu groups leave 0..65535 in the chain\n", groups.size(), nPx, nDef, nNor, nUnd,
                    nClip);
        std::printf("  %-52s %6s %7s %7s %9s %9s\n", "pooled by kinds and levels", "groups", "pixels", "used", "cost", "ideal");
        for (const auto& kv : pools) {
            const Pool&                pl = kv.second;
            const double               cost = pl.used ? pl.sumC / static_cast<double>(pl.used) : 0.0;
            xpe_bpm_eval::NoiseVerdict v;
            if (pl.used) v = xpe_bpm_eval::classifyNoiseCost(cost, nu * static_cast<double>(pl.used), expectSe, limit);
            char ideal[32];
            idealOf(pl.ids, ideal, sizeof ideal);
            char b[900];
            std::snprintf(b, sizeof b, "*\t%s\t%zu\t%zu\t%.0f\t%.4f\t%.4f\t%.4f\t%s\t-\t0\t%s\tpooled\t0\t0\t-\t-\t0\n", kv.first.c_str(), pl.px, pl.used, nu * static_cast<double>(pl.used), v.cost, v.lo, v.hi, labelOf(v, pl.used), ideal);
            ovOut += b;
            if (pl.groups >= 3 && kv.first.find("dead") == std::string::npos)
                std::printf("  %-52s %6zu %7zu %7zu %9.3f %9s  %s\n", kv.first.c_str(), pl.groups, pl.px, pl.used, v.cost, ideal, labelOf(v, pl.used));
        }
        if (!writeBytes(a.str("overlap-out"), ovOut.data(), ovOut.size())) return 3;
    }
    if (!writeBytes(a.str("out"), out.data(), out.size())) return 3;
    std::printf("== the labels are PROVISIONAL (effective degrees of freedom from blocks of 64 pixels; the spatial correlation is not yet checked against an independent replicate)\n");
    std::printf("== noise cost of the response items (%zu verification frames, nu = %.0f per pixel, limit %.2f)\n  %-44s %6s %7s %7s %13s %12s\n", verFiles.size(), nu, limit, "kind/shape/level", "items",
                "defect", "normal", "undetermined", "median cost");
    for (auto& kv : sums)
        std::printf("  %-44s %6d %7d %7d %13d %12.3f\n", kv.first.c_str(), kv.second.items, kv.second.defect, kv.second.normal, kv.second.undetermined,
                    kv.second.costs.empty() ? 0.0 : xpe_bpm_eval::medianOf(kv.second.costs));
    return 0;
}

// ------------------------------------------------------------------------------------------------------------- module access
XpeImageBuffer bufU16(std::vector<uint16_t>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data();
    b.width = w;
    b.height = h;
    b.bitsAllocated = 16;
    b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

/**
 * QA-A-260: the one place the first dark frame leaves the list for a detector run (map-v1, map-v2, lines). The list given is the RAW acquisition list; its first
 * frame (taken before the detector settles) is excluded once and the remaining list goes to the detector of either edition, which is told not to drop another
 * (XPE_BPM_V2_SKIP_NONE). --keep-first-dark, or the older --skip-first-dark 0 (which says the list was trimmed already), uses the list as given.
 */
struct DarkSelection {
    std::vector<std::string> given, used;
    bool                     excluded{false};
};

bool selectDarks(const Args& a, DarkSelection* out) {
    out->given = a.list("dark");
    const bool keep = a.has("keep-first-dark") || (a.has("skip-first-dark") && a.num("skip-first-dark", 1) == 0);
    out->excluded = !keep && !out->given.empty();
    if (out->excluded && out->given.size() < 2) {
        std::fprintf(stderr, "one dark frame given: the first dark frame is excluded (the detector has not settled), so none would be left; give the whole list or --keep-first-dark\n");
        return false;
    }
    out->used = out->given;
    if (out->excluded) out->used.erase(out->used.begin());
    return true;
}

/** The list the detector really receives, printed so that two runs can be compared: count, then path and sha-256 of every used frame in order, and the sha-256 of that listing. */
void printDarkList(const char* command, const DarkSelection& ds) {
    std::string listing;
    for (const std::string& p : ds.used) listing += fs::path(p).generic_string() + "|" + sha256Hex(p) + "\n";
    const auto digest = compute_sha256(reinterpret_cast<const uint8_t*>(listing.data()), listing.size());
    std::string hex;
    for (uint8_t b : digest) {
        char t[3];
        std::snprintf(t, sizeof t, "%02x", b);
        hex += t;
    }
    std::printf("dark_list_used: command=%s given=%zu used=%zu first_excluded=%d list_sha256=%s\n", command, ds.given.size(), ds.used.size(), ds.excluded ? 1 : 0, hex.c_str());
    for (size_t i = 0; i < ds.used.size(); ++i) std::printf("dark_used[%zu]: %s %s\n", i, fs::path(ds.used[i]).generic_string().c_str(), sha256Hex(ds.used[i]).c_str());
}

int cmdMapV1(const Args& a) {
    const uint32_t w = static_cast<uint32_t>(a.num("width", 3072)), h = static_cast<uint32_t>(a.num("height", 3072));
    DarkSelection ds;
    if (!selectDarks(a, &ds)) return 2;
    const std::vector<std::string>& darkFiles = ds.used;
    const std::vector<std::string> brightFiles = a.list("bright");
    if (!a.has("out") || darkFiles.empty() || brightFiles.empty()) {
        usage();
        return 2;
    }
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<std::vector<uint16_t>> darks(darkFiles.size()), brights(brightFiles.size());
    for (size_t k = 0; k < darks.size(); ++k)
        if (!readRaw16(darkFiles[k], n, &darks[k])) return 3;
    for (size_t k = 0; k < brights.size(); ++k)
        if (!readRaw16(brightFiles[k], n, &brights[k])) return 3;
    printDarkList("map-v1", ds);
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : darks) db.push_back(bufU16(v, w, h));
    for (auto& v : brights) bb.push_back(bufU16(v, w, h));
    XpeBpmConfig cfg{};
    cfg.lambda_dark = 8.0f;
    cfg.mask_size_dark = 32u;
    cfg.tolerance_pct = 0.07f;
    cfg.mask_size_bright = 128u;
    cfg.min_frames_dark = static_cast<uint32_t>(a.num("min-dark", 5));
    cfg.min_frames_bright = static_cast<uint32_t>(a.num("min-bright", 10));
    if (xpe_preprocess_init(nullptr) != XPE_OK) return 4;
    std::vector<uint8_t> mask(n, 0);
    XpeImageBuffer mb{};
    mb.data = mask.data();
    mb.width = w;
    mb.height = h;
    mb.bitsAllocated = 8;
    mb.bitsStored = 8;
    mb.format = XPE_PIXEL_UINT8;
    mb.dataSize = mask.size();
    const XpeErrorCode rc = xpe_bpm_generate(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), &cfg, &mb);
    xpe_preprocess_shutdown();
    if (rc != XPE_OK) {
        std::fprintf(stderr, "xpe_bpm_generate failed (%d)\n", static_cast<int>(rc));
        return 4;
    }
    size_t count = 0;
    for (uint8_t m : mask) count += m != 0;
    if (!writeBytes(a.str("out"), mask.data(), mask.size())) return 3;
    std::printf("xpe_bpm_generate: %zu darks, %zu brights, %zu defect pixels -> %s\n", darks.size(), brights.size(), count, a.str("out").c_str());
    return 0;
}

/**
 * --margins L,T,R,B: columns / rows at the edges that are outside the imaging area. Four unsigned decimal numbers separated by commas and nothing else (no sign, no blank, no empty field, no
 * hexadecimal or octal, at most the range of the stored 32-bit field), checked BEFORE anything is converted to a signed int (QA-A-267, Codex #236 R236-2: strtoul had saturated 4294967296 to 4294967295 without an error check, and as a signed int that is x = -1, outside the frame):
 * each margin and each opposite pair must also be smaller than the frame (L, R and L + R below the width, T, B and T + B below the height). Anything else: a message and false (the commands exit 2).
 * parseMargins is shared by map-v2, lines and correct, so all three refuse the same inputs.
 */
bool parseMargins(const Args& a, XpeBpmConfigV2* cfgOut, uint32_t frameW, uint32_t frameH) {
    XpeBpmConfigV2& cfg = *cfgOut;
    const std::string spec = a.str("margins");
    unsigned long long v[4] = {0, 0, 0, 0};
    size_t pos = 0;
    bool   ok = true;
    for (int i = 0; i < 4 && ok; ++i) {
        const size_t stop = (i < 3) ? spec.find(',', pos) : spec.size();
        if (stop == std::string::npos || stop == pos || stop - pos > 10) { ok = false; break; }   // missing field, empty field, or more than ten digits (above 4294967295 anyway)
        unsigned long long x = 0;
        for (size_t c = pos; c < stop; ++c) {
            if (spec[c] < '0' || spec[c] > '9') { ok = false; break; }
            x = x * 10 + static_cast<unsigned long long>(spec[c] - '0');
        }
        if (!ok || x > 0xFFFFFFFFull) { ok = false; break; }
        v[i] = x;
        pos = stop + 1;
    }
    if (ok && (pos != spec.size() + 1)) ok = false;   // text after the fourth number (a fifth field)
    if (!ok) {
        std::fprintf(stderr, "--margins takes L,T,R,B (four unsigned decimal numbers, each at most 4294967295)\n");
        return false;
    }
    if (v[0] >= frameW || v[2] >= frameW || v[0] + v[2] >= frameW || v[1] >= frameH || v[3] >= frameH || v[1] + v[3] >= frameH) {
        std::fprintf(stderr, "--margins %s do not fit a %u x %u frame (each margin and each pair L+R, T+B must be smaller than the frame)\n", spec.c_str(), frameW, frameH);
        return false;
    }
    cfg.margin_left = static_cast<uint32_t>(v[0]);
    cfg.margin_top = static_cast<uint32_t>(v[1]);
    cfg.margin_right = static_cast<uint32_t>(v[2]);
    cfg.margin_bottom = static_cast<uint32_t>(v[3]);
    return true;
}

/** The common loading of the frame lists of map-v2 and lines; false after a message. */
bool loadFrames(const Args& a, uint32_t* w, uint32_t* h, std::vector<std::vector<uint16_t>>* darks, std::vector<std::vector<uint16_t>>* brights, DarkSelection* dsOut) {
    *w = static_cast<uint32_t>(a.num("width", 3072));
    *h = static_cast<uint32_t>(a.num("height", 3072));
    if (!selectDarks(a, dsOut)) return false;
    const std::vector<std::string>& darkFiles = dsOut->used;
    const std::vector<std::string> brightFiles = a.list("bright");
    if (darkFiles.empty() || brightFiles.empty()) return false;
    const size_t n = static_cast<size_t>(*w) * *h;
    darks->resize(darkFiles.size());
    brights->resize(brightFiles.size());
    for (size_t k = 0; k < darks->size(); ++k)
        if (!readRaw16(darkFiles[k], n, &(*darks)[k])) return false;
    for (size_t k = 0; k < brights->size(); ++k)
        if (!readRaw16(brightFiles[k], n, &(*brights)[k])) return false;
    return true;
}

int cmdLines(const Args& a) {
    uint32_t                           w = 0, h = 0;
    std::vector<std::vector<uint16_t>> darks, brights;
    DarkSelection                      ds;
    if (!a.has("out") || !loadFrames(a, &w, &h, &darks, &brights, &ds)) {
        usage();
        return 2;
    }
    printDarkList("lines", ds);
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : darks) db.push_back(bufU16(v, w, h));
    for (auto& v : brights) bb.push_back(bufU16(v, w, h));
    XpeBpmConfigV2 cfg{};
    cfg.skip_first_dark = XPE_BPM_V2_SKIP_NONE;   // QA-A-260: the list was trimmed by selectDarks; the library must not drop another frame
    cfg.min_frames_dark = static_cast<uint32_t>(a.num("min-dark", 5));
    cfg.min_frames_bright = static_cast<uint32_t>(a.num("min-bright", 6));
    if (a.has("margins") && !parseMargins(a, &cfg, w, h)) return 2;
    XpeBpmLineConfig lc{};
    lc.target_false_alarms = static_cast<float>(std::strtod(a.str("fa", "0").c_str(), nullptr));
    lc.min_segment = static_cast<uint32_t>(a.num("min-segment", 0));
    lc.noise_tolerance_db = static_cast<float>(std::strtod(a.str("tolerance-db", "0").c_str(), nullptr));
    const size_t           n = static_cast<size_t>(w) * h;
    std::vector<uint8_t>   mask(n, 0);
    std::vector<XpeBpmLine> lines(4096), unconfirmed(4096);
    XpeImageBuffer         mb{};
    mb.data = mask.data();
    mb.width = w;
    mb.height = h;
    mb.bitsAllocated = 8;
    mb.bitsStored = 8;
    mb.format = XPE_PIXEL_UINT8;
    mb.dataSize = mask.size();
    XpeBpmLineReport rep{};
    if (xpe_preprocess_init(nullptr) != XPE_OK) return 4;
    const XpeErrorCode rc = xpe_bpm_find_lines(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), &cfg, &lc, &mb, lines.data(), static_cast<uint32_t>(lines.size()), unconfirmed.data(), static_cast<uint32_t>(unconfirmed.size()), &rep);
    xpe_preprocess_shutdown();
    if (rc != XPE_OK) {
        std::fprintf(stderr, "xpe_bpm_find_lines failed (%d)\n", static_cast<int>(rc));
        return 4;
    }
    if (!writeBytes(a.str("out"), mask.data(), mask.size())) return 3;
    lines.resize(std::min<size_t>(rep.lines, lines.size()));
    unconfirmed.resize(std::min<size_t>(rep.unconfirmed, unconfirmed.size()));
    if (a.has("unconfirmed")) {   // the candidates whose persistence could not be judged: place and reason, never in the mask
        std::string ut = "orientation\tindex\tstart\tend\tcauses\tstrength\treason\n";
        for (const XpeBpmLine& l : unconfirmed) {
            char b[160];
            std::snprintf(b, sizeof b, "%s\t%u\t%u\t%u\t%u\t%.2f\t%u\n", l.orientation == XPE_BPM_LINE_ROW ? "row" : "column", l.index, l.start, l.end, l.causes, static_cast<double>(l.strength), l.reason);
            ut += b;
        }
        if (!writeBytes(a.str("unconfirmed"), ut.data(), ut.size())) return 3;
    }
    std::string tsv = "orientation\tindex\twidth\tstart\tend\tcauses\tflags\tstrength\tpersistence\n";
    for (const XpeBpmLine& l : lines) {
        char b[160];
        std::snprintf(b, sizeof b, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%.2f\t%.2f\n", l.orientation == XPE_BPM_LINE_ROW ? "row" : "column", l.index, l.width, l.start, l.end, l.causes, l.flags, static_cast<double>(l.strength), static_cast<double>(l.persistence));
        tsv += b;
    }
    if (a.has("tsv") && !writeBytes(a.str("tsv"), tsv.data(), tsv.size())) return 3;
    char j[1600];
    std::snprintf(j, sizeof j,
                  "{\n  \"groups\": %u, \"row_groups\": %u, \"column_groups\": %u, \"full\": %u, \"partial\": %u, \"double\": %u, \"crossings\": %u,\n"
                  "  \"max_adjacent\": %u, \"panel_reject\": %u, \"truncated\": %u, \"line_pixels\": %u, \"pixels\": %u, \"weak_response_stretches\": %u,\n"
                  "  \"unconfirmed\": %u, \"dark_frames_raw\": %u, \"dark_frames_valid\": %u, \"bright_frames_raw\": %u, \"bright_frames_valid\": %u,\n"
                  "  \"null_width\": [%.3f, %.3f, %.3f, %.3f, %.3f, %.3f]\n}\n",
                  rep.lines, rep.row_groups, rep.column_groups, rep.full_lines, rep.partial_lines, rep.double_lines, rep.crossings, rep.max_adjacent, rep.panel_reject, rep.truncated, rep.line_pixels, rep.pixels, rep.weak_response_stretches,
                  rep.unconfirmed, rep.dark_frames_raw, rep.dark_frames_valid, rep.bright_frames_raw, rep.bright_frames_valid,
                  static_cast<double>(rep.null_width[0]), static_cast<double>(rep.null_width[1]), static_cast<double>(rep.null_width[2]), static_cast<double>(rep.null_width[3]),
                  static_cast<double>(rep.null_width[4]), static_cast<double>(rep.null_width[5]));
    if (a.has("report") && !writeBytes(a.str("report"), j, std::strlen(j))) return 3;
    std::printf("xpe_bpm_find_lines: %u unconfirmed candidates (persistence not judged); persistence frames: dark %u of %u raw, bright %u of %u raw\n", rep.unconfirmed, rep.dark_frames_valid, rep.dark_frames_raw,
                rep.bright_frames_valid, rep.bright_frames_raw);
    std::printf("xpe_bpm_find_lines: %u groups (%u rows, %u columns; %u full, %u partial, %u double, %u crossings), widest %u, panel reject %u\n", rep.lines, rep.row_groups, rep.column_groups, rep.full_lines,
                rep.partial_lines, rep.double_lines, rep.crossings, rep.max_adjacent, rep.panel_reject);
    for (const XpeBpmLine& l : lines)
        std::printf("  %s %u width %u [%u, %u) causes %u flags %u z %.1f persistence %.2f\n", l.orientation == XPE_BPM_LINE_ROW ? "row" : "column", l.index, l.width, l.start, l.end, l.causes, l.flags, static_cast<double>(l.strength), static_cast<double>(l.persistence));
    return rep.panel_reject ? 20 : 0;   // 20: three adjacent lines or more, the panel is rejected (the outputs are written all the same)
}

/** merge --in A --in B ... --out C: the union of 8-bit maps (the first non-zero value of the inputs at each pixel). */
int cmdMerge(const Args& a) {
    const std::vector<std::string> in = a.list("in");
    if (in.size() < 2 || !a.has("out")) {
        usage();
        return 2;
    }
    std::vector<uint8_t> out;
    for (size_t k = 0; k < in.size(); ++k) {
        std::vector<uint8_t> b;
        if (!readAllBytes(in[k], &b)) {
            std::fprintf(stderr, "%s: cannot open\n", in[k].c_str());
            return 3;
        }
        if (k == 0) out = b;
        else if (b.size() != out.size()) {
            std::fprintf(stderr, "%s: %zu bytes, the first map has %zu\n", in[k].c_str(), b.size(), out.size());
            return 3;
        } else {
            for (size_t i = 0; i < out.size(); ++i)
                if (out[i] == 0) out[i] = b[i];
        }
    }
    return writeBytes(a.str("out"), out.data(), out.size()) ? 0 : 3;
}

/**
 * defect-xcal --in F [--map-out F] [--area-out F] [--lines-out F] (QA-A-254): what a defect.xcal of format version 1 or 2 holds. --map-out and --area-out write the planes (width x height
 * bytes); --lines-out writes a TSV of the line records and the unconfirmed candidates with their individual stretches.
 */
int cmdDefectXcal(const Args& a) {
    if (!a.has("in")) {
        usage();
        return 2;
    }
    DefectXcal d;
    xpe_clear_alerts();
    const XpeErrorCode rc = read_defect_xcal(a.str("in").c_str(), false, &d);
    if (rc != XPE_OK) {
        std::printf("refused: error code %d\n", static_cast<int>(rc));
        const int32_t n = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < n; ++i) {
            char    msg[1024] = {0};
            int32_t sev = 0;
            if (xpe_get_pending_alert(i, msg, sizeof msg, &sev) == XPE_OK) std::printf("  alert: %s\n", msg);
        }
        xpe_clear_alerts();
        return 3;
    }
    size_t flagged = 0, outside = 0;
    for (uint8_t v : d.map) flagged += v != 0;
    for (uint8_t v : d.area) outside += v == 0;
    size_t segments = 0;
    for (const DefectExtLine& L : d.ext.lines) segments += L.segments.size();
    std::printf("defect.xcal: format version %u, %u x %u, session \"%s\"\n  defect plane: %zu pixels flagged\n  imaging area: %s%zu pixels outside\n  line groups: %zu (%zu stretches), blobs: %zu, unconfirmed candidates: %zu\n",
                d.format_version, d.width, d.height, d.session_id.c_str(), flagged, d.has_area ? "" : "none (version 1: every pixel inside); ", outside, d.ext.lines.size(), segments, d.ext.blobs.size(),
                d.ext.candidates.size());
    if (a.has("map-out") && !writeBytes(a.str("map-out"), d.map.data(), d.map.size())) return 3;
    if (a.has("area-out") && d.has_area && !writeBytes(a.str("area-out"), d.area.data(), d.area.size())) return 3;
    if (a.has("v1-out")) {
        // QA-A-255: the same defect plane as a version-1 file (no area, no lines, no blobs), so that the first and the second edition of the correction can be run on one map
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width        = d.width;
        hdr.height       = d.height;
        hdr.payload_len  = static_cast<uint64_t>(d.map.size());
        std::remove(a.str("v1-out").c_str());
        if (write_xcal_file(a.str("v1-out").c_str(), hdr, nullptr, 0, d.map.data(), hdr.payload_len) != XPE_OK) {
            std::fprintf(stderr, "defect-xcal: cannot write %s\n", a.str("v1-out").c_str());
            return 3;
        }
    }
    if (a.has("lines-out")) {
        std::string tsv = "kind\torientation\tindex\twidth\tstart\tend\tcauses\tflags\treason\tcorrection\tstrength\tpersistence\tstretches\n";
        auto row = [&](const char* kind, const DefectExtLine& L) {
            std::string st;
            for (const DefectExtSegment& g : L.segments) st += (st.empty() ? "" : ";") + std::to_string(g.line) + ":" + std::to_string(g.start) + "-" + std::to_string(g.end);
            char b[320];
            std::snprintf(b, sizeof b, "%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%.2f\t%.2f\t", kind, L.orientation == 0 ? "row" : "column", L.index, L.width, L.start, L.end, L.causes, L.flags, L.reason, L.correction,
                          static_cast<double>(L.strength), static_cast<double>(L.persistence));
            tsv += b + st + "\n";
        };
        for (const DefectExtLine& L : d.ext.lines) row("line", L);
        for (const DefectExtLine& L : d.ext.candidates) row("candidate", L);
        if (!writeBytes(a.str("lines-out"), tsv.data(), tsv.size())) return 3;
    }
    return 0;
}

/**
 * correct-time --set DIR --frame F --width W --height H [--repeat N] (QA-A-255): the time of the defect correction alone (xpe_defect_correct_ex) on one frame, N runs (default 9), after one
 * warm-up run. The frame is read as unsigned 16 bit and converted to float; the output is never read back. Prints the median, the smallest and the largest run in milliseconds and the report of
 * the last run.
 */
int cmdCorrectTime(const Args& a) {
    if (!a.has("set") || !a.has("frame") || !a.has("width") || !a.has("height")) {
        usage();
        return 2;
    }
    const uint32_t w = static_cast<uint32_t>(a.num("width", 0)), h = static_cast<uint32_t>(a.num("height", 0));
    const int      repeat = static_cast<int>(a.num("repeat", 9));
    std::vector<uint16_t> raw;
    if (w == 0 || h == 0 || repeat < 1 || !readRaw16(a.str("frame"), static_cast<size_t>(w) * h, &raw)) return 3;
    if (xpe_preprocess_init(nullptr) != XPE_OK) return 4;
    const fs::path dir(a.str("set"));
    xpe_clear_alerts();
    if (xpe_calib_load_defect_map((dir / "defect.xcal").string().c_str()) != XPE_OK) {
        std::fprintf(stderr, "correct-time: cannot load %s\n", (dir / "defect.xcal").string().c_str());
        return 4;
    }
    std::vector<float> in(raw.begin(), raw.end()), out(raw.size());
    auto buf = [&](std::vector<float>& v) {
        XpeImageBuffer b{};
        b.data = v.data();
        b.width = w;
        b.height = h;
        b.bitsAllocated = 32;
        b.bitsStored = 32;
        b.format = XPE_PIXEL_FLOAT32;
        b.dataSize = v.size() * sizeof(float);
        return b;
    };
    XpeImageBuffer           bi = buf(in), bo = buf(out);
    XpeImageMetadata         meta{};
    XpeDefectCorrectionReport rep{};
    rep.size = sizeof rep;
    std::vector<double> ms;
    for (int r = -1; r < repeat; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        if (xpe_defect_correct_ex(&bi, &bo, &meta, &rep, nullptr) != XPE_OK) return 5;
        const auto t1 = std::chrono::steady_clock::now();
        if (r >= 0) ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    xpe_clear_alerts();
    std::sort(ms.begin(), ms.end());
    std::printf("correct-time %u x %u, %d runs: median %.2f ms, min %.2f ms, max %.2f ms\n"
                "  layers_used %u, isolated %u, blob %u, line %u (groups %u, one-sided %u), low_confidence %u px in %u regions, unfilled %u, out_of_area %u\n",
                w, h, repeat, ms[ms.size() / 2], ms.front(), ms.back(), rep.layers_used, rep.isolated_pixels, rep.blob_pixels, rep.line_pixels, rep.line_groups, rep.one_sided_line_pixels,
                rep.low_confidence_pixels, rep.low_confidence_regions, rep.unfilled_pixels, rep.out_of_area_pixels);
    xpe_preprocess_shutdown();
    return 0;
}

/** blobs --map F [--width W --height H] [--out blobs.tsv]: the blobs of a map with their size. */
/**
 * chain --truth DIR --items a+b+c [--dark D] [--signal S] (QA-A-255): the response of the injection chain for the items in the given order, with the dark level D (default 1000) and the net
 * bright signal S (default 10000); prints "response R clipped C". The arithmetic the overlap measurement uses, for the tests.
 */
int cmdChain(const Args& a) {
    Truth t;
    if (!a.has("truth") || !a.has("items") || !loadTruth(a.str("truth"), &t)) {
        usage();
        return 2;
    }
    std::map<int, const Item*> byId;
    for (const Item& it : t.items) byId[it.id] = &it;
    std::vector<const Item*> chain;
    const std::string        spec = a.str("items");
    size_t                   from = 0;
    while (from <= spec.size()) {
        size_t plus = spec.find('+', from);
        if (plus == std::string::npos) plus = spec.size();
        const auto f = byId.find(std::atoi(spec.substr(from, plus - from).c_str()));
        if (f == byId.end()) {
            std::fprintf(stderr, "chain: no item %s\n", spec.substr(from, plus - from).c_str());
            return 3;
        }
        chain.push_back(f->second);
        from = plus + 1;
    }
    bool         clipped = false;
    const double r = chainResponseAt(chain, a.has("dark") ? std::strtod(a.str("dark").c_str(), nullptr) : 1000.0, a.has("signal") ? std::strtod(a.str("signal").c_str(), nullptr) : 10000.0, &clipped);
    std::printf("response %.4f clipped %d\n", r, clipped ? 1 : 0);
    return 0;
}

int cmdBlobs(const Args& a) {
    const uint32_t w = static_cast<uint32_t>(a.num("width", 3072)), h = static_cast<uint32_t>(a.num("height", 3072));
    std::vector<uint8_t> m;
    if (!a.has("map") || !readAllBytes(a.str("map"), &m) || m.size() != static_cast<size_t>(w) * h) {
        std::fprintf(stderr, "blobs needs --map of width x height bytes\n");
        return 2;
    }
    XpeImageBuffer b{};
    b.data = m.data();
    b.width = w;
    b.height = h;
    b.format = XPE_PIXEL_UINT8;
    b.dataSize = m.size();
    uint32_t count = 0;
    std::vector<XpeBpmBlob> blobs(1u << 16);
    if (xpe_bpm_blobs(&b, blobs.data(), static_cast<uint32_t>(blobs.size()), &count) != XPE_OK) return 4;
    blobs.resize(std::min<size_t>(count, blobs.size()));
    size_t cls[4] = {0, 0, 0, 0};
    for (const XpeBpmBlob& k : blobs) ++cls[k.size == 1 ? 0 : (k.size <= 3 ? 1 : (k.size <= 15 ? 2 : 3))];
    std::printf("blobs: %u (single pixels %zu, 2-3 pixels %zu, 4-15 pixels %zu, 16 and more %zu)\n", count, cls[0], cls[1], cls[2], cls[3]);
    std::string tsv = "x0\ty0\tx1\ty1\tsize\n";
    for (const XpeBpmBlob& k : blobs) {
        char line[96];
        std::snprintf(line, sizeof line, "%u\t%u\t%u\t%u\t%u\n", k.x0, k.y0, k.x1, k.y1, k.size);
        tsv += line;
    }
    if (a.has("out") && !writeBytes(a.str("out"), tsv.data(), tsv.size())) return 3;
    return 0;
}

int cmdMapV2(const Args& a) {
    const uint32_t w = static_cast<uint32_t>(a.num("width", 3072)), h = static_cast<uint32_t>(a.num("height", 3072));
    DarkSelection ds;
    if (!selectDarks(a, &ds)) return 2;
    const std::vector<std::string>& darkFiles = ds.used;
    const std::vector<std::string> brightFiles = a.list("bright");
    if (!a.has("out") || darkFiles.empty() || brightFiles.empty()) {
        usage();
        return 2;
    }
    const size_t n = static_cast<size_t>(w) * h;
    std::vector<std::vector<uint16_t>> darks(darkFiles.size()), brights(brightFiles.size());
    for (size_t k = 0; k < darks.size(); ++k)
        if (!readRaw16(darkFiles[k], n, &darks[k])) return 3;
    for (size_t k = 0; k < brights.size(); ++k)
        if (!readRaw16(brightFiles[k], n, &brights[k])) return 3;
    printDarkList("map-v2", ds);
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : darks) db.push_back(bufU16(v, w, h));
    for (auto& v : brights) bb.push_back(bufU16(v, w, h));
    XpeBpmConfigV2 cfg{};
    cfg.target_false_alarms = static_cast<float>(std::strtod(a.str("target-fa", "1").c_str(), nullptr));
    // QA-A-260: the list is the RAW acquisition list; selectDarks excluded its first frame once (--keep-first-dark or --skip-first-dark 0: the list was trimmed already)
    cfg.skip_first_dark = XPE_BPM_V2_SKIP_NONE;   // QA-A-260: the list was trimmed by selectDarks; the library must not drop another frame
    cfg.min_frames_dark = static_cast<uint32_t>(a.num("min-dark", 5));
    cfg.min_frames_bright = static_cast<uint32_t>(a.num("min-bright", 6));
    if (a.has("margins") && !parseMargins(a, &cfg, w, h)) return 2;
    if (xpe_preprocess_init(nullptr) != XPE_OK) return 4;
    std::vector<uint8_t> mask(n, 0), feat(n, 0), area(n, 0);
    auto outBuf = [&](std::vector<uint8_t>& v) {
        XpeImageBuffer b{};
        b.data = v.data();
        b.width = w;
        b.height = h;
        b.bitsAllocated = 8;
        b.bitsStored = 8;
        b.format = XPE_PIXEL_UINT8;
        b.dataSize = v.size();
        return b;
    };
    XpeImageBuffer mb = outBuf(mask), fb = outBuf(feat), ab = outBuf(area);
    XpeBpmReportV2 rep{};
    const XpeErrorCode rc = xpe_bpm_generate_v2(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), &cfg, &mb, &fb, &ab, &rep);
    xpe_preprocess_shutdown();
    if (rc != XPE_OK) {
        std::fprintf(stderr, "xpe_bpm_generate_v2 failed (%d)\n", static_cast<int>(rc));
        return 4;
    }
    if (!writeBytes(a.str("out"), mask.data(), mask.size())) return 3;
    if (a.has("features") && !writeBytes(a.str("features"), feat.data(), feat.size())) return 3;
    if (a.has("area") && !writeBytes(a.str("area"), area.data(), area.size())) return 3;
    static const char* const names[XPE_BPM_V2_FEATURE_COUNT] = {"dark_mean", "dark_noise", "dose_slope", "dose_linearity", "flat_field", "bright_noise"};
    std::string j = "{\n  \"detector\": \"xpe_bpm_generate_v2\",\n  \"target_false_alarms\": " + fmtLevel(static_cast<double>(cfg.target_false_alarms)) + ",\n  \"skip_first_dark\": " +
                    std::to_string(cfg.skip_first_dark) + ",\n  \"first_dark_excluded_by_tool\": " + (ds.excluded ? "true" : "false") + ",\n  \"dark_frames_given\": " + std::to_string(ds.given.size()) + ",\n  \"pixels\": " + std::to_string(rep.pixels) + ",\n  \"dark_frames_used\": " + std::to_string(rep.dark_frames_used) +
                    ",\n  \"out_of_area_pixels\": " + std::to_string(rep.out_of_area_pixels) + ",\n  \"margins\": [" + std::to_string(cfg.margin_left) + ", " + std::to_string(cfg.margin_top) + ", " +
                    std::to_string(cfg.margin_right) + ", " + std::to_string(cfg.margin_bottom) + "],\n  \"bright_frames_used\": " + std::to_string(rep.bright_frames_used) + ",\n  \"dose_groups\": " + std::to_string(rep.dose_groups) + ",\n  \"groups\": [";
    for (uint32_t g = 0; g < rep.dose_groups; ++g) {
        char b[96];
        std::snprintf(b, sizeof b, "%s{\"signal\": %.2f, \"frames\": %u}", g ? ", " : "", static_cast<double>(rep.group_signal[g]), rep.group_frames[g]);
        j += b;
    }
    j += "],\n  \"features\": [\n";
    for (int f = 0; f < XPE_BPM_V2_FEATURE_COUNT; ++f) {
        char b[330];
        std::snprintf(b, sizeof b, "    {\"name\": \"%s\", \"active\": %u, \"degrees_of_freedom\": %u, \"threshold\": %.4f, \"scale_median\": %.6g, \"flagged\": %u}%s\n", names[f],
                      rep.feature_active[f], rep.degrees_of_freedom[f], static_cast<double>(rep.threshold[f]), static_cast<double>(rep.scale_median[f]), rep.flagged[f],
                      f + 1 < XPE_BPM_V2_FEATURE_COUNT ? "," : "");
        j += b;
    }
    j += "  ],\n  \"flagged_union\": " + std::to_string(rep.flagged_union) + ",\n  \"weak_response_pixels\": " + std::to_string(rep.weak_response_pixels) + ",\n  \"response_ratio_low\": " +
         fmtLevel(static_cast<double>(rep.response_ratio_low)) + ",\n  \"response_ratio_high\": " + fmtLevel(static_cast<double>(rep.response_ratio_high)) + "\n}\n";
    if (a.has("report") && !writeBytes(a.str("report"), j.data(), j.size())) return 3;
    std::printf("xpe_bpm_generate_v2: %u darks used, %u brights in %u dose groups, %u defect pixels (union of the features) -> %s\n", rep.dark_frames_used, rep.bright_frames_used,
                rep.dose_groups, rep.flagged_union, a.str("out").c_str());
    for (int f = 0; f < XPE_BPM_V2_FEATURE_COUNT; ++f)
        std::printf("  %-15s active %u  threshold %.4f  local scale median %.6g  flagged %u\n", names[f], rep.feature_active[f], static_cast<double>(rep.threshold[f]),
                    static_cast<double>(rep.scale_median[f]), rep.flagged[f]);
    return 0;
}

// ------------------------------------------------------------------------------------------------------------- correct
const char* const kCfgFinal = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true}";
const char* const kCfgNoDefect = "{\"bypassTemp\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassDefect\":true}";

bool loadSet(const std::string& dir) {
    xpe_clear_alerts();
    return xpe_calib_load_offset((fs::path(dir) / "offset.xcal").string().c_str()) == XPE_OK && xpe_calib_load_gain((fs::path(dir) / "gain.xcal").string().c_str()) == XPE_OK &&
           xpe_calib_load_defect_map((fs::path(dir) / "defect.xcal").string().c_str()) == XPE_OK;
}

bool runPipeline(const std::string& frame, uint32_t w, uint32_t h, const char* cfg, std::vector<float>* out) {
    std::vector<uint16_t> raw;
    if (!readRaw16(frame, static_cast<size_t>(w) * h, &raw)) return false;
    out->assign(raw.size(), 0.0f);
    const XpeImageBuffer in = bufU16(raw, w, h);
    XpeImageBuffer ob{};
    ob.data = out->data();
    ob.width = w;
    ob.height = h;
    ob.bitsAllocated = 32;
    ob.bitsStored = 32;
    ob.format = XPE_PIXEL_FLOAT32;
    ob.dataSize = out->size() * sizeof(float);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_preprocess_pipeline_out(&in, &ob, &meta, nullptr, nullptr, cfg);
    xpe_clear_alerts();
    if (rc != XPE_OK) {
        std::fprintf(stderr, "pipeline failed on %s (%d)\n", frame.c_str(), static_cast<int>(rc));
        return false;
    }
    return true;
}

using xpe_bpm_eval::LineIndex;
using xpe_bpm_eval::ZSide;
using xpe_bpm_eval::ZStatus;

/** A number for the JSON file, or the string "uncomputable" when it is not finite (the file never holds nan or inf). */
std::string jnum(double v, const char* fmt) {
    if (!std::isfinite(v)) return "\"uncomputable\"";
    char b[64];
    std::snprintf(b, sizeof b, fmt, v);
    return b;
}

std::string ratioText(const LineIndex& l, bool mad) {
    const bool ok = mad ? l.madComputable : l.computable;
    if (!ok) return "uncomputable";
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", mad ? l.ratioMad : l.ratio);
    return b;
}

std::string sideJson(const ZSide& z, const char* indent) {
    char b[400];
    std::snprintf(b, sizeof b, "{\"valid\": %zu, \"invalid\": {\"zero_sigma_equal\": %zu, \"zero_sigma_error\": %zu, \"nonfinite\": %zu}, \"median_z\": %s, \"p95_z\": %s}", z.valid(),
                  z.zeroSigmaEqual, z.zeroSigmaError, z.nonFinite, z.computable() ? jnum(z.median(), "%.6f").c_str() : "\"uncomputable\"",
                  z.computable() ? jnum(z.p95(), "%.6f").c_str() : "\"uncomputable\"");
    (void)indent;
    return b;
}

struct ZGroup {
    ZSide  corrected, uncorrected;
    size_t pixels{0};
};

int cmdCorrect(const Args& a) {
    Truth t;
    const std::vector<std::string> cleanEval = a.list("clean-eval"), injEval = a.list("inj-eval");
    if (!a.has("truth") || !a.has("clean-set") || !a.has("inj-set") || cleanEval.empty() || cleanEval.size() != injEval.size() || !loadTruth(a.str("truth"), &t)) {
        usage();
        return 2;
    }
    if (xpe_preprocess_init(nullptr) != XPE_OK) return 4;
    // QA-A-265: the imaging area of the line-index contract. Without --margins the area is the whole frame and the contract numbers equal the plain ones.
    XpeBpmConfigV2 marginCfg{};
    if (a.has("margins") && !parseMargins(a, &marginCfg, t.w, t.h)) return 2;
    const xpe_bpm_eval::Area area{static_cast<int>(marginCfg.margin_left), static_cast<int>(marginCfg.margin_top), static_cast<int>(t.w) - static_cast<int>(marginCfg.margin_right),
                                  static_cast<int>(t.h) - static_cast<int>(marginCfg.margin_bottom)};
    if (area.x1 - area.x0 < 3 * xpe_bpm_eval::kMinAreaExtent || area.y1 - area.y0 < 3 * xpe_bpm_eval::kMinAreaExtent) {
        std::fprintf(stderr, "--margins leave no imaging area\n");
        return 2;
    }
    std::string json = "{\n  \"frames\": [\n";
    for (size_t f = 0; f < cleanEval.size(); ++f) {
        std::vector<float> C, I, U;
        if (!loadSet(a.str("clean-set")) || !runPipeline(cleanEval[f], t.w, t.h, kCfgFinal, &C)) return 4;
        if (!loadSet(a.str("inj-set")) || !runPipeline(injEval[f], t.w, t.h, kCfgFinal, &I) || !runPipeline(injEval[f], t.w, t.h, kCfgNoDefect, &U)) return 4;
        std::printf("== evaluation frame %s (clean) / %s (planted)\n", baseName(cleanEval[f]).c_str(), baseName(injEval[f]).c_str());

        // correction error against local noise
        std::map<std::string, ZGroup> zs;
        std::vector<std::string> order;
        std::vector<const Item*> byId(t.items.size() + 2, nullptr);
        for (const Item& it : t.items) byId[static_cast<size_t>(it.id)] = &it;
        auto sigmaLoc = [&](int x, int y) {
            double s1 = 0, s2 = 0;
            int m = 0;
            for (int yy = std::max(0, y - 5); yy <= std::min(static_cast<int>(t.h) - 1, y + 5); ++yy)
                for (int xx = std::max(0, x - 5); xx <= std::min(static_cast<int>(t.w) - 1, x + 5); ++xx) {
                    const double v = C[static_cast<size_t>(yy) * t.w + static_cast<size_t>(xx)];
                    s1 += v;
                    s2 += v * v;
                    ++m;
                }
            const double mean = s1 / m;
            return std::sqrt(std::max(0.0, s2 / m - mean * mean));
        };
        for (size_t i = 0; i < C.size(); ++i) {
            const uint16_t id = t.ids[i];
            if (id == 0 || id == kSharedId) continue;
            const Item* it = byId[id];
            if (!it || it->cls == "control") continue;
            const std::string key = groupKey(*it);
            if (!zs.count(key)) order.push_back(key);
            ZGroup& z = zs[key];
            ++z.pixels;
            const int x = static_cast<int>(i % t.w), y = static_cast<int>(i / t.w);
            const double sl = sigmaLoc(x, y);
            double zi = 0, zu = 0;
            const ZStatus si = xpe_bpm_eval::classifyZ(I[i], C[i], sl, &zi), su = xpe_bpm_eval::classifyZ(U[i], C[i], sl, &zu);
            z.corrected.add(si, zi);
            z.uncorrected.add(su, zu);
        }
        std::printf("  correction error |planted - clean| / local sigma (11 x 11 window of the clean output); invalid = sigma zero or a value not finite, never counted as 0\n"
                    "  %-34s %9s %8s %12s %12s | %8s %12s %12s\n", "class/kind/shape/level", "pixels", "valid", "median", "p95", "valid", "median(no fix)", "p95(no fix)");
        json += "    {\"clean\": \"" + baseName(cleanEval[f]) + "\", \"planted\": \"" + baseName(injEval[f]) + "\", \"correction_error\": [\n";
        for (size_t g = 0; g < order.size(); ++g) {
            const ZGroup& z = zs[order[g]];
            auto med = [](const ZSide& s, bool p95) { char b[32]; if (!s.computable()) return std::string("uncomputable"); std::snprintf(b, sizeof b, "%.3f", p95 ? s.p95() : s.median()); return std::string(b); };
            std::printf("  %-34s %9zu %8zu %12s %12s | %8zu %12s %12s   invalid: corrected %zu, no fix %zu\n", order[g].c_str(), z.pixels, z.corrected.valid(), med(z.corrected, false).c_str(),
                        med(z.corrected, true).c_str(), z.uncorrected.valid(), med(z.uncorrected, false).c_str(), med(z.uncorrected, true).c_str(), z.corrected.invalid(), z.uncorrected.invalid());
            json += "      {\"group\": \"" + order[g] + "\", \"pixels\": " + std::to_string(z.pixels) + ", \"corrected\": " + sideJson(z.corrected, "") + ", \"uncorrected\": " + sideJson(z.uncorrected, "") +
                    "}" + (g + 1 < order.size() ? ",\n" : "\n");
        }
        json += "    ],\n    \"line_index\": [\n";

        // D_n per line item
        std::printf("  line index D_n / predicted sigma (|ratio| > 3 = a line remains; sigma from the standard deviation as defined; the MAD-based ratio is auxiliary)\n"
                    "  %-4s %-34s %-6s %12s %12s %12s | %12s\n", "id", "class/kind/shape/level", "part", "corrected", "no fix", "clean", "corrected(mad)");
        int over[3] = {0, 0, 0}, unc[3] = {0, 0, 0}, parts = 0, skipped = 0;
        // the contract: parts of the plain definition split by status; over-3-sigma counts among the measured parts and among the fully inside ones (corrected, no fix, clean)
        int areaFull = 0, areaClipped = 0, areaItemOut = 0, areaRefOut = 0, areaShort = 0, areaOutside = 0, areaThick = 0;
        int areaOver[3] = {0, 0, 0}, areaOverFull[3] = {0, 0, 0}, areaUnc[3] = {0, 0, 0}, areaUncFull[3] = {0, 0, 0};
        bool firstJson = true;
        for (const Item& it : t.items) {
            if (it.cls != "line" && it.cls != "control") continue;
            for (size_t r = 0; r < it.rects.size(); ++r) {
                const LineIndex li = xpe_bpm_eval::lineIndex(I.data(), t.w, t.h, it.rects[r]), lu = xpe_bpm_eval::lineIndex(U.data(), t.w, t.h, it.rects[r]),
                                lc = xpe_bpm_eval::lineIndex(C.data(), t.w, t.h, it.rects[r]);
                if (li.skipped || lu.skipped || lc.skipped) { ++skipped; continue; }
                ++parts;
                {
                    using xpe_bpm_eval::AreaStatus;
                    const xpe_bpm_eval::AreaLineIndex ai = xpe_bpm_eval::lineIndexInArea(I.data(), t.w, t.h, it.rects[r], area), au = xpe_bpm_eval::lineIndexInArea(U.data(), t.w, t.h, it.rects[r], area),
                                                      ac = xpe_bpm_eval::lineIndexInArea(C.data(), t.w, t.h, it.rects[r], area);
                    switch (ai.status) {
                        case AreaStatus::Full: ++areaFull; break;
                        case AreaStatus::Clipped: ++areaClipped; break;
                        case AreaStatus::ItemOutside: ++areaItemOut; break;
                        case AreaStatus::ReferenceOutside: ++areaRefOut; break;
                        case AreaStatus::TooShort: ++areaShort; break;
                        case AreaStatus::OutsideArea: ++areaOutside; break;
                        case AreaStatus::LengthBelowThickness: ++areaThick; break;
                    }
                    if (ai.measured()) {
                        const xpe_bpm_eval::AreaLineIndex* as[3] = {&ai, &au, &ac};
                        for (int k = 0; k < 3; ++k) {
                            if (!as[k]->li.computable) { ++areaUnc[k]; if (ai.status == AreaStatus::Full) ++areaUncFull[k]; }
                            else if (std::fabs(as[k]->li.ratio) > 3.0) {
                                ++areaOver[k];
                                if (ai.status == AreaStatus::Full) ++areaOverFull[k];
                            }
                        }
                    }
                }
                const LineIndex* ls[3] = {&li, &lu, &lc};
                for (int k = 0; k < 3; ++k) {
                    if (!ls[k]->computable) ++unc[k];
                    else if (std::fabs(ls[k]->ratio) > 3.0) ++over[k];
                }
                std::printf("  %-4d %-34s %-6zu %12s %12s %12s | %12s\n", it.id, groupKey(it).c_str(), r, ratioText(li, false).c_str(), ratioText(lu, false).c_str(), ratioText(lc, false).c_str(),
                            ratioText(li, true).c_str());
                json += std::string(firstJson ? "" : ",\n") + "      {\"id\": " + std::to_string(it.id) + ", \"group\": \"" + groupKey(it) + "\", \"part\": " + std::to_string(r) +
                        ", \"ratio_corrected\": " + (li.computable ? jnum(li.ratio, "%.4f") : "\"uncomputable\"") + ", \"ratio_no_fix\": " + (lu.computable ? jnum(lu.ratio, "%.4f") : "\"uncomputable\"") +
                        ", \"ratio_clean\": " + (lc.computable ? jnum(lc.ratio, "%.4f") : "\"uncomputable\"") + ", \"ratio_corrected_mad\": " + (li.madComputable ? jnum(li.ratioMad, "%.4f") : "\"uncomputable\"") +
                        ", \"ratio_no_fix_mad\": " + (lu.madComputable ? jnum(lu.ratioMad, "%.4f") : "\"uncomputable\"") + ", \"ratio_clean_mad\": " + (lc.madComputable ? jnum(lc.ratioMad, "%.4f") : "\"uncomputable\"") + "}";
                firstJson = false;
            }
        }
        std::printf("  parts with |ratio| > 3: corrected %d, no fix %d, clean %d (of %d; uncomputable: corrected %d, no fix %d, clean %d; skipped at the frame edge %d)\n", over[0], over[1], over[2], parts,
                    unc[0], unc[1], unc[2], skipped);
        json += "\n    ],\n";

        // rows and columns outside every item
        std::vector<uint8_t> rowSkip(t.h, 0), colSkip(t.w, 0);
        for (const Item& it : t.items)
            for (const Rect& q : it.rects) {
                if (it.cls != "line" && it.cls != "control") continue;
                if (q.w > q.h) for (int y = std::max(0, q.y - 3); y <= std::min(static_cast<int>(t.h) - 1, q.y + q.h + 2); ++y) rowSkip[static_cast<size_t>(y)] = 1;
                else for (int x = std::max(0, q.x - 3); x <= std::min(static_cast<int>(t.w) - 1, q.x + q.w + 2); ++x) colSkip[static_cast<size_t>(x)] = 1;
            }
        size_t rowsI = 0, rowsC = 0, colsI = 0, colsC = 0, rowsN = 0, colsN = 0, rowsUncI = 0, rowsUncC = 0, colsUncI = 0, colsUncC = 0;
        auto count = [&](const std::vector<float>& img, const Rect& q, size_t* over3, size_t* uncomputable) {
            const LineIndex l = xpe_bpm_eval::lineIndex(img.data(), t.w, t.h, q);
            if (l.skipped) return;
            if (!l.computable) ++*uncomputable;
            else if (std::fabs(l.dn) > 3.0 * l.sigmaPred) ++*over3;
        };
        for (int y = 1; y + 1 < static_cast<int>(t.h); ++y) {
            if (rowSkip[static_cast<size_t>(y)]) continue;
            ++rowsN;
            count(I, Rect{0, y, static_cast<int>(t.w), 1}, &rowsI, &rowsUncI);
            count(C, Rect{0, y, static_cast<int>(t.w), 1}, &rowsC, &rowsUncC);
        }
        for (int x = 1; x + 1 < static_cast<int>(t.w); ++x) {
            if (colSkip[static_cast<size_t>(x)]) continue;
            ++colsN;
            count(I, Rect{x, 0, 1, static_cast<int>(t.h)}, &colsI, &colsUncI);
            count(C, Rect{x, 0, 1, static_cast<int>(t.h)}, &colsC, &colsUncC);
        }
        std::printf("  rows outside every line item: %zu checked, |D_n| > 3 sigma: corrected %zu, clean %zu (uncomputable %zu, %zu); columns: %zu checked, corrected %zu, clean %zu (uncomputable %zu, %zu)\n",
                    rowsN, rowsI, rowsC, rowsUncI, rowsUncC, colsN, colsI, colsC, colsUncI, colsUncC);

        // QA-A-265: the same rows and columns under the imaging-area contract (extent = the area, both reference lines inside the area)
        size_t aRowsN = 0, aRowsI = 0, aRowsC = 0, aColsN = 0, aColsI = 0, aColsC = 0, aRowsUncI = 0, aRowsUncC = 0, aColsUncI = 0, aColsUncC = 0;
        auto areaCount = [&](const std::vector<float>& img, const Rect& q, size_t* over3, size_t* uncomputable) {
            const xpe_bpm_eval::AreaLineIndex l = xpe_bpm_eval::lineIndexInArea(img.data(), t.w, t.h, q, area);
            if (!l.measured()) return false;
            if (!l.li.computable) ++*uncomputable;
            else if (std::fabs(l.li.dn) > 3.0 * l.li.sigmaPred) ++*over3;
            return true;
        };
        for (int y = 1; y + 1 < static_cast<int>(t.h); ++y) {
            if (rowSkip[static_cast<size_t>(y)]) continue;
            if (!areaCount(I, Rect{area.x0, y, area.x1 - area.x0, 1}, &aRowsI, &aRowsUncI)) continue;
            ++aRowsN;
            areaCount(C, Rect{area.x0, y, area.x1 - area.x0, 1}, &aRowsC, &aRowsUncC);
        }
        for (int x = 1; x + 1 < static_cast<int>(t.w); ++x) {
            if (colSkip[static_cast<size_t>(x)]) continue;
            if (!areaCount(I, Rect{x, area.y0, 1, area.y1 - area.y0}, &aColsI, &aColsUncI)) continue;
            ++aColsN;
            areaCount(C, Rect{x, area.y0, 1, area.y1 - area.y0}, &aColsC, &aColsUncC);
        }
        const int areaMeasured = areaFull + areaClipped;
        // QA-A-267 (Codex #236 R236-3): "measured" is what the GEOMETRY allows; a measured part can still be uncomputable (constant or non-finite input), per image. The valid number is the difference.
        std::printf("  imaging-area contract (area columns %d..%d, rows %d..%d): of %d parts, %d geometrically measurable (%d fully inside, %d cut to the area), not measurable: item outside %d, reference line outside %d, extent < %d %d, extent < thickness %d, outside %d\n"
                    "      of the measurable parts, uncomputable: corrected %d, no fix %d, clean %d; so valid: corrected %d, no fix %d, clean %d; |ratio| > 3 among the valid: corrected %d, no fix %d, clean %d; among the fully inside valid parts only: corrected %d, no fix %d, clean %d\n"
                    "      rows outside every line item under the contract: %zu geometrically measurable, uncomputable corrected %zu, clean %zu, beyond 3 sigma: corrected %zu, clean %zu; columns: %zu measurable, uncomputable corrected %zu, clean %zu, beyond 3 sigma: corrected %zu, clean %zu\n",
                    area.x0, area.x1 - 1, area.y0, area.y1 - 1, parts, areaMeasured, areaFull, areaClipped, areaItemOut, areaRefOut, xpe_bpm_eval::kMinAreaExtent, areaShort, areaThick, areaOutside,
                    areaUnc[0], areaUnc[1], areaUnc[2], areaMeasured - areaUnc[0], areaMeasured - areaUnc[1], areaMeasured - areaUnc[2], areaOver[0], areaOver[1], areaOver[2], areaOverFull[0], areaOverFull[1], areaOverFull[2],
                    aRowsN, aRowsUncI, aRowsUncC, aRowsI, aRowsC, aColsN, aColsUncI, aColsUncC, aColsI, aColsC);
        char ab[3200];
        std::snprintf(ab, sizeof ab,
                      "    \"area_contract\": {\"area\": [%d, %d, %d, %d], \"parts\": %d, \"measured\": %d, \"geometry_measurable\": %d, \"fully_inside\": %d, \"cut_to_area\": %d,\n"
                      "      \"not_measurable\": {\"item_outside\": %d, \"reference_outside\": %d, \"extent_too_short\": %d, \"extent_below_thickness\": %d, \"outside_area\": %d},\n"
                      "      \"parts_uncomputable\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d},\n"
                      "      \"parts_valid\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d},\n"
                      "      \"parts_over_3_sigma_measured\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d, \"measured\": %d, \"valid_corrected\": %d, \"valid_no_fix\": %d, \"valid_clean\": %d},\n"
                      "      \"parts_over_3_sigma_fully_inside\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d, \"measured\": %d, \"valid_corrected\": %d, \"valid_no_fix\": %d, \"valid_clean\": %d},\n"
                      "      \"other_rows\": {\"checked\": %zu, \"corrected\": %zu, \"clean\": %zu, \"uncomputable_corrected\": %zu, \"uncomputable_clean\": %zu, \"valid_corrected\": %zu, \"valid_clean\": %zu},\n"
                      "      \"other_columns\": {\"checked\": %zu, \"corrected\": %zu, \"clean\": %zu, \"uncomputable_corrected\": %zu, \"uncomputable_clean\": %zu, \"valid_corrected\": %zu, \"valid_clean\": %zu}},\n",
                      area.x0, area.y0, area.x1, area.y1, parts, areaMeasured, areaMeasured, areaFull, areaClipped, areaItemOut, areaRefOut, areaShort, areaThick, areaOutside,
                      areaUnc[0], areaUnc[1], areaUnc[2], areaMeasured - areaUnc[0], areaMeasured - areaUnc[1], areaMeasured - areaUnc[2],
                      areaOver[0], areaOver[1], areaOver[2], areaMeasured, areaMeasured - areaUnc[0], areaMeasured - areaUnc[1], areaMeasured - areaUnc[2],
                      areaOverFull[0], areaOverFull[1], areaOverFull[2], areaFull, areaFull - areaUncFull[0], areaFull - areaUncFull[1], areaFull - areaUncFull[2],
                      aRowsN, aRowsI, aRowsC, aRowsUncI, aRowsUncC, aRowsN - aRowsUncI, aRowsN - aRowsUncC, aColsN, aColsI, aColsC, aColsUncI, aColsUncC, aColsN - aColsUncI, aColsN - aColsUncC);
        json += ab;
        char b[900];
        std::snprintf(b, sizeof b,
                      "    \"parts_over_3_sigma\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d, \"parts\": %d},\n"
                      "    \"parts_uncomputable\": {\"corrected\": %d, \"no_fix\": %d, \"clean\": %d, \"skipped_at_the_edge\": %d},\n"
                      "    \"other_rows\": {\"checked\": %zu, \"corrected\": %zu, \"clean\": %zu, \"uncomputable_corrected\": %zu, \"uncomputable_clean\": %zu},\n"
                      "    \"other_columns\": {\"checked\": %zu, \"corrected\": %zu, \"clean\": %zu, \"uncomputable_corrected\": %zu, \"uncomputable_clean\": %zu}}%s\n",
                      over[0], over[1], over[2], parts, unc[0], unc[1], unc[2], skipped, rowsN, rowsI, rowsC, rowsUncI, rowsUncC, colsN, colsI, colsC, colsUncI, colsUncC,
                      f + 1 < cleanEval.size() ? "," : "");
        json += b;
    }
    json += "  ]\n}\n";
    xpe_preprocess_shutdown();
    if (a.has("out") && !writeBytes(a.str("out"), json.data(), json.size())) return 3;
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Args a;
    if (!parseArgs(argc, argv, &a)) {
        usage();
        return 2;
    }
    if (a.cmd == "inject") return cmdInject(a);
    if (a.cmd == "score") return cmdScore(a);
    if (a.cmd == "noise-cost") return cmdNoiseCost(a);
    if (a.cmd == "correct") return cmdCorrect(a);
    if (a.cmd == "map-v1") return cmdMapV1(a);
    if (a.cmd == "map-v2") return cmdMapV2(a);
    if (a.cmd == "lines") return cmdLines(a);
    if (a.cmd == "merge") return cmdMerge(a);
    if (a.cmd == "blobs") return cmdBlobs(a);
    if (a.cmd == "chain") return cmdChain(a);
    if (a.cmd == "defect-xcal") return cmdDefectXcal(a);
    if (a.cmd == "correct-time") return cmdCorrectTime(a);
    usage();
    return 2;
}
