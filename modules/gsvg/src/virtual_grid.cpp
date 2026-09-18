/**
 * @file virtual_grid.cpp
 * @brief Virtual grid implementation (#180, QA-B-91). See virtual_grid.h.
 */
#include "virtual_grid.h"
#include "parallel_rows.h"

// Thread-count request, stored in gsvg.cpp (#179, QA-B-105).
int XpeGsvgThreadRequest();

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace xpe_gsvg_detail {

namespace {

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> SplitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::string cell;
    std::istringstream is(line);
    while (std::getline(is, cell, ',')) out.push_back(Trim(cell));
    if (!line.empty() && line.back() == ',') out.emplace_back();
    return out;
}

bool ToNumber(const std::string& s, double& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0' && std::isfinite(v);
}

struct Section {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    int line = 0;
    int Col(const char* name) const {
        for (size_t i = 0; i < header.size(); ++i)
            if (header[i] == name) return static_cast<int>(i);
        return -1;
    }
};

std::string Where(const char* section, const char* what) {
    return std::string("[") + section + "] " + what;
}

// Index of v in a sorted axis, or -1.
int IndexOf(const std::vector<double>& axis, double v) {
    for (size_t i = 0; i < axis.size(); ++i)
        if (std::fabs(axis[i] - v) < 1e-9) return static_cast<int>(i);
    return -1;
}

std::vector<double> UniqueSorted(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end(),
                        [](double x, double y) { return std::fabs(x - y) < 1e-9; }),
            v.end());
    return v;
}

// Bracket v on an ascending axis: lo, hi, weight of hi. false if outside.
bool Bracket(const std::vector<double>& axis, double v, int& lo, int& hi, double& w) {
    if (axis.empty() || v < axis.front() - 1e-9 || v > axis.back() + 1e-9) return false;
    if (axis.size() == 1) { lo = hi = 0; w = 0; return true; }
    size_t i = 0;
    while (i + 2 < axis.size() && v > axis[i + 1]) ++i;
    lo = static_cast<int>(i);
    hi = static_cast<int>(i + 1);
    w = std::clamp((v - axis[i]) / (axis[i + 1] - axis[i]), 0.0, 1.0);
    return true;
}

// (index, weight) pairs for a bracket; one pair when lo == hi or w is 0/1.
std::vector<std::pair<int, double>> Pairs(int lo, int hi, double w) {
    std::vector<std::pair<int, double>> p;
    if (lo == hi || w <= 0) p.push_back({lo, 1.0});
    else if (w >= 1) p.push_back({hi, 1.0});
    else { p.push_back({lo, 1.0 - w}); p.push_back({hi, w}); }
    return p;
}

