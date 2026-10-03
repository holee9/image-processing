// #225 (GUI-C-204): an app of its own for the one scenario that ENDS it. File > Exit closes the window and the process ends; a shared fixture cannot be used for that, because
// every later scenario in the collection would find no window. The scenario that uses this fixture is the only one in its collection.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

public sealed class ExitApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public ExitApplicationFixture()
        : base(FixtureRelativePath)
    {
    }
}

[CollectionDefinition(Name)]
public sealed class ExitApplicationCollection : ICollectionFixture<ExitApplicationFixture>
{
    public const string Name = "gui-exit-application";
}
