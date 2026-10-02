// #225 (GUI-C-192d): an app whose AI worker never answers, so the never-confirmed notice is on screen without a native module.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 1024x1024 raw image and <c>--automation-fault ai-worker-silent:0</c>
/// (command line only, inert without it, loud when armed). Its own app instance, like every fixture that adds a switch.
/// </summary>
public sealed class AiWorkerMuteApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public AiWorkerMuteApplicationFixture()
        : base(FixtureRelativePath, ["--automation-fault", "ai-worker-silent:0"])
    {
    }
}

[CollectionDefinition(Name)]
public sealed class AiWorkerMuteApplicationCollection : ICollectionFixture<AiWorkerMuteApplicationFixture>
{
    public const string Name = "gui-ai-worker-mute-application";
}