double MuAt(double t, double w0, double a, double b) { return w0 - a * t / (1.0 + b * t); }

}  // namespace

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------
std::string ParseParamTable(const std::string& text, ParamTable& out) {
    out = ParamTable{};
    std::map<std::string, Section> sections;
    std::string current;
    std::istringstream is(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(is, raw)) {
        ++lineNo;
        const std::string line = Trim(raw);
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            current = line.substr(1, line.size() - 2);
            if (sections.count(current)) return "section [" + current + "] appears twice";
            sections[current].line = lineNo;
            continue;
        }
        if (current.empty()) return "line " + std::to_string(lineNo) + ": data before any [section]";
        Section& s = sections[current];
        if (s.header.empty()) s.header = SplitCsv(line);
        else s.rows.push_back(SplitCsv(line));
    }

    for (const char* name : {"kernels", "wet", "grid"}) {
        if (!sections.count(name)) return std::string("missing section [") + name + "]";
        if (sections[name].rows.empty()) return std::string("section [") + name + "] has no rows";
    }

    auto cell = [](const Section&, const std::vector<std::string>& row, int col, double& v) {
        return col >= 0 && col < static_cast<int>(row.size()) && ToNumber(row[static_cast<size_t>(col)], v);
    };

    // --- kernels ---------------------------------------------------------
    {
        const Section& s = sections["kernels"];
        const int cT = s.Col("thickness_cm"), cK = s.Col("kvp"), cM = s.Col("model");
        if (cT < 0 || cK < 0 || cM < 0) return Where("kernels", "needs thickness_cm, kvp, model columns");
        bool any4 = false;
        for (const auto& r : s.rows)
            if (cM < static_cast<int>(r.size()) && r[static_cast<size_t>(cM)] == "gauss4") any4 = true;
        const std::string model = any4 ? "gauss4" : "gauss2";
        const int terms = any4 ? 4 : 2;
        int ca[4], cs[4];
        for (int i = 0; i < terms; ++i) {
            const std::string an = "a" + std::to_string(i + 1), sn = "s" + std::to_string(i + 1);
            ca[i] = s.Col(an.c_str());
            cs[i] = s.Col(sn.c_str());
            if (ca[i] < 0 || cs[i] < 0) return Where("kernels", "missing a/s columns for ") + model;
        }
        struct Row { double t, k; KernelNode n; };
        std::vector<Row> rows;
        for (const auto& r : s.rows) {
            if (cM >= static_cast<int>(r.size()) || r[static_cast<size_t>(cM)] != model) continue;
            Row row{};
            if (!cell(s, r, cT, row.t) || !cell(s, r, cK, row.k))
                return Where("kernels", "non-numeric thickness_cm or kvp");
            if (row.t <= 0 || row.k <= 0) return Where("kernels", "thickness_cm and kvp must be > 0");
            row.n.terms = terms;
            for (int i = 0; i < terms; ++i) {
                if (!cell(s, r, ca[i], row.n.a[i]) || !cell(s, r, cs[i], row.n.s[i]))
                    return Where("kernels", "non-numeric coefficient");
                if (row.n.a[i] < 0 || row.n.s[i] <= 0) return Where("kernels", "need a_i >= 0 and s_i > 0");
            }
            rows.push_back(row);
        }
        std::vector<double> ts, ks;
        for (const auto& r : rows) { ts.push_back(r.t); ks.push_back(r.k); }
        out.kThick = UniqueSorted(ts);
        out.kKvp = UniqueSorted(ks);
        out.kernels.assign(out.kThick.size() * out.kKvp.size(), KernelNode{});
        std::vector<int> seen(out.kernels.size(), 0);
        for (const auto& r : rows) {
            const size_t idx = static_cast<size_t>(IndexOf(out.kThick, r.t)) * out.kKvp.size()
                             + static_cast<size_t>(IndexOf(out.kKvp, r.k));
            if (seen[idx]++) return Where("kernels", "duplicate (thickness_cm, kvp) row");
            out.kernels[idx] = r.n;
        }
        for (int v : seen)
            if (!v) return Where("kernels", "thickness x kvp grid is incomplete for ") + model;
    }

    // --- wet ---------------------------------------------------------------
    {
        const Section& s = sections["wet"];
        const int cK = s.Col("kvp"), c0 = s.Col("w0"), cA = s.Col("a"), cB = s.Col("b");
        if (cK < 0 || c0 < 0 || cA < 0 || cB < 0) return Where("wet", "needs kvp, w0, a, b columns");
        std::vector<std::pair<double, std::array<double, 3>>> rows;
        for (const auto& r : s.rows) {
            double k, w0, a, b;
            if (!cell(s, r, cK, k) || !cell(s, r, c0, w0) || !cell(s, r, cA, a) || !cell(s, r, cB, b))
                return Where("wet", "non-numeric value");
            if (w0 <= 0 || b < 0) return Where("wet", "need w0 > 0 and b >= 0");
            rows.push_back({k, {w0, a, b}});
        }
        std::sort(rows.begin(), rows.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i && std::fabs(rows[i].first - rows[i - 1].first) < 1e-9) return Where("wet", "duplicate kvp");
            out.wetKvp.push_back(rows[i].first);
            out.wetW0.push_back(rows[i].second[0]);
            out.wetA.push_back(rows[i].second[1]);
            out.wetB.push_back(rows[i].second[2]);
        }
    }

    // --- grid --------------------------------------------------------------
    // QA-B-101: optional freq_per_cm, thickness_cm, kvp columns. Rows are kept
    // as read; each (ratio, freq) design must cover a full thickness x kVp
    // grid with no duplicates.
    {
        const Section& s = sections["grid"];
        const int cR = s.Col("ratio"), cP = s.Col("tp"), cS = s.Col("ts");
        const int cF = s.Col("freq_per_cm"), cT = s.Col("thickness_cm"), cK = s.Col("kvp");
        if (cR < 0 || cP < 0 || cS < 0) return Where("grid", "needs ratio, tp, ts columns");
        out.gridHasFreq = cF >= 0;
        out.gridHasThick = cT >= 0;
        out.gridHasKvp = cK >= 0;
        for (const auto& r : s.rows) {
            ParamTable::GridRow g{0, 0, 0, 0, 0, 0};
            if (!cell(s, r, cR, g.ratio) || !cell(s, r, cP, g.tp) || !cell(s, r, cS, g.ts))
                return Where("grid", "non-numeric value");
            if ((cF >= 0 && !cell(s, r, cF, g.freq)) || (cT >= 0 && !cell(s, r, cT, g.thick)) ||
                (cK >= 0 && !cell(s, r, cK, g.kvp)))
                return Where("grid", "non-numeric value");
            if (g.tp <= 0 || g.tp > 1 || g.ts < 0 || g.ts > 1) return Where("grid", "need 0 < tp <= 1 and 0 <= ts <= 1");
            if ((cF >= 0 && g.freq <= 0) || (cT >= 0 && g.thick <= 0) || (cK >= 0 && g.kvp <= 0))
                return Where("grid", "freq_per_cm, thickness_cm and kvp must be > 0");
            for (const auto& o : out.gridRows)
                if (o.ratio == g.ratio && o.freq == g.freq && o.thick == g.thick && o.kvp == g.kvp)
                    return Where("grid", out.gridHasThick || out.gridHasKvp || out.gridHasFreq
                                             ? "duplicate (ratio, freq_per_cm, thickness_cm, kvp) row"
                                             : "duplicate ratio");
            out.gridRows.push_back(g);
        }
        // Completeness per design.
        std::vector<std::pair<double, double>> designs;
        std::vector<double> ts, ks;
        for (const auto& g : out.gridRows) {
            if (std::find(designs.begin(), designs.end(), std::make_pair(g.ratio, g.freq)) == designs.end())
                designs.push_back({g.ratio, g.freq});
            ts.push_back(g.thick);
            ks.push_back(g.kvp);
        }
        const size_t need = UniqueSorted(ts).size() * UniqueSorted(ks).size();
        for (const auto& d : designs) {
            size_t n = 0;
            for (const auto& g : out.gridRows) n += (g.ratio == d.first && g.freq == d.second);
            if (n != need) return Where("grid", "thickness x kvp grid is incomplete for a (ratio, freq_per_cm) pair");
        }
    }

    // --- spr_cap (optional) --------------------------------------------------
    out.capFromKernels = !sections.count("spr_cap");
    if (!out.capFromKernels && sections["spr_cap"].rows.empty())
        return "section [spr_cap] has no rows";
    if (!out.capFromKernels) {
        const Section& s = sections["spr_cap"];
        const int cT = s.Col("thickness_cm"), cK = s.Col("kvp"), cC = s.Col("max_spr");
        if (cT < 0 || cK < 0 || cC < 0) return Where("spr_cap", "needs thickness_cm, kvp, max_spr columns");
        std::vector<std::array<double, 3>> rows;
        for (const auto& r : s.rows) {
            double t, k, c;
            if (!cell(s, r, cT, t) || !cell(s, r, cK, k) || !cell(s, r, cC, c))
                return Where("spr_cap", "non-numeric value");
            if (t <= 0 || c < 0) return Where("spr_cap", "need thickness_cm > 0 and max_spr >= 0");
            rows.push_back({t, k, c});
        }
        std::vector<double> ts, ks;
        for (const auto& r : rows) { ts.push_back(r[0]); ks.push_back(r[1]); }
        out.capThick = UniqueSorted(ts);
        out.capKvp = UniqueSorted(ks);
        out.capSpr.assign(out.capThick.size() * out.capKvp.size(), 0.0);
        std::vector<int> seen(out.capSpr.size(), 0);
        for (const auto& r : rows) {
            const size_t idx = static_cast<size_t>(IndexOf(out.capThick, r[0])) * out.capKvp.size()
                             + static_cast<size_t>(IndexOf(out.capKvp, r[1]));
            if (seen[idx]++) return Where("spr_cap", "duplicate (thickness_cm, kvp) row");
            out.capSpr[idx] = r[2];
        }
        for (int v : seen)
            if (!v) return Where("spr_cap", "thickness x kvp grid is incomplete");
    }

    // The slab curve must give one thickness per attenuation over the whole
    // thickness range the kernels cover: L(t) = mu(t)*t strictly increasing.
    const double tMax = out.kThick.back();   // the curve is used up to here
    for (size_t i = 0; i < out.wetKvp.size(); ++i) {
        double prev = 0.0;
        for (int j = 1; j <= 1000; ++j) {
            const double t = tMax * j / 1000.0;
            const double L = MuAt(t, out.wetW0[i], out.wetA[i], out.wetB[i]) * t;
            if (!(L > prev)) return Where("wet", "mu(t)*t is not increasing up to the kernel thickness range");
            prev = L;
        }
    }
    return {};
}

