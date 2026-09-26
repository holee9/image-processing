/**
 * @file hough_transform.cpp
 * @brief Hough transform implementation for line detection
 */

#include "hough_transform.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace xpe {
namespace enhance_advanced {
namespace detail {

HoughTransform::HoughTransform(float thetaStep, float rhoStep)
    : thetaStep_(thetaStep * static_cast<float>(M_PI) / 180.0f)  // Convert degrees to radians
    , rhoStep_(rhoStep)
    , thetaBins_(static_cast<int>(180.0f / thetaStep))
    , maxRho_(0) {
}

Eigen::MatrixXi HoughTransform::buildAccumulator(const Eigen::MatrixXf& edgeMagnitude) {
    const int rows = static_cast<int>(edgeMagnitude.rows());
    const int cols = static_cast<int>(edgeMagnitude.cols());

    // Calculate maximum rho (diagonal of image) — cast sqrt (double) back to int via static_cast
    const double diag = std::sqrt(static_cast<double>(rows) * rows
                                + static_cast<double>(cols) * cols);
    maxRho_ = static_cast<int>(diag / rhoStep_) + 1;

    // Initialize accumulator: [theta bins, rho bins]
    Eigen::MatrixXi accumulator(thetaBins_, 2 * maxRho_);
    accumulator.setZero();

    // Build accumulator by voting
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            float magnitude = edgeMagnitude(y, x);

            // Skip weak edges (lowered threshold for better detection)
            if (magnitude < HoughParams::kEdgeMagnitudeThreshold) {
                continue;
            }

            // Vote for all theta bins
            for (int t = 0; t < thetaBins_; ++t) {
                const float theta = static_cast<float>(t) * thetaStep_;
                const float rho = static_cast<float>(x) * std::cos(theta)
                                + static_cast<float>(y) * std::sin(theta);

                // Convert rho to bin index (offset by maxRho for negative values).
                // #183 (QA-B-98): ROUND, not truncate. At theta = pi/2 in float,
                // cos(theta) is about -4.4e-8, so row y votes rho = y - tiny;
                // truncation filed row 30 under bin 29 and put the top side one
                // line outside, while theta = 0 (cos exactly 1) was unaffected.
                // Truncation toward zero is also asymmetric for negative rho.
                int rhoBin = static_cast<int>(std::lround(rho / rhoStep_)) + maxRho_;

                // Clamp to valid range
                rhoBin = std::max(0, std::min(rhoBin, 2 * maxRho_ - 1));

                // Add vote
                accumulator(t, rhoBin) += static_cast<int>(magnitude);
            }
        }
    }

    return accumulator;
}

std::vector<HoughLine> HoughTransform::detectAxisAlignedLines(
    const Eigen::MatrixXi& accumulator,
    size_t numLines) {

    // Find peaks with higher threshold to reduce noise detection
    // Use adaptive threshold based on accumulator max value for robustness
    int maxAccumulatorValue = accumulator.maxCoeff();
    int adaptiveThreshold = std::max(HoughParams::kBaseAdaptiveThreshold,
                                      static_cast<int>(maxAccumulatorValue * HoughParams::kAdaptiveThresholdFactor));
    std::vector<HoughLine> allPeaks = findPeaks(accumulator, adaptiveThreshold, HoughParams::kDefaultWindowSize);

    // Filter for axis-aligned lines (horizontal: theta ~ 0 or 180, vertical: theta ~ 90)
    std::vector<HoughLine> axisAlignedPeaks;
    for (const auto& line : allPeaks) {
        if (isAxisAligned(line.theta)) {
            axisAlignedPeaks.push_back(line);
        }
    }

    // Sort by strength (descending)
    std::sort(axisAlignedPeaks.begin(), axisAlignedPeaks.end(),
        [](const HoughLine& a, const HoughLine& b) {
            return a.strength > b.strength;
        });

    // Return top N lines
    size_t count = std::min(numLines, axisAlignedPeaks.size());
    return std::vector<HoughLine>(axisAlignedPeaks.begin(), axisAlignedPeaks.begin() + count);
}

