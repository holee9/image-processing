// #225 (GUI-C-191b): an app whose AI worker status read answers "switched off", so the mark is on screen without a native module.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 1024x1024 raw image and <c>--automation-fault ai-worker-disabled</c>
/// (command line only, inert without it, loud when armed: the window title carries the fault marker). Its own app instance, like every
/// fixture that adds a switch, so the leftover sweep is skipped for the same reason the other fault fixtures skip it.
/// </summary>
public sealed class AiWorkerDisabledApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public AiWorkerDisabledApplicationFixture()
        : base(FixtureRelativePath, ["--automation-fault", "ai-worker-disabled"])
    {
    }
}

[CollectionDefinition(Name)]
public sealed class AiWorkerDisabledApplicationCollection : ICollectionFixture<AiWorkerDisabledApplicationFixture>
{
    public const string Name = "gui-ai-worker-disabled-application";
}
