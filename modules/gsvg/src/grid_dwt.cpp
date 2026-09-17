/**
 * @file grid_dwt.cpp
 * @brief Recursive db4 DWT grid suppression (#180, QA-B-90). See grid_dwt.h.
 */
#include "grid_dwt.h"

#include <algorithm>
#include <cmath>

namespace xpe_gsvg_detail {

namespace {

constexpr double kPi = 3.14159265358979323846;

// db4 scaling filter (8 taps, 4 vanishing moments). Sum = sqrt(2).
constexpr double kH[8] = {
    0.2303778133088964,  0.7148465705529154,  0.6308807679298587, -0.0279837694168599,
   -0.1870348117190931,  0.0308413818355607,  0.0328830116668852, -0.0105974017850690,
};
// Wavelet filter g[k] = (-1)^k h[7-k].
constexpr double kG[8] = {
    -0.0105974017850690 * 1.0, -0.0328830116668852,  0.0308413818355607,  0.1870348117190931,
    -0.0279837694168599,       -0.6308807679298587,  0.7148465705529154, -0.2303778133088964,
};

// Periodic analysis of one line: in[0..n) (n even) -> a[0..n/2), d[0..n/2).
void Analyze(const double* in, size_t n, size_t stride, double* a, double* d, size_t ostride) {
    const size_t half = n / 2;
    for (size_t i = 0; i < half; ++i) {
        double sa = 0.0, sd = 0.0;
        for (size_t k = 0; k < 8; ++k) {
            // QA-B-102 (#179): 2i + k < n + 7, so for n >= 8 the periodic wrap
            // is one conditional subtraction -- the same index the modulo gave,
            // without an integer division per tap.
            size_t idx = 2 * i + k;
            if (n >= 8) { if (idx >= n) idx -= n; } else { idx %= n; }
            const double v = in[idx * stride];
            sa += kH[k] * v;
            sd += kG[k] * v;
        }
        a[i * ostride] = sa;
        d[i * ostride] = sd;
    }
}

// Periodic synthesis: out[j] = sum over i,k with (2i+k) mod n == j.
void Synthesize(const double* a, const double* d, size_t ostride, size_t n, double* out, size_t stride) {
    for (size_t j = 0; j < n; ++j) out[j * stride] = 0.0;
    const size_t half = n / 2;
    for (size_t i = 0; i < half; ++i) {
        const double va = a[i * ostride], vd = d[i * ostride];
        for (size_t k = 0; k < 8; ++k) {
            size_t idx = 2 * i + k;   // same wrap as above (QA-B-102)
            if (n >= 8) { if (idx >= n) idx -= n; } else { idx %= n; }
            out[idx * stride] += kH[k] * va + kG[k] * vd;
        }
    }
}

// QA-B-102 (#179): the column passes below run on a transposed copy. A column
// of a (cols x rows) buffer is `cols` doubles apart, so at 3072 x 3072 every
// tap of the 8-tap filter is a cache miss; transposing first makes each filter
// read contiguous. The filter itself is unchanged, so every output double is
// bit-identical -- a transpose only moves values.
void Transpose(const double* src, size_t rows, size_t cols, double* dst) {
    constexpr size_t kBlock = 64;   // 64 doubles = 512 B, a cache line times eight
    for (size_t y0 = 0; y0 < rows; y0 += kBlock)
        for (size_t x0 = 0; x0 < cols; x0 += kBlock) {
            const size_t y1 = std::min(y0 + kBlock, rows), x1 = std::min(x0 + kBlock, cols);
            for (size_t y = y0; y < y1; ++y)
                for (size_t x = x0; x < x1; ++x)
                    dst[x * rows + y] = src[y * cols + x];
        }
}

double BlackmanHarris(size_t i, size_t n) {
    const double a = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(n);
    return 0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2.0 * a) - 0.01168 * std::cos(3.0 * a);
}

// Mean along the lines of a (bw x bh) buffer, as a function of position across them.
std::vector<double> LineProfile(const std::vector<double>& img, int w, int h, Axis axis) {
    const int n = (axis == Axis::Rows) ? h : w;
    const int m = (axis == Axis::Rows) ? w : h;
    std::vector<double> p(static_cast<size_t>(n), 0.0);
    for (int y = 0; y < h; ++y) {
        const double* row = img.data() + static_cast<size_t>(y) * static_cast<size_t>(w);
        for (int x = 0; x < w; ++x)
            p[static_cast<size_t>(axis == Axis::Rows ? y : x)] += row[x];
    }
    for (double& v : p) v /= m;
    return p;
}

// Power at bins 0..n/2 of a Blackman-Harris-windowed profile.
std::vector<double> WindowedPower(std::vector<double> p, bool removeMean, double* windowSum) {
    const size_t n = p.size();
    double mean = 0.0;
    if (removeMean) {
        for (double v : p) mean += v;
        mean /= static_cast<double>(n);
    }
    std::vector<std::complex<double>> c(n);
    double ws = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double wv = BlackmanHarris(i, n);
        ws += wv;
        c[i] = (p[i] - mean) * wv;
    }
    Fft(n).forward(c);
    std::vector<double> out(n / 2 + 1);
    for (size_t k = 0; k <= n / 2; ++k) out[k] = std::norm(c[k]);
    if (windowSum) *windowSum = ws;
    return out;
}

