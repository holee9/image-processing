/**
 * @file xpe_real_frames.cpp
 * @brief QA-A-51 (#151): the real-frame verification harness, built BEFORE the
 *        frames arrive.
 *
 * EXPERIMENT ONLY. Nothing here ships, and this tool changes no default.
 *
 * Why it exists, and why now. QA-A-40..A-50 measured the runtime defect
 * detector entirely on SIMULATED frames. A-47 found the deepest defect of the
 * series precisely where the simulation stopped being representative, and A-50
 * ended with a recommendation -- adopt the 4-direction global sigma estimator,
 * but only together with (alpha=0.95, beta=1.2) -- that no synthetic frame can
 * confirm. The user's decision (2026-09-12) is to obtain real exposures and
 * judge on those.
 *
 * The harness is written first so that the measurement set AND the decision
 * thresholds are fixed before any real data is seen. Choosing the criteria
 * after looking at the data is fitting the conclusion to the data; the numbers
 * in kRules below are committed now, in source, and printed on every run.
 *
 * WHAT A REAL FRAME CAN AND CANNOT ANSWER
 *
 *   A real frame has NO defect ground truth. Nobody has labelled which pixels
 *   are genuinely bad. Therefore:
 *
 *     measurable   flagged-pixel count and rate; WHERE the flagged pixels sit
 *                  (their distribution across local-sigma deciles); the global
 *                  sigma estimate against the local-sigma distribution; the
 *                  difference between two settings on the same frame.
 *
 *     NOT measurable  TPR, and FPR in its literal sense. A flagged pixel on a
 *                  real frame may be a genuine defect. This tool therefore
 *                  never prints "FP" or "TPR" for a real frame -- it prints
 *                  "flagged". The SPEC's own 1% ceiling IS checkable, because
 *                  the SPEC asserts it for a normal exposure.
 *
 *     separate measurement  --inject adds synthetic defects on top of the real
 *                  noise and reports recovery. That number is labelled
 *                  INJECTED-ON-REAL everywhere it appears and is never mixed
 *                  into the flagged-pixel table: it measures the detector
 *                  against defects this tool created, not against real ones.
 *
 * Usage (see also .moai/reports/lane-pre/QA-A-51/HOWTO.md):
 *
 *   xpe_real_frames --in <file> [--in <file> ...] [--raw WxH:BITS] [--inject]
 *   xpe_real_frames --selftest <dir>     (dir produced by xpe_struct_frames --dump)
 *
 * SPEC: XPE-ALG-001 section 9.8 / REQ-P1A-013.  Refs #151 #148 #143
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"
#include "runtime_detection.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

using xpe::preprocess::internal::CollectNeighborValues;
using xpe::preprocess::internal::ComputeMedian;
using xpe::preprocess::internal::ComputeMAD;
using xpe::preprocess::internal::ComputeGlobalSigma;

/* ------------------------------------------------- pre-registered criteria */
//
// FIXED BEFORE ANY REAL FRAME WAS SEEN. Committed in QA-A-51, printed on every
// run. If a later card changes a number here, that change is itself a finding
// and belongs in a report -- it is not a tuning knob.
//
// Grounding for each number, from measurements already on record:
//
//  T1 3.0x   On every synthetic frame A-50 measured, the candidate setting
//            (0.95, 1.2) flagged FEWER pixels than the shipped one, never more
//            (uniform 108->6, edge 1695->630, lines 5126->2333, checker8 22->3).
//            A real frame where the candidate flags more than 3x the shipped
//            count is a qualitative reversal, not measurement scatter.
//  T2 1.0e-2 The SPEC's own ceiling: sum(defectMap) <= width*height*0.01 for a
//            normal exposure (XPE-ALG-001 9.8, REQ-P1A-013).
//  T3 5.0x   A-47/A-50 top-structure enrichment ranged 0.00x..7.97x, and the
//            'lines' frame sat near 7.9x under BOTH settings -- structure alone
//            can produce it. The rule therefore fires only when the CANDIDATE
//            crosses 5.0x while the SHIPPED setting does not, i.e. when the cap
//            is what created the cluster.
//  T4 p10..p90  A global sigma estimate must land inside the frame's OWN local
//            sigma band. Outside it, the global number is describing something
//            the frame's noise does not contain.
//
//            This replaces a first draft that compared Bm4 against the MEDIAN
//            local sigma with a 25% tolerance. That draft was mis-specified, and
//            the QA-A-51 smoke run caught it on the synthetic 'diag' frame,
//            where the true sigma is known to be 12.0: Bm4 read 11.53 (correct)
//            while the median local sigma read 20.76, because a 3x3 local MAD is
//            itself inflated by structure. The rule was rejecting the estimator
//            for agreeing with the truth. It was repaired BEFORE any real frame
//            existed -- which is what building the harness first is for -- and
//            the episode is recorded in the QA-A-51 report rather than quietly
//            tuned away.
//
constexpr double kT1_flagRatioReject   = 3.0;     // candidate/shipped flagged count
constexpr double kT2_ceiling           = 1.0e-2;  // SPEC 1% ceiling
constexpr double kT3_enrichReject      = 5.0;     // top-decile enrichment
// T4 carries no scalar: the band is the frame's own local-sigma p10..p90.

