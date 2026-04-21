namespace ImageProcTest
{
    internal sealed record ViewportRenderStateSnapshot(
        float OriginalWindowCenter,
        float OriginalWindowWidth,
        bool OriginalInvert,
        string OriginalLut,
        float ProcessedWindowCenter,
        float ProcessedWindowWidth,
        bool ProcessedInvert,
        string ProcessedLut,
        bool LinkedWindowLevel,
        string ActiveTarget,
        double Zoom,
        double SwipeFraction,
        string OriginalHistogram,
        string ProcessedHistogram);
}
