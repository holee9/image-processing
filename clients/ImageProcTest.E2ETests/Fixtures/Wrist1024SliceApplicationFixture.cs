// #180 (GUI-C-102): the wrist frame the GSVG scenarios run against. #208 (GUI-C-147): the file is
// 3072x3072; what the app actually loads from it is the first 1024x1024 — hence the name.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled <c>wrist_lat_3072x3072.raw</c> — an
/// 18 MB, 3072x3072 file of which the app loads the FIRST 1024x1024 pixels. The name says both halves
/// because the class was previously called <c>LargeFrameApplicationFixture</c> and its summary said it
/// carried "the size REQ-GSVG-019 states", which it never did (#208, GUI-C-147).
///
/// <para><b>What this fixture is for, stated positively.</b> Not frame size — pixel CONTENT. At the same
/// 1024x1024 the wrist slice is a different image from the default synthetic frame, which is what makes
/// the GSVG scenarios able to see a stage doing something: GUI-C-117 moved a scenario here after finding
/// its assertion passed on the synthetic frame even with the stage disconnected.</para>
///
/// <para><b>REQ-GSVG-019 does not rest on this fixture.</b> That requirement (1.0 s on 3072x3072) is a
/// MODULE requirement and is met by the post lane's module measurement — QA-B-102/103, a real 3072x3072
/// frame at 713-757 ms. Nothing in this collection compares against 1.0 s; the scenarios here assert
/// qualitatively (stage applied, hashes diverge, HUD entries present).</para>
///
/// <para><b>The gap, recorded here so nobody infers it away (#208 §2).</b> The end-to-end time of a
/// 3072x3072 frame THROUGH THE GUI has never been measured. The module number exists; a GUI-traversing
/// number does not. That is not a requirement violation — REQ-GSVG-019 is a module requirement — but
/// there is no basis for reading any result in this collection as "confirmed end-to-end too".</para>
///
/// A separate fixture rather than a parameter on the workflow one: the app loads its frame at start-up,
/// and the other scenarios are written against the 1024 synthetic image (their hashes and timings are its).
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
public sealed class Wrist1024SliceApplicationFixture : ApplicationFixture, IDisposable
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\wrist_lat_3072x3072.raw";

    /// <summary>The 1024x1024 16-bit frame size in bytes - what the app is told to open (<c>--automation-width/height 1024</c>).</summary>
    private const int SliceBytes = 1024 * 1024 * 2;

    private readonly string _slicePath;

    // GUI-C-233n: since GUI-C-232b the app opens a raw file only when its length is exactly width x height x 2 (RawSizeRules), so the 18 MB file can no longer be handed to it with 1024 x 1024.
    // The app used to read the FIRST width x height samples of such a file and ignore the rest (RawImageLoader on origin/main: ReadAllBytes, then samples 0..w*h-1), so the frame these scenarios were
    // measured on is exactly the first SliceBytes of the file. The fixture now writes those bytes to a temp file of exactly that length and opens it: the same pixels, a size the app accepts.
    public Wrist1024SliceApplicationFixture()
        : this(CreateSlice())
    {
    }

    private Wrist1024SliceApplicationFixture(string slicePath)
        : base(slicePath, ["--automation-export-render", RenderDumpPath], skipLeftoverSweep: false)
    {
        _slicePath = slicePath;
    }

    /// <summary>Writes the first <see cref="SliceBytes"/> of the bundled wrist file to a new temp file and returns its absolute path (Path.Combine in the base class keeps an absolute path as it is).</summary>
    private static string CreateSlice()
    {
        var exe = ResolveApplicationExecutable();
        var source = exe is null ? null : Path.Combine(Path.GetDirectoryName(exe)!, FixtureRelativePath);
        var path = Path.Combine(Path.GetTempPath(), $"xpe-gui-e2e-wrist-1024-slice-{Guid.NewGuid():N}.raw");
        if (source is null || !File.Exists(source))
        {
            // No app or no source file: the base class skips with its own reason (the temp file is not created, so its "fixture image not found" skip applies).
            return path;
        }

        var buffer = new byte[SliceBytes];
        using (var input = File.OpenRead(source))
        {
            input.ReadExactly(buffer);
        }

        File.WriteAllBytes(path, buffer);
        return path;
    }

    public new void Dispose()
    {
        base.Dispose();
        try
        {
            File.Delete(_slicePath);
        }
        catch (IOException)
        {
            // Left in the temp folder if something still holds it; the name is unique per run.
        }
    }

    /// <summary>
    /// Where the app writes the rendered BGRA buffer after every render (#200, GUI-C-138). A test that
    /// needs the drawn pixels — rather than only their hash — reads this file right after its own
    /// render completes. Under the test results directory rather than the app directory: the app
    /// directory is swept as a source tree by the leftover guard.
    /// </summary>
    public static string RenderDumpPath { get; } =
        Path.Combine(Path.GetTempPath(), "xpe-gui-e2e-wrist-1024-slice-render.bgra");
}

/// <summary>One app instance for the scenarios that need the wrist slice — loading the 18 MB raw is the expensive part.</summary>
[CollectionDefinition(Name)]
public sealed class Wrist1024SliceApplicationCollection : ICollectionFixture<Wrist1024SliceApplicationFixture>
{
    public const string Name = "gui-wrist-1024-slice-application";
}
