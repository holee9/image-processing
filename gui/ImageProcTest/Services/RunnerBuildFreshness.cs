using System;
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// #225 (GUI-C-166): the console runners' use of <see cref="BuildFreshnessGuard"/>.
///
/// <para><b>Separate from the guard on purpose.</b> <see cref="BuildFreshnessGuard"/> is linked into
/// <c>clients/ImageProcTest.E2ETests</c> (that project deliberately does not reference this app — see its
/// csproj note on MSB3027), so it must stay dependency-free. This class needs
/// <see cref="GuiFixtureManifestService"/> to find the checkout, so it lives beside the guard rather than
/// inside it; linking the guard would otherwise drag the repository-layout service along with it.</para>
/// </summary>
public static class RunnerBuildFreshness
{
    /// <summary>
    /// Refuses to run when the runner's OWN copy of the app assembly is older than the app's sources.
    ///
    /// <para>The subject is the copy in this program's output directory, refreshed only when THIS project
    /// is built. Rebuilding <c>gui/ImageProcTest</c> leaves it untouched, and the runner then drives the
    /// previous app — GUI-C-165 measured exactly that: two falsification arms both exited 0 against an
    /// app assembly two minutes old, and the conclusion being written up ("WPF coerces IsEnabled from
    /// CanExecute") was false.</para>
    /// </summary>
    /// <param name="runnerName">This runner's project name, used in the remedy command.</param>
    public static void EnsureRunnerCarriesCurrentApp(string runnerName)
    {
        var appAssembly = Path.Combine(AppContext.BaseDirectory, "ImageProcTest.dll");

        string appSources;
        try
        {
            var repositoryRoot = GuiFixtureManifestService.FindRepositoryRoot(AppContext.BaseDirectory);
            appSources = Path.Combine(repositoryRoot, "gui", "ImageProcTest");
        }
        catch (InvalidOperationException)
        {
            return;   // not running from a checkout: nothing to compare against
        }

        BuildFreshnessGuard.EnsureArtifactIsNotOlderThanSources(
            appAssembly,
            appSources,
            $"copy of the gui app that {runnerName} runs against",
            $"Build the RUNNER, not the app: dotnet build gui/{runnerName}/{runnerName}.csproj -c Debug. " +
            "Building gui/ImageProcTest alone does not refresh this copy.");
    }
}
