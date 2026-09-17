/**
 * @file grid_test_tools.h
 * @brief Test-side measurement tools for grid suppression work (#180, QA-B-89).
 *
 * Nothing here is product code. Three tools, each checked against inputs whose
 * answer is known (test_grid_tools.cpp):
 *
 *  1. A synthetic grid image: an anatomy-like background (smooth low-frequency
 *     field, a sharp oblique edge, small disks, Gaussian noise) multiplied by a
 *     sinusoidal grid. The grid is point-sampled at the pixel pitch, so a grid
 *     denser than Nyquist shows up at its aliased frequency.
 *  2. Residual grid energy: power in a narrow band around the grid frequency of
 *     the line-averaged profile, divided by the mean power of the bands beside it.
 *  3. Slanted-edge MTF (IEC 62220-1 style): per-row edge fit, projection onto
 *     the edge normal, 4x oversampled ESF, LSF by central difference, MTF by a
 *     direct DFT. The two known sampling responses of that chain (bin average,
 *     central difference) are divided out.
 *
 * Spectra use a direct O(N^2) DFT -- no FFT library is added for a test.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace gsvg_test {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMmPerInch = 25.4;

// Rows: intensity varies with the row index (grid lines run horizontally).
// Columns: intensity varies with the column index (lines run vertically).
enum class GridAxis { Rows, Columns };

struct Image {
    int width = 0;
    int height = 0;
    std::vector<double> px;

    Image() = default;
    Image(int w, int h, double v = 0.0)
        : width(w), height(h), px(static_cast<size_t>(w) * static_cast<size_t>(h), v) {}

    double& at(int x, int y) { return px[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]; }
    double at(int x, int y) const { return px[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)]; }
};

// ---------------------------------------------------------------------------
// (1) Grid frequency and synthetic images
// ---------------------------------------------------------------------------

inline double GridFrequencyPerMm(double linesPerInch) { return linesPerInch / kMmPerInch; }

// Frequency (cycles/mm) a point-sampled sinusoid appears at: fold into [0, fs/2].
inline double AliasedFrequencyPerMm(double linesPerInch, double pitchMm) {
    const double fs = 1.0 / pitchMm;
    const double f = GridFrequencyPerMm(linesPerInch);
    return std::fabs(f - fs * std::round(f / fs));
}

// Not uniform on purpose (#148): smooth field + sharp oblique edge + disks + noise.
inline Image MakeAnatomyBackground(int w, int h, unsigned seed, double noiseSigma) {
    Image img(w, h);
    constexpr struct { double cx, cy, r, c; } kDisks[] = {
        {0.20, 0.25, 0.012, 2500.0}, {0.70, 0.30, 0.020, -2000.0}, {0.45, 0.65, 0.008, 3000.0},
        {0.80, 0.80, 0.015, 1800.0}, {0.30, 0.85, 0.010, -2500.0},
    };
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, noiseSigma);
    for (int y = 0; y < h; ++y) {
        const double v = static_cast<double>(y) / h;
        for (int x = 0; x < w; ++x) {
            const double u = static_cast<double>(x) / w;
            double val = 20000.0
                       + 6000.0 * std::cos(2.0 * kPi * 0.7 * u) * std::sin(2.0 * kPi * 0.5 * v)
                       + 3000.0 * u;
            if (u + 0.3 * v > 0.6) val += 5000.0;                  // sharp oblique edge
            for (const auto& d : kDisks) {
                const double dx = (u - d.cx) * w, dy = (v - d.cy) * h;
                if (dx * dx + dy * dy <= (d.r * w) * (d.r * w)) val += d.c;
            }
            img.at(x, y) = val + (noiseSigma > 0.0 ? noise(rng) : 0.0);
        }
    }
    return img;
}

struct GridSpec {
    double linesPerInch = 103.0;
    double pitchMm = 0.139;
    GridAxis axis = GridAxis::Rows;
    double depth = 0.05;     // modulation depth d: factor = 1 + d*sin(...)
    double phase = 0.3;
};

inline double GridFactor(const GridSpec& g, int x, int y) {
    const int i = (g.axis == GridAxis::Rows) ? y : x;
    const double t = static_cast<double>(i) * g.pitchMm;   // pixel centre, mm
    return 1.0 + g.depth * std::sin(2.0 * kPi * GridFrequencyPerMm(g.linesPerInch) * t + g.phase);
}

inline Image ApplyGrid(const Image& bg, const GridSpec& g) {
    Image out = bg;
    for (int y = 0; y < bg.height; ++y)
        for (int x = 0; x < bg.width; ++x) out.at(x, y) *= GridFactor(g, x, y);
    return out;
}

// The ideal suppressor: divide the known grid out. Used as the "fixed" input.
inline Image RemoveGridExactly(const Image& img, const GridSpec& g) {
    Image out = img;
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) out.at(x, y) /= GridFactor(g, x, y);
    return out;
}

// ---------------------------------------------------------------------------
// Spectra
// ---------------------------------------------------------------------------

// Mean across the grid lines, as a function of position along the modulation axis.
inline std::vector<double> AxisProfile(const Image& img, GridAxis axis) {
    const int n = (axis == GridAxis::Rows) ? img.height : img.width;
    const int m = (axis == GridAxis::Rows) ? img.width : img.height;
    std::vector<double> p(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double acc = 0.0;
        for (int j = 0; j < m; ++j) acc += (axis == GridAxis::Rows) ? img.at(j, i) : img.at(i, j);
        p[static_cast<size_t>(i)] = acc / m;
    }
    return p;
}

// Power at bins 0..N/2 of the mean-removed profile (direct DFT), 4-term
// Blackman-Harris window. QA-B-89 first used Hann: its sidelobes put a fixed
// fraction of a strong grid peak into the neighbour bands, so band/neighbour
// stopped rising at d >= 0.02 (ratio ~1e5). Blackman-Harris sidelobes are
// ~-92 dB; its main lobe is +-4 bins, which sets the band half-width below.
inline std::vector<double> PowerSpectrum(const std::vector<double>& profile) {
    const size_t n = profile.size();
    double mean = 0.0;
    for (double v : profile) mean += v;
    mean /= static_cast<double>(n);
    std::vector<double> w(n);
    for (size_t i = 0; i < n; ++i) {
        const double a = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(n);
        const double bh = 0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2.0 * a)
                        - 0.01168 * std::cos(3.0 * a);
        w[i] = (profile[i] - mean) * bh;
    }
    std::vector<double> power(n / 2 + 1, 0.0);
    for (size_t k = 0; k <= n / 2; ++k) {
        double re = 0.0, im = 0.0;
        const double omega = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            re += w[i] * std::cos(omega * static_cast<double>(i));
            im -= w[i] * std::sin(omega * static_cast<double>(i));
        }
        power[k] = re * re + im * im;
    }
    return power;
}

// Strongest bin at or above minBin, refined by a parabola on log power.
inline double PeakFrequencyPerMm(const Image& img, GridAxis axis, double pitchMm, int minBin = 3) {
    const auto p = PowerSpectrum(AxisProfile(img, axis));
    const size_t n = (p.size() - 1) * 2;
    size_t best = static_cast<size_t>(minBin);
    for (size_t k = best; k < p.size(); ++k)
        if (p[k] > p[best]) best = k;
    double offset = 0.0;
    if (best > 0 && best + 1 < p.size()) {
        const double a = std::log(p[best - 1]), b = std::log(p[best]), c = std::log(p[best + 1]);
        const double den = a - 2.0 * b + c;
        if (den != 0.0) offset = 0.5 * (a - c) / den;
    }
    return (static_cast<double>(best) + offset) / (static_cast<double>(n) * pitchMm);
}

// ---------------------------------------------------------------------------
// (2) Residual grid energy
// ---------------------------------------------------------------------------

struct GridEnergy {
    double band = 0.0;        // mean power, bins within +-halfBand of the grid bin (main lobe)
    double neighbour = 0.0;   // mean power of the bands on either side
    double ratio = 0.0;       // band / neighbour
    long long gridBin = 0;
};

inline GridEnergy ResidualGridEnergy(const Image& img, GridAxis axis, double freqPerMm,
                                     double pitchMm, int halfBand = 4, int neighbourWidth = 10,
                                     int guard = 2, int minBin = 3) {
    const auto p = PowerSpectrum(AxisProfile(img, axis));
    const long long n = static_cast<long long>((p.size() - 1) * 2);
    const long long last = static_cast<long long>(p.size()) - 1;
    const long long kg = std::llround(freqPerMm * pitchMm * static_cast<double>(n));
    GridEnergy e;
    e.gridBin = kg;
    int nb = 0, nn = 0;
    for (long long k = kg - halfBand; k <= kg + halfBand; ++k)
        if (k >= minBin && k <= last) { e.band += p[static_cast<size_t>(k)]; ++nb; }
    const long long inner = halfBand + guard;          // bins skipped each side of the band
    for (long long k = kg - inner - neighbourWidth; k <= kg + inner + neighbourWidth; ++k) {
        if (k >= kg - inner && k <= kg + inner) continue;
        if (k >= minBin && k <= last) { e.neighbour += p[static_cast<size_t>(k)]; ++nn; }
    }
    if (nb > 0) e.band /= nb;
    if (nn > 0) e.neighbour /= nn;
    e.ratio = (e.neighbour > 0.0) ? e.band / e.neighbour : 0.0;
    return e;
}

// ---------------------------------------------------------------------------
// (3) Slanted-edge MTF
// ---------------------------------------------------------------------------

inline double NormalCdf(double z) { return 0.5 * std::erfc(-z / std::sqrt(2.0)); }

// Near-vertical edge, dark left / bright right, rotated by angleDeg about the
// image centre. sigmaPx > 0: Gaussian-blurred edge sampled at pixel centres
// (exact: the blurred step is the normal CDF of the perpendicular distance).
inline Image MakeSlantedEdge(int w, int h, double angleDeg, double sigmaPx,
                             double lo = 1000.0, double hi = 5000.0) {
    Image img(w, h);
    const double t = std::tan(angleDeg * kPi / 180.0);
    const double c = std::cos(angleDeg * kPi / 180.0);
    const double x0 = 0.5 * w, y0 = 0.5 * h;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const double d = ((x - x0) - (y - y0) * t) * c;
            double s;
            if (sigmaPx > 0.0) s = NormalCdf(d / sigmaPx);
            else s = (d > 0.0) ? 1.0 : (d < 0.0 ? 0.0 : 0.5);
            img.at(x, y) = lo + (hi - lo) * s;
        }
    return img;
}

struct EdgeLine { double intercept = 0.0, slope = 0.0; };   // x_edge(y) = intercept + slope*y

// Per row: centroid of |x-derivative| (between-pixel positions i+0.5), then a
// least-squares line through the row positions.
inline EdgeLine FitEdge(const Image& img) {
    double sy = 0, sx = 0, syy = 0, sxy = 0;
    int n = 0;
    for (int y = 0; y < img.height; ++y) {
        double gs = 0.0, gx = 0.0;
        for (int x = 0; x + 1 < img.width; ++x) {
            const double g = std::fabs(img.at(x + 1, y) - img.at(x, y));
            gs += g;
            gx += g * (x + 0.5);
        }
        if (gs <= 0.0) continue;
        const double pos = gx / gs;
        sy += y; sx += pos; syy += static_cast<double>(y) * y; sxy += y * pos; ++n;
    }
    EdgeLine e;
    const double den = n * syy - sy * sy;
    if (n > 1 && den != 0.0) {
        e.slope = (n * sxy - sy * sx) / den;
        e.intercept = (sx - e.slope * sy) / n;
    } else if (n > 0) {
        e.intercept = sx / n;
    }
    return e;
}

enum class TiltCorrection { On, Off };

struct MtfCurve {
    std::vector<double> freq;   // cycles/pixel
    std::vector<double> mtf;
    EdgeLine edge;
};

inline double Sinc(double u) { return (u == 0.0) ? 1.0 : std::sin(kPi * u) / (kPi * u); }

// tilt Off: the fitted line is replaced by a vertical line at its mean position
// -- the falsification case (no projection onto the edge normal).
inline MtfCurve SlantedEdgeMtf(const Image& img, TiltCorrection tilt, double binPx = 0.25,
                               double halfRangePx = 16.0, double fMax = 1.0, double fStep = 0.01) {
    MtfCurve out;
    EdgeLine e = FitEdge(img);
    out.edge = e;
    if (tilt == TiltCorrection::Off) {
        e.intercept = e.intercept + e.slope * 0.5 * (img.height - 1);
        e.slope = 0.0;
    }
    const double cosT = 1.0 / std::sqrt(1.0 + e.slope * e.slope);
    const size_t m = static_cast<size_t>(std::llround(2.0 * halfRangePx / binPx));
    std::vector<double> sum(m, 0.0), cnt(m, 0.0);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            // pixel values sit at integer x; the fitted positions are on the same axis
            const double d = (x - (e.intercept + e.slope * y)) * cosT;
            if (d <= -halfRangePx || d >= halfRangePx) continue;
            const size_t b = static_cast<size_t>((d + halfRangePx) / binPx);
            if (b < m) { sum[b] += img.at(x, y); cnt[b] += 1.0; }
        }
    std::vector<double> esf(m, 0.0);
    std::vector<bool> have(m, false);
    for (size_t i = 0; i < m; ++i)
        if (cnt[i] > 0.0) { esf[i] = sum[i] / cnt[i]; have[i] = true; }
    for (size_t i = 0; i < m; ++i) {                 // fill empty bins linearly
        if (have[i]) continue;
        size_t l = i, r = i;
        while (l > 0 && !have[l]) --l;
        while (r + 1 < m && !have[r]) ++r;
        if (have[l] && have[r] && r != l)
            esf[i] = esf[l] + (esf[r] - esf[l]) * static_cast<double>(i - l) / static_cast<double>(r - l);
        else esf[i] = have[l] ? esf[l] : esf[r];
    }
    std::vector<double> lsf(m, 0.0);
    for (size_t i = 1; i + 1 < m; ++i) lsf[i] = 0.5 * (esf[i + 1] - esf[i - 1]);

    double dc = 0.0;
    for (double v : lsf) dc += v;
    for (double f = 0.0; f <= fMax + 1e-12; f += fStep) {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < m; ++i) {
            const double xi = (static_cast<double>(i) + 0.5) * binPx - halfRangePx;
            re += lsf[i] * std::cos(2.0 * kPi * f * xi);
            im -= lsf[i] * std::sin(2.0 * kPi * f * xi);
        }
        const double raw = std::sqrt(re * re + im * im) / std::fabs(dc);
        // divide out bin averaging (width binPx) and the central difference (span 2*binPx)
        const double resp = Sinc(f * binPx) * Sinc(2.0 * f * binPx);
        out.freq.push_back(f);
        out.mtf.push_back(raw / resp);
    }
    return out;
}

// First frequency where MTF drops below 0.5 (linear interpolation);
// the last sampled frequency if it never does.
inline double Mtf50(const MtfCurve& c) {
    for (size_t i = 1; i < c.mtf.size(); ++i)
        if (c.mtf[i] < 0.5) {
            const double t = (c.mtf[i - 1] - 0.5) / (c.mtf[i - 1] - c.mtf[i]);
            return c.freq[i - 1] + t * (c.freq[i] - c.freq[i - 1]);
        }
    return c.freq.back();
}

inline double GaussianMtf(double sigmaPx, double f) {
    return std::exp(-2.0 * kPi * kPi * sigmaPx * sigmaPx * f * f);
}

}  // namespace gsvg_test