void printRules() {
    std::printf(
        "PRE-REGISTERED DECISION RULES (fixed in QA-A-51, before any real frame)\n"
        "  R1 REJECT candidate   if flagged(candidate) > %.1fx flagged(shipped)   on any frame\n"
        "  R2 REJECT candidate   if flaggedRate(candidate) >= %.1e (SPEC ceiling) on any frame\n"
        "  R3 REJECT cap b=1.2   if topDecileEnrich(candidate) >= %.1fx AND\n"
        "                           topDecileEnrich(shipped)    <  %.1fx          on any frame\n"
        "  R4 ACCEPT candidate   if on EVERY frame: flagged(candidate) <= flagged(shipped)\n"
        "                           AND flaggedRate(candidate) < %.1e\n"
        "                           AND topDecileEnrich(candidate) < %.1fx\n"
        "  R5 Bm4 usable         if on EVERY frame: Bm4 <= Bm AND\n"
        "                           p10(localSigma) <= Bm4 <= p90(localSigma)\n"
        "  otherwise INCONCLUSIVE -- more frames, not a relaxed threshold.\n\n"
        "  R4 is deliberately 'never worse than what ships today', NOT REQ-P1A-013's\n"
        "  1e-5 FPR: that requirement cannot be judged on a frame with no defect\n"
        "  ground truth, and A-40 already recorded it as unmet.\n\n",
        kT1_flagRatioReject, kT2_ceiling, kT3_enrichReject, kT3_enrichReject,
        kT2_ceiling, kT3_enrichReject);
}

/* ------------------------------------------------------------------ frames */

struct RealFrame {
    std::string        name;
    std::string        source;        // "raw" | "dicom"
    uint32_t           width  = 0;
    uint32_t           height = 0;
    uint32_t           bits   = 0;
    std::string        format;
    std::vector<float> pixels;        // always float32 after load
};

size_t pixelCount(const RealFrame& f) {
    return static_cast<size_t>(f.width) * static_cast<size_t>(f.height);
}

XpeImageBuffer wrap(std::vector<float>& p, uint32_t w, uint32_t h) {
    XpeImageBuffer img{};
    img.data = p.data();
    img.width = w;
    img.height = h;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(p.size() * sizeof(float));
    return img;
}

/** Reports why a file was skipped. Never silent -- the card requires the name. */
void skip(const std::string& path, const std::string& why) {
    std::printf("SKIPPED  %s\n         reason: %s\n", path.c_str(), why.c_str());
}

std::string baseName(const std::string& path) {
    const size_t s = path.find_last_of("/\\");
    return (s == std::string::npos) ? path : path.substr(s + 1);
}

std::string lowerExt(const std::string& path) {
    const size_t d = path.find_last_of('.');
    if (d == std::string::npos) return std::string();
    std::string e = path.substr(d + 1);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

/* -------------------------------------------------------------- raw loader */

bool loadRaw(const std::string& path, uint32_t w, uint32_t h, uint32_t bits,
             RealFrame& out) {
    if (w == 0 || h == 0 || (bits != 8 && bits != 16 && bits != 32)) {
        skip(path, "raw input needs --raw WxH:BITS with BITS in {8,16,32}"
                   " (32 = IEEE-754 float32)");
        return false;
    }
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp == nullptr) {
        skip(path, "cannot open for reading");
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    const long bytes = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);

    const size_t n = static_cast<size_t>(w) * h;
    const size_t want = n * (bits / 8u);
    if (bytes < 0 || static_cast<size_t>(bytes) != want) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "size mismatch: file has %ld bytes, %ux%u at %u bits needs %zu",
                      bytes, w, h, bits, want);
        std::fclose(fp);
        skip(path, buf);
        return false;
    }

    out.pixels.assign(n, 0.0f);
    if (bits == 32) {
        const size_t got = std::fread(out.pixels.data(), sizeof(float), n, fp);
        std::fclose(fp);
        if (got != n) { skip(path, "short read"); return false; }
        out.format = "FLOAT32 (little-endian)";
    } else if (bits == 16) {
        std::vector<uint16_t> q(n);
        const size_t got = std::fread(q.data(), sizeof(uint16_t), n, fp);
        std::fclose(fp);
        if (got != n) { skip(path, "short read"); return false; }
        for (size_t i = 0; i < n; ++i) out.pixels[i] = static_cast<float>(q[i]);
        out.format = "UINT16 (little-endian)";
    } else {
        std::vector<uint8_t> q(n);
        const size_t got = std::fread(q.data(), sizeof(uint8_t), n, fp);
        std::fclose(fp);
        if (got != n) { skip(path, "short read"); return false; }
        for (size_t i = 0; i < n; ++i) out.pixels[i] = static_cast<float>(q[i]);
        out.format = "UINT8";
    }
    out.name = baseName(path);
    out.source = "raw";
    out.width = w;
    out.height = h;
    out.bits = bits;
    return true;
}

