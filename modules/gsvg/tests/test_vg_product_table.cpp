/**
 * @file test_vg_product_table.cpp
 * @brief #180 (QA-B-101): the product virtual-grid table and the [grid]
 *        loader extension (line density, thickness and kVp columns).
 *
 * data/vg_table_water_csi600_victre.csv is built by tools/make_vg_table.py
 * from tools/mcsim/tables. The expected numbers below are copied from
 * grid_water_victre.csv by hand, not read through the loader.
 */

#include "virtual_grid.h"
#include "xpe/gsvg/gsvg_api.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace vg = xpe_gsvg_detail;

namespace {

constexpr const char* kProduct = "data/vg_table_water_csi600_victre.csv";
constexpr const char* kSynthetic = "tests/data/virtual_grid_synthetic_table.csv";

const vg::ParamTable& Product() {
    static vg::ParamTable t;
    static const std::string err = vg::LoadParamTable(kProduct, t);
    EXPECT_EQ(err, "");
    return t;
}

std::string ProductConfig(const std::string& freq) {
    return std::string("{\"virtual_grid\": true, \"vg_table_path\": \"") + kProduct +
           "\", \"vg_kvp\": 80, \"vg_grid_ratio\": 10, \"vg_pixel_pitch_mm\": 0.139,"
           " \"vg_air_signal\": 60000, \"vg_iterations\": 3" + freq + "}";
}

}  // namespace

TEST(GsvgVgProductTable, LoadsWithEveryRatioOfReqGsvg016)
{
    const vg::ParamTable& t = Product();
    EXPECT_TRUE(t.gridHasFreq && t.gridHasThick && t.gridHasKvp);
    EXPECT_EQ(t.gridRows.size(), 96u);   // 4 ratios x 2 densities x 3 thicknesses x 4 kVp
    std::set<double> ratios, freqs;
    for (const auto& g : t.gridRows) { ratios.insert(g.ratio); freqs.insert(g.freq); }
    EXPECT_EQ(ratios, (std::set<double>{6, 8, 10, 12}));
    EXPECT_EQ(freqs, (std::set<double>{40, 60}));
    EXPECT_EQ(t.kThick, (std::vector<double>{5, 10, 15, 20, 25, 30}));
    EXPECT_EQ(t.kKvp, (std::vector<double>{60, 70, 80, 90, 100, 110, 120}));
    EXPECT_EQ(t.kernels.front().terms, 4);   // gauss4 rows are used
    EXPECT_EQ(t.wetKvp.size(), 7u);
    EXPECT_TRUE(t.capFromKernels);
}

