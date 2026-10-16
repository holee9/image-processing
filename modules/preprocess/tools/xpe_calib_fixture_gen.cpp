/**
 * @file xpe_calib_fixture_gen.cpp
 * @brief Deterministic XCal calibration fixture generator (QA-A-36, #141 #120)
 *
 * The repository carries no XCal fixtures, and binaries are not committed. This
 * tool builds a complete offset/gain/defect set from synthetic frames so GUI,
 * E2E and CI can create one into a temporary directory at run time.
 *
 * Usage:
 *   xpe_calib_fixture_gen --out <dir> [--width 1024] [--height 1024]
 *                         [--seed 0] [--expiry-ms 0]
 *
 *   --out         destination directory (must exist or be creatable)
 *   --width/-h    frame size in pixels (default 1024 x 1024)
 *   --seed        PRNG seed for the synthetic noise (default 0)
 *   --expiry-ms   expiry timestamp written into each file, Unix ms;
 *                 0 (default) means "never expires" (xcal_format.h 0x20)
 *   --gain-poly   also write gain_poly.xcal (XCAL_TYPE_GAIN_POLY); off by
 *                 default so the scalar set stays byte-identical (QA-A-136, #198)
 *   --poly-degree maximum fitted degree, 1-4 (default 2)
 *   --poly-levels dose levels to acquire, 3 or more (default 4)
 *   --caldata     QA-A-248 (#262): build the set from REAL calibration frames
 *                 instead of synthetic ones. <dir> holds dark.raw,
 *                 bright04.raw, bright05.raw, bright06.raw (uint16, headerless,
 *                 width x height) and BPMap.map (uint8 mask). The maps are made
 *                 the way QA-A-245 (test_zz_a241_measure.cpp,
 *                 A245_ExportAndAppPath) made calib-real-v2: offset from the one
 *                 dark frame (100 ms, 25 C), gain from the three flats with NO
 *                 dark reference (they are already offset-corrected, CalData_6
 *                 README), defect = BPMap.map as it is. --width/--height default
 *                 to 3072 in this mode. Not combinable with --gain-poly.
 *   --dark F, --bright F   QA-A-250: build the set from any number of REAL frames
 *                 (headerless little-endian uint16, width x height, offset-
 *                 UNcorrected: the dark pedestal is still in the bright frames).
 *                 Both options repeat. --dark-list L / --bright-list L read the
 *                 same from a text file, one path per line ('#' and empty lines
 *                 ignored, relative paths are relative to the list file).
 *   --gain-dark-ref mean|none|FILE   the dark reference the gain stage subtracts
 *                 from every bright frame: the rounded mean of the dark inputs
 *                 (default), none, or one raw file.
 *   --compare-bpm FILE   a defect map made by someone else (uint8 or uint16, any
 *                 non-zero value = defective). It is NOT used to make defect.xcal;
 *                 agreement with the generated map goes into manifest.json.
 *                 In this mode offset.xcal = mean of the dark inputs, gain.xcal =
 *                 the module's gain generator over the bright inputs, defect.xcal
 *                 = xpe_bpm_generate over both. Refusals (nothing is written):
 *                 fewer than 5 dark frames, fewer than 5 bright frames (gain) or
 *                 10 (defect map), a file that is missing or not width x height x 2
 *                 bytes, a frame with more than 0.1 % of its pixels at 65535, bright
 *                 frames for the gain map that span more than one dose (QA-A-250b,
 *                 QA-A-250c): the signal of a frame is formed exactly as the gain
 *                 generator forms it, the mean over the pixels of max(0, frame -
 *                 gain dark reference) (reference: --gain-dark-ref, none = 0); the
 *                 largest and the smallest signal differ by more than 1.25, or a
 *                 frame has no signal left (exit 12).
 *                 QA-A-250c: the same file twice in one role (same path, same file
 *                 by fs::equivalent, or same sha-256), exit 13.
 *                 QA-A-250d: --out must not exist or must be an EMPTY directory;
 *                 a non-empty --out is refused (exit 14, nothing is written):
 *                 there is no overwrite, a new set goes to a new folder. The set
 *                 is built in a directory of its own, <out>.building-<pid>-<ms>,
 *                 beside --out and published with ONE directory rename onto --out
 *                 once all four files exist. A refusal or a failure (exit 5-8)
 *                 leaves neither --out nor its own work directory, and a work
 *                 directory of another run is never touched.
 *   --bpm-bright F, --bpm-bright-list L   QA-A-250b: the bright frames of the DEFECT
 *                 map, when they are not the gain frames. The gain map needs one
 *                 dose (xpe_calib_generate_gain averages the frames); the defect
 *                 map may use frames of several doses. Without these options the
 *                 --bright frames serve both.
 *
 * Output: offset.xcal, gain.xcal, defect.xcal, manifest.json
 *         (+ gain_poly.xcal with --gain-poly)
 *
 * DETERMINISM. The same arguments produce byte-identical files. Two things make
 * that true and both are deliberate:
 *
 *  - The synthetic frames come from std::mt19937 seeded with --seed, never from
 *    a random_device or the clock.
 *  - `created_epoch_ms` is written as 0 rather than "now". A wall-clock stamp
 *    would change every run and byte-identity is the point of a fixture; the
 *    field is not load-bearing for any consumer (expiry is carried separately
 *    by --expiry-ms).
 *
 * The pixel data itself comes from the SHIPPED generators
 * (`xpe_calib_generate_offset`, `xpe_calib_generate_gain`, `xpe_bpm_generate`),
 * so a fixture exercises the same maths the product does. Those generators
 * write their own files with a wall-clock stamp, so this tool runs them into a
 * scratch file, reads the payload straight back out of it, and rewrites the
 * final artifact with the fixed header.
 *
 * The payload is read with a plain ifstream rather than through the module's
 * global calibration store: `g_calib` and `g_calib_mutex` are internal to the
 * DLL and not exported (measured -- LNK2001 on both). No new export is added.
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"

#ifdef _WIN32
#include <process.h>
#define XPE_GETPID _getpid
#else
#include <unistd.h>
#define XPE_GETPID getpid
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::string outDir;
    uint32_t    width{1024};
    uint32_t    height{1024};
    uint32_t    seed{0};
    uint64_t    expiryMs{0};

    // QA-A-136 (#198): also emit gain_poly.xcal (XCAL_TYPE_GAIN_POLY).
    //
    // Off by default so the existing three fixtures are byte-identical to what
    // this tool produced before -- callers that only want the scalar set get
    // exactly what they got yesterday. The GUI lane needs a polynomial file to
    // exercise its load-time alert and had no way to make one: the repository
    // holds no .xcal, this tool wrote only the scalar type, and the only
    // callers of xpe_calib_generate_gain_polynomial() were module tests.
    bool        gainPoly{false};
    int32_t     polyDegree{2};
    int32_t     polyLevels{4};

    // QA-A-248 (#262): real calibration frames instead of synthetic ones.
    std::string calDir;
    bool        sizeGiven{false};   // --width/--height was passed

    // QA-A-250: any number of real dark/bright frames.
    std::vector<std::string> darkFiles;
    std::vector<std::string> brightFiles;
    std::string              darkList;
    std::string              brightList;
    std::string              gainDarkRef{"mean"};   // mean | none | a file
    std::string              compareBpm;
    std::vector<std::string> bpmBrightFiles;   // QA-A-250b: bright frames of the defect map when they differ from the gain frames
    std::string              bpmBrightList;
    bool                     framesMode{false};
};

void printUsage() {
    std::fprintf(stderr,
        "usage: xpe_calib_fixture_gen --out <dir> [--width N] [--height N]\n"
        "                             [--seed N] [--expiry-ms N]\n"
        "                             [--gain-poly] [--poly-degree N] [--poly-levels N]\n"
        "                             [--caldata <dir>]\n"
        "                             [--dark F]... [--bright F]... [--dark-list L] [--bright-list L]\n"
        "                             [--gain-dark-ref mean|none|F] [--compare-bpm F]\n"
        "                             [--bpm-bright F]... [--bpm-bright-list L]\n"
        "\n"
        "Writes offset.xcal, gain.xcal, defect.xcal and manifest.json into <dir>.\n"
        "The same arguments always produce byte-identical files.\n"
        "\n"
        "  --gain-poly     also write gain_poly.xcal (XCAL_TYPE_GAIN_POLY), a\n"
        "                  per-pixel dose-dependent gain polynomial\n"
        "  --poly-degree N maximum fitted degree, 1-4 (default 2)\n"
        "  --poly-levels N dose levels to acquire, 3 or more (default 4)\n"
        "  --caldata <dir> use real frames: dark.raw, bright04..06.raw, BPMap.map\n"
        "                  (width/height default to 3072; not with --gain-poly)\n"
        "  --dark/--bright F  one real frame per option (repeat), or --dark-list/--bright-list L\n"
        "                  (a text file, one path per line). At least 5 dark and 10 bright frames.\n"
        "  --gain-dark-ref  dark reference for the gain stage: mean (default), none, or a file\n"
        "  --compare-bpm F  report agreement with someone else's defect map (not used for the set)\n"
        "  --bpm-bright F   bright frames of the defect map only (several doses allowed); the\n"
        "                  --bright frames of the gain map must be ONE dose (max/min <= 1.25)\n");
}

bool parseArgs(int argc, char** argv, Options* opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](uint64_t* dst) {
            if (i + 1 >= argc) return false;
            *dst = std::strtoull(argv[++i], nullptr, 10);
            return true;
        };
        uint64_t value = 0;

        if (arg == "--out" || arg == "--caldata") {
            if (i + 1 >= argc) return false;
            (arg == "--out" ? opt->outDir : opt->calDir) = argv[++i];
        } else if (arg == "--width") {
            if (!next(&value)) return false;
            opt->width = static_cast<uint32_t>(value);
            opt->sizeGiven = true;
        } else if (arg == "--height") {
            if (!next(&value)) return false;
            opt->height = static_cast<uint32_t>(value);
            opt->sizeGiven = true;
        } else if (arg == "--seed") {
            if (!next(&value)) return false;
            opt->seed = static_cast<uint32_t>(value);
        } else if (arg == "--expiry-ms") {
            if (!next(&value)) return false;
            opt->expiryMs = value;
        } else if (arg == "--dark" || arg == "--bright" || arg == "--dark-list" || arg == "--bright-list" ||
                   arg == "--gain-dark-ref" || arg == "--compare-bpm" || arg == "--bpm-bright" || arg == "--bpm-bright-list") {
            if (i + 1 >= argc) return false;
            const std::string v = argv[++i];
            if (arg == "--dark") opt->darkFiles.push_back(v);
            else if (arg == "--bright") opt->brightFiles.push_back(v);
            else if (arg == "--dark-list") opt->darkList = v;
            else if (arg == "--bright-list") opt->brightList = v;
            else if (arg == "--gain-dark-ref") opt->gainDarkRef = v;
            else if (arg == "--bpm-bright") opt->bpmBrightFiles.push_back(v);
            else if (arg == "--bpm-bright-list") opt->bpmBrightList = v;
            else opt->compareBpm = v;
        } else if (arg == "--gain-poly") {
            opt->gainPoly = true;
        } else if (arg == "--poly-degree") {
            if (!next(&value)) return false;
            opt->polyDegree = static_cast<int32_t>(value);
        } else if (arg == "--poly-levels") {
            if (!next(&value)) return false;
            opt->polyLevels = static_cast<int32_t>(value);
        } else if (arg == "--help" || arg == "-?") {
            return false;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            return false;
        }
    }
    if (opt->outDir.empty()) return false;
    opt->framesMode = !opt->darkFiles.empty() || !opt->brightFiles.empty() || !opt->darkList.empty() || !opt->brightList.empty();
    if (opt->framesMode) {
        if (!opt->calDir.empty()) {
            std::fprintf(stderr, "--dark/--bright cannot be combined with --caldata\n");
            return false;
        }
        if (opt->gainPoly) {
            std::fprintf(stderr, "--dark/--bright cannot be combined with --gain-poly\n");
            return false;
        }
        if (!opt->sizeGiven) opt->width = opt->height = 3072u;
    } else if (!opt->compareBpm.empty() || opt->gainDarkRef != "mean" || !opt->bpmBrightFiles.empty() || !opt->bpmBrightList.empty()) {
        std::fprintf(stderr, "--gain-dark-ref, --compare-bpm and --bpm-bright belong to the --dark/--bright mode\n");
        return false;
    }
    if (!opt->calDir.empty()) {
        if (opt->gainPoly) {
            std::fprintf(stderr, "--caldata cannot be combined with --gain-poly\n");
            return false;
        }
        if (!opt->sizeGiven) opt->width = opt->height = 3072u;
    }
    if (opt->width == 0 || opt->height == 0) {
        std::fprintf(stderr, "width and height must be greater than 0\n");
        return false;
    }
    if (opt->width > XCAL_MAX_DIM || opt->height > XCAL_MAX_DIM) {
        std::fprintf(stderr, "width and height must not exceed %u\n", XCAL_MAX_DIM);
        return false;
    }
    // Refuse here rather than letting the generator refuse later: its
    // XPE_ERR_INVALID_INPUT does not say which argument was wrong.
    if (opt->gainPoly) {
        if (opt->polyDegree < 1 || opt->polyDegree > 4) {
            std::fprintf(stderr, "--poly-degree must be 1..4\n");
            return false;
        }
        if (opt->polyLevels < 3) {
            std::fprintf(stderr, "--poly-levels must be 3 or more\n");
            return false;
        }
        // Least squares needs degree+1 points; the generator drops the degree
        // instead of failing, so a file would still be written -- just not at
        // the degree that was asked for. Saying so here keeps the fixture
        // honest about what it contains.
        if (opt->polyLevels < opt->polyDegree + 1) {
            std::fprintf(stderr,
                         "--poly-levels %d cannot support --poly-degree %d "
                         "(least squares needs degree+1 points)\n",
                         opt->polyLevels, opt->polyDegree);
            return false;
        }
    }
    return true;
}

/** Reads the FLOAT32 payload back out of an XCal file written by a generator. */
bool readFloatPayload(const std::string& path, size_t expectedCount,
                      std::vector<float>* out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    XCalFileHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (f.gcount() != static_cast<std::streamsize>(sizeof(hdr))) return false;
    if (std::memcmp(hdr.magic, XCAL_MAGIC, 4) != 0) return false;
    if (hdr.pixel_format != static_cast<uint32_t>(XCAL_FMT_FLOAT32)) return false;
    if (hdr.payload_len != static_cast<uint64_t>(expectedCount) * sizeof(float)) return false;

    f.seekg(static_cast<std::streamoff>(hdr.config_json_len), std::ios::cur);
    out->assign(expectedCount, 0.0f);
    f.read(reinterpret_cast<char*>(out->data()),
           static_cast<std::streamsize>(hdr.payload_len));
    return f.gcount() == static_cast<std::streamsize>(hdr.payload_len);
}