/* ------------------------------------------------------------ dicom loader */
//
// Loaded at RUNTIME, not linked. The ci-preprocess preset configures with
// BUILD_DICOM=OFF, so a link-time dependency would make this tool unbuildable
// in the very preset Lane A uses. The three entry points are re-declared here
// against the published ABI (modules/dicom/include/xpe/dicom/dicom_api.h);
// that hand copy is a real hazard and is recorded as such in the report -- a
// silent ABI drift in xpe_dicom would show up here as garbage pixels, not as a
// link error. Mitigation: every GetProcAddress failure is reported by name.

#ifdef _WIN32
typedef XpeErrorCode (*PfnDicomOpen)(const char*, void**);
typedef XpeErrorCode (*PfnDicomReadImage)(void*, XpeImageBuffer*);
typedef void         (*PfnDicomClose)(void*);

struct DicomApi {
    HMODULE           lib   = nullptr;
    PfnDicomOpen      open  = nullptr;
    PfnDicomReadImage read  = nullptr;
    PfnDicomClose     close = nullptr;
    std::string       why;                 // why it is unavailable

    bool ok() const { return lib != nullptr && open && read && close; }
};

DicomApi loadDicomApi() {
    DicomApi a;
    a.lib = ::LoadLibraryA("xpe_dicom.dll");
    if (a.lib == nullptr) {
        a.why = "xpe_dicom.dll not loadable next to this executable "
                "(the ci-preprocess preset configures BUILD_DICOM=OFF). "
                "Either build with a DICOM-enabled preset and copy the DLL here, "
                "or convert the file to raw and pass --raw WxH:BITS.";
        return a;
    }
    a.open  = reinterpret_cast<PfnDicomOpen>(
                  reinterpret_cast<void*>(::GetProcAddress(a.lib, "xpe_dicom_open")));
    a.read  = reinterpret_cast<PfnDicomReadImage>(
                  reinterpret_cast<void*>(::GetProcAddress(a.lib, "xpe_dicom_read_image")));
    a.close = reinterpret_cast<PfnDicomClose>(
                  reinterpret_cast<void*>(::GetProcAddress(a.lib, "xpe_dicom_close")));
    if (!a.ok()) {
        a.why = "xpe_dicom.dll loaded but one of xpe_dicom_open / "
                "xpe_dicom_read_image / xpe_dicom_close is missing -- ABI drift.";
    }
    return a;
}

bool loadDicom(const std::string& path, DicomApi& api, RealFrame& out) {
    if (!api.ok()) { skip(path, api.why); return false; }

    void* h = nullptr;
    const XpeErrorCode rcOpen = api.open(path.c_str(), &h);
    if (rcOpen != XPE_OK || h == nullptr) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "xpe_dicom_open returned %d", static_cast<int>(rcOpen));
        skip(path, buf);
        return false;
    }

    XpeImageBuffer img{};
    const XpeErrorCode rcRead = api.read(h, &img);
    api.close(h);
    if (rcRead != XPE_OK || img.data == nullptr) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "xpe_dicom_read_image returned %d",
                      static_cast<int>(rcRead));
        skip(path, buf);
        return false;
    }

    const size_t n = static_cast<size_t>(img.width) * img.height;
    out.pixels.assign(n, 0.0f);
    if (img.format == XPE_PIXEL_UINT16) {
        const uint16_t* p = static_cast<const uint16_t*>(img.data);
        for (size_t i = 0; i < n; ++i) out.pixels[i] = static_cast<float>(p[i]);
        out.format = "UINT16 (from DICOM)";
    } else if (img.format == XPE_PIXEL_FLOAT32) {
        const float* p = static_cast<const float*>(img.data);
        for (size_t i = 0; i < n; ++i) out.pixels[i] = p[i];
        out.format = "FLOAT32 (from DICOM)";
    } else if (img.format == XPE_PIXEL_UINT8) {
        const uint8_t* p = static_cast<const uint8_t*>(img.data);
        for (size_t i = 0; i < n; ++i) out.pixels[i] = static_cast<float>(p[i]);
        out.format = "UINT8 (from DICOM)";
    } else {
        xpe_free_image(&img);
        skip(path, "DICOM pixel format not one of UINT8 / UINT16 / FLOAT32");
        return false;
    }

    out.name = baseName(path);
    out.source = "dicom";
    out.width = img.width;
    out.height = img.height;
    out.bits = img.bitsStored;
    xpe_free_image(&img);
    return true;
}
#else
struct DicomApi { bool ok() const { return false; } std::string why =
    "DICOM input is implemented for Windows only in this harness."; };
