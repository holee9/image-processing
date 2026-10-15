// AC-3: DLL resolution is deterministic; AC-8: DLL path within build tree.
using ImageProcTest.IntegrationTests.Fixtures;

namespace ImageProcTest.IntegrationTests.Safety;

/// <summary>
/// Verifies that the DLL resolver uses the project-controlled search path
/// and not arbitrary system directories.
/// Covers REQ-GUI-IT-008, AC-3, AC-9.
/// </summary>
[Trait("Category", "Safety")]
[Collection(NativeLibraryCollection.Name)]
public sealed class DllSearchPathSafetyTests
{
    private readonly NativeLibraryFixture _fixture;

    public DllSearchPathSafetyTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>
    /// REQ-GUI-IT-008: ResolvedDllPath must point to the test output dir or repo build/** tree.
    /// A system PATH load (e.g. C:\Windows\System32) is not acceptable.
    /// </summary>
    [SkippableFact]
    public void ResolvedDllPath_IsUnderBuildTreeOrTestOutput()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var path = _fixture.ResolvedPath;
        Assert.False(string.IsNullOrEmpty(path), "ResolvedPath must be set when DLL is available.");

        // Check it is NOT from a system directory
        var normalized = path.Replace('\\', '/').ToLowerInvariant();
        Assert.DoesNotContain("windows/system32", normalized);
        Assert.DoesNotContain("windows/syswow64", normalized);

        // Must be a real file
        Assert.True(File.Exists(path), $"Resolved path does not exist on disk: {path}");

