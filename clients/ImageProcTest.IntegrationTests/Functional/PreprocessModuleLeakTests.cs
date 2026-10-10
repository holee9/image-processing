using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// GUI-C-233h/j (Codex #177, #178): xpe_preprocess.dll has one process-wide state. A test that initialises it and does not shut it down leaves that state for whichever test runs next, and
/// the next test that expects an uninitialised module (PreprocessHandshakeTests) then fails: <c>xpe_preprocess_init</c> answers INVALID_INPUT while the module is initialised. xUnit v2 orders
/// test collections by a per-run identifier, so WHICH test follows a leaker differs from run to run - the shape of the intermittent failure seen in full runs (the failing runs themselves were
/// not captured, so this is a candidate cause, not a proven one).
///
/// <para>This runs the tests that touch the module, one method at a time, and after each probes the module: if an <c>init</c> is refused, that method left it initialised, and the failure
/// names it. It does not start from a shutdown (that would hide the leak it is looking for). A test that FAILS or throws inside the sweep is reported as a failure of its own, not swallowed.
/// The list is written out, not found by reflection: the safety guard (MockBlockingStaticTests) forbids reflection creation APIs in these sources.</para>
///
/// <para>The list is checked against the code in BOTH directions: <see cref="TheFileList_EqualsTheFilesThatCallTheModule"/> (the files the sweep covers = the files whose calls the
/// collection guard searches for) and <see cref="TheMethodList_EqualsTheTestMethodsOfThoseFiles"/> (every test method is listed, and every listed method still exists).</para>
/// </summary>
[Collection(ImageProcTest.IntegrationTests.Fixtures.PreprocessModuleCollection.Name)]
public sealed class PreprocessModuleLeakTests
{
    // GENERATED from the test methods (GUI-C-233j); the tests below fail when one is added or removed without updating this.
    private static readonly (string Name, Action Run)[] TestsThatTouchTheModule =
    [
        ("DataSizeContractTests.OffsetCorrect_ExactDataSize_PassesSizeGate", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().OffsetCorrect_ExactDataSize_PassesSizeGate()),
        ("DataSizeContractTests.OffsetCorrect_ShortDataSize_ReturnsInvalidInput", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().OffsetCorrect_ShortDataSize_ReturnsInvalidInput()),
        ("DataSizeContractTests.OffsetCorrect_ZeroDataSize_PassesSizeGate", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().OffsetCorrect_ZeroDataSize_PassesSizeGate()),
        ("DataSizeContractTests.LogTransform_ExactDataSize_IsAccepted", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().LogTransform_ExactDataSize_IsAccepted()),
        ("DataSizeContractTests.LogTransform_ShortDataSize_ReturnsInvalidInput", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().LogTransform_ShortDataSize_ReturnsInvalidInput()),
        ("DataSizeContractTests.LogTransform_ZeroDataSize_IsAccepted", () => new ImageProcTest.IntegrationTests.Functional.DataSizeContractTests().LogTransform_ZeroDataSize_IsAccepted()),
        ("GainPolyClampAlertTests.EveryPixelOutsideTheRange_RaisesTheAllVariant_CarryingTheMisloadHint", () => new ImageProcTest.IntegrationTests.P1AReady.GainPolyClampAlertTests(new NullOutput()).EveryPixelOutsideTheRange_RaisesTheAllVariant_CarryingTheMisloadHint()),
        ("GainPolyClampAlertTests.SomePixelsOutsideTheRange_RaiseThePartialVariant_WithoutTheHint", () => new ImageProcTest.IntegrationTests.P1AReady.GainPolyClampAlertTests(new NullOutput()).SomePixelsOutsideTheRange_RaiseThePartialVariant_WithoutTheHint()),
        ("GainPolyClampAlertTests.AScalarGainFile_RaisesNoClampAlert", () => new ImageProcTest.IntegrationTests.P1AReady.GainPolyClampAlertTests(new NullOutput()).AScalarGainFile_RaisesNoClampAlert()),
        ("NonlinearityStageWiringTests.WithoutALut_TheStageRunsAndLeavesTheFrameByteIdentical", () => new ImageProcTest.IntegrationTests.P1AReady.NonlinearityStageWiringTests().WithoutALut_TheStageRunsAndLeavesTheFrameByteIdentical()),
        ("NonlinearityStageWiringTests.ANonLinearPanelWithoutALut_IsRefused", () => new ImageProcTest.IntegrationTests.P1AReady.NonlinearityStageWiringTests().ANonLinearPanelWithoutALut_IsRefused()),
        ("NonlinearityStageWiringTests.TheGuiRunnerReachesTheStages_ThroughThePipelineFunction_NotByCallingThemItself", () => new ImageProcTest.IntegrationTests.P1AReady.NonlinearityStageWiringTests().TheGuiRunnerReachesTheStages_ThroughThePipelineFunction_NotByCallingThemItself()),
        ("PreprocessCorrectionBoundaryTests.Control_AWriteIntoEitherMargin_IsDetected", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().Control_AWriteIntoEitherMargin_IsDetected()),
        ("PreprocessCorrectionBoundaryTests.Offset_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().Offset_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()),
        ("PreprocessCorrectionBoundaryTests.Gain_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().Gain_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()),
        ("PreprocessCorrectionBoundaryTests.Defect_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().Defect_WritesOnlyItsOutputBuffer_AndLeavesItsInputAlone()),
        ("PreprocessCorrectionBoundaryTests.AnOutputBufferDeclaredTooSmall_IsRefused_WithoutWritingBeyondIt", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().AnOutputBufferDeclaredTooSmall_IsRefused_WithoutWritingBeyondIt()),
        ("PreprocessCorrectionBoundaryTests.LoadCalibrated_WhenTheCalibrationStepThrows_LeavesTheModuleUninitialised", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionBoundaryTests().LoadCalibrated_WhenTheCalibrationStepThrows_LeavesTheModuleUninitialised()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_RunTwice_DeterministicRmseIsZero", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_RunTwice_DeterministicRmseIsZero()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_Output_HasNoNanOrInf", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_Output_HasNoNanOrInf()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_InputBuffer_Sha256IsPreserved", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_InputBuffer_Sha256IsPreserved()),
        ("PreprocessCorrectionChainSmokeTests.Control_Sha256Hex_SeesAOneByteChange", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().Control_Sha256Hex_SeesAOneByteChange()),
        ("PreprocessHandshakeTests.PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString()),
        ("PreprocessHandshakeTests.PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds()),
        ("PreprocessHandshakeTests.PreprocessDll_WhenStaged_HasAllRequiredExports", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessDll_WhenStaged_HasAllRequiredExports()),
        ("BaselineReviewFixTests.WhenPreparingTheMapsFailsAfterTheInit_TheModuleIsLeftUninitialised", () => { using var t = new ImageProcTest.IntegrationTests.Functional.BaselineReviewFixTests(); t.WhenPreparingTheMapsFailsAfterTheInit_TheModuleIsLeftUninitialised(); }),
        ("BaselineReviewFixTests.TheRealRunner_ReceivingInvalidInputFromTheRealModule_FailsTheBaseline_WithTheCallAndTheCode", () => { using var t = new ImageProcTest.IntegrationTests.Functional.BaselineReviewFixTests(); t.TheRealRunner_ReceivingInvalidInputFromTheRealModule_FailsTheBaseline_WithTheCallAndTheCode(); }),
    ];

    /// <summary>The files the sweep covers, and for each the method-name prefixes that are swept (null = every test method of the file).</summary>
    private static readonly (string File, string[]? Prefixes)[] CoveredFiles =
    [
        ("Functional/DataSizeContractTests.cs", null),
        ("P1AReady/GainPolyClampAlertTests.cs", null),
        ("P1AReady/NonlinearityStageWiringTests.cs", null),
        ("P1AReady/PreprocessCorrectionBoundaryTests.cs", null),
        ("P1AReady/PreprocessCorrectionChainSmokeTests.cs", null),
        ("P1AReady/PreprocessHandshakeTests.cs", null),
        ("Functional/BaselineReviewFixTests.cs", new[] { "TheRealRunner_", "WhenPreparingTheMaps" }),
    ];

    /// <summary>
    /// Intentional exclusions from the sweep inside the covered files (the reason is the point of the entry): the handshake test that initialises the module ON PURPOSE to show that a second
    /// init is refused. It restores the state itself (finally), and the sweep must not run a test whose job is to hold the module.
    /// </summary>
    private static readonly string[] IntentionallyNotSwept = ["PreprocessHandshakeTests.PreprocessInit_WhileAnotherCallerHoldsTheModuleInitialised_IsRefused_AndSucceedsAfterAShutdown"];

    private static readonly string[] CallMarkers =
    [
        "xpe_preprocess_init(",
        "XpePreprocessNative.InitDelegate>(",
        "GuiPreprocessRunner.Run(",
    ];

    private static readonly Regex TestMethod = new(@"\[(?:SkippableFact|Fact)[^\]]*\]\s*(?:\[[^\]]*\]\s*)*public\s+(?:async\s+Task|Task|void)\s+(\w+)\s*\(\s*\)");

    /// <summary>GainPolyClampAlertTests takes xUnit's output helper; nothing here reads what it writes.</summary>
    private sealed class NullOutput : Xunit.Abstractions.ITestOutputHelper
    {
        public void WriteLine(string message) { }

        public void WriteLine(string format, params object[] args) { }
    }

    private static string ProjectDirectory() =>
        Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;

    /// <summary>Whether the module is uninitialised right now: a first init is accepted (and undone). Returns the code when it is not.</summary>
    private static XpeCommonNative.XpeErrorCode Probe(IntPtr handle)
    {
        Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_init", out var initSym));
        Assert.True(NativeLibrary.TryGetExport(handle, "xpe_preprocess_shutdown", out var shutdownSym));
        var init = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.InitDelegate>(initSym);
        var shutdown = Marshal.GetDelegateForFunctionPointer<XpePreprocessNative.ShutdownDelegate>(shutdownSym);
        var code = init(IntPtr.Zero);
        shutdown();   // undo our own init, and (when the probe was refused) clear the leaker's state so the next method is judged on its own
        return code;
    }

    [SkippableFact]
    public void EveryTestMethodThatTouchesTheModule_LeavesItUninitialised_AndPassesInsideTheSweep()
    {
        var dll = XpePreprocessNative.TryFindDll();
        SkipHelper.SkipIf(dll is null, "Skipped: xpe_preprocess.dll not staged");
        SkipHelper.SkipIf(!NativeLibrary.TryLoad(dll!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {dll}");

        var leakers = new List<string>();
        var failures = new List<string>();
        var ran = 0;
        try
        {
            // the module must be clean before the first method, or the first answer would blame the wrong test
            Probe(handle);
            foreach (var (name, run) in TestsThatTouchTheModule)
            {
                try
                {
                    run();
                    ran++;
                }
                catch (Exception ex) when (ex.GetType().Name.Contains("Skip", StringComparison.Ordinal))
                {
                    // skipped for lack of an input; it did nothing to the module
                }
                catch (Exception ex)
                {
                    // a test that fails here passes in the ordinary run, so the sweep (its order, its caller) changed something: that is a finding of its own, and it still gets probed below
                    failures.Add($"{name}: {ex.GetType().Name}: {(ex.Message.Length > 160 ? ex.Message[..160] : ex.Message).ReplaceLineEndings(" ")}");
                }

                if (Probe(handle) != XpeCommonNative.XpeErrorCode.OK)
                {
                    leakers.Add(name);
                }
            }
        }
        finally
        {
            NativeLibrary.Free(handle);
        }

        Assert.True(ran > 5, $"only {ran} test methods were run: the sweep is not exercising the classes");   // control: the sweep really runs them
        Assert.True(leakers.Count == 0, "these tests leave xpe_preprocess initialised for the next test: " + string.Join(", ", leakers));
        Assert.True(failures.Count == 0, "these tests failed when run by the sweep: " + string.Join(" | ", failures));
    }

    /// <summary>The files whose source calls the module, found the way PreprocessModuleCollectionGuardTests finds them.</summary>
    private static SortedSet<string> FilesThatCallTheModule()
    {
        var project = ProjectDirectory();
        var found = new SortedSet<string>(StringComparer.Ordinal);
        var seen = 0;
        foreach (var file in Directory.EnumerateFiles(project, "*.cs", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(project, file).Replace(Path.DirectorySeparatorChar, '/');
            if (relative.StartsWith("obj/", StringComparison.Ordinal) || relative.StartsWith("bin/", StringComparison.Ordinal) || relative.StartsWith("PInvoke/", StringComparison.Ordinal)
                || relative is "Functional/PreprocessModuleCollectionGuardTests.cs" or "Functional/PreprocessModuleLeakTests.cs")   // the two files that only NAME the markers
            {
                continue;
            }

            seen++;
            var text = File.ReadAllText(file);
            if (CallMarkers.Any(marker => text.Contains(marker, StringComparison.Ordinal)))
            {
                found.Add(relative);
            }
        }

        Assert.True(seen > 50, $"the scan saw only {seen} files: it is not looking at the test project");   // control
        return found;
    }

    [Fact]
    public void TheFileList_EqualsTheFilesThatCallTheModule()
    {
        var searched = FilesThatCallTheModule();
        var covered = CoveredFiles.Select(f => f.File).ToHashSet(StringComparer.Ordinal);
        var notCovered = searched.Where(f => !covered.Contains(f)).ToList();          // calls the module, never swept
        var notSearched = covered.Where(f => !searched.Contains(f)).ToList();         // swept, but no longer calls the module
        Assert.True(notCovered.Count == 0, "files call the preprocess module but are not in the sweep: " + string.Join(", ", notCovered));
        Assert.True(notSearched.Count == 0, "files are in the sweep but no longer call the preprocess module: " + string.Join(", ", notSearched));
    }

    [Fact]
    public void TheMethodList_EqualsTheTestMethodsOfThoseFiles()
    {
        var project = ProjectDirectory();
        var actual = new HashSet<string>(StringComparer.Ordinal);
        foreach (var (file, prefixes) in CoveredFiles)
        {
            var cls = Path.GetFileNameWithoutExtension(file);
            foreach (Match m in TestMethod.Matches(File.ReadAllText(Path.Combine(project, file))))
            {
                var method = m.Groups[1].Value;
                if (prefixes is null || prefixes.Any(p => method.StartsWith(p, StringComparison.Ordinal)))
                {
                    actual.Add($"{cls}.{method}");
                }
            }
        }

        var listed = TestsThatTouchTheModule.Select(t => t.Name).ToList();
        Assert.True(actual.Count > 20, $"the scan found only {actual.Count} test methods: it is not reading the files");   // control
        Assert.Equal(listed.Count, listed.Distinct(StringComparer.Ordinal).Count());                                          // no duplicate names
        var missing = actual.Except(listed).Except(IntentionallyNotSwept).OrderBy(x => x, StringComparer.Ordinal).ToList();   // a test exists that the sweep does not run
        var vanished = listed.Except(actual).OrderBy(x => x, StringComparer.Ordinal).ToList();                                // the sweep lists a test that no longer exists
        var staleExclusions = IntentionallyNotSwept.Where(x => !actual.Contains(x)).ToList();
        Assert.True(missing.Count == 0, "test methods missing from the sweep: " + string.Join(", ", missing));
        Assert.True(vanished.Count == 0, "the sweep lists test methods that no longer exist: " + string.Join(", ", vanished));
        Assert.True(staleExclusions.Count == 0, "an intentional exclusion names a test that no longer exists: " + string.Join(", ", staleExclusions));
    }
}
