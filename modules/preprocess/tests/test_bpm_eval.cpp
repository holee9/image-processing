/**
 * @file test_bpm_eval.cpp
 * @brief Tests of the defect-map evaluation tool xpe_bpm_eval (QA-A-251, #273).
 *
 * The tool runs as a subprocess on small synthetic frames. The injection spec and the scoring definitions it implements are fixed in
 * .moai/reports/lane-pre/QA-A-251/evidence/30_method_fixed_before_measurement.md.
 */

#include <gtest/gtest.h>
#include "defect_ext.hpp"

#include "xpe_bpm_eval_path.h"      // generated: XPE_BPM_EVAL_EXE
#include "xpe_bpm_eval_stats.hpp"      // the numeric parts of the tool (QA-A-251b)
#include "xpe_fixture_gen_path.h"   // generated: XPE_FIXTURE_GEN_EXE
#include "xpe/preprocess_api.h"      // the defect codes of the first generation

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 1024, kH = 1024;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

int runCmd(const std::string& exe, const std::string& args) {
    std::string cmd = "\"" + exe + "\" " + args;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";   // cmd.exe strips the outer pair of quotes
#endif
    return std::system(cmd.c_str());
}

std::vector<uint8_t> readBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void writeBytes(const fs::path& p, const void* d, size_t n) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(static_cast<const char*>(d), static_cast<std::streamsize>(n));
}

std::vector<uint16_t> readFrame(const fs::path& p) {
    const std::vector<uint8_t> b = readBytes(p);
    std::vector<uint16_t> v(b.size() / 2);
    if (!v.empty()) std::memcpy(v.data(), b.data(), v.size() * 2);
    return v;
}

uint32_t nextNoise(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s >> 8;
}

std::string text(const fs::path& p) {
    const std::vector<uint8_t> b = readBytes(p);
    return std::string(b.begin(), b.end());
}

struct Rect {
    int x, y, w, h;
};

struct Item {
    int                id;
    std::string        cls, kind, shape;
    double             level;
    std::vector<Rect>  rects;
};

std::vector<Item> parseTruth(const fs::path& tsv) {
    std::vector<Item> items;
    std::ifstream f(tsv);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, '\t')) c.push_back(cell);
        if (c.size() < 6 || !std::isdigit(static_cast<unsigned char>(c[0][0]))) continue;
        Item it;
        it.id = std::atoi(c[0].c_str());
        it.cls = c[1];
        it.kind = c[2];
        it.shape = c[3];
        it.level = std::atof(c[4].c_str());
        std::stringstream rs(c[5]);
        std::string r;
        while (std::getline(rs, r, ';')) {
            Rect q{};
            std::stringstream qs(r);
            std::string v;
            int part = 0;
            int vals[4] = {0, 0, 0, 0};
            while (std::getline(qs, v, ',') && part < 4) vals[part++] = std::atoi(v.c_str());
            q = Rect{vals[0], vals[1], vals[2], vals[3]};
            it.rects.push_back(q);
        }
        items.push_back(it);
    }
    return items;
}

class BpmEvalTest : public ::testing::Test {
protected:
    fs::path root;
    std::vector<std::string> darks, brights, evals;