XpeImageBuffer u16Buffer(std::vector<uint16_t>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data();
    b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

/** Writes an XCal file with a FIXED created_epoch_ms so runs are comparable. */
XpeErrorCode writeFixture(const std::string& path, XCalType type,
                          XCalPixelFormat format, uint32_t w, uint32_t h,
                          const void* payload, uint64_t payloadLen,
                          uint64_t expiryMs) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = static_cast<uint32_t>(type);
    hdr.pixel_format     = static_cast<uint32_t>(format);
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = 0;                 // fixed: see the determinism note
    hdr.expiry_epoch_ms  = static_cast<int64_t>(expiryMs);
    hdr.payload_len      = payloadLen;
    std::memcpy(hdr.session_id, "fixture\0", 8);

    // Regenerating into a directory that already holds a fixture is the normal
    // case (GUI and CI reuse one path), and write_xcal_file finishes with a
    // rename that an existing destination blocks. Clear it first.
    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());

    return write_xcal_file(path.c_str(), hdr, nullptr, 0,
                           static_cast<const uint8_t*>(payload), payloadLen);
}

std::string sha256Hex(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
    const auto digest = compute_sha256(bytes.data(), bytes.size());

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (uint8_t b : digest) {
        out.push_back(hex[b >> 4]);
        out.push_back(hex[b & 0x0F]);
    }
    return out;
}

