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
 *
 * Output: offset.xcal, gain.xcal, defect.xcal, manifest.json
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

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::string outDir;
    uint32_t    width{1024};
    uint32_t    height{1024};
    uint32_t    seed{0};
    uint64_t    expiryMs{0};
};

void printUsage() {
    std::fprintf(stderr,
        "usage: xpe_calib_fixture_gen --out <dir> [--width N] [--height N]\n"
        "                             [--seed N] [--expiry-ms N]\n"
        "\n"
        "Writes offset.xcal, gain.xcal, defect.xcal and manifest.json into <dir>.\n"
        "The same arguments always produce byte-identical files.\n");
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

        if (arg == "--out") {
            if (i + 1 >= argc) return false;
            opt->outDir = argv[++i];
        } else if (arg == "--width") {
            if (!next(&value)) return false;
            opt->width = static_cast<uint32_t>(value);
        } else if (arg == "--height") {
            if (!next(&value)) return false;
            opt->height = static_cast<uint32_t>(value);
        } else if (arg == "--seed") {
            if (!next(&value)) return false;
            opt->seed = static_cast<uint32_t>(value);
        } else if (arg == "--expiry-ms") {
            if (!next(&value)) return false;
            opt->expiryMs = value;
        } else if (arg == "--help" || arg == "-?") {
            return false;
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            return false;
        }
    }
    if (opt->outDir.empty()) return false;
    if (opt->width == 0 || opt->height == 0) {
        std::fprintf(stderr, "width and height must be greater than 0\n");
        return false;
    }
    if (opt->width > XCAL_MAX_DIM || opt->height > XCAL_MAX_DIM) {
        std::fprintf(stderr, "width and height must not exceed %u\n", XCAL_MAX_DIM);
        return false;
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
std::vector<std::vector<uint16_t>> makeFlatFrames(const Options& opt, int count) {
    std::mt19937 gen(opt.seed + 1u);
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
                const double v = 300.0 + 20000.0 * sensitivity + noise(gen);
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

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, &opt)) {
        printUsage();
        return 2;
    }

    std::error_code ec;
    fs::create_directories(opt.outDir, ec);
    if (!fs::is_directory(opt.outDir)) {
        std::fprintf(stderr, "cannot create output directory: %s\n", opt.outDir.c_str());
        return 3;
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 4;
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
    const size_t pixelCount = static_cast<size_t>(opt.width) * opt.height;

    /* ---------------------------------------------------------- offset ---- */
    {
        auto darks = makeDarkFrames(opt, 8);
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
        auto flats = makeFlatFrames(opt, 4);
        std::vector<XpeImageBuffer> bufs;
        for (auto& f : flats) bufs.push_back(u16Buffer(f, opt.width, opt.height));

        auto darkRef = makeDarkFrames(opt, 1);
        XpeImageBuffer darkBuf = u16Buffer(darkRef[0], opt.width, opt.height);

        if (xpe_calib_generate_gain(bufs.data(), static_cast<int32_t>(bufs.size()),
                                    &darkBuf, gainScratch.c_str(), nullptr) != XPE_OK) {
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

    /* ---------------------------------------------------------- defect ---- */
    size_t defectCount = 0;
    {
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
             << "  \"files\": {\n"
             << "    \"offset.xcal\": \"" << offsetSha << "\",\n"
             << "    \"gain.xcal\": \""   << gainSha   << "\",\n"
             << "    \"defect.xcal\": \"" << defectSha << "\"\n"
             << "  }\n"
             << "}\n";
    manifest.close();

    std::printf("offset.xcal  %s\n", offsetSha.c_str());
    std::printf("gain.xcal    %s\n", gainSha.c_str());
    std::printf("defect.xcal  %s  (%zu defect pixels)\n", defectSha.c_str(), defectCount);
    std::printf("manifest.json written to %s\n", manifestPath.c_str());
    return 0;
}