DicomApi loadDicomApi() { return DicomApi(); }
bool loadDicom(const std::string& path, DicomApi& api, RealFrame&) {
    skip(path, api.why);
    return false;
}
#endif

/* -------------------------------------------------------------- estimators */
//
// Written dimension-generic here, deliberately independent of the versions in
// xpe_struct_frames.cpp. The self-test (--selftest) is what makes that pay:
// if two independent implementations reproduce the same published numbers from
// the same frames, the numbers are a property of the data, not of one TU.

float madSigma(std::vector<float>& d, float undoPairing) {
    if (d.empty()) return 0.0f;
    const size_t mid = d.size() / 2u;
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    const float med = d[mid];
    for (float& v : d) v = std::fabs(v - med);
    std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
    return d[mid] * RUNTIME_DETECTION_MAD_SCALE * undoPairing;
}

/** A: MAD of the pixel VALUES -- the estimator shipped before QA-A-48. */
float estValueMad(const std::vector<float>& px) {
    std::vector<float> d = px;
    return madSigma(d, 1.0f);
}

/**
 * Bm4: min over four directions of the adjacent-difference MAD / sqrt(2).
 *
 * The 1/sqrt(2) is identical for the diagonals: Var(n1-n2) = Var(n1)+Var(n2)
 * -2Cov(n1,n2), and for white noise Cov = 0 at EVERY non-zero separation, so
 * the sqrt(2) pixel pitch of a diagonal does not enter. QA-A-50 falsified the
 * alternative (an extra distance factor) -- it drove a structureless frame to
 * 0.71x. With correlated noise rho(1) != rho(sqrt2) and the four directions
 * would need different factors; that premise is untested on real data.
 */
float estDiffMadMin4(const std::vector<float>& px, uint32_t w, uint32_t h) {
    const float k = 0.70710678f;
    std::vector<float> d;
    d.reserve(static_cast<size_t>(w) * h);
    float best = 0.0f;
    auto consider = [&](float v) { if (v > 0.0f && (best <= 0.0f || v < best)) best = v; };

    // (dx, dy) is the displacement; dy is 0 or 1, dx is -1, 0 or 1.
    auto pass = [&](int dx, int dy) {
        if (w < 2u || h < 2u) return;
        const uint32_t yEnd = h - static_cast<uint32_t>(dy);
        const uint32_t xBeg = (dx < 0) ? 1u : 0u;
        const uint32_t xEnd = (dx > 0) ? (w - 1u) : w;
        d.clear();
        for (uint32_t y = 0; y < yEnd; ++y) {
            for (uint32_t x = xBeg; x < xEnd; ++x) {
                const size_t i = static_cast<size_t>(y) * w + x;
                const size_t j = static_cast<size_t>(y + static_cast<uint32_t>(dy)) * w
                               + static_cast<size_t>(static_cast<int>(x) + dx);
                d.push_back(px[j] - px[i]);
            }
        }
        consider(madSigma(d, k));
    };

    pass(1, 0);    // horizontal
    pass(0, 1);    // vertical
    pass(1, 1);    // diagonal down-right
    pass(-1, 1);   // diagonal down-left
    return best;
}

/* ------------------------------------------------------------ measurement */

struct Deciles { float edge[11]; };   // edge[0]=min .. edge[10]=max

Deciles decileEdges(std::vector<float> v) {
    Deciles d{};
    if (v.empty()) return d;
    std::sort(v.begin(), v.end());
    for (int k = 0; k <= 10; ++k) {
        const size_t idx = static_cast<size_t>(
            (static_cast<double>(k) / 10.0) * static_cast<double>(v.size() - 1));
        d.edge[k] = v[idx];
    }
    return d;
}

struct Quant { float p01, p10, p50, p90, p99, max; };

Quant quantiles(std::vector<float> v) {
    Quant q{0, 0, 0, 0, 0, 0};
    if (v.empty()) return q;
    std::sort(v.begin(), v.end());
    auto at = [&](double f) {
        const size_t k = static_cast<size_t>(f * static_cast<double>(v.size() - 1));
        return v[k];
    };
    q.p01 = at(0.01); q.p10 = at(0.10); q.p50 = at(0.50);
    q.p90 = at(0.90); q.p99 = at(0.99); q.max = v.back();
    return q;
}

