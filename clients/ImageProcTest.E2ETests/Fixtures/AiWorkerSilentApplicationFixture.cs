// #225 (GUI-C-192c): an app whose AI worker answers once and then goes silent, so the status-unconfirmed notice is on screen without a native module.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 1024x1024 raw image and <c>--automation-fault ai-worker-silent</c>
/// (command line only, inert without it, loud when armed). Its own app instance, like every fixture that adds a switch.
/// </summary>
public sealed class AiWorkerSilentApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public AiWorkerSilentApplicationFixture()
        : base(FixtureRelativePath, ["--automation-fault", "ai-worker-silent"])
    {
    }
}

[CollectionDefinition(Name)]
public sealed class AiWorkerSilentApplicationCollection : ICollectionFixture<AiWorkerSilentApplicationFixture>
{
    public const string Name = "gui-ai-worker-silent-application";
}