std::string LoadParamTable(const std::string& path, ParamTable& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "cannot open table file '" + path + "'";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ParseParamTable(ss.str(), out);
}

double GridAtKvp::ResidualAt(double t) const {
    if (thick.size() == 1 || t <= thick.front()) return ts.front() / tp.front();
    if (t >= thick.back()) return ts.back() / tp.back();
    size_t j = 1;
    while (thick[j] < t) ++j;
    const double w = (t - thick[j - 1]) / (thick[j] - thick[j - 1]);
    return ((1 - w) * ts[j - 1] + w * ts[j]) / ((1 - w) * tp[j - 1] + w * tp[j]);
}

std::string SelectGrid(const ParamTable& t, double ratio, double freqPerCm, double kvp, GridAtKvp& out) {
    out = GridAtKvp{};
    if (!t.gridHasFreq && freqPerCm != 0)
        return "vg_grid_frequency_per_cm is set but the [grid] table has no freq_per_cm column";
    if (t.gridHasFreq && freqPerCm == 0)
        return "the [grid] table lists line densities: vg_grid_frequency_per_cm is required";
    std::vector<const ParamTable::GridRow*> rows;
    bool ratioFound = false;
    for (const auto& g : t.gridRows) {
        if (g.ratio != ratio) continue;
        ratioFound = true;
        if (g.freq == freqPerCm) rows.push_back(&g);
    }
    if (!ratioFound) return "grid ratio not in the [grid] table";
    if (rows.empty()) return "grid line density not in the [grid] table for this ratio";

    std::vector<double> ks, ts;
    for (const auto* g : rows) { ks.push_back(g->kvp); ts.push_back(g->thick); }
    const std::vector<double> kAxis = UniqueSorted(ks);
    out.thick = UniqueSorted(ts);
    int k0 = 0, k1 = 0;
    double wk = 0;
    if (t.gridHasKvp && !Bracket(kAxis, kvp, k0, k1, wk)) return "kvp outside the [grid] table";
    auto at = [&](double th, double kv) {
        for (const auto* g : rows) if (g->thick == th && g->kvp == kv) return g;
        return rows.front();   // unreachable: completeness is checked at load
    };
    for (double th : out.thick) {
        const auto* a = at(th, kAxis[static_cast<size_t>(k0)]);
        const auto* b = at(th, kAxis[static_cast<size_t>(k1)]);
        out.tp.push_back((1 - wk) * a->tp + wk * b->tp);
        out.ts.push_back((1 - wk) * a->ts + wk * b->ts);
    }
    return {};
}

// ---------------------------------------------------------------------------
// Kernel and thickness
// ---------------------------------------------------------------------------
bool KernelAt(const ParamTable& t, double thicknessCm, double kvp, BlendedKernel& out) {
    out.a.clear();
    out.s.clear();
    int k0, k1;
    double wk;
    if (!Bracket(t.kKvp, kvp, k0, k1, wk)) return false;
    if (thicknessCm > t.kThick.back() + 1e-9) return false;
    if (thicknessCm <= 0) return true;   // no material, no scatter

    int t0, t1;
    double wt;
    if (thicknessCm < t.kThick.front()) {
        // Below the first node: blend node 0 with "no scatter" at t = 0. This
        // is a boundary condition (zero thickness scatters nothing), not a
        // table value.
        const double scale = thicknessCm / t.kThick.front();
        for (const auto& [kk, w] : Pairs(k0, k1, wk)) {
            const KernelNode& n = t.kernels[static_cast<size_t>(kk)];
            for (int i = 0; i < n.terms; ++i) { out.a.push_back(w * scale * n.a[i]); out.s.push_back(n.s[i]); }
        }
        return true;
    }
    Bracket(t.kThick, thicknessCm, t0, t1, wt);
    for (const auto& [tt, wtt] : Pairs(t0, t1, wt))
        for (const auto& [kk, wkk] : Pairs(k0, k1, wk)) {
            const KernelNode& n = t.kernels[static_cast<size_t>(tt) * t.kKvp.size() + static_cast<size_t>(kk)];
            for (int i = 0; i < n.terms; ++i) { out.a.push_back(wtt * wkk * n.a[i]); out.s.push_back(n.s[i]); }
        }
    return true;
}

double SprCapAt(const ParamTable& t, double thicknessCm, double kvp) {
    if (t.capFromKernels) {
        BlendedKernel k;
        if (!KernelAt(t, std::min(thicknessCm, t.kThick.back()), kvp, k)) return -1.0;
        double sum = 0;
        for (double a : k.a) sum += a;
        return sum;
    }
    int c0, c1, a0, a1;
    double wc, wa;
    if (!Bracket(t.capKvp, kvp, c0, c1, wc)) return -1.0;
    Bracket(t.capThick, std::clamp(thicknessCm, t.capThick.front(), t.capThick.back()), a0, a1, wa);
    const size_t nk = t.capKvp.size();
    auto v = [&](int ti, int ki) { return t.capSpr[static_cast<size_t>(ti) * nk + static_cast<size_t>(ki)]; };
    return (1 - wa) * ((1 - wc) * v(a0, c0) + wc * v(a0, c1)) + wa * ((1 - wc) * v(a1, c0) + wc * v(a1, c1));
}

