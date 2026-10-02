// #225 row 9 (GUI-C-196 M2): the enhance_basic stage's logic against a fake module: order, parameters, the one conversion, rounding, all-or-nothing.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class EnhanceBasicStageTests
{
    /// <summary>A scripted module: records every call with its parameters, can refuse a step, return garbage, or throw.</summary>
    private sealed class FakeBackend(float[]? working = null) : IEnhanceBasicBackend
    {
        public readonly List<string> Calls = [];
        public int Opens;
        public int Reads;
        public int Disposes;
        public string? RefuseStep;
        public int RefuseCode = -1;
        public string? PoisonAfter;
        public string? ThrowAt;
        public bool OpenThrows;
        public Func<float[], float[]>? Transform;

        public IEnhanceImage Open(ushort[] input, int width, int height)
        {
            Opens++;
            if (OpenThrows)
            {
                throw new InvalidOperationException("no working image");
            }

            return new FakeImage(this, working ?? input.Select(v => (float)v).ToArray());
        }

        private sealed class FakeImage(FakeBackend owner, float[] data) : IEnhanceImage
        {
            private float[] _data = data;

            private int Step(string name, string call)
            {
                owner.Calls.Add(call);
                if (owner.ThrowAt == name)
                {
                    throw new InvalidOperationException($"{name} blew up");
                }

                if (owner.PoisonAfter == name)
                {
                    _data = (float[])_data.Clone();
                    _data[0] = float.NaN;
                }

                return owner.RefuseStep == name ? owner.RefuseCode : 0;
            }

            public int LogTransform(float normFactor) => Step("log", $"log({normFactor})");

            public int NoiseReduceBilateral(float sigmaSpace, float sigmaRange) => Step("noise", $"noise({sigmaSpace},{sigmaRange})");

            public int ContrastEnhance(float clipLimit, int tileWidth, int tileHeight) => Step("contrast", $"contrast({clipLimit},{tileWidth},{tileHeight})");

            public int EdgeEnhance(float amount, float radius, float threshold) => Step("edge", $"edge({amount},{radius},{threshold})");

            public long CountNonFinite() => _data.Count(v => !float.IsFinite(v));

            public float[] ReadFloats()
            {
                owner.Reads++;
                return owner.Transform is { } t ? t(_data) : (float[])_data.Clone();
            }

            public void Dispose() => owner.Disposes++;
        }
    }

    private static ushort[] Input(int count = 4) => Enumerable.Range(0, count).Select(i => (ushort)(i * 10)).ToArray();

    [Fact]
    public void TheStepsRunInOrder_OnceEach_WithTheFixedParameters()
    {
        var fake = new FakeBackend();
        var result = EnhanceBasicStage.Run(Input(), 2, 2, fake);

        Assert.True(result.Ran);
        Assert.Equal(
            [
                $"log({BaselineParameters.LogNormFactor})",
                $"noise({BaselineParameters.NoiseSigmaSpace},{BaselineParameters.NoiseSigmaRange})",
                $"contrast({BaselineParameters.ClaheClipLimit},{BaselineParameters.ClaheTileWidth},{BaselineParameters.ClaheTileHeight})",
                $"edge({BaselineParameters.UsmAmount},{BaselineParameters.UsmRadius},{BaselineParameters.UsmThreshold})",
            ],
            fake.Calls);
        Assert.Equal((1, 1, 1), (fake.Opens, fake.Reads, fake.Disposes));   // ONE working image, read back ONCE (design D2), always released
        Assert.Equal(0, result.NaNInfCount);
        Assert.Contains("log ok", result.Summary, StringComparison.Ordinal);
        Assert.Contains("edge(usm 0.5/2/10) ok", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void TheConversionBackTo16Bits_RoundsHalfToEven_AndClampsAndCountsWhatItClamped()
    {
        // 0.5 -> 0, 1.5 -> 2, 2.5 -> 2, 3.5 -> 4 (half to even); -0.4 rounds to -0 and is 0; -1 clamps low; 65535.4 -> 65535;
        // 65535.5 rounds to the even 65536 and clamps high; 70000 clamps high; 65534.5 -> 65534 (even).
        var floats = new float[] { 0.5f, 1.5f, 2.5f, 3.5f, -0.4f, -1f, 65535.4f, 65535.5f, 70000f, 65534.5f };
        var fake = new FakeBackend(floats);
        var result = EnhanceBasicStage.Run(new ushort[10], 5, 2, fake);

        Assert.True(result.Ran);
        Assert.Equal(new ushort[] { 0, 2, 2, 4, 0, 0, 65535, 65535, 65535, 65534 }, result.Pixels);
        Assert.Contains("round-half-even, clamped below 1 above 2", result.Summary, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData("log", 0)]
    [InlineData("noise", 1)]
    [InlineData("contrast", 2)]
    [InlineData("edge", 3)]
    public void AStepTheModuleRefuses_RefusesTheWholeStage_AndTheLaterStepsDoNotRun(string step, int index)
    {
        var fake = new FakeBackend { RefuseStep = step, RefuseCode = -1 };
        var result = EnhanceBasicStage.Run(Input(), 2, 2, fake);

        Assert.False(result.Ran);
        Assert.Null(result.Pixels);                           // no partial result
        Assert.Equal(index + 1, fake.Calls.Count);            // the refused step ran; nothing after it did
        Assert.Contains(step, result.Summary, StringComparison.Ordinal);
        Assert.Contains("return code -1", result.Summary, StringComparison.Ordinal);
        Assert.Equal(0, fake.Reads);                          // nothing was converted
        Assert.Equal(1, fake.Disposes);                       // the buffer is released on the failure path too
    }

    [Fact]
    public void ANonFiniteValueLeftByAStep_RefusesTheStage_AndIsCounted()
    {
        var fake = new FakeBackend { PoisonAfter = "noise" };
        var result = EnhanceBasicStage.Run(Input(), 2, 2, fake);

        Assert.False(result.Ran);
        Assert.Null(result.Pixels);
        Assert.Equal(1, result.NaNInfCount);
        Assert.Contains("noise", result.Summary, StringComparison.Ordinal);
        Assert.Contains("non-finite", result.Summary, StringComparison.Ordinal);
        Assert.Equal(2, fake.Calls.Count);                    // log, noise; contrast and edge did not run on a poisoned image
        Assert.Equal(0, fake.Reads);
    }

    [Fact]
    public void AStepThatThrows_IsARefusalWithItsMessage()
    {
        var fake = new FakeBackend { ThrowAt = "contrast" };
        var result = EnhanceBasicStage.Run(Input(), 2, 2, fake);

        Assert.False(result.Ran);
        Assert.Contains("contrast", result.Summary, StringComparison.Ordinal);
        Assert.Contains("contrast blew up", result.Summary, StringComparison.Ordinal);
        Assert.Equal(1, fake.Disposes);
    }

    [Fact]
    public void AnInputThatIsNotWidthTimesHeight_IsRefused_BeforeAnythingIsOpened()
    {
        var fake = new FakeBackend();
        var result = EnhanceBasicStage.Run(Input(5), 2, 2, fake);

        Assert.False(result.Ran);
        Assert.Equal(0, fake.Opens);
    }

    [Fact]
    public void AWorkingImageThatCannotBeOpened_IsARefusal()
    {
        var result = EnhanceBasicStage.Run(Input(), 2, 2, new FakeBackend { OpenThrows = true });
        Assert.False(result.Ran);
        Assert.Contains("could not open", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void AWorkingImageOfTheWrongSize_IsARefusal()
    {
        var fake = new FakeBackend { Transform = d => d.Take(2).ToArray() };
        var result = EnhanceBasicStage.Run(Input(), 2, 2, fake);
        Assert.False(result.Ran);
        Assert.Contains("2 pixels, not 4", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void TheSameInputTwice_GivesTheSameOutput_WhenTheModuleIsDeterministic()
    {
        var input = Input(16);
        var first = EnhanceBasicStage.Run(input, 4, 4, new FakeBackend(input.Select(v => v * 1.37f).ToArray()));
        var second = EnhanceBasicStage.Run(input, 4, 4, new FakeBackend(input.Select(v => v * 1.37f).ToArray()));

        Assert.True(BaselineDeterminism.Compare(first.Pixels!, second.Pixels!).Identical);
    }

    // ---- the fixed parameters are the module's documented defaults ----------------------------------------------------------------------

    [Fact]
    public void TheFixedParameters_AreTheDefaultsTheModuleHeaderDocuments()
    {
        var header = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("modules/enhance_basic/include/xpe/enhance_basic/enhance_basic_api.h"));

        // The header's own words for each default (so the numbers below are checked against the header, not only against themselves).
        Assert.Matches(@"sigma_space;[^\n]*\(default 3\.0\)", header);
        Assert.Matches(@"sigma_range;[^\n]*\(default 50\.0\)", header);
        Assert.Matches(@"clip_limit;[^\n]*\(default 3\.0\)", header);
        Assert.Matches(@"tile_width;[^\n]*\(default 8\)", header);
        Assert.Matches(@"tile_height;[^\n]*\(default 8\)", header);
        Assert.Matches(@"amount;[^\n]*\(default 0\.5\)", header);
        Assert.Matches(@"radius;[^\n]*\(default 2\.0\)", header);
        Assert.Matches(@"threshold;[^\n]*\(default 10\.0\)", header);

        Assert.Equal((3.0f, 50.0f), (BaselineParameters.NoiseSigmaSpace, BaselineParameters.NoiseSigmaRange));
        Assert.Equal((3.0f, 8, 8), (BaselineParameters.ClaheClipLimit, BaselineParameters.ClaheTileWidth, BaselineParameters.ClaheTileHeight));
        Assert.Equal((0.5f, 2.0f, 10.0f), (BaselineParameters.UsmAmount, BaselineParameters.UsmRadius, BaselineParameters.UsmThreshold));
    }

    // ---- the wiring (source reading: it sees this tree's text only; the Native E2E is what runs it) -------------------------------------

    [Fact]
    public void TheEnhanceStage_IsWiredIntoTheRealBackend_TheResolver_AndRefusedByTheMock()
    {
        var real = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/RealXpeBackend.cs"));
        Assert.Contains("StageIds.EnhanceBasic => RunEnhanceBasicStage(input, rawFrame.Width, rawFrame.Height),", real, StringComparison.Ordinal);
        Assert.Contains("EnhanceBasicStage.Run(input, width, height, new Native.NativeEnhanceBasicBackend())", real, StringComparison.Ordinal);

        var resolver = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs"));
        Assert.Contains("_ when Is(libraryName, EnhanceBasicDll) => NativeModuleLibraryLocator.GetDllCandidates(EnhanceBasicDll, \"image-processing\")", resolver, StringComparison.Ordinal);

        var mock = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/MockXpeBackend.cs"));
        Assert.Contains("StageIds.EnhanceBasic => new StageExecution(false, null, \"Basic enhancement requires the native backend", mock, StringComparison.Ordinal);

        // the ordinary plan never runs it (D3): not one mention in BuildStages
        var plan = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/ProcessingChainPlan.cs"));
        var ordinary = plan[plan.IndexOf("public static IReadOnlyList<StageRequest> BuildStages(", StringComparison.Ordinal)..plan.IndexOf("public static IReadOnlyList<StageRequest> BuildBaselineStages()", StringComparison.Ordinal)];
        Assert.DoesNotContain("EnhanceBasic", ordinary, StringComparison.Ordinal);
    }
}