namespace {

// A candidate side: its pixel position (x for a vertical line, y for a
// horizontal one) and its Hough strength.
struct SideLine { float pos; float strength; };

// Line -> pixel position on its axis, taken where the line crosses the image
// centre: x at row `centre` for a vertical line (theta near 0/180), y at
// column `centre` for a horizontal one. A slightly tilted copy of a side
// (theta 2 or 178 deg) passes through the same centre point, so it lands on
// the same position; measured at row 0 it would sit centre*tan(2 deg) away
// (4.5 px at centre 128), outside the merge window.
float LinePosition(const HoughLine& line, bool vertical, float centre) {
    const float c = std::cos(line.theta), s = std::sin(line.theta);
    if (vertical)
        return std::abs(c) < HoughParams::kTrigEps ? line.rho * c : (line.rho - centre * s) / c;
    return std::abs(s) < HoughParams::kTrigEps ? line.rho * s : (line.rho - centre * c) / s;
}

// D (QA-B-100): one line per position. A line at theta = 178 deg and one at
// theta = 0 describe the same x when their positions agree; the accumulator's
// NMS does not see them as neighbours (theta 0 and 179 sit at opposite ends).
// Lines whose positions lie within the NMS half-window are merged here,
// keeping the strongest, whatever their angles.
std::vector<SideLine> MergeByPosition(const std::vector<HoughLine>& lines, bool vertical,
                                      float centre) {
    std::vector<SideLine> in;
    for (const auto& l : lines) in.push_back({LinePosition(l, vertical, centre), l.strength});
    std::sort(in.begin(), in.end(), [](const SideLine& a, const SideLine& b) {
        if (a.strength != b.strength) return a.strength > b.strength;
        return a.pos < b.pos;
    });
    const float half = static_cast<float>(HoughParams::kDefaultWindowSize / 2);
    std::vector<SideLine> out;
    for (const auto& l : in) {
        bool dup = false;
        for (const auto& k : out)
            if (std::abs(k.pos - l.pos) <= half) { dup = true; break; }
        if (!dup) out.push_back(l);
    }
    return out;   // strongest first
}

// Candidate boundary pixels of one side line. The Sobel response of a step
// is split over the two pixels either side of it, so the boundary pixel is
// one of round(pos) - 1, round(pos), round(pos) + 1.
void AddBoundaryCandidates(float pos, int limit, std::vector<int>& out) {
    const int p = static_cast<int>(std::lround(pos));
    for (int d = -1; d <= 1; ++d) {
        const int v = p + d;
        if (v >= 0 && v < limit) out.push_back(v);
    }
}

// Sums over the blocks between consecutive cut positions; a rectangle whose
// sides lie on cuts is a sum of whole blocks (no full integral image: 3072^2
// doubles would be 76 MB).
// Smallest positive unit of the input (detector DN): the log-domain floor.
constexpr float kSignalFloorDn = 1.0f;

struct BlockSums {
    std::vector<int> xCuts, yCuts;          // ascending, first 0, last size
    std::vector<double> sum;                // 2D prefix over blocks, (nx+1)(ny+1)
    std::vector<double> cnt;
    int nx = 0, ny = 0;

    BlockSums(const float* img, int w, int h, std::vector<int> xc, std::vector<int> yc) {
        xc.push_back(0); xc.push_back(w);
        yc.push_back(0); yc.push_back(h);
        std::sort(xc.begin(), xc.end()); xc.erase(std::unique(xc.begin(), xc.end()), xc.end());
        std::sort(yc.begin(), yc.end()); yc.erase(std::unique(yc.begin(), yc.end()), yc.end());
        xCuts = xc; yCuts = yc;
        nx = static_cast<int>(xc.size()) - 1;
        ny = static_cast<int>(yc.size()) - 1;
        std::vector<int> colBlock(static_cast<size_t>(w)), rowBlock(static_cast<size_t>(h));
        for (int b = 0; b < nx; ++b)
            for (int x = xc[static_cast<size_t>(b)]; x < xc[static_cast<size_t>(b) + 1]; ++x) colBlock[static_cast<size_t>(x)] = b;
        for (int b = 0; b < ny; ++b)
            for (int y = yc[static_cast<size_t>(b)]; y < yc[static_cast<size_t>(b) + 1]; ++y) rowBlock[static_cast<size_t>(y)] = b;
        // Sums are of log(pixel): see the contrast note in the caller. The input
        // is detector signal in DN carried as float; its smallest positive unit
        // is 1 DN, so a pixel below 1 DN holds no measurable signal and is read
        // as 1 DN (log = 0) instead of having no logarithm.
        std::vector<double> block(static_cast<size_t>(nx) * static_cast<size_t>(ny), 0.0);
        for (int y = 0; y < h; ++y) {
            const size_t rowOff = static_cast<size_t>(rowBlock[static_cast<size_t>(y)]) * static_cast<size_t>(nx);
            const float* row = img + static_cast<size_t>(y) * static_cast<size_t>(w);
            for (int x = 0; x < w; ++x) {
                block[rowOff + static_cast<size_t>(colBlock[static_cast<size_t>(x)])] +=
                    row[x] > kSignalFloorDn ? std::log(static_cast<double>(row[x])) : 0.0;
            }
        }
        const size_t W = static_cast<size_t>(nx) + 1;
        sum.assign(W * (static_cast<size_t>(ny) + 1), 0.0);
        cnt.assign(sum.size(), 0.0);
        for (int by = 0; by < ny; ++by)
            for (int bx = 0; bx < nx; ++bx) {
                const double area = static_cast<double>(xc[static_cast<size_t>(bx) + 1] - xc[static_cast<size_t>(bx)]) *
                                    static_cast<double>(yc[static_cast<size_t>(by) + 1] - yc[static_cast<size_t>(by)]);
                const size_t i = (static_cast<size_t>(by) + 1) * W + static_cast<size_t>(bx) + 1;
                sum[i] = block[static_cast<size_t>(by) * static_cast<size_t>(nx) + static_cast<size_t>(bx)]
                       + sum[i - 1] + sum[i - W] - sum[i - W - 1];
                cnt[i] = area + cnt[i - 1] + cnt[i - W] - cnt[i - W - 1];
            }
    }