std::vector<float> localSigmaMap(const XpeImageBuffer& img) {
    const size_t n = static_cast<size_t>(img.width) * img.height;
    std::vector<float> out(n, 0.0f);
    std::vector<float> nbrs, dev;
    for (uint32_t y = 0; y < img.height; ++y) {
        for (uint32_t x = 0; x < img.width; ++x) {
            CollectNeighborValues(&img, x, y, RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE, nbrs);
            if (nbrs.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) continue;
            const float m = ComputeMedian(nbrs);
            dev = nbrs;
            out[static_cast<size_t>(y) * img.width + x] = ComputeMAD(dev, m);
        }
    }
    return out;
}

std::vector<uint8_t> detect(const XpeImageBuffer& img, float floor, float cap) {
    RuntimeDetectionConfig cfg = RuntimeDetection_DefaultConfig();
    cfg.globalSigmaFloor = floor;
    cfg.globalSigmaCap = cap;

    const size_t n = static_cast<size_t>(img.width) * img.height;
    std::vector<uint8_t> map(n, 0);
    std::vector<float> nbrs, dev;
    for (uint32_t y = 0; y < img.height; ++y) {
        for (uint32_t x = 0; x < img.width; ++x) {
            if (xpe::preprocess::internal::DetectDefectivePixel(&img, x, y, cfg, nbrs, dev)) {
                map[static_cast<size_t>(y) * img.width + x] = 1;
            }
        }
    }
    return map;
}

/**
 * Where do the flagged pixels sit? Real frames have no structure oracle, so
 * "structure" is replaced by the frame's own local-sigma deciles: decile 10 is
 * the noisiest tenth of the frame, which is where edges and textured anatomy
 * land. Enrichment = (share of flags in decile 10) / 0.10. 1.0x means flags are
 * spread evenly; 10.0x means every flag is in the noisiest tenth.
 */
struct FlagStats {
    size_t total = 0;
    size_t perDecile[10] = {0};
    double rate = 0.0;
    double topEnrich = 0.0;
    bool   overCeiling = false;
};

FlagStats analyse(const std::vector<uint8_t>& map, const std::vector<float>& local,
                  const Deciles& dec, size_t n) {
    FlagStats s;
    for (size_t i = 0; i < n; ++i) {
        if (!map[i]) continue;
        ++s.total;
        int b = 9;
        for (int k = 0; k < 10; ++k) {
            if (local[i] <= dec.edge[k + 1]) { b = k; break; }
        }
        ++s.perDecile[b];
    }
    s.rate = static_cast<double>(s.total) / static_cast<double>(n);
    if (s.total > 0) {
        s.topEnrich = (static_cast<double>(s.perDecile[9]) / static_cast<double>(s.total)) / 0.10;
    }
    s.overCeiling = (s.rate >= kT2_ceiling);
    return s;
}

/* ------------------------------------------------------------- the report */

struct Setting { const char* label; float alpha; float beta; };
const Setting kSettings[2] = {
    {"shipped   (a=0.80, cap off)", 0.80f, 0.00f},
    {"candidate (a=0.95, b=1.20)",  0.95f, 1.20f},
};

struct FrameVerdict {
    std::string name;
    FlagStats   s[2];       // 0 = shipped, 1 = candidate
    float       bm = 0.0f;
    float       bm4 = 0.0f;
    float       localP10 = 0.0f;
    float       localP90 = 0.0f;
};

/** Injected defects sit on a 32-pixel lattice, away from the border. */
std::vector<size_t> injectionSites(uint32_t w, uint32_t h) {
    std::vector<size_t> sites;
    for (uint32_t y = 16; y + 16 < h; y += 32) {
        for (uint32_t x = 16; x + 16 < w; x += 32) {
            sites.push_back(static_cast<size_t>(y) * w + x);
        }
    }
    return sites;
}