double Median(std::vector<double> v) {
    if (v.empty()) return -1.0;
    const size_t mid = v.size() / 2;
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(mid), v.end());
    return v[mid];
}

// Parabola through log power at k-1, k, k+1.
double Refine(const std::vector<double>& p, size_t k) {
    if (k == 0 || k + 1 >= p.size() || p[k - 1] <= 0.0 || p[k] <= 0.0 || p[k + 1] <= 0.0)
        return static_cast<double>(k);
    const double a = std::log(p[k - 1]), b = std::log(p[k]), c = std::log(p[k + 1]);
    const double den = a - 2.0 * b + c;
    const double off = (den != 0.0) ? 0.5 * (a - c) / den : 0.0;
    return static_cast<double>(k) + std::clamp(off, -0.5, 0.5);
}

}  // namespace

// ---------------------------------------------------------------------------
// Fft
// ---------------------------------------------------------------------------
Fft::Fft(size_t n) : n_(n) {
    m_ = 1;
    size_t r = n;
    while (r > 0 && (r & 1u) == 0u) { r >>= 1u; m_ <<= 1u; }
    q_ = (n == 0) ? 1 : r;
    rev_.assign(m_, 0);
    size_t bits = 0;
    while ((size_t{1} << bits) < m_) ++bits;
    for (size_t i = 0; i < m_; ++i) {
        size_t v = 0;
        for (size_t b = 0; b < bits; ++b)
            if (i & (size_t{1} << b)) v |= size_t{1} << (bits - 1 - b);
        rev_[i] = v;
    }
    roots_.resize(m_ / 2 + 1);
    for (size_t j = 0; j < roots_.size(); ++j)
        roots_[j] = std::polar(1.0, -2.0 * kPi * static_cast<double>(j) / static_cast<double>(m_));
    if (q_ > 1) {
        phase_.resize(n_);
        for (size_t t = 0; t < n_; ++t)
            phase_[t] = std::polar(1.0, -2.0 * kPi * static_cast<double>(t) / static_cast<double>(n_));
    }
}

void Fft::pow2(std::complex<double>* x, size_t m, bool inv) const {
    for (size_t i = 0; i < m; ++i)
        if (i < rev_[i]) std::swap(x[i], x[rev_[i]]);
    for (size_t len = 2; len <= m; len <<= 1u) {
        const size_t step = m / len;
        for (size_t s = 0; s < m; s += len)
            for (size_t j = 0; j < len / 2; ++j) {
                const std::complex<double> w = inv ? std::conj(roots_[j * step]) : roots_[j * step];
                const std::complex<double> u = x[s + j];
                const std::complex<double> v = x[s + j + len / 2] * w;
                x[s + j] = u + v;
                x[s + j + len / 2] = u - v;
            }
    }
}

