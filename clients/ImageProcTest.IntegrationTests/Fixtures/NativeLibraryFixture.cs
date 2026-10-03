// @MX:ANCHOR NativeLibraryFixture — single one-time native DLL resolver; all test collections depend on this.
using System.Reflection;
using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// xUnit collection fixture that resolves xpe_common.dll once per test session.
/// Tests that require the native DLL must join the <see cref="NativeLibraryCollection"/>.
/// If the DLL cannot be located, <see cref="IsAvailable"/> is false and callers
/// should skip native-required assertions.
/// </summary>
public sealed class NativeLibraryFixture : IDisposable
{
    private static readonly string DllName = "xpe_common.dll";

    /// <summary>
    /// Gets a value indicating whether xpe_common.dll was successfully LOADED.
    /// File existence is not enough: the DLL links against spdlog.dll and fmt.dll, so a
    /// present-but-unloadable binary must report false — otherwise dependent tests run and
    /// fail with DllNotFoundException instead of skipping (observed in CI, #98).
    /// </summary>
    public bool IsAvailable { get; }

    /// <summary>Gets the resolved path of xpe_common.dll, or a diagnostic message when unavailable.</summary>
    public string ResolvedPath { get; }

    /// <summary>Gets the loader error message when the DLL was located but could not be loaded.</summary>
    private string LoadError { get; } = string.Empty;

    /// <summary>Gets the skip reason naming which DLL is missing, or why loading it failed.</summary>
    public string SkipReason => IsAvailable
        ? string.Empty
        : LoadError.Length > 0
            ? $"xpe_common.dll found at {ResolvedPath} but failed to load (missing dependency such as spdlog.dll or fmt.dll?): {LoadError}"
            : $"xpe_common.dll not found. Set XPE_NATIVE_DIR or build the native project first. Searched: {ResolvedPath}";

    /// <summary>Process-wide handle of the loaded DLL, reused by <see cref="Resolver"/>.</summary>
    private readonly IntPtr _handle;

    /// <summary>
    /// GUI-C-208 (D2): pinned objects the runtime saw in a full blocking collection when this fixture was created, i.e. before any test of the collection ran. A test that
    /// asks "is anything pinned that was not pinned then" compares against this.
    /// </summary>
    public long PinnedObjectsAtStart { get; } = PinnedObjects.AfterFullCollection();

    /// <summary>GUI-C-225b (REQ-GUI-IT-020): how long the locator took in the FIRST fixture creation of this process, which is what "when the test assembly is loaded" means. Recorded here, once, not re-measured by a test later.</summary>
    public TimeSpan LocateDuration { get; }

    /// <summary>GUI-C-225b: how long <c>NativeLibrary.Load</c> of the located DLL took in that first creation (null when it was not loaded).</summary>
    public TimeSpan? LoadDuration { get; }

    /// <summary>GUI-C-225b: how long the very first <c>xpe_version</c> call took, made by the first creation itself right after the load (null when there was no load or this instance does not own the resolver).</summary>
    public TimeSpan? FirstCallDuration { get; }

    public NativeLibraryFixture()
        : this(Environment.GetEnvironmentVariable("XPE_NATIVE_DIR"), AppContext.BaseDirectory, registerResolver: true)
    {
    }

    /// <summary>
    /// The fixture with its inputs given. <paramref name="registerResolver"/> is false for a fixture a test builds to see what the bootstrap does with no DLL (REQ-GUI-IT-041): the resolver of an
    /// assembly can be set once per process, and it belongs to the real, first fixture.
    /// </summary>
    internal NativeLibraryFixture(string? envDir, string baseDirectory, bool registerResolver)
    {
        if (registerResolver)
        {
            NativeLibrary.SetDllImportResolver(typeof(NativeLibraryFixture).Assembly, Resolver);
        }

        var watch = System.Diagnostics.Stopwatch.StartNew();
        var (found, path) = Locate(envDir, baseDirectory);
        LocateDuration = watch.Elapsed;
        ResolvedPath = path;

        if (!found)
        {
            IsAvailable = false;
            return;
        }

        // Verify it is truly x64 by checking the PE header.
        if (!VerifyX64Pe(path))
        {
            IsAvailable = false;
            ResolvedPath = ArchitectureMismatchDiagnostic(path);
            return;
        }

        // Existence and architecture are necessary but not sufficient — load it.
        try
        {
            watch.Restart();
            _handle = NativeLibrary.Load(path);
            LoadDuration = watch.Elapsed;
            IsAvailable = true;
        }
        catch (Exception ex)
        {
            IsAvailable = false;
            LoadError = ex.Message;
            return;
        }

        if (registerResolver)
        {
            watch.Restart();
            _ = ImageProcTest.IntegrationTests.PInvoke.XpeCommonNative.xpe_version();
            FirstCallDuration = watch.Elapsed;
        }
    }

