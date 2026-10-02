// #225 (GUI-C-198, Codex #78 finding 3): an app whose native directory is a PRIVATE COPY of the run's native directory without xpe_ai.dll, so the file can be added and removed
// while the app runs. Without a native directory that holds xpe_ai.dll there is nothing to copy: the scenario skips with the reason.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

public sealed class AiDllLateApplicationFixture : ApplicationFixture, IDisposable
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public AiDllLateApplicationFixture()
        : base(FixtureRelativePath, Prepare(out var directory, out var source, out var note))
    {
        PrivateDirectory = directory;
        SourceAiDll = source;
        PrepareNote = note;
    }

    /// <summary>The directory the app was pinned to (a copy without xpe_ai.dll).</summary>
    public DirectoryInfo PrivateDirectory { get; }

    /// <summary>Where xpe_ai.dll can be copied from, or null when the run has no native directory with one.</summary>
    public string? SourceAiDll { get; }

    public string PrepareNote { get; }

    private static DirectoryInfo Prepare(out DirectoryInfo directory, out string? source, out string note)
    {
        directory = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-c198-late-{Guid.NewGuid():N}"));
        source = null;
        var nativeDir = Environment.GetEnvironmentVariable(NativeDirVariable);
        if (string.IsNullOrWhiteSpace(nativeDir) || !File.Exists(Path.Combine(nativeDir, "xpe_ai.dll")))
        {
            note = $"{NativeDirVariable} is not set or holds no xpe_ai.dll ('{nativeDir}'), so there is no DLL to add late.";
            return directory;
        }

        foreach (var file in Directory.GetFiles(nativeDir))
        {
            if (!string.Equals(Path.GetFileName(file), "xpe_ai.dll", StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(file, Path.Combine(directory.FullName, Path.GetFileName(file)));
            }
        }

        source = Path.Combine(nativeDir, "xpe_ai.dll");
        note = $"private copy of '{nativeDir}' without xpe_ai.dll in {directory.FullName}";
        return directory;
    }

    void IDisposable.Dispose()
    {
        Dispose();
        try
        {
            PrivateDirectory.Delete(recursive: true);
        }
        catch (IOException)
        {
            // a DLL the app still holds: left for the temp directory's cleanup, never a reason to fail a run
        }
        catch (UnauthorizedAccessException)
        {
        }
    }
}

[CollectionDefinition(Name)]
public sealed class AiDllLateApplicationCollection : ICollectionFixture<AiDllLateApplicationFixture>
{
    public const string Name = "gui-ai-dll-late-application";
}