    size_t xIndex(int cut) const {
        return static_cast<size_t>(std::lower_bound(xCuts.begin(), xCuts.end(), cut) - xCuts.begin());
    }
    size_t yIndex(int cut) const {
        return static_cast<size_t>(std::lower_bound(yCuts.begin(), yCuts.end(), cut) - yCuts.begin());
    }
    // Inclusive pixel rectangle [x0, x1] x [y0, y1]; its edges are cuts.
    void rect(int x0, int y0, int x1, int y1, double& s, double& n) const {
        const size_t W = static_cast<size_t>(nx) + 1;
        const size_t a = xIndex(x0), b = xIndex(x1 + 1), c = yIndex(y0), d = yIndex(y1 + 1);
        s = sum[d * W + b] - sum[c * W + b] - sum[d * W + a] + sum[c * W + a];
        n = cnt[d * W + b] - cnt[c * W + b] - cnt[d * W + a] + cnt[c * W + a];
    }
};

// Computation bound, not a detection threshold: the strongest lines kept per
// orientation. With 3 boundary pixels each, at most (3*16)^2/2 x (3*16)^2/2
// ~ 1.3e6 rectangles are scored, each in O(1).
constexpr size_t kMaxCandidateLinesPerOrientation = 16;

}  // namespace

CollimationRectangle HoughTransform::extractCollimationRectangle(
    const std::vector<HoughLine>& horizontalLines,
    const std::vector<HoughLine>& verticalLines,
    int imageWidth,
    int imageHeight,
    const float* image,
    RectangleChoice choice) {

    // @MX:NOTE: [AUTO] Collimation rectangle extraction -- REQ-ADV-041, REQ-ADV-052
    // @MX:REASON: #183 (QA-B-100): the rectangle is chosen by inside/outside
    //   contrast among merged candidate lines; see RectangleChoice.

    CollimationRectangle result;
    result.x0 = 0;
    result.y0 = 0;
    result.x1 = imageWidth - 1;
    result.y1 = imageHeight - 1;
    result.confidence = 0.0f;

    std::vector<SideLine> hs = MergeByPosition(horizontalLines, false, 0.5f * static_cast<float>(imageWidth));
    std::vector<SideLine> vs = MergeByPosition(verticalLines, true, 0.5f * static_cast<float>(imageHeight));

    // If insufficient lines detected, return full extent with low confidence
    if (hs.size() < 2 || vs.size() < 2) return result;

    const SideLine* ht = nullptr; const SideLine* hb = nullptr;
    const SideLine* vl = nullptr; const SideLine* vr = nullptr;

    if (choice == RectangleChoice::StrengthSum || image == nullptr) {
        // The rule before QA-B-100: the two strongest lines per orientation.
        const SideLine* a = &hs[0]; const SideLine* b = &hs[1];
        const SideLine* c = &vs[0]; const SideLine* d = &vs[1];
        if (a->pos > b->pos) std::swap(a, b);
        if (c->pos > d->pos) std::swap(c, d);
        result.y0 = std::max(0, static_cast<int>(std::lround(a->pos)));
        result.y1 = std::min(imageHeight - 1, static_cast<int>(std::lround(b->pos)));
        result.x0 = std::max(0, static_cast<int>(std::lround(c->pos)));
        result.x1 = std::min(imageWidth - 1, static_cast<int>(std::lround(d->pos)));
        ht = a; hb = b; vl = c; vr = d;
    } else {
        if (hs.size() > kMaxCandidateLinesPerOrientation) hs.resize(kMaxCandidateLinesPerOrientation);
        if (vs.size() > kMaxCandidateLinesPerOrientation) vs.resize(kMaxCandidateLinesPerOrientation);

        // Boundary candidates, each remembering the line it came from.
        struct Cand { int pixel; size_t line; };
        std::vector<Cand> xc, yc;
        for (size_t i = 0; i < vs.size(); ++i) {
            std::vector<int> px;
            AddBoundaryCandidates(vs[i].pos, imageWidth, px);
            for (int p : px) xc.push_back({p, i});
        }
        for (size_t i = 0; i < hs.size(); ++i) {
            std::vector<int> px;
            AddBoundaryCandidates(hs[i].pos, imageHeight, px);
            for (int p : px) yc.push_back({p, i});
        }
        std::vector<int> xCuts, yCuts;
        for (const auto& k : xc) { xCuts.push_back(k.pixel); xCuts.push_back(k.pixel + 1); }
        for (const auto& k : yc) { yCuts.push_back(k.pixel); yCuts.push_back(k.pixel + 1); }
        const BlockSums bs(image, imageWidth, imageHeight, xCuts, yCuts);
        double totalSum, totalCnt;
        bs.rect(0, 0, imageWidth - 1, imageHeight - 1, totalSum, totalCnt);

        double best = -std::numeric_limits<double>::infinity();
        bool found = false;
        for (const auto& l : xc)
            for (const auto& r : xc) {
                if (l.line == r.line || l.pixel >= r.pixel) continue;
                for (const auto& t : yc)
                    for (const auto& b : yc) {
                        if (t.line == b.line || t.pixel >= b.pixel) continue;
                        double s, n;
                        bs.rect(l.pixel, t.pixel, r.pixel, b.pixel, s, n);
                        const double nOut = totalCnt - n;
                        if (n <= 0 || nOut <= 0) continue;
                        // X-ray transmission is multiplicative (Beer-Lambert), so field
                        // contrast is a difference in the log domain: mean(log inside) -
                        // mean(log outside). A linear difference prefers the brightest
                        // sub-region of a non-uniform field (QA-B-100 step scene: x1 = 127);
                        // a ratio of linear means prefers swallowing the scatter halo
                        // (uniform scene: 25..231 over 40..215). See gate.md.
                        const double score = s / n - (totalSum - s) / nOut;
                        if (score > best) {
                            best = score;
                            found = true;
                            result.x0 = l.pixel; result.x1 = r.pixel;
                            result.y0 = t.pixel; result.y1 = b.pixel;
                            vl = &vs[l.line]; vr = &vs[r.line];
                            ht = &hs[t.line]; hb = &hs[b.line];
                        }
                    }
            }
        if (!found) return result;
    }

    // Calculate confidence score from the four chosen lines.
    // REQ-ADV-041: Confidence = sum(4 peak values) / (4 * max_accumulator_value)
    const float totalStrength = ht->strength + hb->strength + vl->strength + vr->strength;
    const float maxExpectedStrength = 4.0f * HoughParams::kMaxExpectedStrengthPerLine;
    if (totalStrength < HoughParams::kMinExpectedStrength) {
        result.confidence = 0.0f;
    } else {
        result.confidence = std::min(1.0f, std::max(0.0f, totalStrength / maxExpectedStrength));
    }
    return result;
}