/** The manifest keys only a --caldata run adds: where the frames came from and their hashes. */
std::string realManifestKeys(const std::vector<std::pair<std::string, std::string>>& inputs) {
    std::string out = "  \"source\": \"caldata\",\n  \"inputs\": {\n";
    for (size_t i = 0; i < inputs.size(); ++i) {
        out += "    \"" + inputs[i].first + "\": \"" + inputs[i].second + "\"" + (i + 1 < inputs.size() ? ",\n" : "\n");
    }
    return out + "  },\n";
}

/** Reads a whole file; false when it is missing or not exactly `bytes` long. */
bool readExact(const std::string& path, size_t bytes, std::vector<uint8_t>* out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return false;
    }
    const std::streamoff size = f.tellg();
    if (size < 0 || static_cast<size_t>(size) != bytes) {
        std::fprintf(stderr, "%s is %lld bytes, expected %zu (width x height of the set)\n",
                     path.c_str(), static_cast<long long>(size), bytes);
        return false;
    }
    f.seekg(0);
    out->assign(bytes, 0u);
    f.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(bytes));
    return f.gcount() == static_cast<std::streamsize>(bytes);
}

/** Reads a headerless little-endian uint16 frame of `count` pixels. */
bool readFrame16(const std::string& path, size_t count, std::vector<uint16_t>* out) {
    std::vector<uint8_t> bytes;
    if (!readExact(path, count * sizeof(uint16_t), &bytes)) return false;
    out->assign(count, 0u);
    std::memcpy(out->data(), bytes.data(), bytes.size());   // x86-64 and ARM64 are little-endian
    return true;
}

/* ------------------------------------------------------------------ frames */

// Dark frames: a flat pedestal plus seeded Gaussian read noise. The pedestal is
// what the offset map should recover.
std::vector<std::vector<uint16_t>> makeDarkFrames(const Options& opt, int count) {
    std::mt19937 gen(opt.seed);
    std::normal_distribution<double> noise(0.0, 8.0);
    const size_t n = static_cast<size_t>(opt.width) * opt.height;

    std::vector<std::vector<uint16_t>> frames;
    for (int f = 0; f < count; ++f) {
        std::vector<uint16_t> px(n);
        for (size_t i = 0; i < n; ++i) {
            const double v = 300.0 + noise(gen);
            px[i] = static_cast<uint16_t>(v < 0.0 ? 0.0 : (v > 65535.0 ? 65535.0 : v));
        }
        frames.push_back(std::move(px));
    }
    return frames;
}

// Flat frames: a smooth left-to-right sensitivity ramp on the pedestal, so the
// normalised gain map has structure rather than being uniformly 1.0.
//
// `exposure` scales the signal (1.0 is the original level) and `seedOffset`
// picks a distinct noise stream. Both default to the pre-QA-A-136 values, so
// makeFlatFrames(opt, n) produces exactly what it always did -- the scalar
// fixture stays byte-identical.
std::vector<std::vector<uint16_t>> makeFlatFrames(const Options& opt, int count,
                                                  double exposure = 1.0,
                                                  uint32_t seedOffset = 1u) {
    std::mt19937 gen(opt.seed + seedOffset);
    std::normal_distribution<double> noise(0.0, 20.0);
    const size_t n = static_cast<size_t>(opt.width) * opt.height;

    std::vector<std::vector<uint16_t>> frames;
    for (int f = 0; f < count; ++f) {
        std::vector<uint16_t> px(n);
        for (uint32_t y = 0; y < opt.height; ++y) {
            for (uint32_t x = 0; x < opt.width; ++x) {
                // +-10% sensitivity across the width, centred on 1.0
                const double sensitivity =
                    0.9 + 0.2 * (static_cast<double>(x) / (opt.width - 1u ? opt.width - 1u : 1u));
                const double v = 300.0 + 20000.0 * exposure * sensitivity + noise(gen);
                px[static_cast<size_t>(y) * opt.width + x] =
                    static_cast<uint16_t>(v < 0.0 ? 0.0 : (v > 65535.0 ? 65535.0 : v));
            }
        }
        frames.push_back(std::move(px));
    }
    return frames;
}

// A handful of defects at fixed coordinates: two dead (stuck low) and two hot
// (stuck high), placed so they survive any reasonable detector threshold.
struct DefectSpot { uint32_t x, y; bool hot; };

std::vector<DefectSpot> defectSpots(const Options& opt) {
    const uint32_t w = opt.width, h = opt.height;
    return {
        {w / 4u,       h / 4u,       false},
        {w / 2u,       h / 2u,       true},
        {3u * w / 4u,  h / 3u,       true},
        {w / 3u,       3u * h / 4u,  false},
    };
}

void injectDefects(std::vector<uint16_t>& frame, const Options& opt) {
    for (const auto& d : defectSpots(opt)) {
        frame[static_cast<size_t>(d.y) * opt.width + d.x] = d.hot ? 65535u : 0u;
    }
}

/* ------------------------------------------------------- frames mode (QA-A-250) */

constexpr size_t kMinDarkFrames       = 5;      // offset map, gain dark reference, defect map (XpeBpmConfig.min_frames_dark)
constexpr size_t kMinGainFrames       = 5;      // SRS-CALIB-FUNC-024, standard tier (5-10 frames)
constexpr size_t kMinBrightForBpm     = 10;     // XpeBpmConfig.min_frames_bright
constexpr double kSaturatedShareLimit = 0.001;  // a frame with more than 0.1 % of its pixels at 65535 is saturated, not defective
// QA-A-250b: bright frames of the GAIN map must come from one dose. Basis (evidence 10_dose_spread_basis.txt, the 20 cyan_test bright frames, offset level =
// mean of the eight generation darks): inside one dose level the largest max/min of the offset-subtracted frame mean is 1.0712 (Bright_17..19, a first-frame
// step); between two dose levels the smallest step is 1.5593, and no adjacent ratio lies between the two. 1.25 sits between them (1.167 times above the largest
// spread inside a level, 1.247 times below the smallest step between levels). One data set; a panel whose dose steps are smaller than 25 % would pass mixed.
constexpr double kGainDoseRatioLimit  = 1.25;

struct InputFrame {
    std::string           role;
    std::string           path;
    std::vector<uint16_t> px;
    std::string           sha;
    double                mean{0.0};
};

std::string jsonText(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string fixed(double v, int digits) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", digits, v);
    return buf;
}

/** One path per line; '#' and empty lines are skipped; a relative path is relative to the list file. */
bool readList(const std::string& listPath, std::vector<std::string>* out) {
    std::ifstream f(listPath);
    if (!f) {
        std::fprintf(stderr, "cannot open list %s\n", listPath.c_str());
        return false;
    }
    const fs::path base = fs::path(listPath).parent_path();
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t b = 0;
        while (b < line.size() && (line[b] == ' ' || line[b] == '\t')) ++b;
        line.erase(0, b);
        if (line.empty() || line[0] == '#') continue;
        const fs::path p(line);
        out->push_back((p.is_absolute() ? p : base / p).generic_string());
    }
    return true;
}