    void SetUp() override {
        root = fs::temp_directory_path() / ("xpe_bpmeval_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                                std::chrono::system_clock::now().time_since_epoch()).count()) + "_" +
                                            ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::create_directories(root / "in");
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    // frames: dark = pedestal + a fixed pattern + noise; bright = dark + a ramp signal + noise
    void makeFrames(size_t nDark, size_t nBright, size_t nEval) {
        uint32_t state = 777u;
        auto frame = [&](bool bright, const std::string& name) {
            std::vector<uint16_t> f(kN);
            for (uint32_t y = 0; y < kH; ++y)
                for (uint32_t x = 0; x < kW; ++x) {
                    double v = 3000.0 + ((x * 7 + y * 13) % 11) + (nextNoise(state) % 9);
                    if (bright) v += 6000.0 * (0.9 + 0.2 * x / (kW - 1)) + (nextNoise(state) % 31);
                    f[static_cast<size_t>(y) * kW + x] = static_cast<uint16_t>(v);
                }
            const fs::path p = root / "in" / name;
            writeBytes(p, f.data(), f.size() * 2);
            return p.string();
        };
        for (size_t k = 0; k < nDark; ++k) darks.push_back(frame(false, "Dark_" + std::to_string(k) + ".raw"));
        for (size_t k = 0; k < nBright; ++k) brights.push_back(frame(true, "Bright_" + std::to_string(k) + ".raw"));
        for (size_t k = 0; k < nEval; ++k) evals.push_back(frame(true, "Eval_" + std::to_string(k) + ".raw"));
    }

    // QA-A-252i: the same kind of frames at a size of the caller's choice (the tests that only check arguments, counts and masks need no 1024 x 1024 frames: a map run takes seconds there)
    void makeFramesSized(uint32_t w, uint32_t h, size_t nDark, size_t nBright) {
        uint32_t state = 777u;
        auto frame = [&](bool bright, const std::string& name) {
            std::vector<uint16_t> f(static_cast<size_t>(w) * h);
            for (uint32_t y = 0; y < h; ++y)
                for (uint32_t x = 0; x < w; ++x) {
                    double v = 3000.0 + ((x * 7 + y * 13) % 11) + (nextNoise(state) % 9);
                    if (bright) v += 6000.0 * (0.9 + 0.2 * x / (w - 1)) + (nextNoise(state) % 31);
                    f[static_cast<size_t>(y) * w + x] = static_cast<uint16_t>(v);
                }
            const fs::path p = root / "in" / name;
            writeBytes(p, f.data(), f.size() * 2);
            return p.string();
        };
        for (size_t k = 0; k < nDark; ++k) darks.push_back(frame(false, "Dark_" + std::to_string(k) + ".raw"));
        for (size_t k = 0; k < nBright; ++k) brights.push_back(frame(true, "Bright_" + std::to_string(k) + ".raw"));
    }

    std::string small() const { return " --width 1024 --height 1024 --margin 16 --line-gap 5 --pixels-per-level 3 --blocks-per-level 2"; }

    std::string files(const char* opt, const std::vector<std::string>& v) const {
        std::string a;
        for (const auto& f : v) a += std::string(" --") + opt + " \"" + f + "\"";
        return a;
    }

    int inject(const fs::path& out, const std::string& extra = "") {
        return runCmd(XPE_BPM_EVAL_EXE, "inject --out \"" + out.string() + "\"" + small() + files("dark", darks) + files("bright", brights) + files("eval", evals) + extra + " > \"" +
                                            (root / "inject.log").string() + "\" 2>&1");
    }

    // a map file (uint8) from the truth ids: flagged where pred(item) holds for the pixel's item (shared pixels are flagged when pred holds for any item that covers them)
    template <class P>
    fs::path mapOf(const fs::path& truth, const std::string& name, P pred) {
        const std::vector<Item> items = parseTruth(truth / "truth.tsv");
        std::vector<uint8_t> m(kN, 0);
        for (const Item& it : items) {
            if (!pred(it)) continue;
            for (const Rect& q : it.rects)
                for (int y = q.y; y < q.y + q.h; ++y)
                    for (int x = q.x; x < q.x + q.w; ++x) m[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = 1;
        }
        const fs::path p = root / name;
        writeBytes(p, m.data(), m.size());
        return p;
    }

    int score(const fs::path& truth, const fs::path& map, const std::string& extra, const fs::path& stdoutFile) {
        return runCmd(XPE_BPM_EVAL_EXE, "score --truth \"" + truth.string() + "\" --map \"" + map.string() + "\"" + extra + " > \"" + stdoutFile.string() + "\" 2>&1");
    }
};

// ---------------------------------------------------------------- injection
TEST_F(BpmEvalTest, InjectIsDeterministicAndDependsOnTheSeed) {
    makeFrames(5, 3, 2);
    const fs::path a = root / "a", b = root / "b", c = root / "c";
    ASSERT_EQ(0, inject(a));
    ASSERT_EQ(0, inject(b));
    ASSERT_EQ(0, inject(c, " --seed 252"));
    EXPECT_EQ(readBytes(a / "truth.tsv"), readBytes(b / "truth.tsv"));
    EXPECT_EQ(readBytes(a / "truth_ids.u16"), readBytes(b / "truth_ids.u16"));
    for (const auto& f : darks) EXPECT_EQ(readBytes(a / "frames" / fs::path(f).filename()), readBytes(b / "frames" / fs::path(f).filename())) << f;
    for (const auto& f : brights) EXPECT_EQ(readBytes(a / "frames" / fs::path(f).filename()), readBytes(b / "frames" / fs::path(f).filename())) << f;
    EXPECT_NE(readBytes(a / "truth.tsv"), readBytes(c / "truth.tsv")) << "another seed must plant elsewhere";
}

namespace {
bool isSmall(const Item& it) { return it.shape == "pixel" || it.shape.compare(0, 5, "block") == 0; }
bool isLineType(const Item& it) { return (it.cls == "line" || it.cls == "control") && !isSmall(it); }
}  // namespace

TEST_F(BpmEvalTest, InjectFollowsTheSpecCountsAndPlacementRules) {
    makeFrames(5, 3, 2);
    // an exclusion map: a block of the frame is declared defective already (large enough that a placement which ignored it would almost surely land in it)
    std::vector<uint8_t> excl(kN, 0);
    for (int y = 280; y < 520; ++y)
        for (int x = 280; x < 520; ++x) excl[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = 1;
    const fs::path ex = root / "excl.u8";
    writeBytes(ex, excl.data(), excl.size());
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out, " --exclude-mask \"" + ex.string() + "\""));
    const std::vector<Item> items = parseTruth(out / "truth.tsv");

    std::map<std::string, int> perClass;
    for (const Item& it : items) ++perClass[it.cls];
    // QA-A-252d: a response below 45 % is a defect, 0.45 and above is normal (class control)
    EXPECT_EQ(69, perClass["line"]);       // dead 1.0, nonlinear x2, noisy x2 at 6 shapes = 30 + gain_loss and efficiency_loss at 0.3, 0.4 and 0.44 = 36, + 3 crossings
    EXPECT_EQ(8, perClass["block"]);       // dead 1.0: 4 sizes x 2
    EXPECT_EQ(48, perClass["pixel"]);      // 16 levels x 3 (dead 1, hot x4, noisy x3, nonlinear x2, gain_loss and efficiency_loss at 0.3, 0.4 and 0.44)
    int controlLines = 0, controlBlocks = 0, controlPixels = 0;
    for (const Item& it : items) {
        if (it.cls != "control") continue;
        if (it.shape == "pixel") ++controlPixels;
        else if (isSmall(it)) ++controlBlocks;
        else ++controlLines;
    }
    EXPECT_EQ(54, controlLines);           // 12 bands (3 kinds x 2 levels x 2 directions) + dead 0.5 and the two loss models at 0.46, 0.6 and 0.7: 7 x 6 shapes = 42
    EXPECT_EQ(8, controlBlocks);           // dead 0.5: 4 sizes x 2
    EXPECT_EQ(33, controlPixels);          // 11 levels x 3 (dead 0.5, slope x4, gain_loss and efficiency_loss at 0.46, 0.6 and 0.7)
    std::map<std::string, int> pixelKinds;
    for (const Item& it : items) if (it.cls == "pixel") ++pixelKinds[it.kind + "/" + std::to_string(it.level)];
    EXPECT_EQ(16u, pixelKinds.size());
    for (const auto& kv : pixelKinds) EXPECT_EQ(3, kv.second) << kv.first;

    // placement rules
    std::vector<std::pair<int, int>> lineRowIntervals, lineColIntervals;   // inclusive, one per line rectangle
    for (const Item& it : items) {
        if (!isLineType(it)) continue;
        for (const Rect& q : it.rects) {
            if (q.w > q.h) lineRowIntervals.push_back({q.y, q.y + q.h - 1});
            else lineColIntervals.push_back({q.x, q.x + q.w - 1});
        }
    }
    auto gapsOk = [](std::vector<std::pair<int, int>> v, int gap) {
        std::sort(v.begin(), v.end());
        for (size_t i = 1; i < v.size(); ++i)
            if (v[i].first - v[i - 1].second <= gap) return false;
        return true;
    };
    EXPECT_TRUE(gapsOk(lineRowIntervals, 5));
    EXPECT_TRUE(gapsOk(lineColIntervals, 5));
    std::vector<Rect> small;
    for (const Item& it : items) {
        for (const Rect& q : it.rects) {
            // the margin binds the line numbers of a line and, when the line is partial, where its segment lies; a full line spans the frame by design
            const bool line = isLineType(it);
            const bool fullAcross = line && q.w == static_cast<int>(kW), fullDown = line && q.h == static_cast<int>(kH);
            if (!fullAcross) { EXPECT_GE(q.x, 16) << it.id; EXPECT_LE(q.x + q.w, static_cast<int>(kW) - 16) << it.id; }
            if (!fullDown) { EXPECT_GE(q.y, 16) << it.id; EXPECT_LE(q.y + q.h, static_cast<int>(kH) - 16) << it.id; }
            // nothing overlaps the exclusion block widened by 3
            const bool overlaps = q.x < 520 + 3 && q.x + q.w > 280 - 3 && q.y < 520 + 3 && q.y + q.h > 280 - 3;
            EXPECT_FALSE(overlaps) << "item " << it.id << " " << it.kind << " " << it.shape << " touches the exclusion zone";
        }
        if (isSmall(it)) {
            const Rect q = it.rects[0];
            for (const auto& r : lineRowIntervals) EXPECT_FALSE(q.y <= r.second + 3 && q.y + q.h - 1 >= r.first - 3) << "item " << it.id << " is within 3 pixels of a line row";
            for (const auto& c : lineColIntervals) EXPECT_FALSE(q.x <= c.second + 3 && q.x + q.w - 1 >= c.first - 3) << "item " << it.id << " is within 3 pixels of a line column";
            small.push_back(q);
        }
    }
    // where two lines cross, the item number map says "shared" (65535), so neither line claims the pixel
    {
        std::vector<uint16_t> ids(kN);
        const std::vector<uint8_t> raw = readBytes(out / "truth_ids.u16");
        ASSERT_EQ(kN * 2, raw.size());
        std::memcpy(ids.data(), raw.data(), raw.size());
        int crossings = 0;
        for (size_t a = 0; a < items.size(); ++a)
            for (size_t b = a + 1; b < items.size(); ++b) {
                if (isSmall(items[a]) || isSmall(items[b])) continue;
                for (const Rect& p : items[a].rects)
                    for (const Rect& q : items[b].rects) {
                        const Rect row = p.w > p.h ? p : q, col = p.w > p.h ? q : p;
                        if (!(p.w > p.h) == !(q.w > q.h)) continue;                                  // two rows or two columns never meet
                        const int x = col.x, y = row.y;
                        if (x < row.x || x >= row.x + row.w || y < col.y || y >= col.y + col.h) continue;   // a partial line that stops short
                        EXPECT_EQ(65535, ids[static_cast<size_t>(y) * kW + static_cast<size_t>(x)]) << "items " << items[a].id << " and " << items[b].id;
                        ++crossings;
                    }
            }
        EXPECT_GT(crossings, 0);
    }
    for (size_t i = 0; i < small.size(); ++i)
        for (size_t j = i + 1; j < small.size(); ++j) {
            const Rect &p = small[i], &q = small[j];
            const int dx = std::max(q.x - (p.x + p.w), p.x - (q.x + q.w)), dy = std::max(q.y - (p.y + p.h), p.y - (q.y + q.h));
            EXPECT_GE(std::max(dx, dy), 8) << "small items " << i << " and " << j << " are closer than 8 pixels";
        }
}

TEST_F(BpmEvalTest, InjectActsAsTheSpecSays) {
    makeFrames(6, 3, 1);
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    const std::vector<Item> items = parseTruth(out / "truth.tsv");
    std::vector<std::vector<uint16_t>> d0, b0, d1, b1;
    for (const auto& f : darks) { d0.push_back(readFrame(f)); d1.push_back(readFrame(out / "frames" / fs::path(f).filename())); }
    for (const auto& f : brights) { b0.push_back(readFrame(f)); b1.push_back(readFrame(out / "frames" / fs::path(f).filename())); }
    std::vector<double> D(kN, 0.0);
    for (const auto& f : d0) for (size_t i = 0; i < kN; ++i) D[i] += f[i];
    for (double& v : D) v /= static_cast<double>(d0.size());

    auto findItem = [&](const std::string& cls, const std::string& kind, const std::string& shape, double level) -> const Item* {
        for (const Item& it : items) if (it.cls == cls && it.kind == kind && it.shape == shape && std::fabs(it.level - level) < 1e-9) return &it;
        return nullptr;
    };
    auto idx = [](const Rect& q) { return static_cast<size_t>(q.y) * kW + static_cast<size_t>(q.x); };

    const Item* hot = findItem("pixel", "hot", "pixel", 200);
    ASSERT_NE(nullptr, hot);
    const size_t ih = idx(hot->rects[0]);
    for (size_t k = 0; k < d0.size(); ++k) EXPECT_EQ(d0[k][ih] + 200, d1[k][ih]) << "hot adds 200 to every dark frame";
    for (size_t k = 0; k < b0.size(); ++k) EXPECT_EQ(b0[k][ih] + 200, b1[k][ih]) << "and to every bright frame";

    const Item* dead = findItem("pixel", "dead", "pixel", 1.0);
    ASSERT_NE(nullptr, dead);
    const size_t id = idx(dead->rects[0]);
    for (size_t k = 0; k < d0.size(); ++k) EXPECT_EQ(d0[k][id], d1[k][id]) << "dead leaves the dark frames alone";
    for (size_t k = 0; k < b0.size(); ++k) EXPECT_NEAR(D[id], b1[k][id], 1.0) << "a fully dead pixel shows its dark level in bright frames";

    const Item* slope = findItem("control", "slope", "pixel", 1.25);
    ASSERT_NE(nullptr, slope);
    const size_t is = idx(slope->rects[0]);
    for (size_t k = 0; k < b0.size(); ++k) EXPECT_NEAR(D[is] + 1.25 * (b0[k][is] - D[is]), b1[k][is], 1.0);
    for (size_t k = 0; k < d0.size(); ++k) EXPECT_EQ(d0[k][is], d1[k][is]);

    const Item* nl = findItem("pixel", "nonlinear", "pixel", 0.4);
    ASSERT_NE(nullptr, nl);
    const size_t in = idx(nl->rects[0]);
    for (size_t k = 0; k < b0.size(); ++k) {
        const double n = b0[k][in] - D[in];
        EXPECT_NEAR(D[in] + n * (1.0 - 0.4 * n / 30000.0), b1[k][in], 1.0);
    }

    const Item* noisy = findItem("pixel", "noisy", "pixel", 4);
    ASSERT_NE(nullptr, noisy);
    const size_t inz = idx(noisy->rects[0]);
    double var = 0;
    for (size_t k = 0; k < d0.size(); ++k) { const double e = static_cast<double>(d1[k][inz]) - d0[k][inz]; var += e * e; }
    EXPECT_GT(var, 0.0) << "noise is added";

    const Item* fixedBand = findItem("control", "fixed_band", "row_full", 100);
    ASSERT_NE(nullptr, fixedBand);
    const Rect fb = fixedBand->rects[0];
    for (int x : {40, 400, 900})
        for (size_t k = 0; k < d0.size(); ++k) {
            const size_t i = static_cast<size_t>(fb.y) * kW + static_cast<size_t>(x);
            // a column line may cross the row: only pixels owned by this item alone are compared
            EXPECT_TRUE(d1[k][i] == d0[k][i] + 100 || d1[k][i] != d0[k][i]) << "fixed band changes the dark frames";
        }

    const Item* gainBand = findItem("control", "gain_band", "col_full", 0.8);
    ASSERT_NE(nullptr, gainBand);
    const Rect gb = gainBand->rects[0];
    const size_t ig = static_cast<size_t>(500) * kW + static_cast<size_t>(gb.x);   // row 500: far from lines? only compared when the pixel is untouched by another item
    std::vector<uint16_t> ids(kN);
    {
        const std::vector<uint8_t> raw = readBytes(out / "truth_ids.u16");
        std::memcpy(ids.data(), raw.data(), raw.size());
    }
    if (ids[ig] == static_cast<uint16_t>(gainBand->id)) {
        for (size_t k = 0; k < b0.size(); ++k) EXPECT_NEAR(D[ig] + 0.8 * (b0[k][ig] - D[ig]), b1[k][ig], 1.0);
        for (size_t k = 0; k < d0.size(); ++k) EXPECT_EQ(d0[k][ig], d1[k][ig]) << "a gain band leaves the dark frames alone";
    }
    // pixels outside every item are not touched
    size_t changed = 0, outside = 0;
    for (size_t i = 0; i < kN; ++i)
        if (ids[i] == 0) { ++outside; changed += (b0[0][i] != b1[0][i]); }
    EXPECT_EQ(0u, changed) << "of " << outside << " pixels outside every item";
}

// ---------------------------------------------------------------- QA-A-252d: response items
TEST_F(BpmEvalTest, ResponseItemsAreDefectsBelow45PercentAndNormalFrom45PercentOn) {
    makeFrames(5, 3, 2);
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    const std::vector<Item> items = parseTruth(out / "truth.tsv");
    auto classOf = [&](const char* kind, const char* shape, double level) -> std::string {
        for (const Item& it : items)
            if (it.kind == kind && it.shape == shape && std::fabs(it.level - level) < 1e-9) return it.cls;
        return "missing";
    };
    for (const char* kind : {"gain_loss", "efficiency_loss"}) {
        EXPECT_EQ("pixel", classOf(kind, "pixel", 0.3)) << kind;
        EXPECT_EQ("pixel", classOf(kind, "pixel", 0.4)) << kind << ": 40 % is below 45 %";
        EXPECT_EQ("pixel", classOf(kind, "pixel", 0.44)) << kind << ": 44 % is below 45 %";
        EXPECT_EQ("control", classOf(kind, "pixel", 0.46)) << kind << ": 46 % is not";
        EXPECT_EQ("control", classOf(kind, "pixel", 0.6)) << kind << ": the label of these items is the RESPONSE axis only; the noise axis is measured (noise-cost)";
        EXPECT_EQ("missing", classOf(kind, "pixel", 0.45)) << kind << ": the exact boundary is not planted (a response estimate has a noise of about 0.0007)";
        EXPECT_EQ("control", classOf(kind, "pixel", 0.7)) << kind;
        EXPECT_EQ("line", classOf(kind, "row_full", 0.3)) << kind;
        EXPECT_EQ("line", classOf(kind, "row_full", 0.44)) << kind;
        EXPECT_EQ("line", classOf(kind, "row_full", 0.4)) << kind;
        EXPECT_EQ("control", classOf(kind, "row_full", 0.6)) << kind;
        EXPECT_EQ("control", classOf(kind, "col_partial", 0.46)) << kind;
        EXPECT_EQ("control", classOf(kind, "row_pair", 0.7)) << kind;
    }
    EXPECT_EQ("line", classOf("dead", "row_full", 1.0));
    EXPECT_EQ("control", classOf("dead", "row_full", 0.5)) << "a response of 0.5 is normal now";
    EXPECT_EQ("control", classOf("dead", "pixel", 0.5));
    EXPECT_EQ("control", classOf("dead", "block4", 0.5));
    EXPECT_EQ("block", classOf("dead", "block4", 1.0));
    for (double lv : {0.75, 0.9, 1.1, 1.25}) EXPECT_EQ("control", classOf("slope", "pixel", lv)) << lv;
    EXPECT_EQ("pixel", classOf("nonlinear", "pixel", 0.4)) << "nonlinearity is judged by the linearity feature, not by a response ratio";
}

TEST_F(BpmEvalTest, EfficiencyLossScalesTheQuantumNoiseVarianceWithTheSignalAndGainLossDoesNot) {
    makeFrames(5, 3, 2);
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    const std::vector<Item> items = parseTruth(out / "truth.tsv");
    const std::string json = text(out / "truth.json");
    const size_t kp = json.find("\"quantum_gain\": ");
    ASSERT_NE(std::string::npos, kp) << json;
    const double kappa = std::atof(json.c_str() + kp + 16);
    EXPECT_GT(kappa, 0.0);
    EXPECT_NE(std::string::npos, json.find("\"quantum_gain_measured\": true")) << "three bright frames of one dose are in the generation set";
    std::vector<std::vector<uint16_t>> d0, b0, b1;
    for (const auto& f : darks) d0.push_back(readFrame(f));
    for (const auto& f : brights) { b0.push_back(readFrame(f)); b1.push_back(readFrame(out / "frames" / fs::path(f).filename())); }
    std::vector<double> D(kN, 0.0);
    for (const auto& f : d0) for (size_t i = 0; i < kN; ++i) D[i] += f[i];
    for (double& v : D) v /= static_cast<double>(d0.size());
    const std::vector<uint8_t> raw = readBytes(out / "truth_ids.u16");
    std::vector<uint16_t> ids(kN);
    std::memcpy(ids.data(), raw.data(), raw.size());
    auto residual = [&](const char* kind, double r, double* meanNet) {
        const Item* row = nullptr;
        for (const Item& it : items) if (it.kind == kind && it.shape == "row_full" && std::fabs(it.level - r) < 1e-9) row = &it;
        EXPECT_NE(nullptr, row) << kind;
        double s1 = 0, s2 = 0, sn = 0;
        size_t cnt = 0;
        if (row) {
            const int y = row->rects[0].y;
            for (size_t k = 0; k < b0.size(); ++k)
                for (int x = 0; x < static_cast<int>(kW); ++x) {
                    const size_t i = static_cast<size_t>(y) * kW + static_cast<size_t>(x);
                    if (ids[i] != static_cast<uint16_t>(row->id)) continue;   // a crossing line is not ours alone
                    const double n = b0[k][i] - D[i];
                    const double e = static_cast<double>(b1[k][i]) - (D[i] + r * n);
                    s1 += e; s2 += e * e; sn += n; ++cnt;
                }
        }
        EXPECT_GT(cnt, 500u);
        *meanNet = cnt ? sn / static_cast<double>(cnt) : 0.0;
        return cnt ? s2 / static_cast<double>(cnt) - (s1 / static_cast<double>(cnt)) * (s1 / static_cast<double>(cnt)) : 0.0;
    };
    double netGain = 0, netEff = 0;
    const double varGain = residual("gain_loss", 0.3, &netGain);
    const double varEff = residual("efficiency_loss", 0.3, &netEff);
    EXPECT_LT(varGain, 0.5) << "gain loss is deterministic (the rounding of the pixel value only)";
    const double predicted = kappa * 0.3 * 0.7 * netEff;
    EXPECT_GT(varEff, 0.7 * predicted) << "efficiency loss adds kappa * r * (1 - r) * signal of noise variance";
    EXPECT_LT(varEff, 1.3 * predicted);
}

// ---------------------------------------------------------------- QA-A-252f: quantum gain, noise cost, two axes
TEST(BpmEvalStatsTest, ChiSquareHelpersMatchKnownValues) {
    using namespace xpe_bpm_eval;
    EXPECT_NEAR(0.454936, chiSquareMedianOverNu(1), 1e-5) << "the median of the sample variance of two frames is 0.455 of the variance";
    EXPECT_NEAR(0.693147, chiSquareMedianOverNu(2), 1e-5) << "three frames: 0.69 (the bias of #208)";
    EXPECT_NEAR(0.870292, chiSquareMedianOverNu(5), 1e-5);
    EXPECT_NEAR(5.023886, chiSquareQuantile(1, 0.975), 1e-4);
    EXPECT_NEAR(9.341818, chiSquareQuantile(10, 0.5), 1e-4);
    EXPECT_NEAR(914.257, chiSquareQuantile(1000, 0.025), 0.02);
    EXPECT_NEAR(1.0 - std::exp(-1.0), gammaP(1.0, 1.0), 1e-12);
    EXPECT_NEAR(0.233, medianRelativeSe(1, 100), 0.005) << "2.33 / sqrt(n) for one degree of freedom";
}

TEST(BpmEvalStatsTest, TheQuantumGainFitRecoversTheLineAndWidensWithTheScatter) {
    using namespace xpe_bpm_eval;
    auto group = [](double signal, double v0, double kappa) { return QuantumGainGroup{signal, v0 + kappa * signal, 2.0, 100000.0}; };
    std::vector<QuantumGainGroup> exact = {group(3000, 9, 0.3), group(9000, 9, 0.3), group(20000, 9, 0.3)};
    const QuantumGainFit f = fitQuantumGain(exact);
    ASSERT_TRUE(f.ok);
    EXPECT_NEAR(0.3, f.kappa, 1e-9);
    EXPECT_NEAR(9.0, f.v0, 1e-6);
    EXPECT_GT(f.kappaSe, 0.0);
    std::vector<QuantumGainGroup> scatter = exact;
    scatter[1].variance *= 1.06;   // one group off the line by 6 %
    const QuantumGainFit g = fitQuantumGain(scatter);
    ASSERT_TRUE(g.ok);
    EXPECT_GT(g.kappaSe, 1.5 * f.kappaSe) << "the scatter about the line widens the standard error";
    EXPECT_FALSE(fitQuantumGain({group(9000, 9, 0.3)}).ok) << "one group cannot separate the readout variance from the quantum part";
}

TEST(BpmEvalStatsTest, TheNoiseVerdictFollowsTheIntervalAgainstTheLimit) {
    using namespace xpe_bpm_eval;
    EXPECT_EQ(NoiseLabel::Defect, classifyNoiseCost(3.3, 1024, 0.04).label);
    EXPECT_EQ(NoiseLabel::Normal, classifyNoiseCost(1.0, 1024, 0.04).label);
    EXPECT_EQ(NoiseLabel::Undetermined, classifyNoiseCost(2.0, 1024, 0.04).label) << "an estimate at the limit: the interval contains it";
    EXPECT_EQ(NoiseLabel::Undetermined, classifyNoiseCost(3.3, 1.0, 0.04).label) << "one degree of freedom cannot show a cost of 3.3";
    EXPECT_EQ(NoiseLabel::Defect, classifyNoiseCost(2.1, 1e6, 0.0).label) << "a precise estimate above the limit";
    EXPECT_EQ(NoiseLabel::Defect, classifyNoiseCost(2.3, 3072, 0.0).label) << "without the error of the expectation the interval of 2.3 lies above 2";
    EXPECT_EQ(NoiseLabel::Undetermined, classifyNoiseCost(2.3, 3072, 0.10).label) << "an expectation known to 10 % widens the interval over the limit";
    const NoiseVerdict v = classifyNoiseCost(1.9, 3072, 0.04);
    EXPECT_LT(v.lo, 1.9);
    EXPECT_GT(v.hi, 1.9);
}

// Gaussian frames with a known quantum gain and readout variance: dark = pedestal + N(0, 9), bright = dark + signal + N(0, v0 + kappa * signal); three doses of three frames and
// two evaluation frames at the middle dose
TEST_F(BpmEvalTest, DefectXcalCommandReadsBothVersionsAndSaysWhyItRefuses) {
    // QA-A-254: a version-2 file made by the writer, the command's summary and the planes it writes out; a damaged file is refused with the alert text in the output
    const uint32_t W = 64, H = 48;
    std::vector<uint8_t> map(static_cast<size_t>(W) * H, 0), area(map.size(), 1);
    map[5 * W + 5] = 1;
    map[6 * W + 7] = 2;
    for (uint32_t x = 0; x < W; ++x) area[x] = 0;   // the top row is outside
    DefectExt ext;
    DefectExtLine L;
    L.orientation = 0; L.index = 20; L.width = 1; L.start = 8; L.end = 40; L.causes = 1; L.correction = XPE_DEFECT_LINE_CORRECT_INTERPOLATE; L.strength = 4.0f;
    L.segments = {{20, 8, 40}};
    ext.lines = {L};
    ext.blobs = {{5, 5, 5, 5, 1}};
    DefectXcalWrite w;
    w.width = W; w.height = H; w.map = map.data(); w.area = area.data(); w.ext = &ext; w.session_id = "t";
    const fs::path f = root / "d2.xcal";
    ASSERT_EQ(XPE_OK, write_defect_xcal(f.string().c_str(), w));
    ASSERT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, "defect-xcal --in \"" + f.string() + "\" --map-out \"" + (root / "m.u8").string() + "\" --area-out \"" + (root / "a.u8").string() + "\" --lines-out \"" +
                                                (root / "l.tsv").string() + "\" > \"" + (root / "d.txt").string() + "\" 2>&1"));
    const std::string out = text(root / "d.txt"), lines = text(root / "l.tsv");
    EXPECT_NE(std::string::npos, out.find("format version 2, 64 x 48")) << out;
    EXPECT_NE(std::string::npos, out.find("defect plane: 2 pixels flagged")) << out;
    EXPECT_NE(std::string::npos, out.find("imaging area: 64 pixels outside")) << out;
    EXPECT_NE(std::string::npos, out.find("line groups: 1 (1 stretches), blobs: 1, unconfirmed candidates: 0")) << out;
    EXPECT_EQ(map, readBytes(root / "m.u8"));
    EXPECT_EQ(area, readBytes(root / "a.u8"));
    EXPECT_NE(std::string::npos, lines.find("line\trow\t20\t1\t8\t40")) << lines;
    EXPECT_NE(std::string::npos, lines.find("20:8-40")) << lines;
    std::vector<uint8_t> b = readBytes(f);
    b[4] = 9;   // an unsupported version number
    writeBytes(root / "bad.xcal", b.data(), b.size());
    EXPECT_EQ(3, runCmd(XPE_BPM_EVAL_EXE, "defect-xcal --in \"" + (root / "bad.xcal").string() + "\" > \"" + (root / "bad.txt").string() + "\" 2>&1"));
    EXPECT_NE(std::string::npos, text(root / "bad.txt").find("alert: XPE_ERR_XCAL_VERSION: file version 9")) << text(root / "bad.txt");
}

