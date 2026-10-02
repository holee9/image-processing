// #225 (GUI-C-193): the test fault switches exist only in a test build.
namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// User decision (2026-10-02): the two test fault switches are removed, as code, from a shipped (Release) build. The build mechanism is
/// <c>gui/XpeTestFaults.props</c> (the symbol XPE_TEST_FAULTS, defined for Debug only) and every use of the seam sits inside
/// <c>#if XPE_TEST_FAULTS</c>. These read the source, so they see the markup of the rule, not a compiled binary: the binary itself was
/// searched and scanned by the GUI-C-193 evidence (`il_scan.txt`, `byte_search.txt`) and a Release-configuration run of the Functional
/// suite compiles and runs the shipped-build tests of <see cref="AutomationArgsTests"/>. What this class adds is a guard against the way
/// the rule erodes: someone uses the seam outside the guard and the Release build either breaks or, worse, quietly keeps the code.
/// </summary>
[Trait("Category", "Functional")]
public sealed class FaultSeamCompiledOutTests
{
    /// <summary>Names that only the fault seam uses. (The view model's own <c>AiWorkerDisabled</c> property is the module's state, not the seam.)</summary>
    private static readonly string[] SeamNames =
    [
        "FaultInjectingBackend", "--automation-fault", "ai-worker-disabled", "display-pipeline-after", "DisplayPipelineFailAfter",
        "AutomationAiWorkerDisabled", "AutomationDisplayPipelineFailAfter", "AiWorkerDisabledFault", "DisplayPipelineFaultPrefix",
        "AnnounceFaultInjection", "_faultInjectionAnnounced", "FAULT INJECTION ARMED",
    ];

    private static string Root() =>
        Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/ImageProcTest.csproj"))!;

    /// <summary>Every code line of <paramref name="text"/> that names the seam while XPE_TEST_FAULTS is not known to be defined there.</summary>
    internal static List<string> UnguardedSeamLines(string text)
    {
        var violations = new List<string>();
        var stack = new Stack<bool>();   // true: this region is compiled only with XPE_TEST_FAULTS
        var number = 0;
        foreach (var raw in text.Split('\n'))
        {
            number++;
            var line = raw.TrimEnd('\r').Trim();
            if (line.StartsWith("#if", StringComparison.Ordinal))
            {
                stack.Push(line.StartsWith("#if XPE_TEST_FAULTS", StringComparison.Ordinal));
                continue;
            }

            if (line.StartsWith("#else", StringComparison.Ordinal) || line.StartsWith("#elif", StringComparison.Ordinal))
            {
                if (stack.Count > 0)
                {
                    stack.Pop();
                    stack.Push(false);   // the other side of a guard is the shipped side
                }

                continue;
            }

            if (line.StartsWith("#endif", StringComparison.Ordinal))
            {
                if (stack.Count > 0)
                {
                    stack.Pop();
                }

                continue;
            }

            // A plain comment (// ...) names things freely: it is not compiled into anything. A DOC comment (/// ...) is: the documentation file
            // of a shipped build carries it, so it is checked like code.
            if ((line.StartsWith("//", StringComparison.Ordinal) && !line.StartsWith("///", StringComparison.Ordinal)) || line.StartsWith("/*", StringComparison.Ordinal))
            {
                continue;
            }

            if (stack.Any(guarded => guarded))
            {
                continue;
            }

            foreach (var name in SeamNames)
            {
                if (line.Contains(name, StringComparison.Ordinal))
                {
                    violations.Add($"line {number}: {line.Trim()} (names '{name}' outside #if XPE_TEST_FAULTS)");
                    break;
                }
            }
        }

        return violations;
    }

    [Fact]
    public void EveryUseOfTheFaultSeam_InTheApp_IsInsideTheTestBuildGuard()
    {
        var violations = new List<string>();
        foreach (var path in Directory.EnumerateFiles(Root(), "*.cs", SearchOption.AllDirectories))
        {
            var sep = Path.DirectorySeparatorChar;
            if (path.Contains($"{sep}obj{sep}", StringComparison.Ordinal) || path.Contains($"{sep}bin{sep}", StringComparison.Ordinal))
            {
                continue;
            }

            violations.AddRange(UnguardedSeamLines(File.ReadAllText(path)).Select(v => $"{Path.GetRelativePath(Root(), path)}: {v}"));
        }

        Assert.True(violations.Count == 0, "The fault seam is used outside #if XPE_TEST_FAULTS, so a shipped build would keep it:" + Environment.NewLine + string.Join(Environment.NewLine, violations));
    }