        // GUI-C-225 (REQ-GUI-IT-008, the wording decided 2026-10-03): under one of the locator's candidate folders. The list is the locator's own (NativeLibraryFixture.CandidateFolders),
        // so a candidate added to or removed from the locator changes this check with it, and the next test holds that list to the folder classes the requirement names.
        Assert.True(NativeLibraryFixture.IsUnderAnyFolderResolved(path, NativeLibraryFixture.CurrentCandidateFolders()),
            $"the resolved DLL {path} (really {NativeLibraryFixture.FinalPathOf(path)}) is not under any of the locator's candidate folders: {string.Join("; ", NativeLibraryFixture.CurrentCandidateFolders())}");
    }

    /// <summary>
    /// GUI-C-225b (REQ-GUI-IT-008, Codex #121): the module that is really in this process is under an approved folder too, after the links in its path are followed. The path check above is made BEFORE
    /// the load; this one reads the loader's own answer AFTER it.
    /// </summary>
    [SkippableFact]
    public void TheLoadedModule_IsReallyUnderAnApprovedFolder_WhenItsLinksAreFollowed()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);
        var loaded = System.Diagnostics.Process.GetCurrentProcess().Modules.Cast<System.Diagnostics.ProcessModule>()
            .Where(m => string.Equals(m.ModuleName, "xpe_common.dll", StringComparison.OrdinalIgnoreCase))
            .Select(m => m.FileName)
            .ToList();
        Assert.NotEmpty(loaded);
        foreach (var module in loaded)
        {
            Assert.True(NativeLibraryFixture.IsUnderAnyFolderResolved(module, NativeLibraryFixture.CurrentCandidateFolders()),
                $"the loaded module {module} (really {NativeLibraryFixture.FinalPathOf(module)}) is not under any approved folder");
        }
    }

    /// <summary>
    /// GUI-C-225b (REQ-GUI-IT-008, Codex #121): each approved kind of folder with its allowed RELATIVE path, the search ORDER and the COUNT, as an independent expectation (written here, not read
    /// from the locator). A candidate added, removed, renamed or reordered inside a kind changes the list and fails here; the kind-level test cannot see that. The falsification arms
    /// (a candidate added, one removed) are in the GUI-C-225b report.
    /// </summary>
    [Fact]
    public void TheCandidateList_IsExactlyTheseFolders_InThisOrder()
    {
        const string env = @"C:\xpe\native";
        const string output = @"C:\xpe\out";
        const string repo = @"C:\xpe\repo";
        string[] repoFolders =
        [
            @"C:\xpe\repo\build\ci-common\bin\Debug",
            @"C:\xpe\repo\build\ci-common\bin",
            @"C:\xpe\repo\build\default\bin\Debug",
            @"C:\xpe\repo\build\default\bin",
            @"C:\xpe\repo\modules\common\build_test\Debug",
            @"C:\xpe\repo\modules\common\build_test\Release",
            @"C:\xpe\repo\clients\ImageProcTest\bin\Debug\net8.0-windows\x64",
        ];

        Assert.Equal([env, output, .. repoFolders], NativeLibraryFixture.CandidateFolders(env, output, repo));
        Assert.Equal([output, .. repoFolders], NativeLibraryFixture.CandidateFolders(null, output, repo));
        Assert.Equal([env, output], NativeLibraryFixture.CandidateFolders(env, output, null));
        Assert.Equal([output], NativeLibraryFixture.CandidateFolders("", output, null));
        Assert.Equal(9, NativeLibraryFixture.CandidateFolders(env, output, repo).Count);
    }

    // ---------------------------------------------------------------- links and junctions (Codex #121)

    private static string MakeFolder(string root, string name)
    {
        var path = Path.Combine(root, name);
        Directory.CreateDirectory(path);
        return path;
    }

    /// <summary>A directory junction needs no privilege (<c>mklink /J</c>); null when it could not be made.</summary>
    private static string? TryMakeJunction(string link, string target)
    {
        var info = new System.Diagnostics.ProcessStartInfo("cmd.exe", $"/c mklink /J \"{link}\" \"{target}\"") { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        using var process = System.Diagnostics.Process.Start(info)!;
        var text = process.StandardOutput.ReadToEnd() + process.StandardError.ReadToEnd();
        process.WaitForExit();
        return process.ExitCode == 0 && Directory.Exists(link) ? null : text;
    }

    /// <summary>
    /// GUI-C-225b: a junction INSIDE an approved folder that points OUTSIDE it. The text comparison says the file is inside (that is the weakness Codex #121 reproduced); the check that follows links
    /// says it is not. The controls: a plain file in the approved folder, and a junction that stays inside it, are accepted.
    /// </summary>
    [SkippableFact]
    public void AJunctionInsideAnApprovedFolder_ThatPointsOutside_IsRejected_AndOneThatStaysInsideIsAccepted()
    {
        var root = Path.Combine(Path.GetTempPath(), $"xpe_c225b_junction_{Guid.NewGuid():N}");
        try
        {
            var approved = MakeFolder(root, "approved");
            var outside = MakeFolder(root, "outside");
            var inner = MakeFolder(approved, "real_inner");
            File.WriteAllBytes(Path.Combine(outside, "xpe_common.dll"), [0x4D, 0x5A]);
            File.WriteAllBytes(Path.Combine(approved, "xpe_common.dll"), [0x4D, 0x5A]);
            File.WriteAllBytes(Path.Combine(inner, "xpe_common.dll"), [0x4D, 0x5A]);
            var failure = TryMakeJunction(Path.Combine(approved, "link_out"), outside);
            if (failure is null) failure = TryMakeJunction(Path.Combine(approved, "link_in"), inner);
            Skip.If(failure is not null, "a directory junction could not be created here: " + failure);

            IReadOnlyList<string> folders = [approved];
            var viaOutside = Path.Combine(approved, "link_out", "xpe_common.dll");
            Assert.True(NativeLibraryFixture.IsUnderAnyFolder(viaOutside, folders), "the text check is expected to be fooled by the junction (that is why it is not enough)");
            Assert.False(NativeLibraryFixture.IsUnderAnyFolderResolved(viaOutside, folders), "a file reached through a junction that points outside the approved folder must be rejected");

            Assert.True(NativeLibraryFixture.IsUnderAnyFolderResolved(Path.Combine(approved, "xpe_common.dll"), folders), "a plain file in the approved folder must be accepted");
            Assert.True(NativeLibraryFixture.IsUnderAnyFolderResolved(Path.Combine(approved, "link_in", "xpe_common.dll"), folders), "a junction that stays inside the approved folder must be accepted");
        }
        finally
        {
            RemoveTree(root, Path.Combine(root, "approved", "link_out"), Path.Combine(root, "approved", "link_in"));
        }
    }

    /// <summary>Removes the junctions themselves first (a recursive delete of a folder with a junction in it is refused), then the folder.</summary>
    private static void RemoveTree(string root, params string[] junctions)
    {
        foreach (var junction in junctions)
        {
            try { if (Directory.Exists(junction)) Directory.Delete(junction, recursive: false); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* temp folder */ }
        }

        try { Directory.Delete(root, recursive: true); } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { /* temp folder */ }
    }

    /// <summary>GUI-C-225b: the same for a FILE link (needs the symbolic-link privilege; the CI runner is elevated, a plain local shell may not be, and then this skips with the reason).</summary>
    [SkippableFact]
    public void AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected()
    {
        var root = Path.Combine(Path.GetTempPath(), $"xpe_c225b_symlink_{Guid.NewGuid():N}");
        try
        {
            var approved = MakeFolder(root, "approved");
            var outside = MakeFolder(root, "outside");
            File.WriteAllBytes(Path.Combine(outside, "xpe_common.dll"), [0x4D, 0x5A]);
            var link = Path.Combine(approved, "xpe_common.dll");
            try
            {
                File.CreateSymbolicLink(link, Path.Combine(outside, "xpe_common.dll"));
            }
            catch (Exception ex) when (ex is UnauthorizedAccessException or IOException)
            {
                Skip.If(true, "a file symbolic link could not be created here (needs developer mode or elevation): " + ex.Message);
            }

            IReadOnlyList<string> folders = [approved];
            Assert.True(File.Exists(link));
            Assert.True(NativeLibraryFixture.IsUnderAnyFolder(link, folders), "the text check is expected to be fooled by the file link");
            Assert.False(NativeLibraryFixture.IsUnderAnyFolderResolved(link, folders), "a file link that points outside the approved folder must be rejected");
        }
        finally
        {
            try { Directory.Delete(root, recursive: true); } catch (IOException) { /* temp folder */ }
        }
    }

    /// <summary>
    /// REQ-GUI-IT-008, the three inputs the requirement says must FAIL: a decoy earlier on PATH, a system folder, and an arbitrary folder outside the candidates. The check is a function of its
    /// inputs, so it is held against them directly (no native DLL needed); the positive control is a file under each kind of candidate.
    /// </summary>
    [Fact]
    public void TheCandidateCheck_RejectsADecoyOnPath_ASystemFolder_AndAnArbitraryFolder_AndAcceptsACandidate()
    {
        var candidates = NativeLibraryFixture.CandidateFolders(@"C:\xpe\native", @"C:\xpe\out", @"C:\xpe\repo");

        var decoyDir = Path.Combine(Path.GetTempPath(), $"xpe_decoy_{Guid.NewGuid():N}");
        var pathWithDecoyFirst = decoyDir + ";" + Environment.GetEnvironmentVariable("PATH");
        Assert.StartsWith(decoyDir, pathWithDecoyFirst, StringComparison.Ordinal);   // the decoy is "first on PATH"; being on PATH does not make a folder a candidate
        Assert.False(NativeLibraryFixture.IsUnderAnyFolder(Path.Combine(decoyDir, "xpe_common.dll"), candidates), "a decoy folder first on PATH must not be accepted");
        Assert.False(NativeLibraryFixture.IsUnderAnyFolder(Path.Combine(Environment.SystemDirectory, "xpe_common.dll"), candidates), "a system folder must not be accepted");
        Assert.False(NativeLibraryFixture.IsUnderAnyFolder(@"C:\somewhere\else\xpe_common.dll", candidates), "an arbitrary folder outside the candidates must not be accepted");
        Assert.False(NativeLibraryFixture.IsUnderAnyFolder(@"C:\xpe\repo\build-old\xpe_common.dll", candidates), "a sibling that only starts like a candidate is not under it");
        Assert.False(NativeLibraryFixture.IsUnderAnyFolder(@"C:\xpe\repo\build\other\xpe_common.dll", candidates), "a folder next to the known build folders is not a candidate");

        foreach (var accepted in new[]
        {
            @"C:\xpe\native\xpe_common.dll",
            @"C:\xpe\out\xpe_common.dll",
            @"C:\xpe\repo\build\ci-common\bin\Debug\xpe_common.dll",
            @"C:\xpe\repo\modules\common\build_test\Release\xpe_common.dll",
            @"C:\xpe\repo\clients\ImageProcTest\bin\Debug\net8.0-windows\x64\xpe_common.dll",
        })
        {
            Assert.True(NativeLibraryFixture.IsUnderAnyFolder(accepted, candidates), $"{accepted} is in a candidate folder and must be accepted");
        }
    }

    /// <summary>
    /// REQ-GUI-IT-008: the locator's candidate list is the five kinds of folder the requirement names (XPE_NATIVE_DIR, repo build/, modules/common/build_test, the test output directory,
    /// clients/ImageProcTest/bin) and nothing else. The kinds are written here from the requirement, not read from the locator: if the locator gains a folder the requirement does not name (or
    /// loses one), this fails, and the requirement or the locator has to be changed on purpose.
    /// </summary>
    [Fact]
    public void TheCandidateList_IsExactlyTheFiveKindsOfFolderTheRequirementNames()
    {
        const string env = @"C:\xpe\native";
        const string output = @"C:\xpe\out";
        const string repo = @"C:\xpe\repo";
        var candidates = NativeLibraryFixture.CandidateFolders(env, output, repo);

        var kinds = new (string Name, string Root)[]
        {
            ("XPE_NATIVE_DIR", env),
            ("test output directory", output),
            ("<repo>/build/", Path.Combine(repo, "build")),
            ("modules/common/build_test", Path.Combine(repo, "modules", "common", "build_test")),
            ("clients/ImageProcTest/bin", Path.Combine(repo, "clients", "ImageProcTest", "bin")),
        };

        foreach (var candidate in candidates)
        {
            Assert.True(kinds.Any(k => NativeLibraryFixture.IsUnderAnyFolder(Path.Combine(candidate, "x.dll"), [k.Root])),
                $"the locator searches {candidate}, which is not one of the folder kinds REQ-GUI-IT-008 names");
        }

        foreach (var kind in kinds)
        {
            Assert.True(candidates.Any(c => NativeLibraryFixture.IsUnderAnyFolder(Path.Combine(c, "x.dll"), [kind.Root])),
                $"the requirement names {kind.Name}, but the locator does not search it");
        }

        Assert.Empty(NativeLibraryFixture.CandidateFolders(null, output, null).Where(c => c != output));   // without an environment folder or a repository, only the output directory is left
    }

    /// <summary>
    /// AC-9 (structural): When a decoy xpe_common.dll is placed in a temp directory
    /// that is on the PATH, the fixture's resolver must still win (env var or AppContext takes priority).
    /// This test creates a temp decoy, adds it to PATH, and verifies our DLL path is unchanged.
    /// </summary>
    /// <summary>The DLL that is actually in the process is the resolved file, and the decoy never was one.</summary>
    [SkippableFact]
    public void TheLoadedModule_IsTheResolvedFile()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);
        var loaded = System.Diagnostics.Process.GetCurrentProcess().Modules.Cast<System.Diagnostics.ProcessModule>()
            .Where(m => string.Equals(m.ModuleName, "xpe_common.dll", StringComparison.OrdinalIgnoreCase))
            .Select(m => m.FileName)
            .ToList();
        Assert.Contains(loaded, f => string.Equals(f, _fixture.ResolvedPath, StringComparison.OrdinalIgnoreCase));
        Assert.DoesNotContain(loaded, f => f.Contains("xpe_decoy_", StringComparison.OrdinalIgnoreCase));
    }

    [SkippableFact]
    public void ADecoyEarlierOnPath_IsNotWhatTheLocatorReturns_WhenItIsPlacedBeforeTheLocatorRuns()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var tempDir = Path.Combine(Path.GetTempPath(), $"xpe_decoy_{Guid.NewGuid():N}");
        Directory.CreateDirectory(tempDir);
        var decoyPath = Path.Combine(tempDir, "xpe_common.dll");

        try
        {
            // Write a 2-byte "decoy" (not a valid PE, load will fail for it).
            File.WriteAllBytes(decoyPath, new byte[] { 0x4D, 0x5A }); // MZ only, not a real DLL

            // Prepend decoy dir to PATH
            var originalPath = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
            Environment.SetEnvironmentVariable("PATH", tempDir + ";" + originalPath);
            try
            {
                // GUI-C-208 (D3): the old version compared the fixture's path to the decoy AFTER the fixture had resolved (once, at session start), so the decoy could not have
                // influenced it. Here the locator runs again with the decoy already first on PATH.
                var (found, located) = NativeLibraryFixture.TryLocateDll();
                Assert.True(found, $"the locator found nothing with a decoy on PATH: {located}");
                Assert.NotEqual(decoyPath, located, StringComparer.OrdinalIgnoreCase);
                Assert.Equal(_fixture.ResolvedPath, located, StringComparer.OrdinalIgnoreCase);
            }
            finally
            {
                Environment.SetEnvironmentVariable("PATH", originalPath);
            }
        }
        finally
        {
            if (File.Exists(decoyPath)) File.Delete(decoyPath);
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, recursive: true);
        }
    }
}