/** Reads every frame, refusing a wrong size (readFrame16) and a saturated frame. Returns 0, 9 or 11. */
int loadFrames(const std::vector<std::string>& paths, const char* role, size_t pixelCount, std::vector<InputFrame>* out) {
    for (const std::string& p : paths) {
        InputFrame fr;
        fr.role = role;
        fr.path = p;
        if (!readFrame16(p, pixelCount, &fr.px)) return 9;
        uint64_t sum = 0;
        size_t saturated = 0;
        for (uint16_t v : fr.px) {
            sum += v;
            if (v == 65535u) ++saturated;
        }
        fr.mean = static_cast<double>(sum) / static_cast<double>(pixelCount);
        const double share = static_cast<double>(saturated) / static_cast<double>(pixelCount);
        if (share > kSaturatedShareLimit) {
            std::fprintf(stderr, "%s: %.4f %% of the pixels are at 65535 (limit %.2f %%): a saturated frame cannot be averaged into a calibration map\n", p.c_str(),
                         share * 100.0, kSaturatedShareLimit * 100.0);
            return 11;
        }
        fr.sha = sha256Hex(p);
        out->push_back(std::move(fr));
    }
    return 0;
}

/** A defect map from someone else: uint8 (N bytes) or uint16 (2N bytes); flags[i] = 1 when the value is not zero. */
bool readReferenceMask(const std::string& path, size_t pixelCount, std::vector<uint8_t>* flags) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        std::fprintf(stderr, "cannot open %s\n", path.c_str());
        return false;
    }
    if (size != pixelCount && size != pixelCount * 2) {
        std::fprintf(stderr, "%s is %llu bytes, expected %zu (uint8) or %zu (uint16) for width x height\n", path.c_str(), static_cast<unsigned long long>(size), pixelCount,
                     pixelCount * 2);
        return false;
    }
    std::vector<uint8_t> bytes;
    if (!readExact(path, static_cast<size_t>(size), &bytes)) return false;
    flags->assign(pixelCount, 0u);
    for (size_t i = 0; i < pixelCount; ++i) {
        (*flags)[i] = (size == pixelCount) ? (bytes[i] != 0u) : ((bytes[2 * i] | bytes[2 * i + 1]) != 0u);
    }
    return true;
}

std::vector<XpeImageBuffer> u16Buffers(std::vector<InputFrame>& frames, uint32_t w, uint32_t h) {
    std::vector<XpeImageBuffer> bufs;
    for (auto& f : frames) bufs.push_back(u16Buffer(f.px, w, h));
    return bufs;
}

std::vector<XpeImageBuffer> u16BuffersOf(const std::vector<InputFrame*>& frames, uint32_t w, uint32_t h) {
    std::vector<XpeImageBuffer> bufs;
    for (InputFrame* f : frames) bufs.push_back(u16Buffer(f->px, w, h));
    return bufs;
}

/**
 * The signal of a bright frame as the gain generator forms it: the mean over the pixels of max(0, frame - dark reference). @p ref empty means no subtraction.
 */
double gainSignalMean(const std::vector<uint16_t>& frame, const std::vector<uint16_t>& ref) {
    double sum = 0.0;
    for (size_t i = 0; i < frame.size(); ++i) {
        const double corrected = static_cast<double>(frame[i]) - (ref.empty() ? 0.0 : static_cast<double>(ref[i]));
        if (corrected > 0.0) sum += corrected;
    }
    return sum / static_cast<double>(frame.size());
}

/**
 * The gain map averages its bright frames, so they must come from one dose. The dose is judged on the signal the generator will see (gainSignalMean with
 * the reference the generator will get), so the check and the generator cannot disagree. Returns 0 and the max/min ratio, or 12 with the reason on stderr.
 */
int checkSingleDose(const std::vector<InputFrame>& brights, const std::vector<uint16_t>& ref, const std::string& refLabel, double* ratio) {
    const InputFrame* lo = nullptr;
    const InputFrame* hi = nullptr;
    double loSig = 0.0, hiSig = 0.0;
    for (const auto& b : brights) {
        const double sig = gainSignalMean(b.px, ref);
        if (sig <= 0.0) {
            std::fprintf(stderr, "%s: no signal is left after subtracting the gain dark reference (%s): mean of max(0, frame - reference) is %.3f; not a bright frame for this reference\n",
                         b.path.c_str(), refLabel.c_str(), sig);
            return 12;
        }
        if (!lo || sig < loSig) { lo = &b; loSig = sig; }
        if (!hi || sig > hiSig) { hi = &b; hiSig = sig; }
    }
    *ratio = hiSig / loSig;
    if (*ratio > kGainDoseRatioLimit) {
        std::fprintf(stderr,
                     "bright frames for the gain map span more than one dose: gain signal (mean of max(0, frame - gain dark reference %s)) from %.1f (%s) to %.1f (%s), ratio %.3f, limit %.2f. The gain "
                     "generator averages its frames and assumes one dose; give frames of one dose with --bright (frames of several doses may go to the defect map with --bpm-bright)\n",
                     refLabel.c_str(), loSig, lo->path.c_str(), hiSig, hi->path.c_str(), *ratio, kGainDoseRatioLimit);
        return 12;
    }
    return 0;
}

// QA-A-250c: the same file twice in one role. A path duplicate is the same normalized path or the same file (fs::equivalent: links, case, ..), a content
// duplicate the same sha-256. The first would be counted twice toward the frame minimum and averaged twice; a duplicate is refused (exit 13), not skipped.
int rejectPathDuplicates(const std::vector<std::string>& paths, const char* role) {
    for (size_t i = 0; i < paths.size(); ++i) {
        for (size_t j = i + 1; j < paths.size(); ++j) {
            std::error_code ec;
            const bool same = fs::path(paths[i]).lexically_normal() == fs::path(paths[j]).lexically_normal() || fs::equivalent(paths[i], paths[j], ec);
            if (same) {
                std::fprintf(stderr, "duplicate %s frame: '%s' and '%s' are the same file; a frame is given once per role\n", role, paths[i].c_str(), paths[j].c_str());
                return 13;
            }
        }
    }
    return 0;
}

int rejectContentDuplicates(const std::vector<const InputFrame*>& frames, const char* role) {
    for (size_t i = 0; i < frames.size(); ++i) {
        for (size_t j = i + 1; j < frames.size(); ++j) {
            if (frames[i]->sha == frames[j]->sha) {
                std::fprintf(stderr, "duplicate %s frame: '%s' and '%s' have the same content (sha-256 %s); a frame is given once per role\n", role, frames[i]->path.c_str(),
                             frames[j]->path.c_str(), frames[i]->sha.c_str());
                return 13;
            }
        }
    }
    return 0;
}

// Test seam: XPE_FIXTURE_GEN_FAIL=<stage> forces that stage to fail (gain, bpm, manifest, commit) so the rollback paths can be exercised.
bool forcedFailure(const char* stage) {
#ifdef _WIN32
    char* v = nullptr;
    size_t len = 0;
    const bool hit = _dupenv_s(&v, &len, "XPE_FIXTURE_GEN_FAIL") == 0 && v != nullptr && std::strcmp(v, stage) == 0;
    std::free(v);
    return hit;
#else
    const char* v = std::getenv("XPE_FIXTURE_GEN_FAIL");
    return v != nullptr && std::strcmp(v, stage) == 0;
#endif
}

// The work directory is a sibling of --out with a name of this run alone (<out>.building-<pid>-<ms>), so a leftover of a killed run or a run on the same
// --out at the same time never shares it. It is created empty and is never cleaned up on behalf of another run. It is removed in every case (after a
// successful publish it no longer exists), together with the parent directories this run had to create.
struct WorkDir {
    fs::path path, firstExisting;
    bool     created{false};
    explicit WorkDir(const fs::path& out) {
        fs::path abs = fs::absolute(out).lexically_normal();
        if (abs.filename().empty()) abs = abs.parent_path();
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        path = abs.parent_path() / (abs.filename().string() + ".building-" + std::to_string(XPE_GETPID()) + "-" + std::to_string(ms));
        firstExisting = abs;
        std::error_code ec;
        while (!firstExisting.empty() && !fs::exists(firstExisting, ec) && firstExisting != firstExisting.parent_path()) firstExisting = firstExisting.parent_path();
        fs::create_directories(path.parent_path(), ec);
        created = fs::create_directory(path, ec);   // false when the name is taken: never adopt a directory that is not ours
    }
    bool ok() const { return created; }
    ~WorkDir() {
        std::error_code ec;
        if (created) fs::remove_all(path, ec);
        for (fs::path d = path.parent_path(); !d.empty() && d != firstExisting && d != d.parent_path(); d = d.parent_path()) fs::remove(d, ec);   // only if empty
    }
};