    /// <summary>GUI-C-209 (D9): what the fixture reports as the resolved path when the located DLL is not x64. Named so a test can hold the wording (it carries the path).</summary>
    internal static string ArchitectureMismatchDiagnostic(string path) => $"Architecture mismatch: {path} is not x64";

    internal static (bool found, string path) TryLocateDll() =>
        Locate(Environment.GetEnvironmentVariable("XPE_NATIVE_DIR"), AppContext.BaseDirectory);

    /// <summary>
    /// GUI-C-225 (REQ-GUI-IT-008, 020, 041): the folders the locator looks in, in the order it looks, as ONE list: <c>XPE_NATIVE_DIR</c> when set, the test output directory, and (when a repository
    /// root is found above it) the known build folders. The locator searches exactly this list and REQ-GUI-IT-008's check reads exactly this list, so the two cannot drift apart.
    /// </summary>
    internal static IReadOnlyList<string> CandidateFolders(string? envDir, string baseDirectory, string? repoRoot)
    {
        var folders = new List<string>();
        if (!string.IsNullOrEmpty(envDir)) folders.Add(envDir);
        folders.Add(baseDirectory);
        if (repoRoot is not null)
        {
            folders.Add(Path.Combine(repoRoot, "build", "ci-common", "bin", "Debug"));
            folders.Add(Path.Combine(repoRoot, "build", "ci-common", "bin"));
            folders.Add(Path.Combine(repoRoot, "build", "default", "bin", "Debug"));
            folders.Add(Path.Combine(repoRoot, "build", "default", "bin"));
            folders.Add(Path.Combine(repoRoot, "modules", "common", "build_test", "Debug"));
            folders.Add(Path.Combine(repoRoot, "modules", "common", "build_test", "Release"));
            folders.Add(Path.Combine(repoRoot, "clients", "ImageProcTest", "bin", "Debug", "net8.0-windows", "x64"));
        }

        return folders;
    }

    /// <summary>The candidate folders for this process (the environment, the test output directory, the repository above it).</summary>
    internal static IReadOnlyList<string> CurrentCandidateFolders() =>
        CandidateFolders(Environment.GetEnvironmentVariable("XPE_NATIVE_DIR"), AppContext.BaseDirectory, FindRepositoryRoot(AppContext.BaseDirectory));

    /// <summary>
    /// The locator, with its inputs given: the first candidate folder that holds the DLL wins. When none does, the message names where it looked (REQ-GUI-IT-041: a missing DLL is reported
    /// with the places that were searched, not just "not found").
    /// </summary>
    internal static (bool found, string path) Locate(string? envDir, string baseDirectory)
    {
        var repoRoot = FindRepositoryRoot(baseDirectory);
        var folders = CandidateFolders(envDir, baseDirectory, repoRoot);
        foreach (var folder in folders)
        {
            var candidate = Path.Combine(folder, DllName);
            if (File.Exists(candidate)) return (true, candidate);
        }

        var searched = string.Join("; ", folders);
        return repoRoot is not null
            ? (false, $"Repo root found at {repoRoot} but no {DllName} in any candidate folder. Searched: {searched}")
            : (false, $"Repository root could not be determined from {baseDirectory}, and no {DllName} in the folders that were searched: {searched}");
    }