class BpmEvalQuantumTest : public BpmEvalTest {
protected:
    static constexpr double kKappa = 0.30, kV0 = 9.0;
    double evalSpread{0.0};   // the two evaluation frames differ in dose by -evalSpread and +evalSpread (QA-A-252g)
    void makeQuantumFrames() {
        std::mt19937_64                    rng(2517);
        std::normal_distribution<double>   unit(0.0, 1.0);
        auto frame = [&](double signal, const std::string& name) {
            std::vector<uint16_t> f(kN);
            const double          sd = std::sqrt(signal > 0.0 ? kV0 + kKappa * signal : 9.0);
            for (size_t i = 0; i < kN; ++i) f[i] = static_cast<uint16_t>(std::floor(3000.0 + signal + sd * unit(rng) + 0.5));
            const fs::path p = root / "in" / name;
            writeBytes(p, f.data(), f.size() * 2);
            return p.string();
        };
        for (int k = 0; k < 5; ++k) darks.push_back(frame(0.0, "Dark_" + std::to_string(k) + ".raw"));
        int b = 0;
        for (double signal : {3000.0, 9000.0, 20000.0})
            for (int k = 0; k < 3; ++k) brights.push_back(frame(signal, "Bright_" + std::to_string(b++) + ".raw"));
        for (int k = 0; k < 2; ++k) evals.push_back(frame(9000.0 * (1.0 + (k ? evalSpread : -evalSpread)), "Eval_" + std::to_string(k) + ".raw"));
    }
    static double number(const std::string& json, const char* key) {
        const size_t p = json.find(std::string("\"") + key + "\": ");
        return p == std::string::npos ? -1.0 : std::atof(json.c_str() + p + std::strlen(key) + 4);
    }
};

TEST_F(BpmEvalQuantumTest, TheQuantumGainIsEstimatedWithoutTheFrameCountBias) {
    makeQuantumFrames();
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    const std::string json = text(out / "truth.json");
    const double kappa = number(json, "quantum_gain"), se = number(json, "quantum_gain_se"), v0 = number(json, "readout_variance"), v0Se = number(json, "readout_variance_se");
    EXPECT_NEAR(kKappa, kappa, 0.018) << "within 6 %: the median of the sample variances of three frames is 0.69 of the variance and would give 0.21 without the correction. " << json;
    EXPECT_GT(se, 0.0) << json;
    EXPECT_LT(se, 0.03) << json;
    EXPECT_LT(std::fabs(kappa - kKappa), 4.0 * se) << "the reported standard error describes the error: " << json;
    EXPECT_GT(v0Se, 0.0) << json;
    EXPECT_LT(std::fabs(v0 - kV0), 4.0 * v0Se) << "the intercept is poorly determined by three doses; its reported error must cover the deviation: " << json;
    EXPECT_NE(std::string::npos, json.find("\"quantum_gain_measured\": true")) << json;
    EXPECT_NE(std::string::npos, json.find("regression of the group variance")) << json;
    EXPECT_NE(std::string::npos, json.find("\"quantum_gain_groups\": [{")) << "the dose dependence is written with the fit: " << json;
}

