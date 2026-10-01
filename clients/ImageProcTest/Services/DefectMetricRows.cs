using System;
using System.Collections.Generic;

namespace ImageProcTest
{
    internal sealed record DetectorMetricRow(string Metric, string Value, string Gate, string Status);

    /// <summary>
    /// The images either side of the defect stage: what it was given and what it returned. The protocol's
    /// <c>Y_no_defect_stage</c> is <see cref="Before"/> (offset and gain already applied, defect correction not).
    /// </summary>
    internal sealed record DefectStageImages(float[] Before, float[] After);

    /// <summary>
    /// The four defect (BPM) metric rows, computed from arrays only (no WPF, no files) so the integration tests link and
    /// run it. Definitions are the canonical protocol's §5.5 (docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md):
    ///
    /// <code>
    /// DefectRecall      = TP / max(TP + FN, 1)
    /// DefectFPR         = FP / max(FP + TN, 1)
    /// DefectResidualADU = mean(abs(Y(defect_pixels) - neighbor_model(defect_pixels)))
    /// GoodPixelDeltaP99 = percentile99(abs(Y(good_pixels) - Y_no_defect_stage(good_pixels)))
    /// </code>
    ///
    /// <para><b>neighbor_model</b> (the protocol names it without defining it; this is the definition used here): for a
    /// defect pixel, the mean of the oracle-GOOD pixels in its 3x3 window of <c>Y</c> (the pixel itself and any other
    /// defect pixel excluded). A defect pixel with no good pixel in its window is left out of the mean; if every defect
    /// pixel is left out the value is NaN. Y is the final output, so the residual is a difference inside one image and does
    /// not depend on the image's level (the old <c>mean(|Y|)</c> did, which is why no real data could meet its 2 ADU line).
    /// The preview is a sampled grid, so "neighbour" means the neighbour on that grid.</para>
    ///
    /// <para><b>Y_no_defect_stage</b> is the image handed to the defect stage, not the raw input: with offset and gain on,
    /// every good pixel differs from the raw input whatever the defect stage does, so comparing with the raw input measured
    /// the other stages. The delta is only defined when the defect stage ran.</para>
    /// </summary>
    internal static class DefectMetricRows
    {
        public static IReadOnlyList<DetectorMetricRow> Build(
            bool[] oracle,
            bool[]? predicted,
            float[] output,
            DefectStageImages? defectStage,
            int width,
            int height)
        {
            var hasPredicted = predicted is not null && predicted.Length == oracle.Length;

            var truePositive = 0;
            var falsePositive = 0;
            var falseNegative = 0;
            var trueNegative = 0;
            if (hasPredicted)
            {
                for (var i = 0; i < oracle.Length; i++)
                {
                    if (oracle[i] && predicted![i]) truePositive++;
                    else if (!oracle[i] && predicted![i]) falsePositive++;
                    else if (oracle[i]) falseNegative++;
                    else trueNegative++;
                }
            }

            var recall = hasPredicted ? Percent(truePositive, Math.Max(1, truePositive + falseNegative)) : double.NaN;
            var falsePositiveRate = hasPredicted ? Percent(falsePositive, Math.Max(1, falsePositive + trueNegative)) : double.NaN;
            var residual = Residual(oracle, output, width, height);

            var rows = new List<DetectorMetricRow>
            {
                hasPredicted
                    ? Row("DefectRecall", recall, "%", DefectMetricGates.RecallGate, DefectMetricGates.RecallPasses(recall))
                    : Unavailable("DefectRecall", "predicted BPM not selected"),
                hasPredicted
                    ? Row("DefectFPR", falsePositiveRate, "%", DefectMetricGates.FprGate, DefectMetricGates.FprPasses(falsePositiveRate))
                    : Unavailable("DefectFPR", "predicted BPM not selected"),
                Reported("DefectResidualADU", residual, "ADU", DefectMetricGates.ResidualGate)
            };

            if (defectStage is null || defectStage.Before.Length != oracle.Length || defectStage.After.Length != oracle.Length)
            {
                rows.Add(Unavailable("GoodPixelDeltaP99", "defect stage not executed"));
            }
            else
            {
                var p99 = GoodPixelDeltaP99(oracle, defectStage.Before, defectStage.After);
                rows.Add(Row("GoodPixelDeltaP99", p99, "ADU", DefectMetricGates.GoodPixelDeltaP99Gate, DefectMetricGates.GoodPixelDeltaP99Passes(p99)));
            }

            return rows;
        }

        public static double GoodPixelDeltaP99(bool[] oracle, float[] before, float[] after)
        {
            var deltas = new List<double>();
            for (var i = 0; i < oracle.Length; i++)
            {
                if (!oracle[i])
                {
                    deltas.Add(Math.Abs(after[i] - before[i]));
                }
            }

            return Percentile99(deltas);
        }

        public static double Residual(bool[] oracle, float[] image, int width, int height)
        {
            if (width <= 0 || height <= 0 || image.Length != oracle.Length || checked(width * height) != image.Length)
            {
                return double.NaN;
            }

            var sum = 0.0;
            var counted = 0;
            for (var y = 0; y < height; y++)
            {
                for (var x = 0; x < width; x++)
                {
                    var index = y * width + x;
                    if (!oracle[index])
                    {
                        continue;
                    }

                    var neighbourSum = 0.0;
                    var neighbours = 0;
                    for (var dy = -1; dy <= 1; dy++)
                    {
                        for (var dx = -1; dx <= 1; dx++)
                        {
                            var nx = x + dx;
                            var ny = y + dy;
                            if ((dx == 0 && dy == 0) || nx < 0 || ny < 0 || nx >= width || ny >= height)
                            {
                                continue;
                            }

                            var neighbour = ny * width + nx;
                            if (oracle[neighbour])
                            {
                                continue;
                            }

                            neighbourSum += image[neighbour];
                            neighbours++;
                        }
                    }

                    if (neighbours == 0)
                    {
                        continue;
                    }

                    sum += Math.Abs(image[index] - neighbourSum / neighbours);
                    counted++;
                }
            }

            return counted > 0 ? sum / counted : double.NaN;
        }

        private static DetectorMetricRow Row(string metric, double value, string unit, string gate, bool passed)
        {
            var formatted = double.IsFinite(value) ? $"{value:0.###} {unit}".TrimEnd() : "n/a";
            var status = double.IsFinite(value) ? passed ? "PASS" : "REVIEW" : "N/A";
            return new DetectorMetricRow(metric, formatted, gate, status);
        }

        /// <summary>A value shown with no pass line: never PASS or REVIEW.</summary>
        private static DetectorMetricRow Reported(string metric, double value, string unit, string gate)
        {
            var formatted = double.IsFinite(value) ? $"{value:0.###} {unit}".TrimEnd() : "n/a";
            return new DetectorMetricRow(metric, formatted, gate, double.IsFinite(value) ? "REPORTED" : "N/A");
        }

        private static DetectorMetricRow Unavailable(string metric, string reason) =>
            new DetectorMetricRow(metric, "not computed", reason, "N/A");

        private static double Percent(int numerator, int denominator) =>
            denominator <= 0 ? 0 : 100.0 * numerator / denominator;

        private static double Percentile99(List<double> values)
        {
            if (values.Count == 0)
            {
                return double.NaN;
            }

            values.Sort();
            var index = (int)Math.Ceiling(values.Count * 0.99) - 1;
            index = Math.Clamp(index, 0, values.Count - 1);
            return values[index];
        }
    }
}
