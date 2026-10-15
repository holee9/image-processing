// #225 / #130 (GUI-C-202): an app whose native directory is a PRIVATE COPY of the run's native directory WITHOUT xpe_ai_worker.exe. The module then cannot start its worker, and every
// bone suppression call fails for that reason alone: a failure the module counts toward the three that switch the worker off, with no model, no signature and no test hook involved.
// Without a native directory that holds xpe_ai.dll and xpe_ai_worker.exe there is nothing to copy: the scenario skips with the reason.
using Xunit;

namespace ImageProcTest.E2ETests.Fixtures;

public sealed class AiWorkerAbsentApplicationFixture : ApplicationFixture, IDisposable
{
    private const string FixtureRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";
    private const string WorkerFileName = "xpe_ai_worker.exe";

    public AiWorkerAbsentApplicationFixture()
        : base(FixtureRelativePath, Prepare(out var directory, out var note))
    {
        PrivateDirectory = directory;
        PrepareNote = note;
    }

    /// <summary>The directory the app was pinned to (a copy without the worker).</summary>
    public DirectoryInfo PrivateDirectory { get; }

    /// <summary>What was prepared, or why nothing could be: the skip reason when the run has no native directory with the AI module and its worker.</summary>
    public string PrepareNote { get; }

    /// <summary>True when the copy was made from a native directory that holds the module and its worker.</summary>
    public bool Prepared => PrepareNote.StartsWith("private copy", StringComparison.Ordinal);

    private static DirectoryInfo Prepare(out DirectoryInfo directory, out string note)
    {
        directory = Directory.CreateDirectory(Path.Combine(Path.GetTempPath(), $"xpe-c202-noworker-{Guid.NewGuid():N}"));
        var nativeDir = Environment.GetEnvironmentVariable(NativeDirVariable);
        if (string.IsNullOrWhiteSpace(nativeDir)
            || !File.Exists(Path.Combine(nativeDir, "xpe_ai.dll"))
            || !File.Exists(Path.Combine(nativeDir, WorkerFileName)))
        {
            note = $"{NativeDirVariable} is not set or holds no xpe_ai.dll with {WorkerFileName} ('{nativeDir}'), so there is no worker to take away.";
            return directory;
        }

        foreach (var file in Directory.GetFiles(nativeDir))
        {
            if (!string.Equals(Path.GetFileName(file), WorkerFileName, StringComparison.OrdinalIgnoreCase))
            {
                File.Copy(file, Path.Combine(directory.FullName, Path.GetFileName(file)));
            }
        }

        note = $"private copy of '{nativeDir}' without {WorkerFileName} in {directory.FullName}";
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
public sealed class AiWorkerAbsentApplicationCollection : ICollectionFixture<AiWorkerAbsentApplicationFixture>
{
    public const string Name = "gui-ai-worker-absent-application";
}