TEST_F(BpmEvalQuantumTest, NoiseCostIsMeasuredNotDerivedFromTheModelAndPixelsAreUndetermined) {
    makeQuantumFrames();
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    std::string args = "noise-cost --truth \"" + out.string() + "\" --out \"" + (root / "nc.tsv").string() + "\"";
    for (const auto& f : darks) args += " --dark \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
    for (const auto& f : evals) args += " --verify \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
    ASSERT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, args + " > \"" + (root / "nc.log").string() + "\" 2>&1")) << text(root / "nc.log");
    std::map<std::string, std::vector<std::string>> byKey;   // kind/shape/level -> label, cost (the last item of the key)
    std::map<std::string, std::vector<std::string>> labels;  // kind/shape/level -> the labels of all its items
    std::ifstream f(root / "nc.tsv");
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        std::vector<std::string> c;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, '\t')) c.push_back(cell);
        ASSERT_EQ(13u, c.size()) << line;
        EXPECT_EQ("provisional", c[12]) << "the labels of the noise truth are provisional";
        byKey[c[1] + "/" + c[2] + "/" + c[3]] = {c[9], c[6], c[7], c[8]};
        labels[c[1] + "/" + c[2] + "/" + c[3]].push_back(c[9]);
    }
    auto cost = [&](const char* key) { return std::atof(byKey[key][1].c_str()); };
    // 1 + kappa * mu * (1 - r) / (r * (v0 + kappa * mu)) at mu = 9000 is 3.33 for r = 0.3 and 1.66 for r = 0.6; a gain loss is 1
    ASSERT_TRUE(byKey.count("gain_loss/row_full/0.3"));
    EXPECT_NEAR(1.0, cost("gain_loss/row_full/0.3"), 0.15);
    EXPECT_EQ("normal", byKey["gain_loss/row_full/0.3"][0]) << "a gain loss amplifies the noise with the signal: no cost";
    EXPECT_NEAR(3.33, cost("efficiency_loss/row_full/0.3"), 0.45);
    EXPECT_EQ("defect", byKey["efficiency_loss/row_full/0.3"][0]);
    EXPECT_NEAR(1.66, cost("efficiency_loss/row_full/0.6"), 0.3);
    EXPECT_NE("defect", byKey["efficiency_loss/row_full/0.6"][0]) << "a cost of 1.66 is below the limit";
    // one pixel has one degree of freedom: its interval reaches 1000 times its estimate upward and 0.2 times downward, so it is undetermined unless its single estimate is
    // below 0.002 (the two frames agree, 3.6 % of the pixels) or above 10 (0.15 %)
    int pixelItems = 0, pixelUndetermined = 0;
    for (const auto& kv : labels) {
        if (kv.first.find("/pixel/") == std::string::npos) continue;
        for (const std::string& l : kv.second) {
            ++pixelItems;
            pixelUndetermined += l == "undetermined" ? 1 : 0;
        }
    }
    ASSERT_GT(pixelItems, 30);
    EXPECT_GE(pixelUndetermined, pixelItems * 8 / 10) << pixelUndetermined << " of " << pixelItems << " pixel items are undetermined";
}

TEST(BpmEvalStatsTest, TheEffectiveDegreesOfFreedomFallWhenThePixelsAreCorrelated) {
    using namespace xpe_bpm_eval;
    std::mt19937_64 rng(11);
    std::normal_distribution<double> unit(0.0, 1.0);
    const size_t n = 4096;
    std::vector<double> iid(n), smooth(n);
    for (size_t i = 0; i < n; ++i) {
        const double z = unit(rng);
        iid[i] = z * z;   // variance ratios of one degree of freedom
    }
    // the same ratios, each value repeated over 16 neighbours: the mean of the item then has a 16 times larger variance than independent pixels would give
    for (size_t i = 0; i < n; ++i) smooth[i] = iid[i / 16 * 16];
    const BlockDof a = blockEffectiveDof(iid, 64), b = blockEffectiveDof(smooth, 64);
    EXPECT_NEAR(1.0, a.mean, 0.1);
    EXPECT_LT(std::fabs(a.lag1), 0.1) << "independent pixels";
    EXPECT_GT(a.dofEff, 0.5 * static_cast<double>(n)) << "independent pixels: about one degree of freedom per pixel (" << a.dofEff << ")";
    EXPECT_GT(b.lag1, 0.8) << "runs of 16 identical values correlate neighbours (15/16)";
    EXPECT_LT(b.dofEff, 0.3 * static_cast<double>(n)) << "correlated pixels: far fewer than one degree of freedom per pixel (" << b.dofEff << ")";
    EXPECT_EQ(0.0, blockEffectiveDof(std::vector<double>(100, 1.0), 64).dofEff) << "fewer than four blocks: not determinable";
}

TEST_F(BpmEvalQuantumTest, NoiseCostTakesTheExposureOutOfTheVerificationFramesAndRawFramesShowWhatItRemoves) {
    // the two verification frames differ in dose by 6 %. A pixel that keeps 0.3 of the value keeps 0.3 of that difference while the row and column means carry all of it: without the
    // exposure taken out the cost of a gain-loss line reads in the hundreds. With it, a gain loss costs 1 and an efficiency loss 3.3.
    evalSpread = 0.03;
    makeQuantumFrames();
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    auto run = [&](const char* extra, const char* name) {
        std::string args = "noise-cost --truth \"" + out.string() + "\" --out \"" + (root / name).string() + "\"" + extra;
        for (const auto& f : darks) args += " --dark \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
        for (const auto& f : evals) args += " --verify \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
        return runCmd(XPE_BPM_EVAL_EXE, args + " > \"" + (root / (std::string(name) + ".log")).string() + "\" 2>&1");
    };
    ASSERT_EQ(0, run("", "nc.tsv")) << text(root / "nc.tsv.log");
    ASSERT_EQ(0, run(" --raw-frames 1", "nc_raw.tsv")) << text(root / "nc_raw.tsv.log");
    auto costOf = [&](const char* file, const char* kind, const char* level, std::string* label, const char* shape = "row_full") {
        std::ifstream f(root / file);
        std::string line;
        std::getline(f, line);
        while (std::getline(f, line)) {
            std::vector<std::string> c;
            std::stringstream ss(line);
            std::string cell;
            while (std::getline(ss, cell, '\t')) c.push_back(cell);
            if (c.size() >= 13 && c[1] == kind && c[2] == shape && c[3] == level) {
                if (label) *label = c[9];
                return std::atof(c[6].c_str());
            }
        }
        return -1.0;
    };
    std::string labelGain, labelEff;
    const double gain = costOf("nc.tsv", "gain_loss", "0.3", &labelGain), eff = costOf("nc.tsv", "efficiency_loss", "0.3", &labelEff);
    // a single pixel item: the row and column means do not carry the exposure difference of a whole line away, so only the scaling can remove it
    const double pixNorm = costOf("nc.tsv", "gain_loss", "0.3", nullptr, "pixel"), pixRaw = costOf("nc_raw.tsv", "gain_loss", "0.3", nullptr, "pixel");
    EXPECT_NEAR(1.0, gain, 0.2) << "a gain loss costs 1 once the exposure is out";
    EXPECT_EQ("normal", labelGain);
    EXPECT_NEAR(3.33, eff, 0.5);
    EXPECT_EQ("defect", labelEff);
    {   // the interval uses the effective degrees of freedom of blocks: never more than pixels x nu, and below it for at least one item whose pixels are not exactly independent
        std::ifstream f(root / "nc.tsv");
        std::string   line;
        std::getline(f, line);
        int fewer = 0, more = 0;
        while (std::getline(f, line)) {
            std::vector<std::string> c;
            std::stringstream        ss(line);
            std::string              cell;
            while (std::getline(ss, cell, '	')) c.push_back(cell);
            if (c.size() < 13) continue;
            const double used = std::atof(c[5].c_str()), indep = std::atof(c[10].c_str());
            fewer += used < indep;
            more += used > indep;
        }
        EXPECT_EQ(0, more) << "the degrees of freedom of the interval never exceed pixels x nu";
        EXPECT_GE(fewer, 1) << "at least one item has fewer effective than independent degrees of freedom";
    }
    EXPECT_GT(pixRaw, 20.0) << "without the scaling the exposure difference of the two frames reads as a noise of the pixel";
    EXPECT_GT(pixRaw, 10.0 * pixNorm) << "the scaling removes it (a chi-square draw of one pixel stays below that: " << pixNorm << ")";
}

TEST_F(BpmEvalQuantumTest, NoiseCostRefusesVerificationFramesWhoseMedianNetSignalIsNotPositive) {
    // QA-A-252h: the dose of a verification frame is the median of its net signal; a frame at or below the dark level has no dose, and the scale would divide by it
    makeQuantumFrames();
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    std::string args = "noise-cost --truth \"" + out.string() + "\" --out \"" + (root / "nc_bad.tsv").string() + "\"";
    for (const auto& f : darks) args += " --dark \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
    for (const auto& f : evals) {
        const fs::path p = out / "frames" / fs::path(f).filename();
        std::vector<char> zeros(static_cast<size_t>(fs::file_size(p)), 0);
        std::ofstream(p, std::ios::binary | std::ios::trunc).write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
        args += " --verify \"" + p.string() + "\"";
    }
    EXPECT_NE(0, runCmd(XPE_BPM_EVAL_EXE, args + " > \"" + (root / "nc_bad.log").string() + "\" 2>&1")) << "all-zero verification frames have no dose";
    EXPECT_NE(std::string::npos, text(root / "nc_bad.log").find("not positive")) << text(root / "nc_bad.log");
    EXPECT_FALSE(fs::exists(root / "nc_bad.tsv")) << "no truth file is written for a dose that is not defined";
}

TEST_F(BpmEvalTest, OverlapCompositeScoresEveryOverlapByOneRuleThroughTheInjectionChain) {
    // a 40 x 40 truth made by hand: 9 items; the 4 rows cross the 3 columns (12 overlaps) plus a hot pixel over item 1 and an undetermined pixel over item 3: 14 overlapped pixels
    struct It { int id; const char* cls; const char* kind; const char* shape; const char* level; int x, y, w, h; };
    const std::vector<It> items = {
        {1, "control", "gain_loss", "row_full", "0.7", 0, 10, 40, 1},      {2, "control", "efficiency_loss", "col_full", "0.6", 15, 0, 1, 40},   // A: 0.7 x 0.6 = 0.42
        {3, "control", "gain_loss", "row_full", "0.9", 0, 25, 40, 1},      {4, "control", "gain_band", "col_full", "0.8", 30, 0, 1, 40},         // B: 0.9 x 0.8 = 0.72
        {5, "control", "dead", "row_full", "0.5", 0, 33, 40, 1},           {6, "control", "gain_loss", "col_full", "0.8", 5, 0, 1, 40},          // C: dead 0.5 x gain 0.8 = 0.40
        {7, "pixel", "hot", "pixel", "100", 20, 10, 1, 1},                                                                                     // D: over item 1 at (20,10)
        {8, "undetermined", "efficiency_loss", "pixel", "0.6", 35, 25, 1, 1},                                                                  // E: over item 3 at (35,25)
        {9, "control", "fixed_band", "row_full", "30", 0, 2, 40, 1}};
    const int W = 40, H = 40;
    std::string tsv = "#xpe_bpm_eval truth 1\nwidth\t40\nheight\t40\nseed\t1\nsigma_t\t3.000000\nitems\t" + std::to_string(items.size()) + "\n";
    std::vector<uint16_t> ids(static_cast<size_t>(W) * H, 0);
    std::vector<int>      cover(ids.size(), 0);
    for (const It& it : items) {
        tsv += std::to_string(it.id) + "\t" + it.cls + "\t" + it.kind + "\t" + it.shape + "\t" + it.level + "\t" + std::to_string(it.x) + "," + std::to_string(it.y) + "," + std::to_string(it.w) + "," + std::to_string(it.h) + "\n";
        for (int y = it.y; y < it.y + it.h; ++y)
            for (int x = it.x; x < it.x + it.w; ++x) ++cover[static_cast<size_t>(y * W + x)];
    }
    for (const It& it : items)
        for (int y = it.y; y < it.y + it.h; ++y)
            for (int x = it.x; x < it.x + it.w; ++x) ids[static_cast<size_t>(y * W + x)] = cover[static_cast<size_t>(y * W + x)] > 1 ? 65535 : static_cast<uint16_t>(it.id);
    const fs::path t = root / "tc";
    fs::create_directories(t);
    writeBytes(t / "truth.tsv", tsv.data(), tsv.size());
    writeBytes(t / "truth_ids.u16", ids.data(), ids.size() * 2);
    std::vector<uint8_t> map(ids.size(), 0);
    map[10 * W + 15] = 1;   // A
    map[25 * W + 30] = 1;   // B
    const fs::path m = root / "tc_map.u8";
    writeBytes(m, map.data(), map.size());
    ASSERT_EQ(0, score(t, m, " --out \"" + (root / "tc_base.json").string() + "\"", root / "tc_base.txt"));
    EXPECT_EQ(std::string::npos, text(root / "tc_base.json").find("overlap_pixels")) << "without --overlap nothing of the composite is reported";
    ASSERT_EQ(0, score(t, m, " --overlap composite --overlap-out \"" + (root / "tc_overlaps.tsv").string() + "\" --out \"" + (root / "tc.json").string() + "\"", root / "tc.txt"));
    const std::string j = text(root / "tc.json"), console = text(root / "tc.txt"), list = text(root / "tc_overlaps.tsv");
    // responses through the chain: row 10 (gain 0.7) x columns 0.6 / 0.8 / 0.8 = 0.42 (defect) / 0.56 / 0.56; row 25 (gain 0.9) x 0.6 / 0.8 / 0.8 = 0.54 / 0.72 / 0.72; row 33 (dead 0.5) x 0.6 / 0.8 / 0.8 =
    // 0.30 / 0.40 / 0.40 (three defects); row 2 (fixed band 30, additive) x the columns = the column response (normal); the hot pixel over row 10 is a defect by itself, the undetermined one stays undetermined
    EXPECT_NE(std::string::npos, j.find("\"overlap_pixels\": 14")) << j;
    EXPECT_NE(std::string::npos, j.find("\"overlap_defect\": 5")) << "0.42, three dead crossings, the hot pixel: " << j;
    EXPECT_NE(std::string::npos, j.find("\"overlap_defect_by_response\": 4")) << "all but the hot pixel are defects only by the response through the chain: " << j;
    EXPECT_NE(std::string::npos, j.find("\"overlap_undetermined\": 1")) << j;
    EXPECT_NE(std::string::npos, j.find("\"overlap_flagged_in_defect\": 1")) << "the map flags A: " << j;
    EXPECT_NE(std::string::npos, j.find("\"overlap_flagged_in_normal\": 1")) << "the map flags B, which is normal by the composite rule: " << j;
    EXPECT_NE(std::string::npos, console.find("distinct overlaps")) << console;
    EXPECT_NE(std::string::npos, list.find("0.4200")) << "A in the list: " << list;
    EXPECT_NE(std::string::npos, list.find("0.4000")) << "dead 0.5 x gain 0.8 in the list: " << list;
    EXPECT_NE(std::string::npos, list.find("0.3000")) << "dead 0.5 x efficiency 0.6 in the list: " << list;
    EXPECT_NE(std::string::npos, list.find("0.7200")) << "B in the list: " << list;
    EXPECT_NE(std::string::npos, console.find("normal->defect 4")) << "four pixels change from normal to defect: " << console;
}

