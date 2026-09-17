// #180 (GUI-C-102): the 3072x3072 frame the GSVG performance requirement is written against.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 3072×3072 raw fixture — the size
/// REQ-GSVG-019 states (1.0 s) and the size the post lane measured the module at.
///
/// A separate fixture rather than a parameter on the workflow one: the app loads its frame at start-up,
/// and the other scenarios are written against the 1024 image (their hashes and timings are its).
/// </summary>
public sealed class LargeFrameApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\wrist_lat_3072x3072.raw";

    public LargeFrameApplicationFixture()
        : base(FixtureRelativePath)
    {
    }
}

/// <summary>One app instance for the large-frame scenarios — loading a 18 MB raw is the expensive part.</summary>
[CollectionDefinition(Name)]
public sealed class LargeFrameApplicationCollection : ICollectionFixture<LargeFrameApplicationFixture>
{
    public const string Name = "gui-large-frame-application";
}