// QA-A-250d: publishes the finished set with one directory rename onto --out. There is no overwrite and nothing is moved aside: --out does not exist, or is
// an empty directory (removed here, put back if the rename fails). A failed rename is a failure (exit 8), never reported as success.
int publishSet(const fs::path& work, const fs::path& out) {
    std::error_code ec;
    const bool wasEmptyDir = fs::is_directory(out, ec);
    if (wasEmptyDir) {
        fs::remove(out, ec);   // removes an empty directory only
        if (ec) {
            std::fprintf(stderr, "cannot publish into %s: %s\n", out.string().c_str(), ec.message().c_str());
            return 8;
        }
    }
    if (forcedFailure("commit")) ec = std::make_error_code(std::errc::io_error);
    else fs::rename(work, out, ec);
    if (ec) {
        if (wasEmptyDir) fs::create_directory(out, ec);
        std::fprintf(stderr, "publishing the finished set to %s failed; nothing was published\n", out.string().c_str());
        return 8;
    }
    return 0;
}

int runFramesMode(const Options& opt) {
    const fs::path out(opt.outDir);
    const size_t pixelCount = static_cast<size_t>(opt.width) * opt.height;

    std::vector<std::string> darkPaths = opt.darkFiles, brightPaths = opt.brightFiles;
    if (!opt.darkList.empty() && !readList(opt.darkList, &darkPaths)) return 9;
    if (!opt.brightList.empty() && !readList(opt.brightList, &brightPaths)) return 9;
    std::vector<std::string> bpmPaths = opt.bpmBrightFiles;
    if (!opt.bpmBrightList.empty() && !readList(opt.bpmBrightList, &bpmPaths)) return 9;
    const bool separateBpm = !bpmPaths.empty();

    {
        std::error_code ec;
        if (fs::exists(out, ec)) {
            if (!fs::is_directory(out, ec)) {
                std::fprintf(stderr, "cannot create output directory: %s exists and is not a directory\n", opt.outDir.c_str());
                return 3;
            }
            if (!fs::is_empty(out, ec)) {
                std::fprintf(stderr, "output directory %s is not empty: nothing was written. A set is never written over another; give a new folder (and point the application at it)\n",
                             opt.outDir.c_str());
                return 14;
            }
        }
    }
    if (const int rc = rejectPathDuplicates(darkPaths, "dark")) return rc;
    if (const int rc = rejectPathDuplicates(brightPaths, "bright")) return rc;
    if (separateBpm) {
        if (const int rc = rejectPathDuplicates(bpmPaths, "bpm_bright")) return rc;
    }

    // Counts first: it costs nothing and needs no file.
    if (darkPaths.size() < kMinDarkFrames) {
        std::fprintf(stderr, "too few dark frames: %zu given, at least %zu are needed (offset map, gain dark reference and defect map)\n", darkPaths.size(), kMinDarkFrames);
        return 10;
    }
    if (brightPaths.size() < kMinGainFrames) {
        std::fprintf(stderr, "too few bright frames: %zu given, at least %zu are needed for the gain map (SRS-CALIB-FUNC-024 standard tier)\n", brightPaths.size(), kMinGainFrames);
        return 10;
    }
    const size_t bpmCount = separateBpm ? bpmPaths.size() : brightPaths.size();
    if (bpmCount < kMinBrightForBpm) {
        std::fprintf(stderr,
                     "too few bright frames: %zu given, at least %zu are needed for the defect map (XpeBpmConfig.min_frames_bright; the defect map may use frames of "
                     "several doses, give them with --bpm-bright)\n",
                     bpmCount, kMinBrightForBpm);
        return 10;
    }

    std::vector<InputFrame> darks, brights;
    if (const int rc = loadFrames(darkPaths, "dark", pixelCount, &darks)) return rc;
    if (const int rc = loadFrames(brightPaths, "bright", pixelCount, &brights)) return rc;

    // The defect map's bright frames: the gain frames themselves, or their own list (frames named in both lists are read once).
    std::vector<InputFrame> bpmOnly;
    std::vector<InputFrame*> bpmFrames;
    if (!separateBpm) {
        for (auto& b : brights) bpmFrames.push_back(&b);
    } else {
        bpmOnly.reserve(bpmPaths.size());
        for (const std::string& path : bpmPaths) {
            InputFrame* shared = nullptr;
            for (auto& b : brights) {
                std::error_code same;
                if (b.path == path || fs::equivalent(b.path, path, same)) shared = &b;
            }
            if (shared) {
                shared->role = "bright_and_bpm_bright";
                bpmFrames.push_back(shared);
                continue;
            }
            std::vector<InputFrame> one;
            if (const int rc = loadFrames(std::vector<std::string>{path}, "bpm_bright", pixelCount, &one)) return rc;
            bpmOnly.push_back(std::move(one[0]));
            bpmFrames.push_back(&bpmOnly.back());
        }
    }

    {
        std::vector<const InputFrame*> d, b, k;
        for (const auto& f : darks) d.push_back(&f);
        for (const auto& f : brights) b.push_back(&f);
        for (const InputFrame* f : bpmFrames) k.push_back(f);
        if (const int rc = rejectContentDuplicates(d, "dark")) return rc;
        if (const int rc = rejectContentDuplicates(b, "bright")) return rc;
        if (separateBpm) {
            if (const int rc = rejectContentDuplicates(k, "bpm_bright")) return rc;
        }
    }

    // The dark reference of the gain stage (read before the dose check: the check uses the same reference as the generator).
    std::vector<uint16_t> refPixels;
    InputFrame refFile;
    const bool refIsFile = opt.gainDarkRef != "mean" && opt.gainDarkRef != "none";
    if (opt.gainDarkRef == "mean") {
        std::vector<uint32_t> acc(pixelCount, 0u);
        for (const auto& d : darks) {
            for (size_t i = 0; i < pixelCount; ++i) acc[i] += d.px[i];
        }
        refPixels.assign(pixelCount, 0u);
        const uint32_t n = static_cast<uint32_t>(darks.size());
        for (size_t i = 0; i < pixelCount; ++i) refPixels[i] = static_cast<uint16_t>((acc[i] + n / 2u) / n);
    } else if (refIsFile) {
        std::vector<std::string> one{opt.gainDarkRef};
        std::vector<InputFrame> got;
        if (const int rc = loadFrames(one, "gain_dark_reference", pixelCount, &got)) return rc;
        refFile = std::move(got[0]);
        refPixels = refFile.px;
    }
    const std::string refLabel = refIsFile ? fs::path(opt.gainDarkRef).generic_string() : opt.gainDarkRef;
    double doseRatio = 0.0;
    if (const int rc = checkSingleDose(brights, refPixels, refLabel, &doseRatio)) return rc;

    std::vector<uint8_t> theirs;
    if (!opt.compareBpm.empty() && !readReferenceMask(opt.compareBpm, pixelCount, &theirs)) return 9;

    // ---- generation, into a work directory of this run beside --out; --out is touched only by publishSet, after all four files exist
    WorkDir workDir(out);
    if (!workDir.ok()) {
        std::fprintf(stderr, "cannot create the work directory %s\n", workDir.path.string().c_str());
        return 3;
    }
    const fs::path work = workDir.path;
    const std::string offsetPath = (work / "offset.xcal").string(), gainPath = (work / "gain.xcal").string(), defectPath = (work / "defect.xcal").string();
    const std::string offsetScratch = (work / "_scratch_offset.xcal").string(), gainScratch = (work / "_scratch_gain.xcal").string();
    for (const std::string& tmp : {offsetScratch, gainScratch}) {
        std::remove(tmp.c_str());
        std::remove((tmp + ".tmp").c_str());
    }

    {
        std::vector<XpeImageBuffer> bufs = u16Buffers(darks, opt.width, opt.height);
        if (xpe_calib_generate_offset(bufs.data(), static_cast<int32_t>(bufs.size()), 100.0f, 25.0f, offsetScratch.c_str(), nullptr) != XPE_OK) {
            std::fprintf(stderr, "xpe_calib_generate_offset failed\n");
            return 5;
        }
        std::vector<float> payload;
        if (!readFloatPayload(offsetScratch, pixelCount, &payload)) {
            std::fprintf(stderr, "reading back the generated offset map failed\n");
            return 5;
        }
        if (writeFixture(offsetPath, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, opt.width, opt.height, payload.data(), payload.size() * sizeof(float), opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing offset.xcal failed\n");
            return 5;
        }
    }
    {
        std::vector<XpeImageBuffer> bufs = u16Buffers(brights, opt.width, opt.height);
        XpeImageBuffer refBuf{};
        if (!refPixels.empty()) refBuf = u16Buffer(refPixels, opt.width, opt.height);
        if (forcedFailure("gain") || xpe_calib_generate_gain(bufs.data(), static_cast<int32_t>(bufs.size()), refPixels.empty() ? nullptr : &refBuf, gainScratch.c_str(), nullptr) != XPE_OK) {
            std::fprintf(stderr, "xpe_calib_generate_gain failed\n");
            return 6;
        }
        std::vector<float> payload;
        if (!readFloatPayload(gainScratch, pixelCount, &payload)) {
            std::fprintf(stderr, "reading back the generated gain map failed\n");
            return 6;
        }
        if (writeFixture(gainPath, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, opt.width, opt.height, payload.data(), payload.size() * sizeof(float), opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing gain.xcal failed\n");
            return 6;
        }
    }
    XpeBpmConfig cfg{};
    cfg.lambda_dark       = 8.0f;
    cfg.mask_size_dark    = 32u;
    cfg.tolerance_pct     = 0.07f;
    cfg.mask_size_bright  = 128u;
    cfg.min_frames_dark   = 5u;
    cfg.min_frames_bright = 10u;
    std::vector<uint8_t> mask(pixelCount, 0u);
    size_t defectCount = 0;
    {
        std::vector<XpeImageBuffer> darkBufs = u16Buffers(darks, opt.width, opt.height), brightBufs = u16BuffersOf(bpmFrames, opt.width, opt.height);
        XpeImageBuffer maskBuf{};
        maskBuf.data = mask.data();
        maskBuf.width = opt.width;
        maskBuf.height = opt.height;
        maskBuf.bitsAllocated = 8;
        maskBuf.bitsStored = 8;
        maskBuf.format = XPE_PIXEL_UINT8;
        maskBuf.dataSize = mask.size();
        const XpeErrorCode rc = forcedFailure("bpm") ? XPE_ERR_PROCESSING_FAILED
                                                     : xpe_bpm_generate(darkBufs.data(), static_cast<uint32_t>(darkBufs.size()), brightBufs.data(), static_cast<uint32_t>(brightBufs.size()), &cfg, &maskBuf);
        if (rc != XPE_OK) {
            std::fprintf(stderr, "xpe_bpm_generate failed (%d)\n", static_cast<int>(rc));
            return 7;
        }
        for (uint8_t m : mask) {
            if (m) ++defectCount;
        }
        if (writeFixture(defectPath, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, opt.width, opt.height, mask.data(), mask.size(), opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing defect.xcal failed\n");
            return 7;
        }
    }
    for (const std::string& tmp : {offsetScratch, gainScratch}) {
        std::remove(tmp.c_str());
        std::remove((tmp + ".tmp").c_str());
    }

    // ---- agreement with someone else's defect map (reported, never used)
    std::string comparison;
    if (!opt.compareBpm.empty()) {
        size_t ours = 0, their = 0, both = 0;
        for (size_t i = 0; i < pixelCount; ++i) {
            const bool o = mask[i] != 0u, t = theirs[i] != 0u;
            ours += o;
            their += t;
            both += (o && t);
        }
        const double recall = their ? static_cast<double>(both) / static_cast<double>(their) : 0.0;
        const double precision = ours ? static_cast<double>(both) / static_cast<double>(ours) : 0.0;
        const size_t theirNegatives = pixelCount - their;
        const double falsePositiveRate = theirNegatives ? static_cast<double>(ours - both) / static_cast<double>(theirNegatives) : 0.0;
        comparison = "  \"bpm_comparison\": {\n    \"reference_file\": \"" + jsonText(fs::path(opt.compareBpm).generic_string()) + "\",\n    \"generated\": " + std::to_string(ours) +
                     ",\n    \"reference\": " + std::to_string(their) + ",\n    \"both\": " + std::to_string(both) + ",\n    \"recall\": " + fixed(recall, 6) +
                     ",\n    \"precision\": " + fixed(precision, 6) + ",\n    \"false_positive_rate\": " + fixed(falsePositiveRate, 8) + "\n  },\n";
        std::printf("bpm comparison: generated %zu, reference %zu, both %zu, recall %.6f, precision %.6f, false positive rate %.8f\n", ours, their, both, recall, precision,
                    falsePositiveRate);
    }

    // ---- manifest
    const std::string offsetSha = sha256Hex(offsetPath), gainSha = sha256Hex(gainPath), defectSha = sha256Hex(defectPath);
    std::string inputs;
    std::vector<const InputFrame*> all;
    for (const auto& d : darks) all.push_back(&d);
    for (const auto& b : brights) all.push_back(&b);
    for (const auto& b : bpmOnly) all.push_back(&b);
    if (refIsFile) all.push_back(&refFile);
    for (size_t i = 0; i < all.size(); ++i) {
        inputs += "    {\"role\": \"" + all[i]->role + "\", \"path\": \"" + jsonText(fs::path(all[i]->path).generic_string()) + "\", \"sha256\": \"" + all[i]->sha +
                  "\", \"mean\": " + fixed(all[i]->mean, 3) + "}" + (i + 1 < all.size() ? ",\n" : "\n");
    }
    std::ofstream manifest((work / "manifest.json").string(), std::ios::binary);
    if (!manifest || forcedFailure("manifest")) {
        std::fprintf(stderr, "writing manifest.json failed\n");
        return 8;
    }
    manifest << "{\n  \"generator\": \"xpe_calib_fixture_gen\",\n  \"mode\": \"frames\",\n  \"width\": " << opt.width << ",\n  \"height\": " << opt.height
             << ",\n  \"expiry_epoch_ms\": " << opt.expiryMs << ",\n  \"frames\": {\"dark\": " << darks.size() << ", \"bright\": " << brights.size()
             << (separateBpm ? ", \"bpm_bright\": " + std::to_string(bpmFrames.size()) : std::string()) << "},\n  \"settings\": {\n    \"offset\": {\"exposure_ms\": 100, \"temperature_c\": 25},\n    \"gain_dark_reference\": \"" << jsonText(opt.gainDarkRef)
             << "\",\n    \"bpm\": {\"lambda_dark\": " << fixed(cfg.lambda_dark, 1) << ", \"mask_size_dark\": " << cfg.mask_size_dark << ", \"mask_size_bright\": "
             << cfg.mask_size_bright << ", \"tolerance_pct\": " << fixed(cfg.tolerance_pct, 3) << ", \"min_frames_dark\": " << cfg.min_frames_dark
             << ", \"min_frames_bright\": " << cfg.min_frames_bright << "},\n    \"saturated_share_limit\": " << fixed(kSaturatedShareLimit, 4) << ",\n    \"gain_dose_ratio\": " << fixed(doseRatio, 4)
             << ",\n    \"gain_dose_ratio_limit\": " << fixed(kGainDoseRatioLimit, 2)
             << ",\n    \"gain_dose_basis\": \"mean over pixels of max(0, bright - gain dark reference); reference: " << jsonText(refLabel) << "\"\n  },\n  \"inputs\": [\n"
             << inputs << "  ],\n  \"defect_pixels\": " << defectCount << ",\n"
             << comparison << "  \"files\": {\n    \"offset.xcal\": \"" << offsetSha << "\",\n    \"gain.xcal\": \"" << gainSha << "\",\n    \"defect.xcal\": \"" << defectSha
             << "\"\n  }\n}\n";
    manifest.close();
    if (!manifest) {
        std::fprintf(stderr, "writing manifest.json failed\n");
        return 8;
    }
    if (const int rc = publishSet(work, out)) return rc;

    std::printf("offset.xcal  %s\ngain.xcal    %s\ndefect.xcal  %s  (%zu defect pixels)\nmanifest.json written to %s\n", offsetSha.c_str(), gainSha.c_str(), defectSha.c_str(),
                defectCount, (out / "manifest.json").string().c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, &opt)) {
        printUsage();
        return 2;
    }

    if (!opt.framesMode) {   // the frames path creates --out itself, only when the whole set exists (QA-A-250c)
        std::error_code ec;
        fs::create_directories(opt.outDir, ec);
        if (!fs::is_directory(opt.outDir)) {
            std::fprintf(stderr, "cannot create output directory: %s\n", opt.outDir.c_str());
            return 3;
        }
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 4;
    }

    if (opt.framesMode) {   // QA-A-250: its own path; the synthetic and --caldata paths below are untouched
        const int rc = runFramesMode(opt);
        xpe_preprocess_shutdown();
        return rc;
    }

    const fs::path out(opt.outDir);
    const std::string offsetPath = (out / "offset.xcal").string();
    const std::string gainPath   = (out / "gain.xcal").string();
    const std::string defectPath = (out / "defect.xcal").string();
    // One scratch name per stage, each removed before use. A shared name failed
    // on the second stage, and a leftover from a previous run failed the first:
    // write_xcal_file renames its .tmp onto the destination and an existing file
    // there blocks the rename (measured -9 IO_FAILED, the same shape QA-A-25 and
    // QA-A-35 hit).
    const std::string offsetScratch = (out / "_scratch_offset.xcal").string();
    const std::string gainScratch   = (out / "_scratch_gain.xcal").string();
    for (const std::string& tmp : {offsetScratch, gainScratch}) {
        std::remove(tmp.c_str());
        std::remove((tmp + ".tmp").c_str());
    }
    // QA-A-136 (#198): the polynomial path makes one intermediate gain map per
    // dose level. They are collected here and removed at the end -- the caller
    // asked for a polynomial fixture, not for the maps it was fitted from.
    const std::string gainPolyPath = (out / "gain_poly.xcal").string();
    std::vector<std::string> scratchFiles;
    if (opt.gainPoly) {
        std::remove(gainPolyPath.c_str());
        std::remove((gainPolyPath + ".tmp").c_str());
    }
    const size_t pixelCount = static_cast<size_t>(opt.width) * opt.height;

    // QA-A-248 (#262): with --caldata the frames come from files. They are all
    // read before anything is written, so a missing or wrongly sized input
    // leaves no half-made set behind.
    const bool real = !opt.calDir.empty();
    std::vector<uint16_t> realDark;
    std::vector<std::vector<uint16_t>> realFlats(3);
    std::vector<uint8_t> realMask;
    std::vector<std::pair<std::string, std::string>> inputShas;   // file name, sha-256
    if (real) {
        const fs::path in(opt.calDir);
        const char* const flatNames[3] = {"bright04.raw", "bright05.raw", "bright06.raw"};
        bool ok = readFrame16((in / "dark.raw").string(), pixelCount, &realDark);
        for (int k = 0; ok && k < 3; ++k) {
            ok = readFrame16((in / flatNames[k]).string(), pixelCount, &realFlats[static_cast<size_t>(k)]);
        }
        ok = ok && readExact((in / "BPMap.map").string(), pixelCount, &realMask);
        if (!ok) {
            xpe_preprocess_shutdown();
            return 9;
        }
        for (const char* n : {"dark.raw", "bright04.raw", "bright05.raw", "bright06.raw", "BPMap.map"}) {
            inputShas.emplace_back(n, sha256Hex((in / n).string()));
        }
    }

    /* ---------------------------------------------------------- offset ---- */
    {
        auto darks = real ? std::vector<std::vector<uint16_t>>{realDark} : makeDarkFrames(opt, 8);
        std::vector<XpeImageBuffer> bufs;
        for (auto& d : darks) bufs.push_back(u16Buffer(d, opt.width, opt.height));

        // The shipped generator does the averaging; it writes its own file with
        // a wall-clock stamp, so the result is reloaded and rewritten fixed.
        if (xpe_calib_generate_offset(bufs.data(), static_cast<int32_t>(bufs.size()),
                                      100.0f, 25.0f, offsetScratch.c_str(), nullptr) != XPE_OK) {
            std::fprintf(stderr, "xpe_calib_generate_offset failed\n");
            return 5;
        }
        std::vector<float> payload;
        if (!readFloatPayload(offsetScratch, pixelCount, &payload)) {
            std::fprintf(stderr, "reading back the generated offset map failed\n");
            return 5;
        }
        if (writeFixture(offsetPath, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32,
                         opt.width, opt.height, payload.data(),
                         payload.size() * sizeof(float), opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing offset.xcal failed\n");
            return 5;
        }
    }

    /* ------------------------------------------------------------ gain ---- */
    {
        auto flats = real ? realFlats : makeFlatFrames(opt, 4);
        std::vector<XpeImageBuffer> bufs;
        for (auto& f : flats) bufs.push_back(u16Buffer(f, opt.width, opt.height));

        auto darkRef = makeDarkFrames(opt, real ? 0 : 1);
        XpeImageBuffer darkBuf = darkRef.empty() ? XpeImageBuffer{} : u16Buffer(darkRef[0], opt.width, opt.height);

        // Real flats are already offset-corrected: no dark reference (QA-A-245).
        if (xpe_calib_generate_gain(bufs.data(), static_cast<int32_t>(bufs.size()),
                                    real ? nullptr : &darkBuf, gainScratch.c_str(), nullptr) != XPE_OK) {
            std::fprintf(stderr, "xpe_calib_generate_gain failed\n");
            return 6;
        }
        std::vector<float> payload;
        if (!readFloatPayload(gainScratch, pixelCount, &payload)) {
            std::fprintf(stderr, "reading back the generated gain map failed\n");
            return 6;
        }
        if (writeFixture(gainPath, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32,
                         opt.width, opt.height, payload.data(),
                         payload.size() * sizeof(float), opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing gain.xcal failed\n");
            return 6;
        }
    }

    /* ------------------------------------------------- gain polynomial ---- */
    //
    // QA-A-136 (#198). xpe_calib_generate_gain_polynomial() takes PATHS TO GAIN
    // FILES, not frames, so one scalar gain map is generated per dose level
    // first and the polynomial is fitted across those.
    //
    // The dose levels are expressed in the same units as the pixel values, as
    // the rest of the calibration path does (QA-A-121 note in gain_correct.cpp,
    // cyan_test's CalSet levels named by their ADU): each level's exposure
    // multiplies the flat-field signal, and the dose handed to the fit is that
    // level's nominal signal. Strictly ascending, so the QA-A-124 sort guard
    // accepts it.
    //
    // The file this writes is not byte-identical to a rerun of the scalar set
    // because it is a different file; the scalar three are unaffected.
    if (opt.gainPoly) {
        std::vector<std::string> levelPaths;
        std::vector<const char*> levelPtrs;
        std::vector<double>      doses;

        for (int32_t lv = 0; lv < opt.polyLevels; ++lv) {
            // 0.40, 0.60, 0.80, 1.00, ... -- ascending, spanning a useful part
            // of the range without clipping at the top level.
            const double exposure = 0.40 + 0.20 * static_cast<double>(lv);
            const std::string lvlScratch =
                (out / ("_scratch_gain_lv" + std::to_string(lv) + ".xcal")).string();
            const std::string lvlPath =
                (out / ("_scratch_gainmap_lv" + std::to_string(lv) + ".xcal")).string();

            auto lvlFlats = makeFlatFrames(opt, 4, exposure,
                                           2u + static_cast<uint32_t>(lv));
            std::vector<XpeImageBuffer> lvlBufs;
            for (auto& f : lvlFlats) lvlBufs.push_back(u16Buffer(f, opt.width, opt.height));

            auto lvlDark = makeDarkFrames(opt, 1);
            XpeImageBuffer lvlDarkBuf = u16Buffer(lvlDark[0], opt.width, opt.height);

            if (xpe_calib_generate_gain(lvlBufs.data(),
                                        static_cast<int32_t>(lvlBufs.size()),
                                        &lvlDarkBuf, lvlScratch.c_str(),
                                        nullptr) != XPE_OK) {
                std::fprintf(stderr, "generating gain map for dose level %d failed\n", lv);
                return 7;
            }
            // The polynomial generator reads these back through the loader, so
            // they have to be real XCal gain files -- rewrite with the fixed
            // header like every other artifact here.
            std::vector<float> lvlPayload;
            if (!readFloatPayload(lvlScratch, pixelCount, &lvlPayload)) {
                std::fprintf(stderr, "reading back gain map for level %d failed\n", lv);
                return 7;
            }
            if (writeFixture(lvlPath, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32,
                             opt.width, opt.height, lvlPayload.data(),
                             lvlPayload.size() * sizeof(float), opt.expiryMs) != XPE_OK) {
                std::fprintf(stderr, "writing gain map for level %d failed\n", lv);
                return 7;
            }

            levelPaths.push_back(lvlPath);
            doses.push_back(20000.0 * exposure);
            scratchFiles.push_back(lvlScratch);
            scratchFiles.push_back(lvlPath);
        }
        for (const auto& s : levelPaths) levelPtrs.push_back(s.c_str());

        if (xpe_calib_generate_gain_polynomial(levelPtrs.data(), doses.data(),
                                               static_cast<int32_t>(doses.size()),
                                               opt.polyDegree,
                                               gainPolyPath.c_str()) != XPE_OK) {
            std::fprintf(stderr, "xpe_calib_generate_gain_polynomial failed\n");
            return 7;
        }
    }

    /* ---------------------------------------------------------- defect ---- */
    size_t defectCount = 0;
    if (real) {
        for (uint8_t m : realMask) if (m) ++defectCount;
        if (writeFixture(defectPath, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK,
                         opt.width, opt.height, realMask.data(), realMask.size(),
                         opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing defect.xcal failed\n");
            return 7;
        }
    } else {
        auto darks  = makeDarkFrames(opt, 5);
        auto brights = makeFlatFrames(opt, 10);
        for (auto& d : darks)   injectDefects(d, opt);
        for (auto& b : brights) injectDefects(b, opt);

        std::vector<XpeImageBuffer> darkBufs, brightBufs;
        for (auto& d : darks)   darkBufs.push_back(u16Buffer(d, opt.width, opt.height));
        for (auto& b : brights) brightBufs.push_back(u16Buffer(b, opt.width, opt.height));

        std::vector<uint8_t> mask(pixelCount, 0u);
        XpeImageBuffer maskBuf{};
        maskBuf.data = mask.data();
        maskBuf.width = opt.width; maskBuf.height = opt.height;
        maskBuf.bitsAllocated = 8; maskBuf.bitsStored = 8;
        maskBuf.format = XPE_PIXEL_UINT8;
        maskBuf.dataSize = mask.size();

        // The documented defaults (preprocess_api.h:826-836). A zero-initialised
        // config fails validation -- mask_size_dark has a floor of 32 and
        // mask_size_bright a floor of 128 -- which is what made the first run
        // report -1 and fall through to the direct marking below.
        XpeBpmConfig cfg{};
        cfg.lambda_dark       = 8.0f;
        cfg.mask_size_dark    = 32u;
        cfg.tolerance_pct     = 0.07f;
        cfg.mask_size_bright  = 128u;
        cfg.min_frames_dark   = 5u;
        cfg.min_frames_bright = 10u;
        const XpeErrorCode rc = xpe_bpm_generate(
            darkBufs.data(), static_cast<uint32_t>(darkBufs.size()),
            brightBufs.data(), static_cast<uint32_t>(brightBufs.size()),
            &cfg, &maskBuf);

        if (rc != XPE_OK) {
            // The detector is not the subject of this fixture. It legitimately
            // declines frames smaller than its 128-pixel bright window, so a
            // small --width/--height still produces a usable set: mark the
            // injected coordinates directly, and say so on stderr rather than
            // pretending the detector ran.
            std::fprintf(stderr,
                         "note: xpe_bpm_generate returned %d; marking the "
                         "injected coordinates directly\n", static_cast<int>(rc));
            std::fill(mask.begin(), mask.end(), static_cast<uint8_t>(0));
            for (const auto& d : defectSpots(opt)) {
                mask[static_cast<size_t>(d.y) * opt.width + d.x] = 1u;
            }
        }
        for (uint8_t m : mask) if (m) ++defectCount;

        if (writeFixture(defectPath, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK,
                         opt.width, opt.height, mask.data(), mask.size(),
                         opt.expiryMs) != XPE_OK) {
            std::fprintf(stderr, "writing defect.xcal failed\n");
            return 7;
        }
    }

    for (const std::string& tmp : {offsetScratch, gainScratch}) {
        std::remove(tmp.c_str());
        std::remove((tmp + ".tmp").c_str());
    }
    xpe_preprocess_shutdown();

    /* --------------------------------------------------------- manifest --- */
    const std::string offsetSha = sha256Hex(offsetPath);
    const std::string gainSha   = sha256Hex(gainPath);
    const std::string defectSha = sha256Hex(defectPath);

    const std::string manifestPath = (out / "manifest.json").string();
    std::ofstream manifest(manifestPath, std::ios::binary);
    if (!manifest) {
        std::fprintf(stderr, "writing manifest.json failed\n");
        return 8;
    }
    manifest << "{\n"
             << "  \"generator\": \"xpe_calib_fixture_gen\",\n"
             << "  \"seed\": " << opt.seed << ",\n"
             << "  \"width\": " << opt.width << ",\n"
             << "  \"height\": " << opt.height << ",\n"
             << "  \"expiry_epoch_ms\": " << opt.expiryMs << ",\n"
             << "  \"defect_pixels\": " << defectCount << ",\n"
             << (real ? realManifestKeys(inputShas) : std::string())
             << "  \"files\": {\n"
             << "    \"offset.xcal\": \"" << offsetSha << "\",\n"
             << "    \"gain.xcal\": \""   << gainSha   << "\",\n"
             << "    \"defect.xcal\": \"" << defectSha << "\""
             << (opt.gainPoly
                     ? ",\n    \"gain_poly.xcal\": \"" + sha256Hex(gainPolyPath) + "\"\n"
                     : std::string("\n"))
             << "  }\n"
             << "}\n";
    manifest.close();

    std::printf("offset.xcal  %s\n", offsetSha.c_str());
    std::printf("gain.xcal    %s\n", gainSha.c_str());
    std::printf("defect.xcal  %s  (%zu defect pixels)\n", defectSha.c_str(), defectCount);
    if (opt.gainPoly) {
        std::printf("gain_poly.xcal %s  (degree<=%d, %d dose levels)\n",
                    sha256Hex(gainPolyPath).c_str(), opt.polyDegree, opt.polyLevels);
    }
    // The intermediate per-level gain maps are an implementation detail of the
    // fit; the caller asked for a polynomial fixture, not for them.
    for (const std::string& tmp : scratchFiles) {
        std::remove(tmp.c_str());
        std::remove((tmp + ".tmp").c_str());
    }
    std::printf("manifest.json written to %s\n", manifestPath.c_str());
    return 0;
}
