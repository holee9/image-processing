// #180 (GUI-C-102): the 3072x3072 frame the GSVG performance requirement is written against.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 3072×3072 raw fixture — the size
/// REQ-GSVG-019 states (1.0 s) and the size the post lane measured the module at.
///
/// A separate fixture rather than a parameter on the workflow one: the app loads its frame at start-up,
/// and the other scenarios are written against the 1024 image (their hashes and timings are its).
///
/// <para><b>Measured caveat (#200, GUI-C-138).</b> The base fixture passes
/// <c>--automation-width 1024 --automation-height 1024</c> for every raw path, this one included, so the
/// app loads the FIRST 1024×1024 pixels of the 18 MB file rather than the whole 3072² frame. Asked
/// directly, the app reports <c>RAW 1024x1024, min=2481, max=6080, bytes=18874368</c> here against
/// <c>RAW 3072x3072, min=2481, max=15451</c> when given 3072 — different pixel content, not a crop of
/// the same statistics. Recorded rather than changed: the scenarios in this collection have their
/// timings and pinned hashes measured against what it actually loads, so changing the size here would
/// move every one of them. Whether the GSVG cost requirement is satisfied by this slice is a question
/// for the lane that owns that requirement, not a detail to tidy up here.</para>
/// </summary>
public sealed class LargeFrameApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\wrist_lat_3072x3072.raw";

    public LargeFrameApplicationFixture()
        : base(FixtureRelativePath, ["--automation-export-render", RenderDumpPath], skipLeftoverSweep: false)
    {
    }

    /// <summary>
    /// Where the app writes the rendered BGRA buffer after every render (#200, GUI-C-138). A test that
    /// needs the drawn pixels — rather than only their hash — reads this file right after its own
    /// render completes. Under the test results directory rather than the app directory: the app
    /// directory is swept as a source tree by the leftover guard.
    /// </summary>
    public static string RenderDumpPath { get; } =
        Path.Combine(Path.GetTempPath(), "xpe-gui-e2e-large-frame-render.bgra");
}

/// <summary>One app instance for the large-frame scenarios — loading a 18 MB raw is the expensive part.</summary>
[CollectionDefinition(Name)]
public sealed class LargeFrameApplicationCollection : ICollectionFixture<LargeFrameApplicationFixture>
{
    public const string Name = "gui-large-frame-application";
}
