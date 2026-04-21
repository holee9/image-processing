using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    public sealed record HistogramData(
        IReadOnlyList<int> Bins,
        int TotalCount,
        int MaxCount,
        float SourceMin,
        float SourceMax)
    {
        public static HistogramData Empty { get; } = new([], 0, 0, 0, 0);

        public string Summary => TotalCount == 0
            ? "empty"
            : $"pixels={TotalCount}, maxBin={MaxCount}, source={SourceMin:0.###}..{SourceMax:0.###}";

        public static HistogramData FromBins(IReadOnlyList<int> bins, float sourceMin, float sourceMax) =>
            new(
                bins,
                bins.Sum(),
                bins.Count == 0 ? 0 : bins.Max(),
                sourceMin,
                sourceMax);
    }
}