void Fft::forward(std::vector<std::complex<double>>& x) const {
    if (q_ == 1) { pow2(x.data(), m_, false); return; }
    // split into q decimated sub-sequences x_j[i] = x[j + q i], i < m
    std::vector<std::complex<double>> sub(q_ * m_);
    for (size_t j = 0; j < q_; ++j)
        for (size_t i = 0; i < m_; ++i) sub[j * m_ + i] = x[j + q_ * i];
    for (size_t j = 0; j < q_; ++j) pow2(sub.data() + j * m_, m_, false);
    // X[k] = sum_j Y_j[k mod m] e^{-2 pi i k j / n}
    for (size_t k = 0; k < n_; ++k) {
        std::complex<double> acc = 0.0;
        const size_t km = k % m_;
        for (size_t j = 0; j < q_; ++j) acc += sub[j * m_ + km] * phase_[(k * j) % n_];
        x[k] = acc;
    }
}

void Fft::inverse(std::vector<std::complex<double>>& x) const {
    // inverse = conj(forward(conj(X))) / n
    for (auto& v : x) v = std::conj(v);
    forward(x);
    const double s = 1.0 / static_cast<double>(n_);
    for (auto& v : x) v = std::conj(v) * s;
}

// ---------------------------------------------------------------------------
// DWT
// ---------------------------------------------------------------------------
Level Dwt2(const std::vector<double>& img, int w, int h) {
    Level lv;
    lv.srcW = w; lv.srcH = h;
    lv.w = w + (w & 1);
    lv.h = h + (h & 1);
    const size_t W = static_cast<size_t>(lv.w), H = static_cast<size_t>(lv.h);
    std::vector<double> pad(W * H);
    for (size_t y = 0; y < H; ++y) {
        const size_t sy = std::min(y, static_cast<size_t>(h) - 1);
        for (size_t x = 0; x < W; ++x) {
            const size_t sx = std::min(x, static_cast<size_t>(w) - 1);
            pad[y * W + x] = img[sy * static_cast<size_t>(w) + sx];
        }
    }
    const size_t hw = W / 2, hh = H / 2;
    // rows: L and H images, each (W/2) x H
    std::vector<double> lo(hw * H), hi(hw * H);
    for (size_t y = 0; y < H; ++y)
        Analyze(pad.data() + y * W, W, 1, lo.data() + y * hw, hi.data() + y * hw, 1);
    lv.ll.assign(hw * hh, 0.0); lv.lh.assign(hw * hh, 0.0);
    lv.hl.assign(hw * hh, 0.0); lv.hh.assign(hw * hh, 0.0);
    // Columns, on a transposed copy (see Transpose): same filter, same order.
    std::vector<double> colIn(hw * H), colA(hw * hh), colD(hw * hh);
    Transpose(lo.data(), H, hw, colIn.data());
    for (size_t x = 0; x < hw; ++x)
        Analyze(colIn.data() + x * H, H, 1, colA.data() + x * hh, colD.data() + x * hh, 1);
    Transpose(colA.data(), hw, hh, lv.ll.data());
    Transpose(colD.data(), hw, hh, lv.lh.data());
    Transpose(hi.data(), H, hw, colIn.data());
    for (size_t x = 0; x < hw; ++x)
        Analyze(colIn.data() + x * H, H, 1, colA.data() + x * hh, colD.data() + x * hh, 1);
    Transpose(colA.data(), hw, hh, lv.hl.data());
    Transpose(colD.data(), hw, hh, lv.hh.data());
    return lv;
}

