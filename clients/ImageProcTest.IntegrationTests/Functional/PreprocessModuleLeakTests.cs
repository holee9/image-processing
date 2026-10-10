using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// GUI-C-233h (Codex #177, finding 3): xpe_preprocess.dll has one process-wide state. A test that initialises it and does not shut it down leaves that state for whichever test runs next, and
/// the next test that expects an uninitialised module (PreprocessHandshakeTests) then fails: <c>xpe_preprocess_init</c> answers INVALID_INPUT while the module is initialised. xUnit v2 orders
/// test collections by a per-run identifier, so WHICH test follows a leaker differs from run to run - the shape of the intermittent failure seen in full runs (the failing runs themselves were
/// not captured, so this is a candidate cause, not a proven one).
///
/// <para>This runs the tests of the classes that touch the module, one method at a time, and after each probes the module: if an <c>init</c> is refused, that method left it initialised, and the
/// failure names it. It does not start from a shutdown (that would hide the leak it is looking for). The list is written out, not found by reflection: the safety guard
/// (MockBlockingStaticTests) forbids reflection creation APIs in these sources. <see cref="TheList_CoversEveryTestMethodOfTheClasses"/> keeps the list complete.</para>
/// </summary>
[Collection(ImageProcTest.IntegrationTests.Fixtures.PreprocessModuleCollection.Name)]
public sealed class PreprocessModuleLeakTests
{
    // GENERATED from the six classes' test methods (GUI-C-233h); the completeness test below fails when one is added or removed without updating this.
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
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_WithoutCalibration_ReturnsCalibNotLoaded()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_RunTwice_DeterministicRmseIsZero", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_RunTwice_DeterministicRmseIsZero()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_Output_HasNoNanOrInf", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_Output_HasNoNanOrInf()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_DefectStage_CorrectsExactlyTheMarkedPixels_AndLeavesTheRestBitIdentical()),
        ("PreprocessCorrectionChainSmokeTests.CorrectionChain_InputBuffer_Sha256IsPreserved", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().CorrectionChain_InputBuffer_Sha256IsPreserved()),
        ("PreprocessCorrectionChainSmokeTests.Control_Sha256Hex_SeesAOneByteChange", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessCorrectionChainSmokeTests().Control_Sha256Hex_SeesAOneByteChange()),
        ("PreprocessHandshakeTests.PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessVersion_WhenDllStaged_ReturnsNonEmptyString()),
        ("PreprocessHandshakeTests.PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessInitShutdown_WhenDllStaged_LifecycleSucceeds()),
        ("PreprocessHandshakeTests.PreprocessDll_WhenStaged_HasAllRequiredExports", () => new ImageProcTest.IntegrationTests.P1AReady.PreprocessHandshakeTests().PreprocessDll_WhenStaged_HasAllRequiredExports()),
    ];

    private static readonly string[] ClassFiles =
    [
        "Functional/DataSizeContractTests.cs",
        "P1AReady/GainPolyClampAlertTests.cs",
        "P1AReady/NonlinearityStageWiringTests.cs",
        "P1AReady/PreprocessCorrectionBoundaryTests.cs",
        "P1AReady/PreprocessCorrectionChainSmokeTests.cs",
        "P1AReady/PreprocessHandshakeTests.cs",
    ];

    /// <summary>GainPolyClampAlertTests takes xUnit's output helper; nothing here reads what it writes.</summary>
    private sealed class NullOutput : Xunit.Abstractions.ITestOutputHelper
    {
        public void WriteLine(string message) { }

        public void WriteLine(string format, params object[] args) { }
    }

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
    public void EveryTestMethodOfTheModuleClasses_LeavesTheModuleUninitialised()
    {
        var dll = XpePreprocessNative.TryFindDll();
        SkipHelper.SkipIf(dll is null, "Skipped: xpe_preprocess.dll not staged");
        SkipHelper.SkipIf(!NativeLibrary.TryLoad(dll!, out var handle), $"Skipped: xpe_preprocess.dll load failed: {dll}");

        var leakers = new List<string>();
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
                catch (Exception)
                {
                    // a failing test is somebody else's finding; what matters here is what it left behind
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
    }

    [Fact]
    public void TheList_CoversEveryTestMethodOfTheClasses()
    {
        var project = Path.GetDirectoryName(BenchmarkRunnerServiceTests.ResolveRepositoryFile("clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj"))!;
        var pattern = new System.Text.RegularExpressions.Regex(@"\[(?:SkippableFact|Fact)[^\]]*\]\s*(?:\[[^\]]*\]\s*)*public\s+(?:async\s+Task|Task|void)\s+(\w+)\s*\(\s*\)");
        var listed = TestsThatTouchTheModule.Select(t => t.Name).ToHashSet(StringComparer.Ordinal);
        var missing = new List<string>();
        var seen = 0;
        foreach (var file in ClassFiles)
        {
            var text = File.ReadAllText(Path.Combine(project, file));
            var cls = Path.GetFileNameWithoutExtension(file);
            foreach (System.Text.RegularExpressions.Match m in pattern.Matches(text))
            {
                seen++;
                var name = $"{cls}.{m.Groups[1].Value}";
                if (!listed.Contains(name) && !name.Contains("WhileAnotherCallerHolds", StringComparison.Ordinal))
                {
                    missing.Add(name);
                }
            }
        }

        Assert.True(seen > 20, $"the scan found only {seen} test methods: it is not reading the classes");   // control
        Assert.True(missing.Count == 0, "test methods missing from PreprocessModuleLeakTests.TestsThatTouchTheModule: " + string.Join(", ", missing));
        Assert.Equal(listed.Count, TestsThatTouchTheModule.Length);   // no duplicate names
    }
}
