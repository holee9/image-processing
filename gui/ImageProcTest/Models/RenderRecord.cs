namespace ImageProcTest.Models;

/// <summary>
/// GUI-C-233g (Codex #171-#176): everything the app says about the picture that is ON SCREEN, in one immutable object that is created at the moment the render is committed
/// and replaced or dropped as a whole.
///
/// <para><b>Why one object.</b> Six reviews in a row found the same defect: a record of the drawn image (window, summary, timings, chain, calibration modes, then the backend that produced it)
/// outliving or being overwritten by an event (new frame, settings change, display failure, backend switch). Each fix cleared one more field, and the next field was the one left. With
/// one record there is nothing to forget: the HUD, the panels and the automation report read their APPLIED values only from here, and "no render" is a single <c>null</c>.</para>
///
/// <para><b>Lanes (GUI-C-233h, Codex #177).</b> The comparison viewport's Reference (<c>LaneA</c>) and Candidate (<c>LaneB</c>) pictures belong to the render too: they are produced after the commit
/// by the same backend and are added to THIS record (by <c>Id</c>, so a newer render or an invalidation wins), and they go with it. They are the same kind of thing as the main picture and
/// had been the one piece of "the drawn image" outside the record.</para>
///
/// <para><b>What is not in here.</b> What is asked for now (<c>Settings</c>) and what is running now (the backend's runtime info) are different facts from what produced the picture,
/// and the report keeps them under their own names (<c>requested</c>, the top-level runtime keys).</para>
/// </summary>
public sealed record RenderRecord(
    AppSettings Inputs,
    RenderBackend Backend,
    RenderedVoi Voi,
    ChainResult Chain,
    string ChainStatus,
    string AiLabel,
    bool PreprocessRan,
    string PreprocessStages,
    string DisplaySummary,
    string Timings,
    long Id = 0,
    System.Windows.Media.ImageSource? LaneA = null,
    System.Windows.Media.ImageSource? LaneB = null);

/// <summary>The backend that PRODUCED a render, captured when it was committed (not read from the backend that is current later).</summary>
public sealed record RenderBackend(string Mode, string BackendName, string CommonVersion, string DisplayVersion, string NativeSource)
{
    public const string RealBackendName = "RealXpeBackend";

    public static RenderBackend From(BackendRuntimeInfo info) => new(
        string.Equals(info.BackendName, RealBackendName, StringComparison.Ordinal) ? "Native" : "Mock",
        info.BackendName,
        info.Version,
        info.DisplayVersion,
        info.NativeSource);
}

/// <summary>The window the render used. Null parts mean "not reported / not meaningful", never the settings' numbers.</summary>
public sealed record RenderedVoi(float? Center, float? Width, string? Mode, bool? Automatic)
{
    public static RenderedVoi From(AppSettings inputs, AppliedVoiWindow? applied)
    {
        if (applied is not null)
        {
            // GUI-C-233b: what the display stage really used (for the automatic window, the module's numbers), not the settings.
            return new RenderedVoi(applied.Center, applied.Width, applied.Mode, applied.Automatic);
        }

        if (inputs.VoiWindowAuto)
        {
            // An automatic window whose numbers the backend did not report: the settings' center/width were never used, so they are not shown as if they were.
            return new RenderedVoi(null, null, null, null);
        }

        return new RenderedVoi(inputs.VoiWindowCenter, inputs.VoiWindowWidth, inputs.VoiLutMode, null);
    }
}