std::vector<double> Idwt2(const Level& lv) {
    const size_t W = static_cast<size_t>(lv.w), H = static_cast<size_t>(lv.h);
    const size_t hw = W / 2;
    std::vector<double> lo(hw * H), hi(hw * H);
    {
        const size_t hh = H / 2;
        std::vector<double> a(hw * hh), d(hw * hh), out(hw * H);
        Transpose(lv.ll.data(), hh, hw, a.data());
        Transpose(lv.lh.data(), hh, hw, d.data());
        for (size_t x = 0; x < hw; ++x)
            Synthesize(a.data() + x * hh, d.data() + x * hh, 1, H, out.data() + x * H, 1);
        Transpose(out.data(), hw, H, lo.data());
        Transpose(lv.hl.data(), hh, hw, a.data());
        Transpose(lv.hh.data(), hh, hw, d.data());
        for (size_t x = 0; x < hw; ++x)
            Synthesize(a.data() + x * hh, d.data() + x * hh, 1, H, out.data() + x * H, 1);
        Transpose(out.data(), hw, H, hi.data());
    }
    std::vector<double> full(W * H);
    for (size_t y = 0; y < H; ++y)
        Synthesize(lo.data() + y * hw, hi.data() + y * hw, 1, W, full.data() + y * W, 1);
    const size_t sw = static_cast<size_t>(lv.srcW), sh = static_cast<size_t>(lv.srcH);
    std::vector<double> out(sw * sh);
    for (size_t y = 0; y < sh; ++y)
        std::copy_n(full.data() + y * W, sw, out.data() + y * sw);
    return out;
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------
InputPeak DetectInputGrid(const std::vector<double>& img, int w, int h, Axis axis) {
    InputPeak r;
    double ws = 0.0;
    const auto p = WindowedPower(LineProfile(img, w, h, axis), true, &ws);
    const size_t n = (axis == Axis::Rows) ? static_cast<size_t>(h) : static_cast<size_t>(w);
    const size_t last = p.size() - 1;
    // kMinBin leaves at least two bins on the low side: with only the high side
    // to compare against, a spectrum that simply falls with frequency (any
    // anatomy) looks like a peak at its first bins (QA-B-90 first run).
    constexpr size_t kInner = 6, kOuter = 30, kMinBin = kInner + 2;
    size_t best = 0;
    double bestProm = 0.0;
    for (size_t k = kMinBin; k <= last; ++k) {
        if (p[k] <= 0.0) continue;
        std::vector<double> left, right;
        for (size_t j = (k > kOuter ? k - kOuter : 1); j + kInner <= k; ++j)
            if (j >= 1) left.push_back(p[j]);
        for (size_t j = k + kInner; j <= std::min(last, k + kOuter); ++j) right.push_back(p[j]);
        if (left.empty() && right.empty()) continue;
        const double ref = std::max(Median(left), Median(right));
        const double prom = (ref > 0.0) ? p[k] / ref : 1e300;
        if (prom > bestProm) { bestProm = prom; best = k; }
    }
    if (best == 0) return r;
    r.prominence = bestProm;
    r.amplitude = 2.0 * std::sqrt(p[best]) / ws;
    r.freq = Refine(p, best) / static_cast<double>(n);
    r.detected = bestProm > kInputProminence && r.amplitude > kMinAmplitudeDn;
    return r;
}

Placement PlaceInSubbands(double f, int maxLevels) {
    double phi = f;
    for (int l = 1; l <= maxLevels; ++l) {
        if (phi >= 0.25) return {l, 1.0 - 2.0 * phi};
        phi *= 2.0;
    }
    return {};
}

SubbandCheck CheckSubband(const std::vector<double>& band, int bw, int bh, Axis axis,
                          double predictedFreq) {
    SubbandCheck c;
    const auto p = WindowedPower(LineProfile(band, bw, bh, axis), false, nullptr);
    const size_t n = (axis == Axis::Rows) ? static_cast<size_t>(bh) : static_cast<size_t>(bw);
    double mean = 0.0, sq = 0.0;
    for (double v : p) { mean += v; sq += v * v; }
    mean /= static_cast<double>(p.size());
    const double var = std::max(0.0, sq / static_cast<double>(p.size()) - mean * mean);
    c.threshold = mean + kSubbandSigma * std::sqrt(var);

    long long lo = 0, hi = static_cast<long long>(p.size()) - 1;
    if (predictedFreq >= 0.0) {
        const long long kp = std::llround(predictedFreq * static_cast<double>(n));
        lo = std::max(lo, kp - kSubbandGateBins);
        hi = std::min(hi, kp + kSubbandGateBins);
    }
    if (lo > hi) return c;
    size_t best = static_cast<size_t>(lo);
    for (long long k = lo; k <= hi; ++k)
        if (p[static_cast<size_t>(k)] > p[best]) best = static_cast<size_t>(k);
    c.peakPower = p[best];
    c.peakBin = Refine(p, best);
    c.detected = c.peakPower > c.threshold && c.peakPower > 0.0;
    return c;
}

void BandStop(std::vector<double>& band, int bw, int bh, Axis axis, double peakBin,
              double sigmaBins) {
    const size_t L = (axis == Axis::Rows) ? static_cast<size_t>(bh) : static_cast<size_t>(bw);
    const size_t lines = (axis == Axis::Rows) ? static_cast<size_t>(bw) : static_cast<size_t>(bh);
    const Fft fft(L);
    std::vector<double> gain(L);
    const double s2 = 2.0 * sigmaBins * sigmaBins;
    for (size_t k = 0; k < L; ++k) {
        const double ks = (k <= L / 2) ? static_cast<double>(k) : static_cast<double>(k) - static_cast<double>(L);
        const double a = ks - peakBin, b = ks + peakBin;
        const double g1 = 1.0 - std::exp(-a * a / s2);
        const double g2 = (peakBin == 0.0) ? 1.0 : 1.0 - std::exp(-b * b / s2);
        gain[k] = g1 * g2;
    }
    std::vector<std::complex<double>> c(L);
    const size_t W = static_cast<size_t>(bw);
    auto at = [&](size_t line, size_t t) -> double& {
        return (axis == Axis::Rows) ? band[t * W + line] : band[line * W + t];
    };
    for (size_t line = 0; line < lines; ++line) {
        for (size_t t = 0; t < L; ++t) c[t] = at(line, t);
        fft.forward(c);
        for (size_t k = 0; k < L; ++k) c[k] *= gain[k];
        fft.inverse(c);
        for (size_t t = 0; t < L; ++t) at(line, t) = c[t].real();
    }
}

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------
int MaxLevels(int w, int h) {
    int m = std::min(w, h), l = 0;
    while (m >= 2) { m /= 2; ++l; }
    return std::max(0, l - 4);
}

double FoldDouble(double phi) {
    const double x = 2.0 * phi;
    return std::fabs(x - std::round(x));
}

Report SuppressGrid(uint16_t* pixels, int w, int h, const Options& opt) {
    Report rep;
    rep.maxLevels = MaxLevels(w, h);
    if (rep.maxLevels <= 0 || pixels == nullptr) return rep;

    const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
    std::vector<double> img(pixels, pixels + count);

    // Detection always looks at the linear image: the amplitude floor is in DN.
    bool activeRows = true, activeCols = true;
    if (opt.inputGate && opt.replay == nullptr) {
        rep.rows.input = DetectInputGrid(img, w, h, Axis::Rows);
        rep.cols.input = DetectInputGrid(img, w, h, Axis::Columns);
        if (rep.rows.input.detected) rep.rows.place = PlaceInSubbands(rep.rows.input.freq, rep.maxLevels);
        if (rep.cols.input.detected) rep.cols.place = PlaceInSubbands(rep.cols.input.freq, rep.maxLevels);
        activeRows = rep.rows.place.level > 0;
        activeCols = rep.cols.place.level > 0;
        if (!activeRows && !activeCols) return rep;   // nothing to remove: untouched
    }
    if (opt.logDomain)
        for (double& v : img) v = std::log(v + 1.0);

    std::vector<Level> levels;
    std::vector<double> cur = std::move(img);
    int cw = w, ch = h;
    bool changed = false;
    auto apply = [&](const Decision& d, Level& lv) {
        const int bw = lv.w / 2, bh = lv.h / 2;
        std::vector<double>& band = (d.axis == Axis::Rows) ? lv.lh : lv.hl;
        if (opt.bandStop) { BandStop(band, bw, bh, d.axis, d.peakBin, opt.sigmaBins); changed = true; }
    };

    if (opt.replay != nullptr) {
        // Same decisions, different image: used to measure what the filtering
        // does to anatomy on its own (the MTF case).
        rep.decisions = opt.replay->decisions;
        rep.levelsUsed = opt.replay->levelsUsed;
        for (int l = 1; l <= rep.levelsUsed; ++l) {
            Level lv = Dwt2(cur, cw, ch);
            for (const Decision& d : rep.decisions)
                if (d.level == l) apply(d, lv);
            cur = lv.ll;
            cw = lv.w / 2; ch = lv.h / 2;
            levels.push_back(std::move(lv));
        }
    } else {
        bool doneRows = !activeRows, doneCols = !activeCols;
        double phiRows = rep.rows.input.freq, phiCols = rep.cols.input.freq;   // grid frequency entering level l
        for (int l = 1; l <= rep.maxLevels; ++l) {
            Level lv = Dwt2(cur, cw, ch);
            const int bw = lv.w / 2, bh = lv.h / 2;
            // At level l the grid sits (in every sub-band, through the db4
            // transition bands) at fold(2*phi). Each level's detail band is checked
            // there; past the level where the ideal split puts the grid, the first
            // level without a detection ends the descent (auto-stop).
            //
            // Tang et al. stop at the first detection. Here the descent goes on
            // past it: db4's transition band leaves part of the grid in the
            // approximation band, and that part surfaces in the next levels'
            // detail bands at the folded frequency (QA-B-90: stopping at the first
            // detection left 60 lpi at 4.2e4 x the grid-free level, 165 x after).
            auto visit = [&](AxisReport& ar, bool& done, double& phi, Axis axis) {
                if (done) return;
                const double sub = FoldDouble(phi);
                const double gate = opt.inputGate ? sub : -1.0;
                SubbandCheck chk = CheckSubband(axis == Axis::Rows ? lv.lh : lv.hl, bw, bh, axis, gate);
                ar.checks.push_back(chk);
                if (chk.detected) {
                    Decision d{l, axis, chk.peakBin};
                    apply(d, lv);
                    rep.decisions.push_back(d);
                    ++ar.filteredLevels;
                } else if (opt.autoStop && (!opt.inputGate || l >= ar.place.level)) {
                    done = true;
                }
                phi = sub;
            };
            visit(rep.rows, doneRows, phiRows, Axis::Rows);
            visit(rep.cols, doneCols, phiCols, Axis::Columns);
            cur = lv.ll;
            cw = bw; ch = bh;
            levels.push_back(std::move(lv));
            rep.levelsUsed = l;
            if (doneRows && doneCols) break;
        }
    }
    if (!changed) return rep;   // nothing filtered: keep the input bytes exactly

    std::vector<double> rec = levels.back().ll;
    for (size_t i = levels.size(); i-- > 0;) {
        levels[i].ll = std::move(rec);
        rec = Idwt2(levels[i]);
    }
    for (size_t i = 0; i < count; ++i) {
        double v = std::round(opt.logDomain ? std::exp(rec[i]) - 1.0 : rec[i]);
        if (v < 0.0) v = 0.0;
        if (v > 65535.0) v = 65535.0;
        pixels[i] = static_cast<uint16_t>(v);
    }
    return rep;
}

}  // namespace xpe_gsvg_detail
