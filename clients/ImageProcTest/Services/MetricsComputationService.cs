using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal sealed record DetectorMetricRow(string Metric, string Value, string Gate, string Status);

    internal sealed record DetectorDomainMetrics(
        IReadOnlyList<DetectorMetricRow> DarkMetrics,
        IReadOnlyList<DetectorMetricRow> FlatMetrics,
        IReadOnlyList<DetectorMetricRow> DefectMetrics);

    internal static class MetricsComputationService
    {
        public static DetectorDomainMetrics Empty(string reason)
        {
            return new DetectorDomainMetrics(
                [Unavailable("DarkBias", reason), Unavailable("DSNU_ADU", reason), Unavailable("DarkReduction_dB", reason), Unavailable("ClampRate", reason)],
                [Unavailable("PRNU_CV", reason), Unavailable("FlatResidualPct", reason), Unavailable("FPN_Reduction_dB", reason), Unavailable("LineArtifactScore", reason)],
                [Unavailable("DefectRecall", reason), Unavailable("DefectFPR", reason), Unavailable("DefectResidualADU", reason), Unavailable("GoodPixelDeltaP99", reason)]);
        }

        public static DetectorDomainMetrics Compute(
            RawPreviewResult preview,
            ReadOnlySpan<float> output,
            FixtureCaseInfo? fixtureCase,
            IReadOnlyList<NativePreviewStageResult> stages)
        {
            if (preview.SampledPixels.Length != output.Length)
            {
                throw new ArgumentException("Input and output buffers must have the same sampled pixel count.");
            }

            var offsetExecuted = StageExecuted(stages, "offset");
            var gainExecuted = StageExecuted(stages, "gain");
            var defectExecuted = StageExecuted(stages, "defect");

            return new DetectorDomainMetrics(
                offsetExecuted
                    ? ComputeDarkMetrics(preview.SampledPixels, output)
                    : Empty("offset stage not executed").DarkMetrics,
                gainExecuted
                    ? ComputeFlatMetrics(preview.SampledPixels, output, preview.PreviewWidth, preview.PreviewHeight)
                    : Empty("gain stage not executed").FlatMetrics,
                defectExecuted || HasRole(fixtureCase, CalibrationRole.DefectOracle)
                    ? ComputeDefectMetrics(preview, output, fixtureCase)
                    : Empty("defect stage/oracle not available").DefectMetrics);
        }

        private static IReadOnlyList<DetectorMetricRow> ComputeDarkMetrics(
            ReadOnlySpan<ushort> original,
            ReadOnlySpan<float> output)
        {
            var rawStats = Stats(original);
            var correctedStats = Stats(output);
            var darkReduction = RatioDb(rawStats.StandardDeviation, correctedStats.StandardDeviation);
            var clampRate = Percent(CountClamped(output), output.Length);

            return
            [
                Row("DarkBias", correctedStats.Mean, "ADU", "<= 5 ADU", correctedStats.Mean <= 5.0),
                Row("DSNU_ADU", correctedStats.StandardDeviation, "ADU", "<= 20 ADU", correctedStats.StandardDeviation <= 20.0),
                Row("DarkReduction_dB", darkReduction, "dB", ">= 10 dB", darkReduction >= 10.0),
                Row("ClampRate", clampRate, "%", "<= 1%", clampRate <= 1.0)
            ];
        }

        private static IReadOnlyList<DetectorMetricRow> ComputeFlatMetrics(
            ReadOnlySpan<ushort> original,
            ReadOnlySpan<float> output,
            int width,
            int height)
        {
            var rawStats = Stats(original);
            var correctedStats = Stats(output);
            var prnuBefore = CoefficientOfVariationPct(rawStats);
            var prnuAfter = CoefficientOfVariationPct(correctedStats);
            var fpnReduction = RatioDb(prnuBefore, prnuAfter);
            var lineArtifactScore = ComputeLineArtifactScore(output, width, height);

            return
            [
                Row("PRNU_CV", prnuAfter, "%", "<= 1%", prnuAfter <= 1.0),
                Row("FlatResidualPct", prnuAfter, "%", "<= 1.0%", prnuAfter <= 1.0),
                Row("FPN_Reduction_dB", fpnReduction, "dB", ">= 20 dB", fpnReduction >= 20.0),
                Row("LineArtifactScore", lineArtifactScore, "%", "<= 10% (profile FFT)", lineArtifactScore <= 10.0)
            ];
        }

        private static IReadOnlyList<DetectorMetricRow> ComputeDefectMetrics(
            RawPreviewResult preview,
            ReadOnlySpan<float> output,
            FixtureCaseInfo? fixtureCase)
        {
            var oracle = LoadMask(preview, fixtureCase, CalibrationRole.DefectOracle);
            if (oracle is null)
            {
                return Empty("reference oracle BPM not selected").DefectMetrics;
            }

            var predicted = LoadMask(preview, fixtureCase, CalibrationRole.Defect);
            var hasPredicted = predicted is not null && predicted.Length == oracle.Length;

            var badCount = 0;
            var goodCount = 0;
            var residualSum = 0.0;
            var goodDeltas = new List<double>();
            var truePositive = 0;
            var falsePositive = 0;
            var falseNegative = 0;
            var trueNegative = 0;

            for (var i = 0; i < oracle.Length; i++)
            {
                if (oracle[i])
                {
                    badCount++;
                    residualSum += Math.Abs(output[i]);
                }
                else
                {
                    goodCount++;
                    goodDeltas.Add(Math.Abs(output[i] - preview.SampledPixels[i]));
                }

                if (!hasPredicted)
                {
                    continue;
                }

                if (oracle[i] && predicted![i])
                {
                    truePositive++;
                }
                else if (!oracle[i] && predicted![i])
                {
                    falsePositive++;
                }
                else if (oracle[i])
                {
                    falseNegative++;
                }
                else
                {
                    trueNegative++;
                }
            }

            var recall = hasPredicted
                ? Percent(truePositive, Math.Max(1, truePositive + falseNegative))
                : double.NaN;
            var falsePositiveRate = hasPredicted
                ? Percent(falsePositive, Math.Max(1, falsePositive + trueNegative))
                : double.NaN;
            var residual = badCount > 0 ? residualSum / badCount : double.NaN;
            var p99 = goodCount > 0 ? Percentile99(goodDeltas) : double.NaN;

            return
            [
                hasPredicted
                    ? Row("DefectRecall", recall, "%", ">= 95%", recall >= 95.0)
                    : Unavailable("DefectRecall", "predicted BPM not selected"),
                hasPredicted
                    ? Row("DefectFPR", falsePositiveRate, "%", "<= 0.001%", falsePositiveRate <= 0.001)
                    : Unavailable("DefectFPR", "predicted BPM not selected"),
                Row("DefectResidualADU", residual, "ADU", "<= 2 ADU", residual <= 2.0),
                Row("GoodPixelDeltaP99", p99, "ADU", "<= 0 ADU", p99 <= 0.0)
            ];
        }

        private static DetectorMetricRow Row(string metric, double value, string unit, string gate, bool passed)
        {
            var formatted = double.IsFinite(value)
                ? $"{value:0.###} {unit}".TrimEnd()
                : "n/a";
            var status = double.IsFinite(value)
                ? passed ? "PASS" : "REVIEW"
                : "N/A";
            return new DetectorMetricRow(metric, formatted, gate, status);
        }

        private static DetectorMetricRow Unavailable(string metric, string reason)
        {
            return new DetectorMetricRow(metric, "not computed", reason, "N/A");
        }

        private static bool StageExecuted(IReadOnlyList<NativePreviewStageResult> stages, string stage)
        {
            return stages.Any(item =>
                item.Executed &&
                string.Equals(item.Stage, stage, StringComparison.OrdinalIgnoreCase));
        }

        private static bool HasRole(FixtureCaseInfo? fixtureCase, CalibrationRole role)
        {
            return fixtureCase?.CalibrationFiles.Any(file => file.Role == role) == true;
        }

        private static (double Mean, double StandardDeviation) Stats(ReadOnlySpan<ushort> values)
        {
            var sum = 0.0;
            var sumSq = 0.0;
            for (var i = 0; i < values.Length; i++)
            {
                var value = values[i];
                sum += value;
                sumSq += (double)value * value;
            }

            return FinalizeStats(sum, sumSq, values.Length);
        }

        private static (double Mean, double StandardDeviation) Stats(ReadOnlySpan<float> values)
        {
            var count = 0;
            var sum = 0.0;
            var sumSq = 0.0;
            for (var i = 0; i < values.Length; i++)
            {
                var value = values[i];
                if (!float.IsFinite(value))
                {
                    continue;
                }

                count++;
                sum += value;
                sumSq += (double)value * value;
            }

            return FinalizeStats(sum, sumSq, count);
        }

        private static (double Mean, double StandardDeviation) FinalizeStats(double sum, double sumSq, int count)
        {
            if (count <= 0)
            {
                return (0, 0);
            }

            var mean = sum / count;
            var variance = Math.Max(0, (sumSq / count) - (mean * mean));
            return (mean, Math.Sqrt(variance));
        }

        private static double CoefficientOfVariationPct((double Mean, double StandardDeviation) stats)
        {
            return Math.Abs(stats.Mean) <= double.Epsilon
                ? double.PositiveInfinity
                : 100.0 * stats.StandardDeviation / Math.Abs(stats.Mean);
        }

        private static double RatioDb(double before, double after)
        {
            if (before <= 0 || after <= 0)
            {
                return before <= after ? 0 : double.PositiveInfinity;
            }

            return 20.0 * Math.Log10(before / after);
        }

        private static int CountClamped(ReadOnlySpan<float> values)
        {
            var count = 0;
            for (var i = 0; i < values.Length; i++)
            {
                if (float.IsFinite(values[i]) && values[i] <= 0.5f)
                {
                    count++;
                }
            }

            return count;
        }

        private static double Percent(int numerator, int denominator)
        {
            return denominator <= 0 ? 0 : 100.0 * numerator / denominator;
        }

        private static double ComputeLineArtifactScore(ReadOnlySpan<float> image, int width, int height)
        {
            if (width <= 1 || height <= 1 || image.Length != checked(width * height))
            {
                return double.NaN;
            }

            var rowProfile = new double[height];
            var colProfile = new double[width];
            for (var y = 0; y < height; y++)
            {
                var rowSum = 0.0;
                for (var x = 0; x < width; x++)
                {
                    var value = image[(y * width) + x];
                    if (!float.IsFinite(value))
                    {
                        value = 0;
                    }

                    rowSum += value;
                    colProfile[x] += value;
                }

                rowProfile[y] = rowSum / width;
            }

            for (var x = 0; x < width; x++)
            {
                colProfile[x] /= height;
            }

            return Math.Max(ComputeProfileMidBandPct(rowProfile), ComputeProfileMidBandPct(colProfile));
        }

        private static double ComputeProfileMidBandPct(double[] profile)
        {
            const int maxProfileLength = 1024;
            if (profile.Length < 4)
            {
                return 0;
            }

            var compact = DownsampleProfile(profile, Math.Min(maxProfileLength, profile.Length));
            var mean = compact.Average();
            var totalEnergy = 0.0;
            var midEnergy = 0.0;
            var n = compact.Length;

            for (var k = 1; k <= n / 2; k++)
            {
                var real = 0.0;
                var imag = 0.0;
                for (var i = 0; i < n; i++)
                {
                    var window = 0.5 - (0.5 * Math.Cos((2.0 * Math.PI * i) / Math.Max(1, n - 1)));
                    var sample = (compact[i] - mean) * window;
                    var angle = -2.0 * Math.PI * k * i / n;
                    real += sample * Math.Cos(angle);
                    imag += sample * Math.Sin(angle);
                }

                var energy = (real * real) + (imag * imag);
                totalEnergy += energy;
                var frequency = k / (double)n;
                if (frequency >= 0.05 && frequency < 0.30)
                {
                    midEnergy += energy;
                }
            }

            return totalEnergy <= double.Epsilon
                ? 0
                : 100.0 * midEnergy / totalEnergy;
        }

        private static double[] DownsampleProfile(double[] profile, int targetLength)
        {
            if (profile.Length == targetLength)
            {
                return profile.ToArray();
            }

            var result = new double[targetLength];
            for (var i = 0; i < targetLength; i++)
            {
                var start = (int)Math.Floor(i * profile.Length / (double)targetLength);
                var end = (int)Math.Floor((i + 1) * profile.Length / (double)targetLength);
                end = Math.Max(start + 1, Math.Min(profile.Length, end));
                var sum = 0.0;
                for (var j = start; j < end; j++)
                {
                    sum += profile[j];
                }

                result[i] = sum / (end - start);
            }

            return result;
        }

        private static bool[]? LoadMask(
            RawPreviewResult preview,
            FixtureCaseInfo? fixtureCase,
            CalibrationRole role)
        {
            var source = fixtureCase?.CalibrationFiles.FirstOrDefault(file => file.Role == role);
            if (source is null || !File.Exists(source.Path))
            {
                return null;
            }

            var extension = Path.GetExtension(source.Path);
            if (extension.Equals(".map", StringComparison.OrdinalIgnoreCase))
            {
                return LoadByteMask(source.Path, preview);
            }

            try
            {
                var raw = RawPreviewService.LoadUInt16Preview(source.Path);
                if (raw.PreviewWidth != preview.PreviewWidth || raw.PreviewHeight != preview.PreviewHeight)
                {
                    return null;
                }

                return raw.SampledPixels.Select(value => value != 0).ToArray();
            }
            catch (Exception)
            {
                return null;
            }
        }

        private static bool[]? LoadByteMask(string path, RawPreviewResult preview)
        {
            var file = new FileInfo(path);
            var previewPixelCount = checked(preview.PreviewWidth * preview.PreviewHeight);
            if (file.Length == previewPixelCount)
            {
                return File.ReadAllBytes(path).Select(value => value != 0).ToArray();
            }

            var fullPixelCount = checked((long)preview.Width * preview.Height);
            if (file.Length != fullPixelCount)
            {
                return null;
            }

            var mask = new bool[previewPixelCount];
            var row = new byte[preview.Width];
            using var stream = File.OpenRead(path);
            for (var py = 0; py < preview.PreviewHeight; py++)
            {
                var sourceY = Math.Min(py * preview.SampleStride, preview.Height - 1);
                stream.Position = checked((long)sourceY * preview.Width);
                ReadFull(stream, row);
                for (var px = 0; px < preview.PreviewWidth; px++)
                {
                    var sourceX = Math.Min(px * preview.SampleStride, preview.Width - 1);
                    mask[(py * preview.PreviewWidth) + px] = row[sourceX] != 0;
                }
            }

            return mask;
        }

        private static void ReadFull(Stream stream, byte[] buffer)
        {
            var offset = 0;
            while (offset < buffer.Length)
            {
                var read = stream.Read(buffer, offset, buffer.Length - offset);
                if (read == 0)
                {
                    throw new EndOfStreamException("Unexpected end of mask file.");
                }

                offset += read;
            }
        }

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