TEST_F(BpmEvalQuantumTest, NoiseCostRefusesAnEmptyDoseSampleAndANonPositiveMedianEachWithItsOwnReason) {
    // QA-A-254 (Codex #216): the two refusals are tested apart. (1) --sample-margin larger than half the image leaves no unplanted interior pixel: refused before nth_element, the message says "no unplanted";
    // (2) frames at zero: the median net signal is not positive, the message says "not positive". Neither writes a truth file.
    makeQuantumFrames();
    const fs::path out = root / "t";
    ASSERT_EQ(0, inject(out));
    auto args = [&](const std::string& name, const std::string& extra) {
        std::string a = "noise-cost --truth \"" + out.string() + "\" --out \"" + (root / name).string() + "\"" + extra;
        for (const auto& f : darks) a += " --dark \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
        for (const auto& f : evals) a += " --verify \"" + (out / "frames" / fs::path(f).filename()).string() + "\"";
        return a;
    };
    EXPECT_NE(0, runCmd(XPE_BPM_EVAL_EXE, args("nc_empty.tsv", " --sample-margin 100000") + " > \"" + (root / "nc_empty.log").string() + "\" 2>&1"));
    const std::string logEmpty = text(root / "nc_empty.log");
    EXPECT_NE(std::string::npos, logEmpty.find("no unplanted interior pixel")) << logEmpty;
    EXPECT_EQ(std::string::npos, logEmpty.find("not positive")) << "a different refusal: " << logEmpty;
    EXPECT_FALSE(fs::exists(root / "nc_empty.tsv"));
    for (const auto& f : evals) {
        const fs::path p = out / "frames" / fs::path(f).filename();
        std::vector<char> zeros(static_cast<size_t>(fs::file_size(p)), 0);
        std::ofstream(p, std::ios::binary | std::ios::trunc).write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
    }
    EXPECT_NE(0, runCmd(XPE_BPM_EVAL_EXE, args("nc_zero.tsv", "") + " > \"" + (root / "nc_zero.log").string() + "\" 2>&1"));
    const std::string logZero = text(root / "nc_zero.log");
    EXPECT_NE(std::string::npos, logZero.find("not positive")) << logZero;
    EXPECT_EQ(std::string::npos, logZero.find("no unplanted")) << logZero;
    EXPECT_FALSE(fs::exists(root / "nc_zero.tsv"));
}

TEST_F(BpmEvalTest, TheScoreCombinesTheTwoAxesAndCountsUndeterminedItemsInNeitherDirection) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    const std::vector<Item> items = parseTruth(t / "truth.tsv");
    std::string tsv = "id\tkind\tshape\tlevel\tpixels\tdof\tcost\tlo\thi\tlabel\n";
    int promoted = -1;
    for (const Item& it : items) {
        if (it.kind == "efficiency_loss" && it.shape == "row_full" && std::fabs(it.level - 0.46) < 1e-9) {
            tsv += std::to_string(it.id) + "\tefficiency_loss\trow_full\t0.46\t100\t100\t2.4\t2.1\t2.7\tdefect\n";   // response normal, noise defect
            promoted = it.id;
        } else if (it.kind == "efficiency_loss" && it.shape == "pixel" && std::fabs(it.level - 0.6) < 1e-9) {
            tsv += std::to_string(it.id) + "\tefficiency_loss\tpixel\t0.6\t1\t1\t1.5\t0.01\t90\tundetermined\n";
        } else if (it.kind == "gain_loss" && it.shape == "row_full" && std::fabs(it.level - 0.3) < 1e-9) {
            tsv += std::to_string(it.id) + "\tgain_loss\trow_full\t0.3\t100\t100\t1.0\t0.9\t1.1\tnormal\n";   // response defect, noise normal: still a defect
        }
    }
    ASSERT_GT(promoted, 0);
    writeBytes(root / "noise.tsv", tsv.data(), tsv.size());
    // the map flags exactly the promoted line and the undetermined pixels
    const fs::path m = mapOf(t, "m.u8", [](const Item& it) { return (it.kind == "efficiency_loss" && it.shape == "row_full" && std::fabs(it.level - 0.46) < 1e-9) || (it.kind == "efficiency_loss" && it.shape == "pixel" && std::fabs(it.level - 0.6) < 1e-9); });
    ASSERT_EQ(0, score(t, m, " --out \"" + (root / "base.json").string() + "\"", root / "base.txt"));
    const std::string base = text(root / "base.json");
    EXPECT_EQ(std::string::npos, base.find("\"control_false_detections\": 0")) << "without the noise axis the flagged control items are false detections: " << base;
    ASSERT_EQ(0, score(t, m, " --noise-truth \"" + (root / "noise.tsv").string() + "\" --out \"" + (root / "axes.json").string() + "\"", root / "axes.txt"));
    const std::string j = text(root / "axes.json"), console = text(root / "axes.txt");
    EXPECT_NE(std::string::npos, j.find("\"noise_axis_promoted_to_defect\": 1")) << j;
    EXPECT_NE(std::string::npos, j.find("\"undetermined_items\": 3")) << "the three pixels of efficiency_loss 0.6: " << j;
    EXPECT_NE(std::string::npos, j.find("\"group\": \"line/efficiency_loss/row_full/0.46\", \"items\": 1, \"detected\": 1")) << "the promoted line is a defect and was found: " << j;
    EXPECT_NE(std::string::npos, j.find("\"outside_truth\": 0,")) << "a flag on an undetermined item is neither a hit nor a false alarm: " << j;
    EXPECT_NE(std::string::npos, j.find("\"group\": \"line/gain_loss/row_full/0.3\"")) << "a response defect stays a defect whatever the noise axis says: " << j;
    EXPECT_NE(std::string::npos, console.find("undetermined items (3;")) << console;
}

// ---------------------------------------------------------------- scoring
TEST_F(BpmEvalTest, ScoreSeparatesAPerfectMapFromAnEmptyOne) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    const fs::path perfect = mapOf(t, "perfect.u8", [](const Item& it) { return it.cls != "control"; });
    const fs::path empty = mapOf(t, "empty.u8", [](const Item&) { return false; });
    ASSERT_EQ(0, score(t, perfect, " --clean-map \"" + empty.string() + "\" --half-a \"" + perfect.string() + "\" --half-b \"" + perfect.string() + "\" --out \"" + (root / "m1.json").string() + "\"", root / "s1.txt"));
    const std::string j1 = text(root / "m1.json");
    EXPECT_NE(std::string::npos, j1.find("\"control_false_detections\": 0")) << j1;
    EXPECT_NE(std::string::npos, j1.find("\"new_false_alarms\": 0")) << j1;
    EXPECT_NE(std::string::npos, j1.find("\"jaccard_all\": 1.000000")) << j1;
    const std::string s1 = text(root / "s1.txt");
    EXPECT_EQ(std::string::npos, s1.find("FALSE DETECTION")) << s1;
    // every detection group is fully detected
    for (size_t pos = j1.find("\"items\": "); pos != std::string::npos && pos < j1.find("\"flagged\""); pos = j1.find("\"items\": ", pos + 1)) {
        const int items = std::atoi(j1.c_str() + pos + 9);
        const size_t dpos = j1.find("\"detected\": ", pos);
        ASSERT_NE(std::string::npos, dpos);
        EXPECT_EQ(items, std::atoi(j1.c_str() + dpos + 12));
    }

    ASSERT_EQ(0, score(t, empty, " --out \"" + (root / "m2.json").string() + "\"", root / "s2.txt"));
    const std::string j2 = text(root / "m2.json");
    EXPECT_EQ(std::string::npos, j2.find("\"detected\": 1")) << "an empty map detects nothing";
    EXPECT_EQ(std::string::npos, j2.find("\"detected\": 2")) << j2;
    EXPECT_NE(std::string::npos, j2.find("\"control_false_detections\": 0"));
}

TEST_F(BpmEvalTest, MapV2TakesMarginsAndWritesTheAreaMask) {
    constexpr size_t W = 128, H = 128, N = W * H;   // QA-A-252i: small frames, the masks are what is checked
    makeFramesSized(W, H, 6, 8);
    auto run = [&](const std::string& margins, const fs::path& out, const fs::path& area) {
        std::string args = "map-v2 --out \"" + out.string() + "\" --area \"" + area.string() + "\" --width 128 --height 128 --min-bright 2 --skip-first-dark 0" +
                           (margins.empty() ? "" : " --margins " + margins);
        args += files("dark", darks) + files("bright", brights) + " > \"" + (root / "mv2.log").string() + "\" 2>&1";
        return runCmd(XPE_BPM_EVAL_EXE, args);
    };
    const fs::path out = root / "mv2.u8", area = root / "mv2_area.u8";
    ASSERT_EQ(0, run("6,2,1,1", out, area)) << text(root / "mv2.log");
    const std::vector<uint8_t> m = readBytes(out), a = readBytes(area);
    ASSERT_EQ(N, m.size());
    ASSERT_EQ(N, a.size());
    for (size_t y = 0; y < H; ++y)
        for (size_t x = 0; x < W; ++x) {
            const bool margin = x < 6 || y < 2 || x >= W - 1 || y >= H - 1;
            ASSERT_EQ(margin ? 0 : 1, a[y * W + x]) << "(" << x << "," << y << ")";
            if (margin) ASSERT_EQ(0, m[y * W + x]) << "outside the area the map is good: (" << x << "," << y << ")";
        }
    EXPECT_EQ(2, run("1,2,3", root / "bad1.u8", root / "bad1_area.u8")) << "three numbers are not a margin specification";
    EXPECT_EQ(2, run("6,2,1,1,5", root / "bad2.u8", root / "bad2_area.u8"));
    EXPECT_EQ(2, run("6,2,1,x", root / "bad3.u8", root / "bad3_area.u8"));
    EXPECT_EQ(0, run("", root / "none.u8", root / "none_area.u8")) << text(root / "mv2.log");
    const std::vector<uint8_t> n0 = readBytes(root / "none_area.u8");
    EXPECT_EQ(static_cast<ptrdiff_t>(N), std::count(n0.begin(), n0.end(), static_cast<uint8_t>(1))) << "no margins: the whole frame is the area";
}