FrameVerdict reportFrame(RealFrame& f, bool inject) {
    FrameVerdict v;
    v.name = f.name;
    const size_t n = pixelCount(f);
    XpeImageBuffer img = wrap(f.pixels, f.width, f.height);

    const Quant vals = quantiles(f.pixels);
    const float a  = estValueMad(f.pixels);
    const float bm = ComputeGlobalSigma(&img);                 // shipped: min(h,v)
    const float b4 = estDiffMadMin4(f.pixels, f.width, f.height);

    const std::vector<float> local = localSigmaMap(img);
    const Quant lq = quantiles(local);
    const Deciles dec = decileEdges(local);

    v.bm = bm; v.bm4 = b4;
    v.localP10 = lq.p10; v.localP90 = lq.p90;

    std::printf("== %s ==\n", f.name.c_str());
    std::printf("  source      : %s\n", f.source.c_str());
    std::printf("  dims        : %u x %u   bitsStored %u   format %s   pixels %zu\n",
                f.width, f.height, f.bits, f.format.c_str(), n);
    std::printf("  values      : p01 %.1f  p50 %.1f  p99 %.1f  max %.1f\n",
                vals.p01, vals.p50, vals.p99, vals.max);
    std::printf("  local sigma : p01 %.3f  p10 %.3f  p50 %.3f  p90 %.3f  p99 %.3f  max %.3f\n",
                lq.p01, lq.p10, lq.p50, lq.p90, lq.p99, lq.max);
    std::printf("  global sigma: A(value-MAD) %.4f   Bm(shipped) %.4f   Bm4 %.4f\n", a, bm, b4);
    if (lq.p50 > 0.0f) {
        std::printf("                Bm/medLocal %.3fx   Bm4/medLocal %.3fx\n",
                    static_cast<double>(bm) / lq.p50, static_cast<double>(b4) / lq.p50);
    }

    for (int k = 0; k < 2; ++k) {
        const float floor = kSettings[k].alpha * bm;
        const float cap   = kSettings[k].beta * bm;
        const std::vector<uint8_t> map = detect(img, floor, cap);
        v.s[k] = analyse(map, local, dec, n);

        std::printf("  -- %s --\n", kSettings[k].label);
        std::printf("     floor %.4f  cap %.4f   flagged %zu   flaggedRate %.4e   1%% %s\n",
                    floor, cap, v.s[k].total, v.s[k].rate,
                    v.s[k].overCeiling ? "OVER" : "ok");
        std::printf("     flagged by local-sigma decile (d1=quietest .. d10=noisiest):\n       ");
        for (int d = 0; d < 10; ++d) std::printf("%8zu", v.s[k].perDecile[d]);
        std::printf("\n     top-decile enrichment %.2fx\n", v.s[k].topEnrich);

        if (inject) {
            const std::vector<size_t> sites = injectionSites(f.width, f.height);
            const float amps[4] = {5.0f, 6.0f, 8.0f, 10.0f};
            std::printf("     INJECTED-ON-REAL (synthetic defects added to this real frame;\n"
                        "     this is NOT a measurement of real defects) sites %zu\n       ",
                        sites.size());
            for (int ai = 0; ai < 4; ++ai) {
                std::vector<float> inj = f.pixels;
                for (size_t site : sites) {
                    const float s = (local[site] > 0.0f) ? local[site] : bm;
                    inj[site] += amps[ai] * s;
                }
                XpeImageBuffer ii = wrap(inj, f.width, f.height);
                const std::vector<uint8_t> m = detect(ii, floor, cap);
                size_t tp = 0;
                for (size_t site : sites) if (m[site]) ++tp;
                std::printf("  recovered@%.0fs %.6f",
                            static_cast<double>(amps[ai]),
                            static_cast<double>(tp) / static_cast<double>(sites.size()));
            }
            std::printf("\n");
        }
        std::fflush(stdout);
    }
    std::printf("\n");
    return v;
}

/** Applies the pre-registered rules across every frame and prints the verdict. */
void applyRules(const std::vector<FrameVerdict>& all) {
    if (all.empty()) {
        std::printf("VERDICT: no frame was read. Nothing measured, nothing decided.\n");
        return;
    }

    bool r1 = false, r2 = false, r3 = false, r5 = true;
    bool acceptAll = true;
    std::string why;

    for (const FrameVerdict& v : all) {
        const double shipped = static_cast<double>(v.s[0].total);
        const double cand    = static_cast<double>(v.s[1].total);
        if (shipped > 0.0 && cand > kT1_flagRatioReject * shipped) {
            r1 = true;
            why += "  R1 fired on " + v.name + "\n";
        }
        if (v.s[1].rate >= kT2_ceiling) {
            r2 = true;
            why += "  R2 fired on " + v.name + "\n";
        }
        if (v.s[1].topEnrich >= kT3_enrichReject && v.s[0].topEnrich < kT3_enrichReject) {
            r3 = true;
            why += "  R3 fired on " + v.name + "\n";
        }
        if (!(cand <= shipped && v.s[1].rate < kT2_ceiling
              && v.s[1].topEnrich < kT3_enrichReject)) {
            acceptAll = false;
        }
        const bool inBand = (v.bm4 >= v.localP10 && v.bm4 <= v.localP90);
        if (!(v.bm4 <= v.bm && inBand)) {
            r5 = false;
            why += "  R5 failed on " + v.name + "\n";
        }
    }

    std::printf("VERDICT over %zu frame(s)\n", all.size());
    std::printf("  R1 candidate flag blow-up : %s\n", r1 ? "REJECT" : "not fired");
    std::printf("  R2 SPEC 1%% ceiling        : %s\n", r2 ? "REJECT" : "not fired");
    std::printf("  R3 cap-created cluster    : %s\n", r3 ? "REJECT cap" : "not fired");
    std::printf("  R5 Bm4 usable             : %s\n", r5 ? "yes" : "NO");
    if (r1 || r2 || r3) {
        std::printf("  => REJECT the candidate setting on real data.\n");
    } else if (acceptAll) {
        std::printf("  => ACCEPT: candidate is never worse than the shipped setting here.\n");
    } else {
        std::printf("  => INCONCLUSIVE: no rejection rule fired, but R4 is not met on every\n"
                    "     frame. More frames are needed -- do NOT relax a threshold.\n");
    }
    if (!why.empty()) std::printf("%s", why.c_str());
    std::printf("\nReminder: flagged != false positive. These frames carry no defect\n"
                "ground truth, so TPR and literal FPR are NOT measured above.\n");
}

