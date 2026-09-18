/**
 * @file virtual_grid.h
 * @brief Virtual grid: iterative thickness + scatter estimation from a
 *        parameter table, residual scatter of a chosen grid ratio, pyramid
 *        contrast and a simple de-noise (#180, QA-B-91).
 *
 * Every physical number comes from a ParamTable. There are no built-in
 * coefficients: a missing table, a missing section, or a request outside the
 * table's grid is an error and the caller leaves the image untouched
 * (REQ-GSVG-024).
 *
 * Sources for each stage (see #180, 3rd survey):
 *  - water-equivalent thickness from I/I0 with the slab curve
 *    mu(t) = W0 - a*t/(1+b*t), L = -ln(P/I0) = mu(t)*t  (US 7,907,697 B2)
 *  - scatter as a thickness-indexed, spatially variant kernel superposition
 *    S = sum_y K(x-y; T(y), kVp) P(y)  (US 7,907,697; Sun & Star-Lack 2010)
 *  - kernel = sum of Gaussians (Stankovic 2017, Bhatia 2017); the table form
 *    K(r) = sum_i a_i/(2 pi s_i^2) exp(-r^2 / 2 s_i^2), r in cm at the
 *    detector, normalised to the primary (tools/mcsim/README.md)
 *  - multiplicative update P <- I / (1 + SPR), SPR capped by the table
 *    (US 7,907,697)
 *  - scatter on a reduced grid, then interpolated up (Sakaltras 2023)
 *  - Laplacian pyramid contrast (US 8,064,676 B2)
 *
 * Internal to gsvg.dll; declared here so the module tests can drive the
 * falsification switches.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xpe_gsvg_detail {

// ---------------------------------------------------------------------------
// Parameter table
//
// One text file, four sections. Lines starting with '#' and blank lines are
// ignored. Each section starts with its [name] line followed by a CSV header;
// columns are found by name, so extra columns are allowed.
//
//   [kernels]   thickness_cm,kvp,model,a1,s1,a2,s2[,a3,s3,a4,s4]...
//               (the tools/mcsim/tables CSV rows can be pasted unchanged)
//   [wet]       kvp,w0,a,b          mu(t) = w0 - a*t/(1+b*t)   [1/cm], t in cm
//   [grid]      ratio,tp,ts[,freq_per_cm][,thickness_cm][,kvp]
//               primary / scatter transmission of the grid (QA-B-101). The
//               optional columns make the rows a table: one (ratio,
//               freq_per_cm) pair per grid design, each over a full
//               thickness x kVp grid. Without them there is one row per ratio.
//   [spr_cap]   thickness_cm,kvp,max_spr          (optional, see below)
//
// Kernels: gauss4 rows are used when the section has any, otherwise gauss2.
// The chosen model must cover a full thickness x kVp grid.
//
// SPR cap source: SprCapAt() reads [spr_cap] when present, otherwise the
// infinite-field SPR of the chosen kernel rows, sum(a_i), blended like the
// kernel itself (QA-B-93). How that value is applied is CapMode (QA-B-94): the
// default takes its LARGEST value at the exposure's kVp over the whole image.
// ---------------------------------------------------------------------------
struct KernelNode {
    int terms = 0;            // 2 or 4
    double a[4] = {0, 0, 0, 0};
    double s[4] = {0, 0, 0, 0};   // cm
};

struct ParamTable {
    std::vector<double> kThick, kKvp;          // kernel grid axes (ascending)
    std::vector<KernelNode> kernels;           // [iT * kKvp.size() + iK]

    std::vector<double> wetKvp;                // ascending
    std::vector<double> wetW0, wetA, wetB;

    // [grid] rows as read. A column the file does not have reads as 0 and its
    // flag is false (QA-B-101).
    struct GridRow { double ratio, freq, thick, kvp, tp, ts; };
    std::vector<GridRow> gridRows;
    bool gridHasFreq = false, gridHasThick = false, gridHasKvp = false;

    bool capFromKernels = false;               // no [spr_cap] section
    std::vector<double> capThick, capKvp;      // ascending (empty when capFromKernels)
    std::vector<double> capSpr;                // [iT * capKvp.size() + iK]
};

// The [grid] transmissions of one grid design at one kVp, over thickness
// (QA-B-101). thick is ascending; a single node means no thickness dependence.
struct GridAtKvp {
    std::vector<double> thick, tp, ts;
    // Ts/Tp at thickness t: linear between nodes, the end value outside them.
    double ResidualAt(double t) const;
};
// Picks the rows of (ratio, freqPerCm) and interpolates them to kvp.
// freqPerCm must be 0 when the table has no freq_per_cm column and one of the
// listed densities when it has. Returns an empty string or the reason.
std::string SelectGrid(const ParamTable& t, double ratio, double freqPerCm, double kvp,
                       GridAtKvp& out);

// Returns an empty string on success, otherwise the reason.
std::string ParseParamTable(const std::string& text, ParamTable& out);
std::string LoadParamTable(const std::string& path, ParamTable& out);

// ---------------------------------------------------------------------------
// Per-image settings. All required; there are no defaults.
// ---------------------------------------------------------------------------
struct VgSettings {
    double kvp = 0;
    double gridRatio = 0;
    double gridFreqPerCm = 0;   // line density; 0 when the table lists one design per ratio
    double pixelPitchMm = 0;
    double airSignal = 0;       // I0: detector signal without an object [DN]
    int    iterations = 0;      // >= 1
    // Post-steps. REQ-GSVG-013 asks for a 4..8 level Laplacian pyramid, so the
    // default runs one (QA-B-111, #180); 0 turns both post-steps off.
    int    pyramidLevels = 4;   // 0 = off, otherwise 4..8
    double pyramidGain = 1.3;   // detail gain (1 = unchanged)
    double denoiseK = 2.0;      // 0 = off; soft threshold k * sigma on the finest band
};

// SPR / over-correction guard (QA-B-94 compares these on synthetic scenes).
enum class CapMode {
    // Default: GlobalSum (QA-B-94). On synthetic scenes it was the only table-
    // derived guard that left correct-kernel results unchanged (4 scenes) and
    // removed negative / near-zero primaries with x2, x3 kernels. LocalSum
    // bound on correct data at thickness edges and on under-estimated tables;
    // the fixed floors need an eps that the synthetic scenes cannot justify.
    None,          // C0: multiplicative update only
    LocalSum,      // C1: SprCapAt(local thickness) -- [spr_cap] or local sum(a_i)
    GlobalSum,     // C2: largest SprCapAt over the table's thickness nodes at this kVp
    PrimaryFloor,  // C3: P >= eps * I, i.e. SPR <= 1/eps - 1
    SmoothFloor,   // C4: S(x) <= (1 - eps) * min of I over a window of one reduced-grid
                   //     block around x (scatter is smooth and never exceeds I)
};

// What the post-steps (pyramid contrast, de-noise) see where the field mask
// excludes a pixel (#189, QA-B-108). The pyramid reads neighbours, so this
// choice reaches pixels INSIDE the mask near its boundary.
enum class MaskOutside {
    Zero,       // current behaviour: excluded pixels are 0 during the post-steps
    Replicate,  // clamp to the mask's bounding rectangle (nearest in-field value)
    Keep,       // leave the corrected values there (no zeroing)
    RectOnly,   // run the post-steps on the mask's bounding rectangle only
};

// Test switches for the falsification cases. Production uses the defaults.
struct VgSwitches {
    bool thicknessIndex = true;   // false: one global thickness (image mean)
    CapMode cap = CapMode::GlobalSum;
    double capEps = 0.0;          // eps for PrimaryFloor / SmoothFloor
    int reductionFactor = 0;      // 0: derived from the narrowest kernel term (QA-B-95: fixed to compare models)
    bool clampThickness = true;   // false: thickness above the table refuses the image (pre-QA-B-93)
    bool useFieldMask = true;     // false: a passed mask is ignored (QA-B-96 falsification)
    // #189 (QA-B-110): edge replicate is the default. It leaves the smallest
    // artificial step at the field boundary (0.3598 against the post-step-free
    // 0.3210; zero measured 0.5670, QA-B-108) and matches the physical fact
    // that outside the field the signal is low scatter, not zero. The other
    // choices stay available as settings.
    MaskOutside maskOutside = MaskOutside::Replicate;
};

struct VgReport {
    std::string error;            // empty on success
    int coarseW = 0, coarseH = 0, factor = 0;
    double maxThicknessCm = 0;
    double meanSpr = 0;           // coarse, after the last iteration
    // Fractions of reduced-grid pixels in the last iteration (QA-B-93):
    double clampedHighFraction = 0;  // thickness above the table, limited to its maximum
    double belowTableFraction = 0;   // 0 < thickness < first kernel node (kernel faded to 0 at t = 0)
    double cappedFraction = 0;       // SPR limited by the cap
    // Share of full-resolution pixels (I > 0) whose final primary estimate
    // lies above the table's thickness range. This is the pixel share of
    // over-range material; clampedHighFraction undercounts it at object edges,
    // where a reduced-grid block averages the dense region with its surroundings.
    double aboveTableFullRes = 0;
    // Full-res pixels with I - S < 0 before clamping to 0. With the cap on this
    // cannot happen: S <= I*cap/(1+cap) gives I - S >= I/(1+cap) >= 0.
    size_t negativePrimary = 0;
    size_t clippedHigh = 0;          // full-res output pixels above 65535 before clamping
};

// Scatter kernel at (thickness, kVp), bilinear in the table's node kernels
// (kernels are blended, not their coefficients). thickness below the first
// node blends towards zero scatter at t = 0; above the last node -> false.
struct BlendedKernel {
    std::vector<double> a, s;   // one entry per Gaussian term
};
bool KernelAt(const ParamTable& t, double thicknessCm, double kvp, BlendedKernel& out);

// SPR cap at (thickness, kVp): [spr_cap] bilinear (thickness clamped to its
// range), or sum(a_i) of KernelAt when the section is absent. < 0 when kVp
// lies outside the range that supplies the cap.
double SprCapAt(const ParamTable& t, double thicknessCm, double kvp);

// Minimum over the (2r+1)^2 square around each pixel, edges clamped.
std::vector<double> MinFilter2D(const std::vector<double>& img, int w, int h, int r);

// Water-equivalent thickness [cm] for L = -ln(P/I0). Returns < 0 when the
// solution lies above tMax.
double ThicknessFromLogAtten(double L, double w0, double a, double b, double tMax);

// Runs the full chain on img (width*height DN values, in place).
// On error img is left untouched and report.error says why.
//
// fieldMask (QA-B-96): width*height bytes, non-zero = inside the collimated
// field, or nullptr. Pixels outside the field are not scatter sources (they
// enter the estimate as 0) and are returned unchanged.
VgReport RunVirtualGrid(std::vector<double>& img, int width, int height,
                        const ParamTable& table, const VgSettings& settings,
                        const VgSwitches& sw = VgSwitches{},
                        const uint8_t* fieldMask = nullptr);

// Primary + scatter forward model used by the tests' synthetic images:
// returns P + (P * K[T]) on the full-resolution grid for a given thickness map.
// Same kernel evaluation as RunVirtualGrid, without reduction.
std::vector<double> ForwardScatter(const std::vector<double>& primary,
                                   const std::vector<double>& thicknessCm,
                                   int width, int height, const ParamTable& table,
                                   double kvp, double pixelPitchMm);

// Gaussian-sum convolution of src with the thickness-indexed kernel, on a
// grid of the given pitch. Out-of-image primary is zero (collimated field).
// Returns scatter per pixel (same units as src). Empty on out-of-table input.
std::vector<double> ScatterEstimate(const std::vector<double>& primary,
                                    const std::vector<double>& thicknessCm,
                                    int width, int height, const ParamTable& table,
                                    double kvp, double pitchCm);

}  // namespace xpe_gsvg_detail