TEST_F(BpmEvalTest, MapV2SkipsTheFirstDarkFrameOfTheRawListUnlessToldNot) {
    makeFramesSized(128, 128, 6, 8);   // QA-A-252i: only the number of frames used is checked
    auto used = [&](const std::string& skipArg) {
        std::string args = "map-v2 --out \"" + (root / "sk.u8").string() + "\" --report \"" + (root / "sk.json").string() + "\" --width 128 --height 128 --min-bright 2 --min-dark 3" + skipArg;
        args += files("dark", darks) + files("bright", brights) + " > \"" + (root / "sk.log").string() + "\" 2>&1";
        EXPECT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, args)) << text(root / "sk.log");
        const std::string j = text(root / "sk.json");
        const size_t pos = j.find("\"dark_frames_used\": ");
        return pos == std::string::npos ? -1 : std::atoi(j.c_str() + pos + 20);
    };
    EXPECT_EQ(5, used("")) << "six raw frames, no argument: the first is skipped";
    EXPECT_EQ(5, used(" --skip-first-dark 1"));
    EXPECT_EQ(6, used(" --skip-first-dark 0")) << "an explicit 0 says the list was trimmed already";
}

TEST_F(BpmEvalTest, MergeTakesTheUnionOfMapsAndBlobsCountsTheGroupsWithTheirSize) {
    std::vector<uint8_t> a(kN, 0), b(kN, 0);
    for (size_t x = 100; x < 104; ++x) a[200 * kW + x] = 1;   // a row of four in A
    b[200 * kW + 103] = 2;                                    // overlaps the last pixel of A: A's value stays
    b[500 * kW + 500] = 3;                                    // a single pixel only in B
    writeBytes(root / "ma.u8", a.data(), a.size());
    writeBytes(root / "mb.u8", b.data(), b.size());
    const int rc = runCmd(XPE_BPM_EVAL_EXE, "merge --in \"" + (root / "ma.u8").string() + "\" --in \"" + (root / "mb.u8").string() + "\" --out \"" + (root / "mc.u8").string() + "\" > \"" + (root / "mg.log").string() + "\" 2>&1");
    ASSERT_EQ(0, rc) << text(root / "mg.log");
    const std::vector<uint8_t> c = readBytes(root / "mc.u8");
    ASSERT_EQ(kN, c.size());
    EXPECT_EQ(1, c[200 * kW + 103]) << "the first non-zero value wins";
    EXPECT_EQ(3, c[500 * kW + 500]);
    EXPECT_EQ(0, c[10]);
    EXPECT_EQ(2, runCmd(XPE_BPM_EVAL_EXE, "merge --in \"" + (root / "ma.u8").string() + "\" --out \"" + (root / "x.u8").string() + "\" > \"" + (root / "mg2.log").string() + "\" 2>&1")) << "one input is not a merge";
    // blobs: the row of four and the single pixel
    const int rb = runCmd(XPE_BPM_EVAL_EXE, "blobs --map \"" + (root / "mc.u8").string() + "\" --width 1024 --height 1024 --out \"" + (root / "blobs.tsv").string() + "\" > \"" + (root / "bl.log").string() + "\" 2>&1");
    ASSERT_EQ(0, rb) << text(root / "bl.log");
    EXPECT_NE(std::string::npos, text(root / "bl.log").find("blobs: 2 (single pixels 1, 2-3 pixels 0, 4-15 pixels 1, 16 and more 0)")) << text(root / "bl.log");
    EXPECT_NE(std::string::npos, text(root / "blobs.tsv").find("100\t200\t103\t200\t4")) << text(root / "blobs.tsv");
}

TEST_F(BpmEvalTest, LinesCommandWritesTheMaskTheTableAndTheReportAndChecksItsArguments) {
    makeFrames(6, 8, 0);
    auto run = [&](const std::string& extra, const fs::path& out) {
        std::string args = "lines --out \"" + out.string() + "\" --tsv \"" + (root / "ln.tsv").string() + "\" --report \"" + (root / "ln.json").string() + "\" --width 1024 --height 1024 --min-bright 2 --min-segment 32" + extra;
        args += files("dark", darks) + files("bright", brights) + " > \"" + (root / "ln.log").string() + "\" 2>&1";
        return runCmd(XPE_BPM_EVAL_EXE, args);
    };
    const int rc = run(" --margins 6,2,1,1", root / "ln.u8");
    EXPECT_TRUE(rc == 0 || rc == 20) << "0, or 20 when three adjacent lines are found: " << rc << " " << text(root / "ln.log");
    EXPECT_EQ(kN, readBytes(root / "ln.u8").size());
    const std::string j = text(root / "ln.json");
    EXPECT_NE(std::string::npos, j.find("\"panel_reject\": ")) << j;
    EXPECT_NE(std::string::npos, j.find("\"pixels\": 1")) << "the imaging area: " << j;
    EXPECT_NE(std::string::npos, text(root / "ln.tsv").find("orientation\tindex\twidth")) << text(root / "ln.tsv");
    EXPECT_EQ(2, run(" --margins 6,2,1", root / "bad.u8")) << "three numbers are not a margin specification";
    EXPECT_EQ(rc == 20 ? 20 : 0, run(" --margins 6,2,1,1 --tolerance-db 3", root / "ln2.u8")) << "the tolerance option is accepted";
    EXPECT_EQ(4, run(" --tolerance-db 30", root / "bad2.u8")) << "a tolerance beyond 20 dB is refused by the library";
}

TEST_F(BpmEvalTest, ScoreWithAnAreaMaskIgnoresFlagsOutsideItAndKeepsCodeFourADefect) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    // flags in six columns and two rows at the edges; the area mask says those pixels are outside the imaging area
    std::vector<uint8_t> m(kN, 0), area(kN, 1);
    for (size_t y = 0; y < kH; ++y)
        for (size_t x = 0; x < kW; ++x)
            if (x < 6 || y < 2) {
                m[y * kW + x] = 4;   // 4 is the noisy defect code of the first generation: a plain map keeps that meaning
                area[y * kW + x] = 0;
            }
    const fs::path flagged = root / "oa.u8", mask = root / "oa_area.u8";
    writeBytes(flagged, m.data(), m.size());
    writeBytes(mask, area.data(), area.size());
    const fs::path empty = mapOf(t, "empty_oa.u8", [](const Item&) { return false; });
    const std::string common = " --clean-map \"" + flagged.string() + "\"";   // the clean map holds the same flags: they must not count there either
    ASSERT_EQ(0, score(t, flagged, common + " --area \"" + mask.string() + "\" --out \"" + (root / "oa.json").string() + "\"", root / "oa.txt"));
    const std::string j = text(root / "oa.json");
    EXPECT_NE(std::string::npos, j.find("\"flagged\": 0,")) << "with the area mask the flags outside it are not counted: " << j;
    EXPECT_NE(std::string::npos, j.find("\"clean_flagged\": 0,")) << "nor in the clean map: " << j;
    EXPECT_NE(std::string::npos, j.find("\"new_false_alarms\": 0,")) << j;
    // the control: without the mask the same map is full of defects (code 4 counts as a defect)
    ASSERT_EQ(0, score(t, flagged, common + " --out \"" + (root / "oa2.json").string() + "\"", root / "oa2.txt"));
    EXPECT_EQ(std::string::npos, text(root / "oa2.json").find("\"flagged\": 0,"));
}

TEST_F(BpmEvalTest, ScoreCountsControlHitsFalseAlarmsAndHalfAgreement) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    // a map that flags only the control bands plus one stray pixel far from every item
    std::vector<uint8_t> ids8 = readBytes(t / "truth_ids.u16");
    std::vector<uint16_t> ids(kN);
    std::memcpy(ids.data(), ids8.data(), ids8.size());
    size_t stray = kN;
    for (size_t i = kN / 2; i < kN && stray == kN; ++i) {
        const int x = static_cast<int>(i % kW), y = static_cast<int>(i / kW);
        if (x < 100 || x > 900 || y < 100 || y > 900) continue;
        bool free = true;
        for (int dy = -4; dy <= 4 && free; ++dy)
            for (int dx = -4; dx <= 4 && free; ++dx) free = ids[static_cast<size_t>(y + dy) * kW + static_cast<size_t>(x + dx)] == 0;
        if (free) stray = i;
    }
    ASSERT_NE(kN, stray);
    fs::path ctrl = mapOf(t, "ctrl.u8", [](const Item& it) { return it.cls == "control"; });
    std::vector<uint8_t> m = readBytes(ctrl);
    m[stray] = 1;
    {   // a pixel two places beside a planted pixel is inside the widened truth: not a false alarm
        const std::vector<Item> items = parseTruth(t / "truth.tsv");
        const Item* px = nullptr;
        for (const Item& it : items) if (it.cls == "pixel") { px = &it; break; }
        ASSERT_NE(nullptr, px);
        const size_t beside = static_cast<size_t>(px->rects[0].y) * kW + static_cast<size_t>(px->rects[0].x + 2);
        ASSERT_EQ(0, ids[beside]);
        m[beside] = 1;
    }
    writeBytes(ctrl, m.data(), m.size());
    // expected new false alarms, counted here from the truth (QA-A-251b: the control bands are not part of the zone left out): flagged pixels outside the true
    // defects widened by 2; those that lie on a control band by themselves are the "in controls" share
    std::vector<uint8_t> nearReal(kN, 0);
    for (const Item& it : parseTruth(t / "truth.tsv")) {
        if (it.cls == "control") continue;
        for (const Rect& q : it.rects)
            for (int y = std::max(0, q.y - 2); y < std::min(static_cast<int>(kH), q.y + q.h + 2); ++y)
                for (int x = std::max(0, q.x - 2); x < std::min(static_cast<int>(kW), q.x + q.w + 2); ++x) nearReal[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = 1;
    }
    size_t expectNew = 0, expectInControls = 0;
    for (size_t i = 0; i < kN; ++i) {
        if (!m[i] || nearReal[i]) continue;
        ++expectNew;
        expectInControls += (ids[i] != 0 && ids[i] != 65535);
    }
    ASSERT_GT(expectInControls, 1000u) << "the control bands are flagged: thousands of pixels";
    const fs::path empty = mapOf(t, "empty.u8", [](const Item&) { return false; });
    ASSERT_EQ(0, score(t, ctrl, " --clean-map \"" + empty.string() + "\" --half-a \"" + ctrl.string() + "\" --half-b \"" + empty.string() + "\" --out \"" + (root / "m.json").string() + "\"", root / "s.txt"));
    const std::string j = text(root / "m.json");
    int nControl = 0;
    for (const Item& it : parseTruth(t / "truth.tsv")) nControl += it.cls == "control" ? 1 : 0;
    EXPECT_EQ(95, nControl);
    EXPECT_NE(std::string::npos, j.find("\"control_items\": 95")) << j;
    EXPECT_NE(std::string::npos, j.find("\"control_false_detections\": 95")) << j;
    EXPECT_NE(std::string::npos, j.find("\"control_groups\"")) << "the pixel and block controls are summed per kind and level";
    EXPECT_NE(std::string::npos, j.find("\"group\": \"control/efficiency_loss/pixel/0.46\", \"items\": 3, \"false_detections\": 3")) << j;
    EXPECT_NE(std::string::npos, j.find("\"new_false_alarms\": " + std::to_string(expectNew) + ",")) << "the flagged control bands and the stray pixel, not the pixel beside a true defect: " << j;
    EXPECT_NE(std::string::npos, j.find("\"new_false_alarms_in_controls\": " + std::to_string(expectInControls) + ",")) << j;
    EXPECT_NE(std::string::npos, j.find("\"jaccard_all\": 0.000000")) << "a map against an empty map agrees on nothing: " << j;
}