TEST(GsvgVgProductTable, KeepsTheNotCalibratedMarks)
{
    std::ifstream f(kProduct, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    const std::string text = ss.str();
    EXPECT_NE(text.find("NOT CALIBRATED"), std::string::npos);
    // each source header's own mark
    EXPECT_NE(text.find("# XPE virtual-grid scatter kernels -- simulation-based, not calibrated"), std::string::npos);
    EXPECT_NE(text.find("# [wet] water-equivalent thickness model -- simulation-based, not calibrated"), std::string::npos);
    EXPECT_NE(text.find("# [grid] anti-scatter grid transmissions -- simulation-based, not calibrated"), std::string::npos);
}

TEST(GsvgVgProductTable, SelectsOneDesignAndInterpolatesKvp)
{
    const vg::ParamTable& t = Product();
    vg::GridAtKvp g;
    ASSERT_EQ(vg::SelectGrid(t, 10, 40, 80, g), "");
    EXPECT_EQ(g.thick, (std::vector<double>{10, 20, 30}));
    EXPECT_DOUBLE_EQ(g.tp[0], 0.765601);    // ratio 10, 40 /cm, 10 cm, 80 kVp
    EXPECT_DOUBLE_EQ(g.ts[0], 0.0774988);

    // 70 kVp: halfway between the 60 and 80 kVp rows
    vg::GridAtKvp h;
    ASSERT_EQ(vg::SelectGrid(t, 10, 40, 70, h), "");
    EXPECT_NEAR(h.tp[0], 0.5 * (0.761284 + 0.765601), 1e-12);
    EXPECT_NEAR(h.ts[0], 0.5 * (0.0652465 + 0.0774988), 1e-12);

    // the other density is a different design
    vg::GridAtKvp d60;
    ASSERT_EQ(vg::SelectGrid(t, 10, 60, 80, d60), "");
    EXPECT_DOUBLE_EQ(d60.ts[0], 0.107463);

    EXPECT_NE(vg::SelectGrid(t, 10, 0, 80, g), "");     // density required
    EXPECT_NE(vg::SelectGrid(t, 10, 50, 80, g), "");    // not listed
    EXPECT_NE(vg::SelectGrid(t, 7, 40, 80, g), "");     // ratio not listed
    EXPECT_NE(vg::SelectGrid(t, 10, 40, 130, g), "");   // kVp outside
    EXPECT_NE(vg::SelectGrid(t, 10, 40, 55, g), "");
}

TEST(GsvgVgProductTable, ResidualFollowsThickness)
{
    vg::GridAtKvp g;
    g.thick = {10, 20, 30};
    g.tp = {0.8, 0.7, 0.6};
    g.ts = {0.08, 0.14, 0.18};
    EXPECT_DOUBLE_EQ(g.ResidualAt(0), 0.1);          // below: first node
    EXPECT_DOUBLE_EQ(g.ResidualAt(10), 0.1);
    EXPECT_DOUBLE_EQ(g.ResidualAt(15), 0.11 / 0.75); // Ts and Tp blended, then divided
    EXPECT_DOUBLE_EQ(g.ResidualAt(30), 0.3);
    EXPECT_DOUBLE_EQ(g.ResidualAt(45), 0.3);         // above: last node

    const vg::ParamTable& t = Product();
    vg::GridAtKvp p;
    ASSERT_EQ(vg::SelectGrid(t, 10, 40, 80, p), "");
    EXPECT_LT(p.ResidualAt(10), p.ResidualAt(30));   // thicker object, harder scatter, more passes
}

TEST(GsvgVgProductTable, OldTableRefusesALineDensity)
{
    vg::ParamTable t;
    ASSERT_EQ(vg::LoadParamTable(kSynthetic, t), "");
    vg::GridAtKvp g;
    EXPECT_EQ(vg::SelectGrid(t, 10, 0, 80, g), "");   // control
    EXPECT_EQ(g.thick.size(), 1u);
    EXPECT_NE(vg::SelectGrid(t, 10, 40, 80, g), "");
}

TEST(GsvgVgProductTable, LoaderRejectsIncompleteAndDuplicateDesigns)
{
    const std::string head = "[kernels]\nthickness_cm,kvp,model,a1,s1,a2,s2\n"
                             "10,80,gauss2,0.3,1,0.1,5\n10,100,gauss2,0.3,1,0.1,5\n"
                             "[wet]\nkvp,w0,a,b\n80,0.27,0.004,0.06\n100,0.25,0.003,0.05\n[grid]\n";
    vg::ParamTable t;
    const std::string ok = head + "ratio,tp,ts,freq_per_cm,kvp\n10,0.7,0.1,40,80\n10,0.7,0.1,40,100\n"
                                  "10,0.7,0.1,60,80\n10,0.7,0.1,60,100\n";
    EXPECT_EQ(vg::ParseParamTable(ok, t), "");   // control
    const std::string missing = head + "ratio,tp,ts,freq_per_cm,kvp\n10,0.7,0.1,40,80\n10,0.7,0.1,40,100\n"
                                       "10,0.7,0.1,60,80\n";
    EXPECT_NE(vg::ParseParamTable(missing, t), "");
    const std::string dup = head + "ratio,tp,ts,freq_per_cm,kvp\n10,0.7,0.1,40,80\n10,0.7,0.1,40,80\n";
    EXPECT_NE(vg::ParseParamTable(dup, t), "");
    const std::string zeroFreq = head + "ratio,tp,ts,freq_per_cm\n10,0.7,0.1,0\n";
    EXPECT_NE(vg::ParseParamTable(zeroFreq, t), "");
}

TEST(GsvgVgProductTable, PublicApiRunsTheProductTable)
{
    std::vector<uint16_t> src(256 * 256, 20000), dst(src.size());
    XpeGsvgResult res{};
    res.structSize = sizeof(res);

    void* h = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&h, ProductConfig(", \"vg_grid_frequency_per_cm\": 40").c_str()), XPE_OK);
    EXPECT_EQ(xpe_gsvg_process_ex(h, src.data(), src.size(), dst.data(), dst.size(), 256, 256,
                                  nullptr, 0, nullptr, 0, &res), XPE_OK);
    EXPECT_EQ(res.virtualGridApplied, 1);
    EXPECT_NE(dst, src);
    xpe_gsvg_shutdown(h);

    // the density is required by this table: refused at process time, original kept
    ASSERT_EQ(xpe_gsvg_init(&h, ProductConfig("").c_str()), XPE_OK);
    EXPECT_EQ(xpe_gsvg_process_ex(h, src.data(), src.size(), dst.data(), dst.size(), 256, 256,
                                  nullptr, 0, nullptr, 0, &res), XPE_ERR_CONFIG_INVALID);
    EXPECT_EQ(res.reason, XPE_GSVG_REASON_VG_REFUSED);
    EXPECT_EQ(dst, src);
    xpe_gsvg_shutdown(h);

    // a non-positive density is a config error at init
    EXPECT_EQ(xpe_gsvg_init(&h, ProductConfig(", \"vg_grid_frequency_per_cm\": 0").c_str()),
              XPE_ERR_CONFIG_INVALID);
}