std::vector<HoughLine> HoughTransform::findPeaks(
    const Eigen::MatrixXi& accumulator,
    int threshold,
    int windowSize) {

    std::vector<HoughLine> peaks;

    const int rows = static_cast<int>(accumulator.rows());
    const int cols = static_cast<int>(accumulator.cols());

    // Non-maximum suppression
    for (int t = 0; t < rows; ++t) {
        for (int r = 0; r < cols; ++r) {
            int value = accumulator(t, r);

            if (value < threshold) {
                continue;
            }

            // Check if this is a local maximum
            bool isMax = true;
            for (int dt = -windowSize / 2; dt <= windowSize / 2 && isMax; ++dt) {
                for (int dr = -windowSize / 2; dr <= windowSize / 2 && isMax; ++dr) {
                    int nt = t + dt;
                    int nr = r + dr;

                    if (nt >= 0 && nt < rows && nr >= 0 && nr < cols) {
                        if (accumulator(nt, nr) > value) {
                            isMax = false;
                        }
                    }
                }
            }

            if (isMax) {
                const float theta = static_cast<float>(t) * thetaStep_;
                const float rho = static_cast<float>(r - maxRho_) * rhoStep_;
                peaks.emplace_back(theta, rho, static_cast<float>(value));
            }
        }
    }

    return peaks;
}

bool HoughTransform::isAxisAligned(float theta) const {
    // Normalize theta to [0, 180) degrees
    float degrees = theta * 180.0f / static_cast<float>(M_PI);
    while (degrees < 0.0f) degrees += 180.0f;
    while (degrees >= 180.0f) degrees -= 180.0f;

    // Check if within +-5 degrees of horizontal (0 or 180) or vertical (90)
    const float tolerance = HoughParams::kAxisAlignmentTolerance;

    bool isHorizontal = (degrees <= tolerance) || (degrees >= 180.0f - tolerance);
    bool isVertical = (std::abs(degrees - 90.0f) <= tolerance);

    return isHorizontal || isVertical;
}

} // namespace detail
} // namespace enhance_advanced
} // namespace xpe