TEST_F(BpmEvalTest, ScoreCallsALineDetectedWhenMostOfItsOwnPixelsAreFlagged) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    const std::vector<Item> items = parseTruth(t / "truth.tsv");
    // 60 % of one full dead line is flagged, 40 % of another: the first is detected, the second is not (the bound is 50 % of the pixels the line owns alone,
    // so the margins are wide enough that the few pixels shared with crossing lines cannot change the verdict)
    const Item *rowFull = nullptr, *colFull = nullptr;
    for (const Item& it : items)
        if (it.cls == "line" && it.kind == "dead" && it.level == 1.0) {
            if (it.shape == "row_full") rowFull = &it;
            if (it.shape == "col_full") colFull = &it;
        }
    ASSERT_TRUE(rowFull && colFull);
    std::vector<uint8_t> m(kN, 0);
    const Rect r = rowFull->rects[0], c = colFull->rects[0];
    for (int x = 0; x < static_cast<int>(kW) * 60 / 100; ++x) m[static_cast<size_t>(r.y) * kW + static_cast<size_t>(x)] = 1;   // 60 % of the row line
    for (int y = 0; y < static_cast<int>(kH) * 40 / 100; ++y) m[static_cast<size_t>(y) * kW + static_cast<size_t>(c.x)] = 1;   // 40 % of the column line
    const fs::path mp = root / "half.u8";
    writeBytes(mp, m.data(), m.size());
    ASSERT_EQ(0, score(t, mp, " --out \"" + (root / "m.json").string() + "\"", root / "s.txt"));
    const std::string j = text(root / "m.json");
    auto detectedOf = [&](const std::string& group) {
        const size_t p = j.find("\"group\": \"" + group + "\"");
        if (p == std::string::npos) return -1;
        const size_t d = j.find("\"detected\": ", p);
        return std::atoi(j.c_str() + d + 12);
    };
    EXPECT_EQ(1, detectedOf("line/dead/row_full/1")) << j;
    EXPECT_EQ(0, detectedOf("line/dead/col_full/1")) << j;
}

// ---------------------------------------------------------------- the whole chain on a small panel
TEST_F(BpmEvalTest, CorrectRunsOnTwoSetsAndReportsTheLineIndex) {
    makeFrames(7, 12, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    auto set = [&](const fs::path& framesDir, const fs::path& out, const char* tag) {
        // QA-A-252i: --bpm-v2 (about 8 s on 1024 x 1024 frames; the first-generation map of the default takes about 40 s, three times in this test)
        std::string args = "--out \"" + out.string() + "\" --width 1024 --height 1024 --expiry-ms 4102444800000 --bpm-v2";
        for (const auto& f : darks) args += " --dark \"" + (framesDir / fs::path(f).filename()).string() + "\"";
        for (size_t k = 0; k < 10; ++k) args += " --bright \"" + (framesDir / fs::path(brights[k]).filename()).string() + "\"";
        args += " > \"" + (root / (std::string(tag) + ".log")).string() + "\" 2>&1";
        return runCmd(XPE_FIXTURE_GEN_EXE, args);
    };
    ASSERT_EQ(0, set(root / "in", root / "clean_set", "cs"));
    ASSERT_EQ(0, set(t / "frames", root / "inj_set", "is"));
    const std::string cmd = "correct --truth \"" + t.string() + "\" --clean-set \"" + (root / "clean_set").string() + "\" --inj-set \"" + (root / "inj_set").string() + "\" --clean-eval \"" +
                            evals[0] + "\" --inj-eval \"" + (t / "frames" / fs::path(evals[0]).filename()).string() + "\" --out \"" + (root / "c.json").string() + "\" > \"" +
                            (root / "c.txt").string() + "\" 2>&1";
    ASSERT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, cmd)) << text(root / "c.txt");
    const std::string out = text(root / "c.txt"), j = text(root / "c.json");
    EXPECT_NE(std::string::npos, out.find("correction error")) << out;
    EXPECT_NE(std::string::npos, out.find("line index D_n")) << out;
    EXPECT_NE(std::string::npos, j.find("\"parts_over_3_sigma\"")) << j;
    EXPECT_NE(std::string::npos, j.find("pixel/dead/pixel/1")) << "the correction error is reported per kind and level: " << j;

    // the line index has the right sign and size: a fixed band of 100 pixel values is invisible (defect correction off) when the offset map came from the planted
    // frames (it absorbed the band) and stands out when the map came from clean frames; the clean frame itself shows nothing. With the defect correction on, the
    // planted-frames map flags the absorbed band as defective and the interpolation changes it: that residue is what the corrected column of the table measures
    auto ratioOf = [](const std::string& json, const std::string& group, const char* key) {
        const size_t g = json.find("\"group\": \"" + group + "\"", json.find("\"line_index\""));
        if (g == std::string::npos) return std::nan("");
        const size_t k = json.find(std::string("\"") + key + "\": ", g);
        return std::atof(json.c_str() + k + std::strlen(key) + 4);
    };
    EXPECT_LT(std::fabs(ratioOf(j, "control/fixed_band/row_full/100", "ratio_no_fix")), 3.0) << j;
    const std::string cmd2 = "correct --truth \"" + t.string() + "\" --clean-set \"" + (root / "clean_set").string() + "\" --inj-set \"" + (root / "clean_set").string() + "\" --clean-eval \"" +
                             evals[0] + "\" --inj-eval \"" + (t / "frames" / fs::path(evals[0]).filename()).string() + "\" --out \"" + (root / "c2.json").string() + "\" > \"" +
                             (root / "c2.txt").string() + "\" 2>&1";
    ASSERT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, cmd2)) << text(root / "c2.txt");
    const std::string j2 = text(root / "c2.json");
    // QA-A-252f: 4.0, not 5.0. The ratio depends on where the injection happens to put the band (the local noise it is divided by); with the six response levels the
    // placement changed and it came out 4.81. The point of the test is the separation from the 3.0 of the known band, which 4.0 keeps.
    EXPECT_GT(ratioOf(j2, "control/fixed_band/row_full/100", "ratio_no_fix"), 4.0) << "a band the offset map does not know stands out (standard-deviation denominator, QA-A-251b), with the right sign: " << j2;
    EXPECT_LT(std::fabs(ratioOf(j2, "control/fixed_band/row_full/100", "ratio_clean")), 3.0) << "the clean frame shows no band: " << j2;

    // no nan or inf ever reaches the JSON
    auto lower = [](std::string x) {
        for (char& ch : x) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return x;
    };
    for (const std::string* js : {&j, &j2}) {
        EXPECT_EQ(std::string::npos, lower(*js).find("nan")) << *js;
        EXPECT_EQ(std::string::npos, lower(*js).find("inf")) << *js;
    }

    // Codex #204 finding 1 end to end: a clean output without any noise (a set made of constant frames, a constant clean frame) leaves the local sigma at zero.
    // The groups must come out "uncomputable" with their invalid samples counted, not as a median of 0, and the line index of the clean output likewise.
    fs::create_directories(root / "constant");
    std::vector<std::string> cd, cb;
    for (int k = 0; k < 6; ++k) {
        std::vector<uint16_t> f(kN, static_cast<uint16_t>(3000 + k));   // constant, but each frame its own value (identical frames are refused)
        const fs::path pth = root / "constant" / ("d" + std::to_string(k) + ".raw");
        writeBytes(pth, f.data(), f.size() * 2);
        cd.push_back(pth.string());
    }
    for (int k = 0; k < 10; ++k) {
        std::vector<uint16_t> f(kN, static_cast<uint16_t>(9000 + k));
        const fs::path pth = root / "constant" / ("b" + std::to_string(k) + ".raw");
        writeBytes(pth, f.data(), f.size() * 2);
        cb.push_back(pth.string());
    }
    std::string cargs = "--out \"" + (root / "const_set").string() + "\" --width 1024 --height 1024 --expiry-ms 4102444800000 --bpm-v2";
    for (const auto& d : cd) cargs += " --dark \"" + d + "\"";
    for (const auto& b : cb) cargs += " --bright \"" + b + "\"";
    cargs += " > \"" + (root / "cs.log").string() + "\" 2>&1";
    ASSERT_EQ(0, runCmd(XPE_FIXTURE_GEN_EXE, cargs)) << text(root / "cs.log");
    std::vector<uint16_t> flat(kN, 5000);
    writeBytes(root / "constant" / "clean_eval.raw", flat.data(), flat.size() * 2);
    const std::string cmd3 = "correct --truth \"" + t.string() + "\" --clean-set \"" + (root / "const_set").string() + "\" --inj-set \"" + (root / "inj_set").string() + "\" --clean-eval \"" +
                             (root / "constant" / "clean_eval.raw").string() + "\" --inj-eval \"" + (t / "frames" / fs::path(evals[0]).filename()).string() + "\" --out \"" +
                             (root / "c3.json").string() + "\" > \"" + (root / "c3.txt").string() + "\" 2>&1";
    ASSERT_EQ(0, runCmd(XPE_BPM_EVAL_EXE, cmd3)) << text(root / "c3.txt");
    const std::string j3 = text(root / "c3.json");
    EXPECT_NE(std::string::npos, j3.find("\"median_z\": \"uncomputable\"")) << j3.substr(0, 1500);
    EXPECT_NE(std::string::npos, j3.find("\"valid\": 0,")) << "no valid sample in the groups";
    EXPECT_NE(std::string::npos, j3.find("\"zero_sigma_error\": ")) << "the invalid samples are counted with their reason";
    EXPECT_EQ(std::string::npos, j3.find("\"median_z\": 0")) << "never a median of 0 for a group that cannot be computed";
    EXPECT_NE(std::string::npos, j3.find("\"ratio_clean\": \"uncomputable\"")) << "the line index of a noiseless clean output cannot be computed";
    EXPECT_EQ(std::string::npos, j3.find("\"ratio_clean\": 0")) << "and is not 0";
    EXPECT_EQ(std::string::npos, lower(j3).find("nan"));
    EXPECT_EQ(std::string::npos, lower(j3).find("inf"));
    const size_t pu = j3.find("\"parts_uncomputable\": {\"corrected\": ");
    ASSERT_NE(std::string::npos, pu);
    EXPECT_NE(std::string::npos, j3.find("\"clean\": ", pu));
    EXPECT_GT(std::atoi(j3.c_str() + j3.find("\"clean\": ", pu) + 9), 0) << "uncomputable parts are counted, not dropped: " << j3.substr(pu, 200);
}


// ---------------------------------------------------------------- QA-A-251b: the numeric parts of the tool, called directly (Codex #204)
// Definitions: .moai/reports/lane-pre/QA-A-251b/evidence/30_method_fixed_before_the_fix.md section 1.
TEST(BpmEvalStatsTest, MedianAndPercentileFollowTheDefinition) {
    std::vector<double> v;
    for (int i = 100; i >= 0; --i) v.push_back(i);   // 101 values, unsorted
    EXPECT_EQ(50.0, xpe_bpm_eval::medianOf(v));
    EXPECT_EQ(95.0, xpe_bpm_eval::percentileOf(v, 0.95));
    std::vector<double> ten;
    for (int i = 9; i >= 0; --i) ten.push_back(i);
    EXPECT_EQ(5.0, xpe_bpm_eval::medianOf(ten)) << "v[floor(m / 2)]";
    EXPECT_EQ(9.0, xpe_bpm_eval::percentileOf(ten, 0.95)) << "floor(0.95 * 9 + 0.5) = 9: the +0.5 matters";
    EXPECT_EQ(0.0, xpe_bpm_eval::percentileOf(ten, 0.0));
    EXPECT_EQ(9.0, xpe_bpm_eval::percentileOf(ten, 1.0));
    EXPECT_EQ(7.0, xpe_bpm_eval::medianOf({7.0}));
    EXPECT_EQ(7.0, xpe_bpm_eval::percentileOf({7.0}, 0.95));
}

TEST(BpmEvalStatsTest, ASampleThatCannotBeComputedIsInvalidWithItsReasonAndNeverZero) {
    using xpe_bpm_eval::ZStatus;
    double z = -1.0;
    EXPECT_EQ(ZStatus::Valid, xpe_bpm_eval::classifyZ(120.0, 100.0, 4.0, &z));
    EXPECT_DOUBLE_EQ(5.0, z);
    EXPECT_EQ(ZStatus::ZeroSigmaError, xpe_bpm_eval::classifyZ(120.0, 100.0, 0.0, &z)) << "a real error against a noise of zero: x / 0";
    EXPECT_EQ(ZStatus::ZeroSigmaEqual, xpe_bpm_eval::classifyZ(100.0, 100.0, 0.0, &z)) << "0 / 0";
    EXPECT_EQ(ZStatus::NonFinite, xpe_bpm_eval::classifyZ(std::nan(""), 100.0, 4.0, &z));
    EXPECT_EQ(ZStatus::NonFinite, xpe_bpm_eval::classifyZ(120.0, 100.0, std::nan(""), &z));
    EXPECT_EQ(ZStatus::NonFinite, xpe_bpm_eval::classifyZ(HUGE_VAL, 100.0, 4.0, &z));
    EXPECT_DOUBLE_EQ(5.0, z) << "an invalid sample leaves the output alone";
}