    [Fact]
    public void TheGuardCheck_ItselfSeesAnUnguardedUse_AndAcceptsAGuardedOne()
    {
        const string leaked = "var x = new FaultInjectingBackend();\n";
        const string guarded = "#if XPE_TEST_FAULTS\nvar x = new FaultInjectingBackend();\n#else\nvar y = 1;\n#endif\n";
        const string elseSide = "#if XPE_TEST_FAULTS\nvar a = 1;\n#else\nvar x = new FaultInjectingBackend();\n#endif\n";
        const string otherSymbol = "#if DEBUG\nvar x = new FaultInjectingBackend();\n#endif\n";
        const string docComment = "/// <summary>armed by --automation-fault</summary>\npublic int X;\n";
        const string plainComment = "// armed by --automation-fault\npublic int X;\n";

        Assert.Single(UnguardedSeamLines(leaked));
        Assert.Empty(UnguardedSeamLines(guarded));
        Assert.Single(UnguardedSeamLines(elseSide));        // the #else side is the shipped side
        Assert.Single(UnguardedSeamLines(otherSymbol));     // only XPE_TEST_FAULTS counts as the guard
        Assert.Single(UnguardedSeamLines(docComment));      // a doc comment ends up in the shipped documentation file
        Assert.Empty(UnguardedSeamLines(plainComment));     // a plain comment ends up nowhere
    }

    [Fact]
    public void TheFaultFile_IsGuardedAsAWhole_AndNotCompiledOutsideATestBuild()
    {
        var file = File.ReadAllText(Path.Combine(Root(), "Services", "FaultInjectingBackend.cs"));
        var firstCode = file.Split('\n').Select(l => l.Trim()).First(l => !l.StartsWith("//", StringComparison.Ordinal) && l.Length > 0);
        Assert.StartsWith("#if XPE_TEST_FAULTS", firstCode, StringComparison.Ordinal);
        Assert.EndsWith("#endif", file.TrimEnd(), StringComparison.Ordinal);

        var project = File.ReadAllText(Path.Combine(Root(), "ImageProcTest.csproj"));
        Assert.Contains("<ItemGroup Condition=\"'$(XpeTestFaults)' != 'true'\">", project, StringComparison.Ordinal);
        Assert.Contains("<Compile Remove=\"Services\\FaultInjectingBackend.cs\" />", project, StringComparison.Ordinal);
    }

    [Fact]
    public void TheBuildSymbol_IsDefinedOnlyForDebug_AndEveryProjectThatCompilesTheSeamImportsIt()
    {
        var props = File.ReadAllText(Path.Combine(Root(), "..", "XpeTestFaults.props"));
        Assert.Contains("<XpeTestFaults Condition=\"'$(XpeTestFaults)' == '' and '$(Configuration)' == 'Debug'\">true</XpeTestFaults>", props, StringComparison.Ordinal);
        Assert.Contains("<DefineConstants>$(DefineConstants);XPE_TEST_FAULTS</DefineConstants>", props, StringComparison.Ordinal);
        Assert.Contains("Condition=\"'$(XpeTestFaults)' == 'true'\"", props, StringComparison.Ordinal);

        foreach (var project in new[]
        {
            "gui/ImageProcTest/ImageProcTest.csproj",
            "gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj",
            "clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj",
        })
        {
            var text = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile(project));
            Assert.Contains("XpeTestFaults.props\" />", text, StringComparison.Ordinal);
        }
    }

    [Fact]
    public void TheFaultSwitchTests_AreGuarded_AndTheShippedBuildCounterpartsExist()
    {
        var args = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/Functional/AutomationArgsTests.cs"));
        Assert.Contains("#if XPE_TEST_FAULTS", args, StringComparison.Ordinal);
        Assert.Contains("#else", args, StringComparison.Ordinal);
        Assert.Contains("TheFaultSwitch_IsNotRecognised_InAShippedBuild", args, StringComparison.Ordinal);
        Assert.Contains("\"--automation-fault is not a recognised automation switch.\"", args, StringComparison.Ordinal);
    }
}
