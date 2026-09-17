/**
 * @file grid_dwt.h
 * @brief Grid-line suppression by recursive 2D DWT, sub-band detection and a
 *        Gaussian band-stop on the detected sub-band only (#180, QA-B-90).
 *
 * Method: Tang et al. 2015, Med Phys 42(4):1721-1729 -- decompose recursively,
 * stop at the level whose detail sub-band carries the grid, band-stop that
 * sub-band only, reconstruct. Parameters start from the repository design
 * documents: db4 (GSVG-SAD-001:128), max levels log2(min(M,N)) - 4 (SAD:129),
 * 3-sigma sub-band threshold (SAD:130, GSVG-SDD-001:175), H = 1 - exp(-d^2 /
 * 2 sigma_f^2) with sigma_f = 1.5 bins (SDD:301-305).
 *
 * Internal to gsvg.dll. Declared here so the module's tests can drive the
 * switches the falsification cases need.
 */
#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xpe_gsvg_detail {

// ---------------------------------------------------------------------------
// FFT for any length. n = q * 2^k: the power-of-two part is an iterative
// radix-2 transform; an odd factor q > 1 is handled by splitting the signal
// into q decimated sub-sequences (O(n*q) to recombine). No external library.
// ---------------------------------------------------------------------------
class Fft {
public:
    explicit Fft(size_t n);
    size_t size() const { return n_; }
    void forward(std::vector<std::complex<double>>& x) const;   // X[k] = sum x[t] e^{-2 pi i k t / n}
    void inverse(std::vector<std::complex<double>>& x) const;   // exact inverse of forward()

private:
    void pow2(std::complex<double>* x, size_t m, bool inv) const;
    size_t n_ = 0, q_ = 1, m_ = 1;          // n = q * m, m a power of two
    std::vector<size_t> rev_;               // bit reversal for m
    std::vector<std::complex<double>> roots_;   // e^{-2 pi i j / m}
    std::vector<std::complex<double>> phase_;   // e^{-2 pi i t / n}, t < n (odd-factor recombination)
};

// ---------------------------------------------------------------------------
// db4 periodic 2D DWT (orthonormal, 8 taps). Odd sizes are padded by
// repeating the last row/column; the reconstruction crops them off again.
// ---------------------------------------------------------------------------
struct Level {
    int w = 0, h = 0;          // size of the image this level decomposed (even)
    int srcW = 0, srcH = 0;    // size before even-padding
    std::vector<double> ll, lh, hl, hh;   // each (w/2) x (h/2)
    // lh: low along x, high along y  -> horizontal lines (rows axis)
    // hl: high along x, low along y  -> vertical lines (columns axis)
};

Level Dwt2(const std::vector<double>& img, int w, int h);
std::vector<double> Idwt2(const Level& lv);   // returns srcW x srcH

enum class Axis { Rows, Columns };

// ---------------------------------------------------------------------------
// Grid detection on the input image
// ---------------------------------------------------------------------------
struct InputPeak {
    bool detected = false;
    double freq = 0.0;         // cycles/pixel, folded into [0, 0.5]
    double prominence = 0.0;   // peak power / larger of the two neighbour medians
    double amplitude = 0.0;    // estimated sinusoid amplitude of the line profile, DN
};
// Threshold on prominence, and the smallest profile amplitude acted on.
// 100 was the first value: grid-free anatomy (QA-B-90 fixtures) reached 507
// with no noise, while a d = 0.05 grid reaches >= 4e6.
constexpr double kInputProminence = 1e4;
constexpr double kMinAmplitudeDn = 0.5;

InputPeak DetectInputGrid(const std::vector<double>& img, int w, int h, Axis axis);

// Where a grid at input frequency f ends up: the first level (1-based) whose
// detail sub-band holds it, and its frequency there (cycles/sample). level 0
// means it does not reach a detail band within maxLevels.
struct Placement { int level = 0; double subFreq = 0.0; };
Placement PlaceInSubbands(double f, int maxLevels);

// ---------------------------------------------------------------------------
// Sub-band detection (3 sigma) and band-stop
// ---------------------------------------------------------------------------
struct SubbandCheck {
    bool detected = false;
    double peakBin = 0.0;      // refined bin of the peak in the sub-band profile spectrum
    double peakPower = 0.0;
    double threshold = 0.0;    // mean + 3 * std of that spectrum
};
constexpr double kSubbandSigma = 3.0;     // SDD:175 energyThresholdSigma
constexpr int kSubbandGateBins = 3;       // peak must sit this close to the predicted bin

SubbandCheck CheckSubband(const std::vector<double>& band, int bw, int bh, Axis axis,
                          double predictedFreq);

constexpr double kBandStopSigmaBins = 1.5;    // SDD:305

// Gaussian band-stop at +-peakBin (bins of an FFT the length of the lines
// across the grid), applied to every line of the sub-band.
void BandStop(std::vector<double>& band, int bw, int bh, Axis axis, double peakBin,
              double sigmaBins = kBandStopSigmaBins);

// ---------------------------------------------------------------------------
// Whole pipeline
// ---------------------------------------------------------------------------
// One band-stop applied: which level, which axis (-> which detail band), where.
struct Decision {
    int level = 0;
    Axis axis = Axis::Rows;
    double peakBin = 0.0;
};

struct Report;

struct Options {
    bool bandStop = true;    // false: detect and decompose, but filter nothing
    // true: past the level holding the grid, the first level whose detail band
    // no longer shows it ends the descent. false: every level to maxLevels.
    bool autoStop = true;
    // false: skip the input-spectrum gate, so every level is decomposed and
    // every sub-band is checked (the gate is what keeps a grid-free image untouched)
    bool inputGate = true;
    // true: work on log(pixel). A grid multiplies the image; in log it adds,
    // so it is a line in the spectrum instead of a line modulated by the anatomy.
    bool logDomain = false;
    // Gaussian width of the band-stop. Wider suppresses more and blurs more
    // (QA-B-90 sweep, gate.md); the default is the design document value.
    double sigmaBins = kBandStopSigmaBins;
    // non-null: skip detection and apply exactly these decisions (tests)
    const Report* replay = nullptr;
};

struct AxisReport {
    InputPeak input;
    Placement place;
    int filteredLevels = 0;              // number of bands band-stopped on this axis
    std::vector<SubbandCheck> checks;    // detail band, one per level visited
};

struct Report {
    int maxLevels = 0;
    int levelsUsed = 0;
    AxisReport rows, cols;
    std::vector<Decision> decisions;
};

int MaxLevels(int w, int h);   // floor(log2(min(w,h))) - 4, at least 0

// Suppresses grid lines in place. Pixels are rounded and clamped to 0..65535.
Report SuppressGrid(uint16_t* pixels, int w, int h, const Options& opt = {});

}  // namespace xpe_gsvg_detail