double ThicknessFromLogAtten(double L, double w0, double a, double b, double tMax) {
    if (!(L > 0)) return 0.0;
    if (MuAt(tMax, w0, a, b) * tMax < L) return -1.0;
    double lo = 0.0, hi = tMax;
    for (int i = 0; i < 60; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (MuAt(mid, w0, a, b) * mid < L) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

namespace {

// Normalised 1D Gaussian taps (sum 1), support +-4 sigma.
std::vector<double> GaussTaps(double sigmaPx) {
    const int r = std::max(1, static_cast<int>(std::ceil(4.0 * sigmaPx)));
    std::vector<double> g(static_cast<size_t>(2 * r + 1));
    double sum = 0;
    for (int i = -r; i <= r; ++i) {
        const double v = std::exp(-0.5 * (i / sigmaPx) * (i / sigmaPx));
        g[static_cast<size_t>(i + r)] = v;
        sum += v;
    }
    for (double& v : g) v /= sum;
    return g;
}

// Separable convolution, zero outside the image; adds a * (src * G) to acc.
void AddGaussConv(const std::vector<double>& src, int w, int h, const std::vector<double>& g,
                  double a, std::vector<double>& acc, std::vector<double>& tmp) {
    const int r = static_cast<int>(g.size() / 2);
    tmp.assign(src.size(), 0.0);
    // QA-B-105: the row pass writes one row from one row, the column pass one
    // column from one column -- both independent, and each output keeps the
    // same tap order.
    const int threads = xpe_parallel::ResolveThreads(XpeGsvgThreadRequest(), std::max(w, h));
    xpe_parallel::ForRows(h, threads, [&](int yBegin, int yEnd) {
    for (int y = yBegin; y < yEnd; ++y) {
        const double* s = &src[static_cast<size_t>(y) * static_cast<size_t>(w)];
        double* d = &tmp[static_cast<size_t>(y) * static_cast<size_t>(w)];
        for (int x = 0; x < w; ++x) {
            const int lo = std::max(-r, -x), hi = std::min(r, w - 1 - x);
            double v = 0;
            for (int k = lo; k <= hi; ++k) v += g[static_cast<size_t>(k + r)] * s[x + k];
            d[x] = v;
        }
    }
    });
    xpe_parallel::ForRows(w, threads, [&](int xBegin, int xEnd) {
    for (int x = xBegin; x < xEnd; ++x) {
        for (int y = 0; y < h; ++y) {
            const int lo = std::max(-r, -y), hi = std::min(r, h - 1 - y);
            double v = 0;
            for (int k = lo; k <= hi; ++k)
                v += g[static_cast<size_t>(k + r)] * tmp[static_cast<size_t>(y + k) * static_cast<size_t>(w) + static_cast<size_t>(x)];
            acc[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] += a * v;
        }
    }
    });
}

}  // namespace

std::vector<double> ScatterEstimate(const std::vector<double>& primary,
                                    const std::vector<double>& thicknessCm,
                                    int width, int height, const ParamTable& table,
                                    double kvp, double pitchCm) {
    int k0, k1;
    double wk;
    if (!Bracket(table.kKvp, kvp, k0, k1, wk)) return {};
    const size_t n = primary.size();
    for (double t : thicknessCm)
        if (t > table.kThick.back() + 1e-9) return {};

    // Linear hat weights over thickness nodes, with an extra node at t = 0
    // that carries no scatter. Blending kernels this way is exact for the
    // KernelAt() definition and keeps every convolution separable.
    const size_t nT = table.kThick.size();
    std::vector<double> acc(n, 0.0), tmp, weighted(n);
    for (size_t m = 0; m < nT; ++m) {
        bool any = false;
        for (size_t i = 0; i < n; ++i) {
            const double t = thicknessCm[i];
            double w = 0;
            const double tm = table.kThick[m];
            const double tl = m ? table.kThick[m - 1] : 0.0;
            const double tr = m + 1 < nT ? table.kThick[m + 1] : tm;
            if (t > tl && t <= tm) w = (t - tl) / (tm - tl);
            else if (t > tm && t < tr) w = (tr - t) / (tr - tm);
            weighted[i] = w * primary[i];
            any = any || weighted[i] != 0.0;
        }
        if (!any) continue;
        for (const auto& [kk, wkk] : Pairs(k0, k1, wk)) {
            const KernelNode& node = table.kernels[m * table.kKvp.size() + static_cast<size_t>(kk)];
            for (int i = 0; i < node.terms; ++i) {
                if (node.a[i] <= 0) continue;
                AddGaussConv(weighted, width, height, GaussTaps(node.s[i] / pitchCm),
                             wkk * node.a[i], acc, tmp);
            }
        }
    }
    return acc;
}

std::vector<double> ForwardScatter(const std::vector<double>& primary,
                                   const std::vector<double>& thicknessCm,
                                   int width, int height, const ParamTable& table,
                                   double kvp, double pixelPitchMm) {
    std::vector<double> s = ScatterEstimate(primary, thicknessCm, width, height, table, kvp,
                                            pixelPitchMm / 10.0);
    if (s.empty()) return s;
    for (size_t i = 0; i < s.size(); ++i) s[i] += primary[i];
    return s;
}

// ---------------------------------------------------------------------------
// Pyramid (US 8,064,676 B2): Laplacian pyramid with a 5-tap binomial filter.
// ---------------------------------------------------------------------------
namespace {

struct Plane { int w = 0, h = 0; std::vector<double> v; };

constexpr double kB[5] = {1 / 16.0, 4 / 16.0, 6 / 16.0, 4 / 16.0, 1 / 16.0};

// 1D taps (source index, weight) per output index. The 2D binomial filter is
// separable, so each resampling is a row pass followed by a column pass.
struct Tap { int src; double w; };
using Taps = std::vector<std::vector<Tap>>;

Taps ReduceTaps(int srcN, int outN) {
    Taps t(static_cast<size_t>(outN));
    for (int o = 0; o < outN; ++o)
        for (int i = -2; i <= 2; ++i)
            t[static_cast<size_t>(o)].push_back({std::clamp(2 * o + i, 0, srcN - 1), kB[i + 2]});
    return t;
}

Taps ExpandTaps(int srcN, int outN) {
    Taps t(static_cast<size_t>(outN));
    for (int o = 0; o < outN; ++o)
        for (int i = -2; i <= 2; ++i) {
            if ((o + i) & 1) continue;   // only the even (sampled) positions carry data
            t[static_cast<size_t>(o)].push_back({std::clamp((o + i) / 2, 0, srcN - 1), 2.0 * kB[i + 2]});
        }
    return t;
}

Plane Resample(const Plane& p, int w, int h, const Taps& tx, const Taps& ty) {
    std::vector<double> rows(static_cast<size_t>(w) * static_cast<size_t>(p.h));
    // QA-B-105: both passes are per output row; the tap order per output is
    // unchanged. The pyramid LEVELS stay sequential (each reduces the previous).
    const int threads = xpe_parallel::ResolveThreads(XpeGsvgThreadRequest(), std::max(p.h, h));
    xpe_parallel::ForRows(p.h, threads, [&](int yBegin, int yEnd) {
    for (int y = yBegin; y < yEnd; ++y) {
        const double* s = &p.v[static_cast<size_t>(y) * static_cast<size_t>(p.w)];
        double* d = &rows[static_cast<size_t>(y) * static_cast<size_t>(w)];
        for (int x = 0; x < w; ++x) {
            double v = 0;
            for (const Tap& k : tx[static_cast<size_t>(x)]) v += k.w * s[k.src];
            d[x] = v;
        }
    }
    });
    Plane q{w, h, std::vector<double>(static_cast<size_t>(w) * static_cast<size_t>(h))};
    xpe_parallel::ForRows(h, threads, [&](int yBegin, int yEnd) {
    for (int y = yBegin; y < yEnd; ++y) {
        double* d = &q.v[static_cast<size_t>(y) * static_cast<size_t>(w)];
        for (int x = 0; x < w; ++x) d[x] = 0;
        for (const Tap& k : ty[static_cast<size_t>(y)]) {
            const double* s = &rows[static_cast<size_t>(k.src) * static_cast<size_t>(w)];
            for (int x = 0; x < w; ++x) d[x] += k.w * s[x];
        }
    }
    });
    return q;
}

Plane Reduce(const Plane& p) {
    const int w = (p.w + 1) / 2, h = (p.h + 1) / 2;
    return Resample(p, w, h, ReduceTaps(p.w, w), ReduceTaps(p.h, h));
}

Plane Expand(const Plane& p, int w, int h) {
    return Resample(p, w, h, ExpandTaps(p.w, w), ExpandTaps(p.h, h));
}

// Median absolute value (noise sigma = MAD / 0.6745 for a zero-mean band).
double Mad(std::vector<double> v) {
    for (double& x : v) x = std::fabs(x);
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    return v[mid];
}

void PyramidContrast(std::vector<double>& img, int w, int h, int levels, double gain, double k) {
    // QA-B-107 (#180): the caller's buffer is MOVED in and the result moved
    // back out, and each Gaussian level is moved into its Laplacian band
    // instead of copied. At 3072x3072 one plane is 75.5 MB, and these two
    // copies were 151 MB of it. Moving changes no value.
    std::vector<Plane> g{Plane{w, h, std::move(img)}};
    for (int l = 0; l < levels; ++l) g.push_back(Reduce(g.back()));
    std::vector<Plane> lap(static_cast<size_t>(levels));
    for (int l = 0; l < levels; ++l) {
        const int fw = g[static_cast<size_t>(l)].w, fh = g[static_cast<size_t>(l)].h;
        Plane up = Expand(g[static_cast<size_t>(l) + 1], fw, fh);
        // g[l] is not read again (the reconstruction starts from g.back()), so
        // it becomes the band in place.
        lap[static_cast<size_t>(l)] = std::move(g[static_cast<size_t>(l)]);
        for (size_t i = 0; i < up.v.size(); ++i) lap[static_cast<size_t>(l)].v[i] -= up.v[i];
    }
    // De-noise: soft threshold on the finest band only.
    if (k > 0) {
        std::vector<double>& b = lap[0].v;
        const double thr = k * Mad(b) / 0.6745;
        for (double& x : b) x = (x > thr) ? x - thr : (x < -thr ? x + thr : 0.0);
    }
    Plane cur = g.back();
    for (int l = levels - 1; l >= 0; --l) {
        const Plane& band = lap[static_cast<size_t>(l)];
        Plane up = Expand(cur, band.w, band.h);
        for (size_t i = 0; i < up.v.size(); ++i) up.v[i] += gain * band.v[i];
        cur = std::move(up);
    }
    img = std::move(cur.v);
}

}  // namespace

namespace {

// Running minimum over [i - r, i + r] (clamped), O(n) (van Herk / Gil-Werman).
void MinFilter1D(const double* src, double* dst, int n, int stride, int r) {
    const int w = 2 * r + 1;
    std::vector<double> g(static_cast<size_t>(n + w)), hbuf(static_cast<size_t>(n + w));
    auto at = [&](int i) { return src[static_cast<size_t>(std::clamp(i, 0, n - 1)) * static_cast<size_t>(stride)]; };
    const int len = n + w - 1;   // padded sequence index k maps to i = k - r
    for (int k = 0; k < len; ++k)
        g[static_cast<size_t>(k)] = (k % w == 0) ? at(k - r) : std::min(g[static_cast<size_t>(k - 1)], at(k - r));
    for (int k = len - 1; k >= 0; --k)
        hbuf[static_cast<size_t>(k)] = (k == len - 1 || (k + 1) % w == 0) ? at(k - r)
                                       : std::min(hbuf[static_cast<size_t>(k + 1)], at(k - r));
    for (int i = 0; i < n; ++i) {
        // window of padded indices [i, i + w - 1]
        dst[static_cast<size_t>(i) * static_cast<size_t>(stride)] =
            std::min(hbuf[static_cast<size_t>(i)], g[static_cast<size_t>(i + w - 1)]);
    }
}

}  // namespace

std::vector<double> MinFilter2D(const std::vector<double>& img, int w, int h, int r) {
    std::vector<double> rows(img.size()), out(img.size());
    for (int y = 0; y < h; ++y)
        MinFilter1D(&img[static_cast<size_t>(y) * static_cast<size_t>(w)], &rows[static_cast<size_t>(y) * static_cast<size_t>(w)], w, 1, r);
    for (int x = 0; x < w; ++x)
        MinFilter1D(&rows[static_cast<size_t>(x)], &out[static_cast<size_t>(x)], h, w, r);
    return out;
}

// ---------------------------------------------------------------------------
// Chain
// ---------------------------------------------------------------------------
VgReport RunVirtualGrid(std::vector<double>& io, int width, int height,
                        const ParamTable& table, const VgSettings& st, const VgSwitches& sw,
                        const uint8_t* fieldMask) {
    VgReport rep;
    auto fail = [&](const std::string& why) { rep.error = why; return rep; };

    if (width <= 0 || height <= 0 || io.size() != static_cast<size_t>(width) * static_cast<size_t>(height))
        return fail("image size mismatch");

    // QA-B-96: outside the collimated field a detector still records scatter
    // (QA-B-95: about 3000 DN next to a 30 cm field). Those pixels are not
    // object, so the estimate reads them as 0. `img` below is what the chain
    // reads; `io` keeps the caller's pixels until the result is written.
    const uint8_t* mask = sw.useFieldMask ? fieldMask : nullptr;
    std::vector<double> masked;
    if (mask) {
        masked = io;
        for (size_t i = 0; i < masked.size(); ++i)
            if (!mask[i]) masked[i] = 0.0;
    }
    const std::vector<double>& img = mask ? masked : io;
    if (st.iterations < 1) return fail("iterations must be >= 1");
    if (!(st.pixelPitchMm > 0)) return fail("pixel pitch must be > 0");
    if (!(st.airSignal > 0)) return fail("air signal must be > 0");
    if (st.pyramidLevels != 0 && (st.pyramidLevels < 4 || st.pyramidLevels > 8))
        return fail("pyramid levels must be 4..8");
    if (st.pyramidLevels == 0 && (st.pyramidGain != 1.0 || st.denoiseK != 0.0))
        return fail("pyramid gain / de-noise need pyramid levels");
    if (!(st.pyramidGain > 0) || st.denoiseK < 0) return fail("need pyramid gain > 0 and de-noise k >= 0");
    if (st.pyramidLevels && std::min(width, height) < (1 << st.pyramidLevels))
        return fail("image too small for the pyramid levels");

    // Table coverage for this exposure.
    int k0, k1;
    double wk;
    if (!Bracket(table.kKvp, st.kvp, k0, k1, wk)) return fail("kvp outside the kernel table");
    int w0i, w1i;
    double ww;
    if (!Bracket(table.wetKvp, st.kvp, w0i, w1i, ww)) return fail("kvp outside the [wet] table");
    if (SprCapAt(table, table.kThick.front(), st.kvp) < 0) return fail("kvp outside the [spr_cap] table");
    GridAtKvp grid;
    if (const std::string why = SelectGrid(table, st.gridRatio, st.gridFreqPerCm, st.kvp, grid); !why.empty())
        return fail(why);

    const double W0 = table.wetW0[static_cast<size_t>(w0i)] * (1 - ww) + table.wetW0[static_cast<size_t>(w1i)] * ww;
    const double A = table.wetA[static_cast<size_t>(w0i)] * (1 - ww) + table.wetA[static_cast<size_t>(w1i)] * ww;
    const double B = table.wetB[static_cast<size_t>(w0i)] * (1 - ww) + table.wetB[static_cast<size_t>(w1i)] * ww;
    const double tMax = table.capFromKernels ? table.kThick.back()
                                             : std::min(table.kThick.back(), table.capThick.back());
    const double tMin = table.kThick.front();

    // Reduced grid: coarse pitch at most half the narrowest kernel term.
    double sMin = 1e300;
    for (size_t m = 0; m < table.kThick.size(); ++m)
        for (const auto& [kk, unused] : Pairs(k0, k1, wk)) {
            (void)unused;
            const KernelNode& n = table.kernels[m * table.kKvp.size() + static_cast<size_t>(kk)];
            for (int i = 0; i < n.terms; ++i) sMin = std::min(sMin, n.s[i]);
        }
    const double pitchCm = st.pixelPitchMm / 10.0;
    const int f = sw.reductionFactor > 0 ? sw.reductionFactor
                                         : std::max(1, static_cast<int>(std::floor(0.5 * sMin / pitchCm)));
    const int cw = (width + f - 1) / f, ch = (height + f - 1) / f;
    rep.factor = f;
    rep.coarseW = cw;
    rep.coarseH = ch;

    std::vector<double> Ic(static_cast<size_t>(cw) * static_cast<size_t>(ch), 0.0);
    {
        std::vector<int> cnt(Ic.size(), 0);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const size_t c = static_cast<size_t>(y / f) * static_cast<size_t>(cw) + static_cast<size_t>(x / f);
                Ic[c] += img[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
                ++cnt[c];
            }
        for (size_t i = 0; i < Ic.size(); ++i) Ic[i] /= cnt[i];
    }

    // Joint thickness / scatter iteration on the reduced grid. Thickness is
    // read from the current primary estimate, not from I: scatter raises I/I0
    // and would make the object look thinner.
    if ((sw.cap == CapMode::PrimaryFloor || sw.cap == CapMode::SmoothFloor) &&
        !(sw.capEps > 0 && sw.capEps < 1))
        return fail("cap eps must be in (0, 1)");
    double globalCap = 0;
    for (double tn : table.kThick) globalCap = std::max(globalCap, SprCapAt(table, tn, st.kvp));
    // C4: smallest measured value within one reduced-grid block of each pixel,
    // and its block minimum on the reduced grid (the reach of the bilinear
    // up-sampling).
    std::vector<double> iMinFull, iMinC;
    if (sw.cap == CapMode::SmoothFloor) {
        iMinFull = MinFilter2D(img, width, height, f);
        iMinC.assign(Ic.size(), 1e300);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                double& m = iMinC[static_cast<size_t>(y / f) * static_cast<size_t>(cw) + static_cast<size_t>(x / f)];
                m = std::min(m, iMinFull[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]);
            }
    }
    auto capFor = [&](size_t i, double t) {
        switch (sw.cap) {
        case CapMode::None:         return 1e300;
        case CapMode::LocalSum:     return SprCapAt(table, t, st.kvp);
        case CapMode::GlobalSum:    return globalCap;
        case CapMode::PrimaryFloor: return 1.0 / sw.capEps - 1.0;
        case CapMode::SmoothFloor: {
            const double sMax = (1.0 - sw.capEps) * std::max(iMinC[i], 0.0);
            const double pMin = Ic[i] - sMax;
            return pMin > 0 ? sMax / pMin : 0.0;
        }
        }
        return 1e300;
    };

    std::vector<double> P = Ic, T(Ic.size()), spr(Ic.size()), cap(Ic.size());
    size_t nHigh = 0, nLow = 0, nCapped = 0;
    for (int it = 0; it < st.iterations; ++it) {
        double tSum = 0;
        size_t tN = 0;
        nHigh = nLow = nCapped = 0;
        for (size_t i = 0; i < P.size(); ++i) {
            if (P[i] <= 0) { T[i] = 0; continue; }
            double t = ThicknessFromLogAtten(-std::log(P[i] / st.airSignal), W0, A, B, tMax);
            if (t < 0) {
                // QA-B-93 (lead decision): a region thicker than the table is
                // limited to the table maximum and counted, instead of refusing
                // the whole image.
                if (!sw.clampThickness) return fail("estimated thickness above the table range");
                t = tMax;
                ++nHigh;
            } else if (t > 0 && t < tMin) {
                ++nLow;
            }
            T[i] = t;
            tSum += t;
            ++tN;
        }
        if (!sw.thicknessIndex) {
            const double mean = tN ? tSum / static_cast<double>(tN) : 0.0;
            for (size_t i = 0; i < T.size(); ++i) if (P[i] > 0) T[i] = mean;
        }
        const std::vector<double> S = ScatterEstimate(P, T, cw, ch, table, st.kvp, pitchCm * f);
        if (S.empty()) return fail("estimated thickness above the table range");
        for (size_t i = 0; i < P.size(); ++i) {
            cap[i] = capFor(i, T[i]);
            double r = P[i] > 0 ? S[i] / P[i] : 0.0;
            if (r > cap[i]) { r = cap[i]; ++nCapped; }
            spr[i] = r;
        }
        for (size_t i = 0; i < P.size(); ++i) P[i] = Ic[i] / (1.0 + spr[i]);
    }
    const double nC = static_cast<double>(P.size());
    rep.clampedHighFraction = static_cast<double>(nHigh) / nC;
    rep.belowTableFraction = static_cast<double>(nLow) / nC;
    rep.cappedFraction = static_cast<double>(nCapped) / nC;
    double sprSum = 0;
    for (size_t i = 0; i < T.size(); ++i) {
        rep.maxThicknessCm = std::max(rep.maxThicknessCm, T[i]);
        sprSum += spr[i];
    }
    rep.meanSpr = sprSum / nC;

    // Coarse scatter from the last update, spread back to full resolution
    // (bilinear on coarse pixel centres).
    std::vector<double> Sc(Ic.size());
    for (size_t i = 0; i < Sc.size(); ++i) Sc[i] = Ic[i] - P[i];
    auto upsample = [&](const std::vector<double>& c) {
        std::vector<double> o(img.size());
        for (int y = 0; y < height; ++y) {
            const double cy = std::clamp((y + 0.5) / f - 0.5, 0.0, static_cast<double>(ch - 1));
            const int y0 = static_cast<int>(cy), y1 = std::min(y0 + 1, ch - 1);
            const double fy = cy - y0;
            for (int x = 0; x < width; ++x) {
                const double cx = std::clamp((x + 0.5) / f - 0.5, 0.0, static_cast<double>(cw - 1));
                const int x0 = static_cast<int>(cx), x1 = std::min(x0 + 1, cw - 1);
                const double fx = cx - x0;
                auto v = [&](int xx, int yy) { return c[static_cast<size_t>(yy) * static_cast<size_t>(cw) + static_cast<size_t>(xx)]; };
                o[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] =
                    (1 - fy) * ((1 - fx) * v(x0, y0) + fx * v(x1, y0)) + fy * ((1 - fx) * v(x0, y1) + fx * v(x1, y1));
            }
        }
        return o;
    };
    std::vector<double> Sf = upsample(Sc);
    std::vector<double> capF = (sw.cap == CapMode::LocalSum) ? upsample(cap) : std::vector<double>{};

    // Residual scatter of the chosen grid, normalised to its primary
    // transmission: O = P + (Ts/Tp) * S.
    // NOTE: this step is our derivation from the IEC 60627 definitions of
    // Tp and Ts (grid image = Tp*P + Ts*S, divided by Tp), not a method taken
    // from the literature.
    // QA-B-101: Ts/Tp at each reduced-grid pixel's estimated thickness (a
    // table without a thickness column gives one value), spread like the scatter.
    std::vector<double> Rc(T.size());
    for (size_t i = 0; i < Rc.size(); ++i) Rc[i] = grid.ResidualAt(T[i]);
    std::vector<double> Rf = upsample(Rc);
    // A full-resolution primary darker than this lies above the table's
    // thickness range (QA-B-93: the share of such pixels is reported).
    const double pAtTMax = st.airSignal * std::exp(-MuAt(tMax, W0, A, B) * tMax);
    size_t nFullHigh = 0, nFull = 0;
    // QA-B-107 (#180): the result goes into io itself. Each output pixel reads
    // only its own input pixel (plus Sf/Rf at the same index), so writing in
    // place gives the same values; it saves one full-resolution buffer
    // (75.5 MB at 3072x3072). With a mask, the pixels the mask excludes are
    // kept in a compact copy first, because the post-steps below run over the
    // whole image and would otherwise overwrite them.
    std::vector<double>& out = io;
    // The old code wrote into a zero-initialised buffer, so the post-steps saw
    // ZEROS outside the mask; the pyramid reads neighbours, so those zeros are
    // part of the result inside the mask. Writing in place keeps that exactly:
    // the excluded pixels are saved, then zeroed, then restored at the end.
    std::vector<double> maskedOut;
    int mx0 = 0, my0 = 0, mx1 = width - 1, my1 = height - 1;   // mask bounding box
    if (mask) {
        mx0 = width; my0 = height; mx1 = -1; my1 = -1;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (mask[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]) {
                    mx0 = std::min(mx0, x); mx1 = std::max(mx1, x);
                    my0 = std::min(my0, y); my1 = std::max(my1, y);
                }
        if (mx1 < mx0 || my1 < my0) { mx0 = my0 = 0; mx1 = width - 1; my1 = height - 1; }
        for (size_t i = 0; i < io.size(); ++i)
            if (!mask[i]) maskedOut.push_back(io[i]);
    }
    // QA-B-105: per pixel, independent. The two counters are summed per band
    // and added up afterwards (integers, so the order does not matter).
    // #180 (QA-B-112): the guard's floor per pixel, and the value before the
    // post-steps. Only allocated when an option needs them.
    std::vector<double> postFloor, preOut;
    if (sw.postGuard != PostGuard::None && st.pyramidLevels)
        postFloor.assign(io.size(), 0.0);

    std::atomic<size_t> negativeAtomic{0}, fullHighAtomic{0}, fullAtomic{0};
    xpe_parallel::ForRows(height, xpe_parallel::ResolveThreads(XpeGsvgThreadRequest(), height),
                          [&](int yBegin, int yEnd) {
    size_t bandNeg = 0, bandHigh = 0, bandFull = 0;
    for (size_t i = static_cast<size_t>(yBegin) * static_cast<size_t>(width);
         i < static_cast<size_t>(yEnd) * static_cast<size_t>(width); ++i) {
        if (mask && !mask[i]) continue;   // restored after the post-steps
        double S = Sf[i];
        switch (sw.cap) {
        case CapMode::None: break;
        case CapMode::LocalSum:     S = std::min(S, img[i] * capF[i] / (1.0 + capF[i])); break;
        case CapMode::GlobalSum:    S = std::min(S, img[i] * globalCap / (1.0 + globalCap)); break;
        case CapMode::PrimaryFloor: S = std::min(S, (1.0 - sw.capEps) * img[i]); break;
        case CapMode::SmoothFloor:  S = std::min(S, (1.0 - sw.capEps) * std::max(iMinFull[i], 0.0)); break;
        }
        double p = img[i] - S;
        if (img[i] > 0) { ++bandFull; if (p < pAtTMax) ++bandHigh; }
        if (p < 0) { ++bandNeg; p = 0; }
        // #180 (QA-B-112): the smallest primary this pixel's cap allows.
        if (!postFloor.empty()) {
            // No cap means no bound, so the floor is 0 -- not I/(1+0) = I,
            // which would pin the output to the input (QA-B-113: the MC case
            // GsvgVirtualGridMc.CompareToPrimary caught exactly that).
            switch (sw.cap) {
            case CapMode::None:
                postFloor[i] = 0.0;
                break;
            case CapMode::LocalSum:
                postFloor[i] = img[i] / (1.0 + capF[i]);
                break;
            case CapMode::GlobalSum:
                postFloor[i] = img[i] / (1.0 + globalCap);
                break;
            case CapMode::PrimaryFloor:
                postFloor[i] = sw.capEps > 0 ? sw.capEps * img[i] : 0.0;
                break;
            case CapMode::SmoothFloor:
                postFloor[i] = img[i] - (1.0 - sw.capEps) * std::max(iMinFull[i], 0.0);
                break;
            }
            if (postFloor[i] < 0) postFloor[i] = 0;
        }
        out[i] = p + Rf[i] * std::max(S, 0.0);
    }
    negativeAtomic.fetch_add(bandNeg, std::memory_order_relaxed);
    fullHighAtomic.fetch_add(bandHigh, std::memory_order_relaxed);
    fullAtomic.fetch_add(bandFull, std::memory_order_relaxed);
    });
    nFull = fullAtomic.load();
    nFullHigh = fullHighAtomic.load();
    rep.negativePrimary += negativeAtomic.load();

    rep.aboveTableFullRes = nFull ? static_cast<double>(nFullHigh) / static_cast<double>(nFull) : 0.0;

    // The up-sampled scatter and residual maps are read only by the loop above;
    // released here so the pyramid below does not run alongside them
    // (151 MB at 3072x3072).
    Sf.clear(); Sf.shrink_to_fit();
    Rf.clear(); Rf.shrink_to_fit();
    capF.clear(); capF.shrink_to_fit();

    if (!postFloor.empty() && (sw.postGuard == PostGuard::GlobalDetailScale ||
                               sw.postGuard == PostGuard::SymmetricHeadroom))
        preOut = out;

    // #189 (QA-B-108): what the post-steps see outside the mask. Zero is the
    // behaviour that shipped; the others are measured in the report.
    if (mask && sw.maskOutside != MaskOutside::Keep) {
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const size_t i = static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
                if (mask[i]) continue;
                if (sw.maskOutside == MaskOutside::Zero) {
                    out[i] = 0.0;
                } else if (sw.maskOutside == MaskOutside::Replicate) {
                    const int cx = std::clamp(x, mx0, mx1), cy = std::clamp(y, my0, my1);
                    out[i] = out[static_cast<size_t>(cy) * static_cast<size_t>(width) + static_cast<size_t>(cx)];
                }
            }
    }

    if (st.pyramidLevels) {
        if (mask && sw.maskOutside == MaskOutside::RectOnly) {
            // Post-steps on the mask's bounding rectangle only.
            const int rw = mx1 - mx0 + 1, rh = my1 - my0 + 1;
            std::vector<double> sub(static_cast<size_t>(rw) * static_cast<size_t>(rh));
            for (int y = 0; y < rh; ++y)
                for (int x = 0; x < rw; ++x)
                    sub[static_cast<size_t>(y) * static_cast<size_t>(rw) + static_cast<size_t>(x)] =
                        out[static_cast<size_t>(y + my0) * static_cast<size_t>(width) + static_cast<size_t>(x + mx0)];
            PyramidContrast(sub, rw, rh, st.pyramidLevels, st.pyramidGain, st.denoiseK);
            for (int y = 0; y < rh; ++y)
                for (int x = 0; x < rw; ++x)
                    out[static_cast<size_t>(y + my0) * static_cast<size_t>(width) + static_cast<size_t>(x + mx0)] =
                        sub[static_cast<size_t>(y) * static_cast<size_t>(rw) + static_cast<size_t>(x)];
        } else {
            PyramidContrast(out, width, height, st.pyramidLevels, st.pyramidGain, st.denoiseK);
        }
    }

    // #180 (QA-B-112): hold the guard's bound on the OUTPUT.
    if (!postFloor.empty()) {
        if (sw.postGuard == PostGuard::Clamp) {
            for (size_t i = 0; i < out.size(); ++i) {
                if (mask && !mask[i]) continue;
                if (out[i] < postFloor[i]) out[i] = postFloor[i];
            }
        } else if (sw.postGuard == PostGuard::GlobalDetailScale) {
            double t = 1.0;
            for (size_t i = 0; i < out.size(); ++i) {
                if (mask && !mask[i]) continue;
                const double d = out[i] - preOut[i];
                if (d < 0) {
                    const double head = preOut[i] - postFloor[i];
                    if (head <= 0) { t = 0.0; break; }
                    t = std::min(t, head / -d);
                }
            }
            rep.postGuardScale = t;
            if (t < 1.0)
                for (size_t i = 0; i < out.size(); ++i) {
                    if (mask && !mask[i]) continue;
                    out[i] = preOut[i] + t * (out[i] - preOut[i]);
                }
        } else if (sw.postGuard == PostGuard::SymmetricHeadroom) {
            for (size_t i = 0; i < out.size(); ++i) {
                if (mask && !mask[i]) continue;
                const double head = std::max(preOut[i] - postFloor[i], 0.0);
                const double d = out[i] - preOut[i];
                if (d > head) out[i] = preOut[i] + head;
                else if (d < -head) out[i] = preOut[i] - head;
            }
        }
        size_t below = 0;
        for (size_t i = 0; i < out.size(); ++i) {
            if (mask && !mask[i]) continue;
            if (out[i] < postFloor[i] - 1e-9 * std::max(postFloor[i], 1.0)) ++below;
        }
        rep.belowGuardFloor = static_cast<double>(below) / static_cast<double>(out.size());
    }

    if (mask) {
        size_t m = 0;
        for (size_t i = 0; i < out.size(); ++i)
            if (!mask[i]) out[i] = maskedOut[m++];
    }

    for (double v : out) if (v > 65535.0) ++rep.clippedHigh;
    return rep;
}

}  // namespace xpe_gsvg_detail
