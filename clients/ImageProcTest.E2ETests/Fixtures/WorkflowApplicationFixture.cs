// XPE-GUI-E2E-001 §4.2: the workflow suite needs an image already on screen.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// An <see cref="ApplicationFixture"/> launched with the bundled 1024×1024 raw fixture.
///
/// The image arrives through <c>--automation-raw</c>, the app's own supported entry, rather than by
/// driving the Open dialog: a modal Win32 dialog is the most brittle surface a UI suite can automate
/// and the card excludes it. The app still starts in interactive mode — no <c>--automation-report</c>
/// — so the self-driving scenario stays off and the scenarios below own the window.
/// </summary>
public sealed class WorkflowApplicationFixture : ApplicationFixture
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public WorkflowApplicationFixture()
        : base(FixtureRelativePath)
    {
    }
}

/// <summary>One app instance per workflow class — launching and loading is the expensive part.</summary>
[CollectionDefinition(Name)]
public sealed class WorkflowApplicationCollection : ICollectionFixture<WorkflowApplicationFixture>
{
    public const string Name = "gui-workflow-application";
}
