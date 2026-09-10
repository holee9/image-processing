// #129: the gui app must resolve native DLLs through the shared policy, not the app directory alone.
using System.Text.RegularExpressions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Guards the gui side of the search policy (GUI-C-32).
///
/// GUI-C-31 measured the hazard: gui used plain <c>[DllImport]</c> and its backend factory hard-coded
/// <c>AppContext.BaseDirectory</c>, so <c>XPE_NATIVE_DIR</c> — which clients obeys (#129) — was inert
/// there. One environment variable, two apps, different answers.
///
/// The check is textual for the same reason as <c>NativeCallWrapperGuardTests</c>: the gui types are
/// WPF-bound and cannot be loaded from this assembly. What it pins is the *wiring* — a resolver is
/// installed, and the factory asks the locators rather than the app directory.
/// </summary>
[Trait("Category", "Functional")]
public sealed class GuiNativeSearchAdoptionTests
{
    private const string ResolverPath = "gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs";
    private const string FactoryPath = "gui/ImageProcTest/Services/XpeBackendFactory.cs";
    private const string AppPath = "gui/ImageProcTest/App.xaml.cs";

    /// <summary>
    /// The resolver exists, registers itself with the runtime, and takes its candidates from the
    /// shared locators — not from a second copy of the search order.
    /// </summary>
    [Fact]
    public void GuiResolver_UsesTheSharedLocators()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(ResolverPath));

        Assert.Contains("NativeLibrary.SetDllImportResolver", source, StringComparison.Ordinal);
        Assert.Contains("XpeCommonLibraryLocator.GetDllCandidates", source, StringComparison.Ordinal);
        Assert.Contains("NativeModuleLibraryLocator.GetDllCandidates", source, StringComparison.Ordinal);
    }

    /// <summary>
    /// App startup installs the resolver. Without this the resolver is dead code and every DllImport
    /// falls back to the Windows loader — the exact state GUI-C-31 measured.
    /// </summary>
    [Fact]
    public void AppStartup_InstallsTheResolver()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(AppPath));

        Assert.Matches(new Regex(@"GuiNativeLibraryResolver\.Install\(\)"), source);
    }

    /// <summary>
    /// The backend factory resolves through the locators. It used to combine AppContext.BaseDirectory
    /// directly, which made it decide "no native DLL" and fall back to Mock BEFORE any P/Invoke could
    /// reach the resolver — measured in GUI-C-32: installing the resolver alone changed nothing.
    /// </summary>
    [Fact]
    public void BackendFactory_ResolvesThroughTheLocators_NotTheAppDirectoryAlone()
    {
        var source = File.ReadAllText(ResolveRepositoryFile(FactoryPath));

        Assert.Contains("XpeCommonLibraryLocator.TryFindDll", source, StringComparison.Ordinal);
        Assert.Contains("NativeModuleLibraryLocator.TryFindDll", source, StringComparison.Ordinal);

        // The app-directory path may remain as a fallback for reporting, but never as the only source.
        var baseDirectoryOnly = Regex.Matches(
            source, @"Path\.Combine\(AppContext\.BaseDirectory, ""xpe_\w+\.dll""\)");
        Assert.True(
            baseDirectoryOnly.Count <= 2,
            $"{FactoryPath} builds {baseDirectoryOnly.Count} app-directory paths; the locators should " +
            "be the primary source with at most one fallback per DLL.");
    }

    /// <summary>
    /// Walks up from the test output directory to find a repository file. A missing file FAILS
    /// rather than skips — silently skipping would restore the blind spot this class removes.
    /// </summary>
    private static string ResolveRepositoryFile(string relativePath)
    {
        var native = relativePath.Replace('/', Path.DirectorySeparatorChar);
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(dir.FullName, native);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        Assert.Fail(
            $"Could not locate {relativePath} by walking up from {AppContext.BaseDirectory}. " +
            "This test reads repository sources directly and must not be run outside the repository tree.");
        return string.Empty; // unreachable
    }
}