    /// <summary>
    /// GUI-C-225b (REQ-GUI-IT-008, Codex #121): where a path REALLY is. Every segment of the path that is a symbolic link or a junction is replaced by what it points to (and that target is resolved
    /// the same way), so a file link inside an approved folder, or an approved-looking folder that is a junction to somewhere else, shows its true place. The text comparison in
    /// <see cref="IsUnderAnyFolder"/> alone cannot see that.
    /// </summary>
    internal static string FinalPathOf(string path) => FinalPathOf(path, 0);

    private static string FinalPathOf(string path, int depth)
    {
        var full = Path.GetFullPath(path);
        if (depth > 32) return full;   // a link cycle: stop where we are rather than loop
        var root = Path.GetPathRoot(full) ?? string.Empty;
        var current = root;
        foreach (var segment in full[root.Length..].Split([Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries))
        {
            var next = Path.Combine(current, segment);
            FileSystemInfo info = Directory.Exists(next) ? new DirectoryInfo(next) : new FileInfo(next);
            if (info.LinkTarget is not null && info.ResolveLinkTarget(returnFinalTarget: true) is { } target)
            {
                next = FinalPathOf(target.FullName, depth + 1);
            }

            current = next;
        }

        return current;
    }

    /// <summary>
    /// The check REQ-GUI-IT-008 makes: the file's REAL place (links and junctions followed) is under the REAL place of one of the approved folders. A candidate folder that does not exist cannot hold
    /// the file and is skipped.
    /// </summary>
    internal static bool IsUnderAnyFolderResolved(string path, IEnumerable<string> folders) =>
        IsUnderAnyFolder(FinalPathOf(path), folders.Where(Directory.Exists).Select(f => FinalPathOf(f)));

    /// <summary>True when <paramref name="path"/> is a file under one of <paramref name="folders"/> (whole path segments: a sibling folder that only starts with the same text is not under it).</summary>
    internal static bool IsUnderAnyFolder(string path, IEnumerable<string> folders)
    {
        var full = Path.GetFullPath(path);
        foreach (var folder in folders)
        {
            var prefix = Path.GetFullPath(folder).TrimEnd('\\', '/') + Path.DirectorySeparatorChar;
            if (full.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)) return true;
        }

        return false;
    }

    private static string? FindRepositoryRoot(string start)
    {
        var dir = new DirectoryInfo(start);
        while (dir is not null)
        {
            if (Directory.Exists(Path.Combine(dir.FullName, ".git")) ||
                Directory.Exists(Path.Combine(dir.FullName, "modules", "common")))
            {
                return dir.FullName;
            }
            dir = dir.Parent;
        }
        return null;
    }

    internal static bool VerifyX64Pe(string path)
    {
        try
        {
            using var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            // PE signature: MZ header at offset 0, PE header offset at 0x3C
            var buf = new byte[0x40];
            if (fs.Read(buf, 0, buf.Length) < 0x40) return false;
            if (buf[0] != 'M' || buf[1] != 'Z') return false;
            var peOffset = BitConverter.ToInt32(buf, 0x3C);
            fs.Seek(peOffset + 4, SeekOrigin.Begin); // skip "PE\0\0"
            var machBuf = new byte[2];
            if (fs.Read(machBuf, 0, 2) < 2) return false;
            var machine = BitConverter.ToUInt16(machBuf, 0);
            return machine == 0x8664; // IMAGE_FILE_MACHINE_AMD64
        }
        catch
        {
            return false;
        }
    }

    private IntPtr Resolver(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!string.Equals(libraryName, DllName, StringComparison.OrdinalIgnoreCase))
            return IntPtr.Zero;

        // Reuse the handle opened in the constructor. Zero when the DLL was never
        // loaded, which keeps IsAvailable and P/Invoke resolution on one decision.
        return _handle;
    }

    public void Dispose() { /* DLL lifetime is process-scoped; the handle is not freed */ }
}

/// <summary>xUnit collection definition that shares a single <see cref="NativeLibraryFixture"/>.</summary>
[CollectionDefinition(NativeLibraryCollection.Name)]
public sealed class NativeLibraryCollection : ICollectionFixture<NativeLibraryFixture>
{
    public const string Name = "NativeLibrary";
}