/* --------------------------------------------------------------- selftest */

/**
 * Feeds the QA-A-47/A-49 synthetic frames -- dumped as uint16 raws by
 * `xpe_struct_frames --dump` -- through this harness's OWN input path and
 * compares the sigma estimates against the published QA-A-50 table. A mismatch
 * beyond the quantisation tolerance is a defect in THIS harness, not a finding.
 */
int selftest(const std::string& dir) {
    struct Ref { const char* name; float trueSigmaMed; float bmRatio; float bm4Ratio; };
    const Ref refs[7] = {
        {"uniform",  10.0000f, 1.00f, 1.00f},
        {"scatter",  17.8446f, 0.99f, 0.99f},
        {"edge",     12.0000f, 1.39f, 1.39f},
        {"lines",    12.0000f, 1.00f, 1.00f},
        {"checker2", 12.0000f, 2.05f, 2.05f},
        {"checker8", 12.0000f, 1.15f, 1.15f},
        {"diag",     12.0000f, 2.66f, 1.00f},
    };

    // Two tolerances, and the difference between them IS the point.
    //
    // float32 path: the harness reads exactly what QA-A-50 measured, so any
    //   deviation is a defect in this file. 0.01x.
    //
    // uint16 path: a difference-MAD on integer data is itself an integer, so
    //   the estimate can only take the values MAD * 1.4826 * 0.70710678, i.e.
    //   it is quantised in steps of kQuantStep ~= 1.048 ADU. Expressed as a
    //   ratio to the frame's true sigma that is kQuantStep / trueSigma -- 0.105
    //   at sigma 10, 0.087 at sigma 12. This is not slack granted to the
    //   harness; it is a property of the data a real detector delivers, and it
    //   is derived here rather than read off the measurements.
    const double kFloatTol  = 0.01;
    const double kQuantStep = static_cast<double>(RUNTIME_DETECTION_MAD_SCALE) * 0.70710678;

    std::printf("SELF-TEST: this harness against the published QA-A-50 table.\n");
    std::printf("Frames come from `xpe_struct_frames --dump`, in both forms:\n");
    std::printf("  <name>_f32.raw  float32, exactly what QA-A-50 measured -> tolerance %.2fx\n",
                kFloatTol);
    std::printf("  <name>.raw      uint16, what a detector delivers        -> tolerance\n");
    std::printf("                  %.4f/trueSigma (the MAD quantisation step)\n\n", kQuantStep);
    std::printf("  frame       trueSig  |            float32             |             uint16\n");
    std::printf("                       |     Bm  ref     d      Bm4  ref     d |"
                "     Bm  ref     d      Bm4  ref     d\n");

    int bad = 0, seen = 0;
    for (const Ref& r : refs) {
        RealFrame ff, fq;
        const std::string pf = dir + "/" + std::string(r.name) + "_f32.raw";
        const std::string pq = dir + "/" + std::string(r.name) + ".raw";
        const bool okF = loadRaw(pf, 1024u, 1024u, 32u, ff);
        const bool okQ = loadRaw(pq, 1024u, 1024u, 16u, fq);
        if (!okF || !okQ) { ++bad; continue; }
        ++seen;

        XpeImageBuffer iF = wrap(ff.pixels, ff.width, ff.height);
        XpeImageBuffer iQ = wrap(fq.pixels, fq.width, fq.height);
        const double bF  = ComputeGlobalSigma(&iF)  / r.trueSigmaMed;
        const double b4F = estDiffMadMin4(ff.pixels, ff.width, ff.height) / r.trueSigmaMed;
        const double bQ  = ComputeGlobalSigma(&iQ)  / r.trueSigmaMed;
        const double b4Q = estDiffMadMin4(fq.pixels, fq.width, fq.height) / r.trueSigmaMed;

        const double quantTol = kQuantStep / r.trueSigmaMed;
        const bool okAll =
            std::fabs(bF  - r.bmRatio)  <= kFloatTol &&
            std::fabs(b4F - r.bm4Ratio) <= kFloatTol &&
            std::fabs(bQ  - r.bmRatio)  <= quantTol  &&
            std::fabs(b4Q - r.bm4Ratio) <= quantTol;
        if (!okAll) ++bad;

        std::printf("  %-10s %7.3f | %5.2fx %5.2fx %+.3f  %5.2fx %5.2fx %+.3f |"
                    " %5.2fx %5.2fx %+.3f  %5.2fx %5.2fx %+.3f  %s\n",
                    r.name, r.trueSigmaMed,
                    bF,  r.bmRatio,  bF  - r.bmRatio,
                    b4F, r.bm4Ratio, b4F - r.bm4Ratio,
                    bQ,  r.bmRatio,  bQ  - r.bmRatio,
                    b4Q, r.bm4Ratio, b4Q - r.bm4Ratio,
                    okAll ? "" : "<-- MISMATCH");
        std::fflush(stdout);
    }

    std::printf("\n  frames read %d/7, mismatches %d\n", seen, bad);
    if (bad == 0) {
        std::printf("  SELF-TEST PASS -- an independent estimator implementation, reading\n"
                    "  through this harness's own raw input path, reproduces QA-A-50 on the\n"
                    "  float32 frames and stays inside the derived quantisation bound on the\n"
                    "  uint16 ones.\n");
        return 0;
    }
    std::printf("  SELF-TEST FAIL -- fix the harness before trusting any real-frame number.\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> inputs;
    std::string selftestDir;
    uint32_t rawW = 0, rawH = 0, rawBits = 0;
    bool inject = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            inputs.push_back(argv[++i]);
        } else if (std::strcmp(argv[i], "--selftest") == 0 && i + 1 < argc) {
            selftestDir = argv[++i];
        } else if (std::strcmp(argv[i], "--inject") == 0) {
            inject = true;
        } else if (std::strcmp(argv[i], "--raw") == 0 && i + 1 < argc) {
            unsigned w = 0, h = 0, b = 0;
            if (std::sscanf(argv[++i], "%ux%u:%u", &w, &h, &b) == 3) {
                rawW = w; rawH = h; rawBits = b;
            } else {
                std::fprintf(stderr, "--raw expects WxH:BITS, e.g. 3072x3072:16\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", argv[i]);
            return 2;
        }
    }

    if (inputs.empty() && selftestDir.empty()) {
        std::printf(
            "QA-A-51 real-frame harness (#151). Nothing here changes a default.\n\n"
            "  xpe_real_frames --in <file> [--in <file> ...] [--raw WxH:BITS] [--inject]\n"
            "  xpe_real_frames --selftest <dir>\n\n"
            "  --in        a .dcm file, or a .raw/.bin file (then --raw is required)\n"
            "  --raw       geometry for raw inputs, e.g. --raw 3072x3072:16\n"
            "  --inject    additionally add synthetic defects on top of the real\n"
            "              noise and report recovery (labelled INJECTED-ON-REAL)\n"
            "  --selftest  read the frames written by `xpe_struct_frames --dump <dir>`\n"
            "              and check this harness against the published QA-A-50 table\n\n");
        printRules();
        return 0;
    }

    if (xpe_preprocess_init(nullptr) != XPE_OK) {
        std::fprintf(stderr, "xpe_preprocess_init failed\n");
        return 1;
    }

    int rc = 0;
    if (!selftestDir.empty()) {
        rc = selftest(selftestDir);
        xpe_preprocess_shutdown();
        return rc;
    }

    printRules();

    DicomApi dicom = loadDicomApi();
    std::vector<FrameVerdict> verdicts;

    for (const std::string& path : inputs) {
        const std::string ext = lowerExt(path);
        RealFrame f;
        bool ok = false;
        if (ext == "raw" || ext == "bin" || ext == "img") {
            ok = loadRaw(path, rawW, rawH, rawBits, f);
        } else if (ext == "dcm" || ext == "dicom" || ext.empty()) {
            ok = loadDicom(path, dicom, f);
        } else {
            skip(path, "unrecognised extension '" + ext +
                       "' -- expected .dcm/.dicom (DICOM) or .raw/.bin/.img (+ --raw)");
        }
        if (ok) verdicts.push_back(reportFrame(f, inject));
    }

    applyRules(verdicts);
    xpe_preprocess_shutdown();
    return rc;
}
