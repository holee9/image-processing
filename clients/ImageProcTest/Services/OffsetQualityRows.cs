using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.InteropServices;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-253 (#273): the result of the module's signed offset quality measurement, <c>XpeOffsetQuality</c> of preprocess_api.h (xpe_verify_offset_signed). 120 bytes, Pack = 8; the tests assert the
    /// size and the offset of every field. A field is read only when its bit of <see cref="ValidMask"/> is set (the module writes 0 into the others, and 0 is a legal measured value).
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct XpeOffsetQuality
    {
        public uint StructSize;
        public uint WrittenSize;
        public uint ValidMask;
        public uint RoiPixels;
        public ulong PixelCount;
        public uint MapWidth;
        public uint MapHeight;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] MapSha256;
        public double DarkBiasSigned;
        public double DsnuSigned;
        public double DarkReductionDb;
        public double ClampRatePct;
        public double ZeroOutputRatePct;
        public double DarkBiasOutput;
        public double DsnuOutput;
    }

    /// <summary>The five measurement groups of <see cref="XpeOffsetQuality.ValidMask"/> (XPE_OFFSETQ_* of the header).</summary>
    internal static class OffsetQualityBits
    {
        public const uint RoiSigned = 0x01;
        public const uint DarkReduction = 0x02;
        public const uint RoiOutput = 0x04;
        public const uint ClampRate = 0x08;
        public const uint MapIdent = 0x10;
        public const int StructBytes = 120;
    }

    /// <summary>
    /// GUI-C-253b (Codex #240 B-3): what the opened raw image IS, as the USER said it (the app has no other means of knowing: a fixture case's roles belong to its calibration files and are partly inferred
    /// from file names, and an opened image carries no acquisition metadata). The dark quality rows are measured only for <see cref="Dark"/>. The default is <see cref="Unconfirmed"/>; nothing guesses it
    /// from a file name or from the mean brightness.
    /// </summary>
    internal enum RawInputKind
    {
        Unconfirmed,
        Dark,
        Flat,
        Clinical,
    }

    /// <summary>The decision before the module is called: is this input allowed to be measured as a dark frame, and is there an original-size map?</summary>
    internal static class OffsetQualityPlan
    {
        internal const string UnconfirmedReason = "다크 프레임 미확인 — 다크 품질을 측정하지 않음";

        /// <summary>
        /// Null = the module may be called. Otherwise the outcome to show and NO call is made. The input kind is checked first (a frame that is not a confirmed dark is never measured, whatever its map),
        /// then the map (a separate reason).
        /// </summary>
        public static OffsetQualityOutcome? WhyNotCalled(RawInputKind kind, int rawWidth, int rawHeight, int? mapWidth, int? mapHeight)
        {
            if (kind == RawInputKind.Unconfirmed) return OffsetQualityOutcome.NotMeasured(UnconfirmedReason);
            if (kind != RawInputKind.Dark)
            {
                var name = kind == RawInputKind.Flat ? "밝은 영상(평탄)" : "임상·기타 영상";
                return OffsetQualityOutcome.NotMeasured($"다크 프레임이 아님(입력 종류: {name}) — 다크 품질을 측정하지 않음");
            }

            if (mapWidth is null || mapHeight is null) return OffsetQualityOutcome.NotMeasured("offset 맵 요청이 없음");
            if (mapWidth != rawWidth || mapHeight != rawHeight)
            {
                return OffsetQualityOutcome.NotMeasured($"원본 크기({rawWidth}x{rawHeight}) offset 맵이 없음: 이 화면의 맵은 미리보기 크기({mapWidth}x{mapHeight}). 위 칸은 미리보기 기준");
            }

            return null;
        }
    }

    /// <summary>
    /// What came back from the module, in the order the caller must read it: the return code, then <c>writtenSize</c>, then the bit of each group. <see cref="NotMeasuredReason"/> is set when nothing was
    /// measured (the module was not asked, or it refused, or it wrote less than the whole structure); then <see cref="Quality"/> carries no meaning and no row shows a number.
    /// </summary>
    internal sealed record OffsetQualityOutcome(string? NotMeasuredReason, XpeOffsetQuality Quality)
    {
        public bool Measured => NotMeasuredReason is null;

        public static OffsetQualityOutcome NotMeasured(string reason) => new(reason, default);

        /// <summary>Reads a result: a failed code means no field is read, whatever the struct holds; a struct written short of its 120 bytes is not read either.</summary>
        public static OffsetQualityOutcome FromModule(int returnCode, XpeOffsetQuality quality)
        {
            if (returnCode != 0) return NotMeasured($"module returned error code {returnCode}");
            if (quality.WrittenSize != OffsetQualityBits.StructBytes) return NotMeasured($"module wrote {quality.WrittenSize} bytes, not {OffsetQualityBits.StructBytes}");
            return new OffsetQualityOutcome(null, quality);
        }
    }

    /// <summary>
    /// GUI-C-253: the "measured" rows of the dark metrics grid. They sit BELOW the existing rows (DarkBias, DSNU_ADU, DarkReduction_dB, ClampRate), which are unchanged: they stay "preview basis" with their own
    /// limits and pass column. These rows have no limit and no verdict (the module approves none for a signed value): the status column says REPORTED or N/A, never PASS or REVIEW. A group the module did not
    /// measure reads "측정 안 됨" and names why; it is never shown as 0, as passed, or as blank.
    /// </summary>
    internal static class OffsetQualityRows
    {
        /// <summary>One line of help about the sign changing from frame to frame (two dark levels); the wording comes from the leader after protocol draft 03 is settled. Empty = no row.</summary>
        internal const string SignChangesHelp = "";

        internal const string NotMeasuredText = "측정 안 됨";
        internal const string BasisMetric = "측정값 기준";

        private const string Population = "측정값 · 판정 없음";

        /// <summary>The text of every measured row while the input kind was changed after the run: the old numbers belong to the old kind and are not shown again, not even when "다크 프레임" is chosen again.</summary>
        internal const string KindChangedReason = "입력 종류 변경 — 다시 실행 필요";

        private const string MetricSigned = "DarkBias 부호 있음", MetricSignedDsnu = "DSNU 부호 있음", MetricReduction = "DarkReduction 부호 있음", MetricOutput = "DarkBias 출력", MetricOutputDsnu = "DSNU 출력",
            MetricClamp = "ClampRate x<0 원본 전체", MetricZero = "출력 0 비율 원본 전체", MetricMap = "Offset 맵 SHA-256(payload)", MetricHelp = "도움말";

        private static readonly HashSet<string> MeasuredMetrics = new(StringComparer.Ordinal)
        {
            BasisMetric, MetricSigned, MetricSignedDsnu, MetricReduction, MetricOutput, MetricOutputDsnu, MetricClamp, MetricZero, MetricMap, MetricHelp,
        };

        /// <summary>True for the rows this class adds (and only those): the four existing dark rows and their verdicts are never among them.</summary>
        public static bool IsMeasuredRow(DetectorMetricRow row) => MeasuredMetrics.Contains(row.Metric);

        /// <summary>
        /// GUI-C-253c (Codex #241 R241-3): the user changed the input kind while a run's rows are on screen. ONLY the measured rows change: they now say why nothing is measured for the NEW kind
        /// ("입력 종류 변경 — 다시 실행 필요" for a dark, since measuring needs a new run; the kind's own reason for any other). The existing rows are returned untouched, in order. No module call happens
        /// here. When the list holds no measured row (the offset stage did not run, or no run yet) the same list is returned.
        /// </summary>
        public static IReadOnlyList<DetectorMetricRow> Invalidate(IReadOnlyList<DetectorMetricRow> rows, RawInputKind newKind)
        {
            var existing = new List<DetectorMetricRow>();
            foreach (var row in rows)
            {
                if (!IsMeasuredRow(row)) existing.Add(row);
            }

            if (existing.Count == rows.Count) return rows;
            var outcome = newKind == RawInputKind.Dark
                ? OffsetQualityOutcome.NotMeasured(KindChangedReason)
                : OffsetQualityPlan.WhyNotCalled(newKind, 0, 0, null, null)!;
            return Append(existing, outcome);
        }

        /// <summary>The existing rows exactly as they were, then the measured rows: nothing of the existing rows (value, limit, status) is read, changed or reordered.</summary>
        public static IReadOnlyList<DetectorMetricRow> Append(IReadOnlyList<DetectorMetricRow> existing, OffsetQualityOutcome outcome)
        {
            var rows = new List<DetectorMetricRow>(existing);
            rows.AddRange(Build(outcome));
            return rows;
        }

        public static IReadOnlyList<DetectorMetricRow> Build(OffsetQualityOutcome outcome)
        {
            var q = outcome.Quality;
            // GUI-C-253c (Codex #241 R241-2): the base row states what actually happened. A measurement says where the "dark" comes from: the USER's choice, not acquisition metadata. When nothing was
            // measured it says so and why; "측정 조건" describes what a measurement needs, which is not the same as having it.
            const string OldRows = "위 DarkBias·DSNU_ADU·DarkReduction_dB·ClampRate 는 미리보기 기준이라 모집단과 단계가 다르다. 한계는 그 칸에만 적용된다";
            var rows = new List<DetectorMetricRow>
            {
                outcome.Measured
                    ? new(BasisMetric,
                        "입력 종류: 다크 프레임(사용자 선택, 취득 메타데이터로 확인하지 않음)",
                        "측정 조건: 원본 크기 원시 영상 + 같은 크기 offset 맵 · " + OldRows,
                        "INFO")
                    : new(BasisMetric,
                        "미측정 — " + outcome.NotMeasuredReason,
                        "측정 조건: 사용자가 다크 프레임으로 선택한 원본 크기 원시 영상 + 같은 크기 offset 맵(충족하지 않으면 측정하지 않음) · " + OldRows,
                        "N/A"),
            };

            string Why(uint bit) => outcome.Measured
                ? $"모듈이 이 지표를 측정하지 않았다(validMask 비트 0x{bit:X2} 꺼짐: 어두운 영역이 너무 작거나 원시에 변동이 없는 경우 등)"
                : outcome.NotMeasuredReason!;

            DetectorMetricRow Row(string metric, uint bit, double value, string unit, string note) =>
                outcome.Measured && (q.ValidMask & bit) != 0
                    ? double.IsFinite(value)
                        ? new DetectorMetricRow(metric, $"{value.ToString("0.####", CultureInfo.InvariantCulture)} {unit}".TrimEnd(), $"{Population} · {note}", "REPORTED")
                        : new DetectorMetricRow(metric, NotMeasuredText, "모듈이 유한하지 않은 값을 돌려줌", "N/A")
                    : new DetectorMetricRow(metric, NotMeasuredText, Why(bit), "N/A");

            rows.Add(Row(MetricSigned, OffsetQualityBits.RoiSigned, q.DarkBiasSigned, "화소값", "offset 을 뺀 뒤 0 바닥 전 · 원시의 어두운 10분위"));
            rows.Add(Row(MetricSignedDsnu, OffsetQualityBits.RoiSigned, q.DsnuSigned, "화소값", "offset 을 뺀 뒤 0 바닥 전 · 원시의 어두운 10분위"));
            rows.Add(Row(MetricReduction, OffsetQualityBits.DarkReduction, q.DarkReductionDb, "dB", "원시 대비 offset 뺀 뒤 · 원시의 어두운 10분위"));
            rows.Add(Row(MetricOutput, OffsetQualityBits.RoiOutput, q.DarkBiasOutput, "화소값", "0 바닥과 반올림 뒤 UINT16 출력 · 같은 어두운 10분위"));
            rows.Add(Row(MetricOutputDsnu, OffsetQualityBits.RoiOutput, q.DsnuOutput, "화소값", "0 바닥과 반올림 뒤 UINT16 출력 · 같은 어두운 10분위"));
            rows.Add(Row(MetricClamp, OffsetQualityBits.ClampRate, q.ClampRatePct, "%", "0 바닥 전에 음수인 화소 비율 · 프레임 전체"));
            rows.Add(Row(MetricZero, OffsetQualityBits.ClampRate, q.ZeroOutputRatePct, "%", "반올림 뒤 출력이 0 인 화소 비율 · 프레임 전체(위 ClampRate 와 다른 양)"));

            rows.Add(outcome.Measured && (q.ValidMask & OffsetQualityBits.MapIdent) != 0 && q.MapSha256 is { Length: 32 }
                ? new DetectorMetricRow(MetricMap, ShortHash(q.MapSha256), $"{Population} · 맵 payload(float32 바이트열) 기준이라 .xcal 헤더의 해시와 다르다 · 만료를 검사하지 않으므로 적용 가능한 맵이라는 뜻이 아니다", "REPORTED")
                : new DetectorMetricRow(MetricMap, NotMeasuredText, Why(OffsetQualityBits.MapIdent), "N/A"));

            if (SignChangesHelp.Length > 0) rows.Add(new DetectorMetricRow(MetricHelp, SignChangesHelp, Population, "INFO"));
            return rows;
        }

        private static string ShortHash(byte[] hash) => Convert.ToHexString(hash).ToLowerInvariant()[..16] + "…";
    }
}