TEST(BpmEvalStatsTest, AGroupWithoutAValidSampleIsUncomputableNotZero) {
    using xpe_bpm_eval::ZSide;
    using xpe_bpm_eval::ZStatus;
    ZSide corrected, uncorrected;
    for (int i = 0; i <= 100; ++i) {
        corrected.add(ZStatus::Valid, i);
        uncorrected.add(ZStatus::ZeroSigmaError, 0.0);   // every sample of the uncorrected side is invalid
    }
    corrected.add(ZStatus::NonFinite, 0.0);   // an invalid corrected sample does not remove the valid ones, nor touch the other side
    ASSERT_TRUE(corrected.computable());
    EXPECT_EQ(101u, corrected.valid());
    EXPECT_EQ(1u, corrected.invalid());
    EXPECT_EQ(1u, corrected.nonFinite);
    EXPECT_EQ(50.0, corrected.median());
    EXPECT_EQ(95.0, corrected.p95());
    EXPECT_FALSE(uncorrected.computable());
    EXPECT_EQ(0u, uncorrected.valid());
    EXPECT_EQ(101u, uncorrected.invalid());
    EXPECT_EQ(101u, uncorrected.zeroSigmaError);
    EXPECT_EQ(0u, uncorrected.zeroSigmaEqual);
}

namespace {

// A row-type (or column-type) test image: the line before the item carries a * (-1)^p, the item's lines are the constant c, the line after is 0, everything else 0.
std::vector<float> lineImage(bool rowType, uint32_t w, uint32_t h, int first, int k, int p0, int len, double a, double c) {
    std::vector<float> img(static_cast<size_t>(w) * h, 0.0f);
    auto set = [&](int line, int pos, double v) {
        if (rowType) img[static_cast<size_t>(line) * w + static_cast<size_t>(pos)] = static_cast<float>(v);
        else img[static_cast<size_t>(pos) * w + static_cast<size_t>(line)] = static_cast<float>(v);
    };
    for (int p = 0; p < (rowType ? static_cast<int>(w) : static_cast<int>(h)); ++p) set(first - 1, p, a * ((p % 2) ? -1.0 : 1.0));
    for (int t = 0; t < k; ++t)
        for (int p = p0; p < p0 + len; ++p) set(first + t, p, c);
    return img;
}

// The expected ratio from the definition, written out independently of the header: residuals of the alternating line are +-2a (m = len - 2 of them, m even,
// mean 0), their sample standard deviation is 2a sqrt(m / (m - 1)).
double expectedRatio(int k, int len, double a, double c, double* sigmaPix) {
    const double m = len - 2;
    const double sd = 2.0 * a * std::sqrt(m / (m - 1.0));
    *sigmaPix = sd / std::sqrt(1.5);
    const double pred = *sigmaPix * std::sqrt(1.0 / (k * static_cast<double>(len)) + 1.0 / (2.0 * len));
    return c / pred;
}

}  // namespace

TEST(BpmEvalStatsTest, LineIndexHasTheExactRatioForOneAndTwoLinesRowsAndColumns) {
    const double a = 3.0, c = 7.0;
    struct Case { bool row; int k; int len; };
    for (const Case& cs : {Case{true, 1, 200}, Case{true, 2, 200}, Case{false, 1, 200}, Case{false, 2, 200}, Case{true, 1, 100}}) {
        const uint32_t w = cs.row ? 300u : 40u, h = cs.row ? 40u : 300u;
        const std::vector<float> img = lineImage(cs.row, w, h, 10, cs.k, 10, cs.len, a, c);
        const xpe_bpm_eval::Rect q = cs.row ? xpe_bpm_eval::Rect{10, 10, cs.len, cs.k} : xpe_bpm_eval::Rect{10, 10, cs.k, cs.len};
        const xpe_bpm_eval::LineIndex li = xpe_bpm_eval::lineIndex(img.data(), w, h, q);
        ASSERT_FALSE(li.skipped);
        ASSERT_TRUE(li.computable);
        double sigmaPix = 0;
        const double want = expectedRatio(cs.k, cs.len, a, c, &sigmaPix);
        EXPECT_NEAR(c, li.dn, 1e-9) << (cs.row ? "row " : "column ") << cs.k << " line(s)";
        EXPECT_NEAR(sigmaPix, li.sigmaPix, 1e-9 * sigmaPix);
        EXPECT_NEAR(want, li.ratio, 1e-9 * want) << (cs.row ? "row " : "column ") << cs.k << " line(s), length " << cs.len;
        EXPECT_NE(li.sigmaPix, li.sigmaPixMad) << "the standard deviation decides, not the MAD";
    }
}

TEST(BpmEvalStatsTest, TheMadBasedValueIsAuxiliaryAndMayBeUncomputableWhileTheDefinedOneIsNot) {
    // the line before the item is zero except a spike: three residuals are not zero, the MAD is zero, the standard deviation is not
    std::vector<float> img = lineImage(true, 300, 40, 10, 1, 10, 200, 0.0, 5.0);
    img[static_cast<size_t>(9) * 300 + 100] = 1000.0f;
    const xpe_bpm_eval::LineIndex li = xpe_bpm_eval::lineIndex(img.data(), 300, 40, xpe_bpm_eval::Rect{10, 10, 200, 1});
    ASSERT_FALSE(li.skipped);
    EXPECT_TRUE(li.computable) << "the defined (standard deviation) denominator is finite and positive";
    EXPECT_GT(li.sigmaPred, 0.0);
    EXPECT_FALSE(li.madComputable) << "the MAD of this line is zero: the auxiliary ratio cannot be computed";
}

TEST(BpmEvalStatsTest, ALineIndexWithoutADenominatorIsUncomputableAndEdgesAreSkipped) {
    // a constant line before the item: sigma = 0, and D = 5 is a real difference against no noise at all (x / 0), not a number
    const std::vector<float> img = lineImage(true, 300, 40, 10, 1, 10, 200, 0.0, 5.0);
    const xpe_bpm_eval::LineIndex li = xpe_bpm_eval::lineIndex(img.data(), 300, 40, xpe_bpm_eval::Rect{10, 10, 200, 1});
    EXPECT_FALSE(li.skipped);
    EXPECT_FALSE(li.computable);
    EXPECT_FALSE(li.madComputable);
    // and 0 / 0: the same, D = 0
    const std::vector<float> flat = lineImage(true, 300, 40, 10, 1, 10, 200, 0.0, 0.0);
    EXPECT_FALSE(xpe_bpm_eval::lineIndex(flat.data(), 300, 40, xpe_bpm_eval::Rect{10, 10, 200, 1}).computable);
    // a value that is not finite
    std::vector<float> bad = lineImage(true, 300, 40, 10, 1, 10, 200, 3.0, 5.0);
    bad[static_cast<size_t>(10) * 300 + 50] = std::nanf("");
    EXPECT_FALSE(xpe_bpm_eval::lineIndex(bad.data(), 300, 40, xpe_bpm_eval::Rect{10, 10, 200, 1}).computable);
    // at the frame edge there is no line before the item
    EXPECT_TRUE(xpe_bpm_eval::lineIndex(img.data(), 300, 40, xpe_bpm_eval::Rect{10, 0, 200, 1}).skipped);
}

namespace {

std::vector<size_t> exclusivePixels(const fs::path& truth, int itemId) {
    const std::vector<uint8_t> raw = readBytes(truth / "truth_ids.u16");
    std::vector<uint16_t> ids(kN);
    std::memcpy(ids.data(), raw.data(), raw.size());
    std::vector<size_t> out;
    for (size_t i = 0; i < kN; ++i)
        if (ids[i] == itemId) out.push_back(i);
    return out;
}

}  // namespace

TEST_F(BpmEvalTest, ScoreCallsALineDetectedAtExactlyHalfOfItsOwnPixelsAndNotBelow) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    const std::vector<Item> items = parseTruth(t / "truth.tsv");
    const Item* pick = nullptr;
    std::vector<size_t> own;
    for (const Item& it : items) {
        if (it.cls != "line" || it.kind != "dead" || it.shape.find("partial") == std::string::npos) continue;
        own = exclusivePixels(t, it.id);
        if (own.size() >= 100 && own.size() % 2 == 0) { pick = &it; break; }
    }
    ASSERT_NE(nullptr, pick) << "no partial dead line with an even number of own pixels";
    const std::string group = pick->cls + "/" + pick->kind + "/" + pick->shape + "/" + (pick->level == 1.0 ? "1" : "0.5");
    auto detectedWith = [&](size_t flagged, const char* name) {
        std::vector<uint8_t> m(kN, 0);
        for (size_t i = 0; i < flagged; ++i) m[own[i]] = 1;
        const fs::path mp = root / name;
        writeBytes(mp, m.data(), m.size());
        EXPECT_EQ(0, score(t, mp, " --out \"" + (root / (std::string(name) + ".json")).string() + "\"", root / (std::string(name) + ".txt")));
        const std::string j = text(root / (std::string(name) + ".json"));
        const size_t p = j.find("\"group\": \"" + group + "\"");
        EXPECT_NE(std::string::npos, p) << group;
        return std::atoi(j.c_str() + j.find("\"detected\": ", p) + 12);
    };
    EXPECT_EQ(1, detectedWith(own.size() / 2, "exact")) << group << ": exactly half of " << own.size() << " is detected";
    EXPECT_EQ(0, detectedWith(own.size() / 2 - 1, "below")) << group << ": one pixel fewer is not";
}

TEST_F(BpmEvalTest, FlagsOnANormalControlBandAreNewFalseAlarmsEvenBelowHalf) {
    makeFrames(5, 3, 2);
    const fs::path t = root / "t";
    ASSERT_EQ(0, inject(t));
    const std::vector<Item> items = parseTruth(t / "truth.tsv");
    const Item* band = nullptr;
    for (const Item& it : items)
        if (it.cls == "control" && it.kind == "fixed_band" && it.shape == "row_full" && it.level == 100.0) band = &it;
    ASSERT_NE(nullptr, band);
    const std::vector<size_t> own = exclusivePixels(t, band->id);
    ASSERT_GT(own.size(), 500u);
    // pixels of the band at least 3 away from every true defect (the widened true defects are left out of the false alarms by definition)
    std::vector<uint8_t> nearReal(kN, 0);
    for (const Item& it : items) {
        if (it.cls == "control") continue;
        for (const Rect& q : it.rects)
            for (int y = std::max(0, q.y - 2); y < std::min(static_cast<int>(kH), q.y + q.h + 2); ++y)
                for (int x = std::max(0, q.x - 2); x < std::min(static_cast<int>(kW), q.x + q.w + 2); ++x) nearReal[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = 1;
    }
    std::vector<size_t> usable;
    for (size_t i : own) if (!nearReal[i]) usable.push_back(i);
    const size_t flagged = own.size() * 40 / 100;   // 40 %, under the half that makes a band a false detection
    ASSERT_GE(usable.size(), flagged);
    std::vector<uint8_t> m(kN, 0), none(kN, 0);
    for (size_t i = 0; i < flagged; ++i) m[usable[i]] = 1;
    writeBytes(root / "part.u8", m.data(), m.size());
    writeBytes(root / "none.u8", none.data(), none.size());
    ASSERT_EQ(0, score(t, root / "part.u8", " --clean-map \"" + (root / "none.u8").string() + "\" --out \"" + (root / "m.json").string() + "\"", root / "s.txt"));
    const std::string j = text(root / "m.json");
    EXPECT_NE(std::string::npos, j.find("\"control_false_detections\": 0")) << "40 % of a band is under the half that makes a false detection: " << j;
    EXPECT_NE(std::string::npos, j.find("\"new_false_alarms\": " + std::to_string(flagged) + ",")) << "but every flagged pixel of the band is a new false alarm: " << j;
    EXPECT_NE(std::string::npos, j.find("\"new_false_alarms_in_controls\": " + std::to_string(flagged) + ",")) << j;
}

TEST_F(BpmEvalTest, InjectRefusesTwoInputsWithTheSameFileName) {
    makeFrames(5, 3, 2);
    fs::create_directories(root / "other");
    fs::copy_file(darks[0], root / "other" / fs::path(darks[0]).filename());
    const fs::path out = root / "t";
    const int rc = runCmd(XPE_BPM_EVAL_EXE, "inject --out \"" + out.string() + "\"" + small() + files("dark", darks) + files("dark", {(root / "other" / fs::path(darks[0]).filename()).string()}) +
                                                files("bright", brights) + " > \"" + (root / "i.log").string() + "\" 2>&1");
    EXPECT_EQ(2, rc);
    EXPECT_FALSE(fs::exists(out / "truth.tsv"));
    EXPECT_NE(std::string::npos, text(root / "i.log").find("file name")) << text(root / "i.log");
}

}  // namespace
