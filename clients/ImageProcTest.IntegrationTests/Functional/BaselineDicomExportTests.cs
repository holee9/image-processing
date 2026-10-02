// #225 row 9 (GUI-C-196 M3): the DICOM export's verdict against a scripted module — each way the file can be wrong, and what is reported.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

[Trait("Category", "Functional")]
public sealed class BaselineDicomExportTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "xpe-baseline-dicom-" + Guid.NewGuid().ToString("N"));

    public void Dispose()
    {
        try
        {
            Directory.Delete(_root, recursive: true);
        }
        catch (IOException)
        {
        }
    }

    private static readonly BaselineDicomMetadata Meta = new("CHEST", 120f, 0.15f);
    private static readonly ushort[] Pixels = [0, 1, 40000, 65535, 7, 8];

    private sealed class FakeSession : IDicomSession
    {
        public int WriteCode;
        public bool WriteThrows;
        public int ValidateCode;
        public string Report = "{\"valid\":true,\"errors\":[],\"warnings\":[]}";
        public bool ValidateThrows;
        public Func<ushort[], ushort[]> Stored = p => (ushort[])p.Clone();
        public DicomReadBack? ReadOverride;
        public BaselineDicomMetadata? StoredMeta = Meta;
        public readonly List<string> Calls = [];

        public int Write(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata)
        {
            Calls.Add("write");
            if (WriteThrows)
            {
                throw new IOException("disk full");
            }

            return WriteCode;
        }

        public (int Code, string Json) Validate(string path)
        {
            Calls.Add("validate");
            if (ValidateThrows)
            {
                throw new InvalidOperationException("validator exploded");
            }

            return (ValidateCode, Report);
        }

        public DicomReadBack ReadBack(string path)
        {
            Calls.Add("read");
            return ReadOverride ?? new DicomReadBack(0, 0, 0, 3, 2, Stored(Pixels), StoredMeta, null);
        }
    }

    private BaselineDicomResult Run(FakeSession fake) =>
        BaselineDicomExport.Export(Path.Combine(_root, "baseline-1", "out.dcm"), Pixels, 3, 2, Meta, fake);

    [Fact]
    public void EverythingAgrees_Passes_AndEachStepRanOnce()
    {
        var fake = new FakeSession();
        var result = Run(fake);

        Assert.True(result.Passed, result.Summary);
        Assert.Equal(["write", "validate", "read"], fake.Calls);
        Assert.Contains("validator: valid", result.Summary, StringComparison.Ordinal);
        Assert.Contains("pixels identical", result.Summary, StringComparison.Ordinal);
        Assert.True(Directory.Exists(Path.Combine(_root, "baseline-1")));   // the evidence folder is created
    }

    [Fact]
    public void AWriteTheModuleRefuses_StopsBeforeValidatingOrReading()
    {
        var fake = new FakeSession { WriteCode = 7 };
        var result = Run(fake);

        Assert.False(result.Passed);
        Assert.False(result.Written);
        Assert.Equal(["write"], fake.Calls);
        Assert.Contains("xpe_dicom_write returned 7", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void AWriteThatThrows_IsAFailureWithTheMessage()
    {
        var result = Run(new FakeSession { WriteThrows = true });
        Assert.False(result.Passed);
        Assert.Contains("disk full", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void AFileTheValidatorCallsInvalid_Fails_EvenWhenThePixelsComeBackIdentical()
    {
        var fake = new FakeSession { Report = "{\"valid\":false,\"errors\":[{\"tag\":\"0010,0020\",\"message\":\"Missing required tag\"}],\"warnings\":[]}" };
        var result = Run(fake);

        Assert.False(result.Passed);
        Assert.True(result.ReportProduced);
        Assert.False(result.Valid);
        Assert.True(result.Pixels is { Identical: true });   // the pixels were fine: it is the validator that failed the file
        Assert.Contains("NOT valid", result.Summary, StringComparison.Ordinal);
        Assert.Contains("0010,0020", result.Summary, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData("")]
    [InlineData("not json at all")]
    [InlineData("{\"errors\":[]}")]
    [InlineData("{\"valid\":\"true\"}")]   // the string "true" is not the value true
    [InlineData("[true]")]
    public void AReportThatDoesNotSayValidTrue_IsNotValid(string report)
    {
        var result = Run(new FakeSession { Report = report });
        Assert.False(result.Passed);
        Assert.False(result.Valid);
    }

    [Fact]
    public void AValidatorThatReturnsAnErrorCode_ProducedNoReport()
    {
        var result = Run(new FakeSession { ValidateCode = 3 });
        Assert.False(result.Passed);
        Assert.False(result.ReportProduced);
        Assert.Contains("return code 3", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void AValidatorThatThrows_ProducedNoReport_AndSaysWhy()
    {
        var result = Run(new FakeSession { ValidateThrows = true });
        Assert.False(result.Passed);
        Assert.Contains("validator exploded", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void OnePixelDifferentAfterTheRoundTrip_Fails_AndSaysWhere()
    {
        var result = Run(new FakeSession { Stored = p => { var c = (ushort[])p.Clone(); c[4] ^= 1; return c; } });

        Assert.False(result.Passed);
        Assert.NotNull(result.Pixels);
        Assert.False(result.Pixels!.Identical);
        Assert.Equal(4, result.Pixels.FirstIndex);
        Assert.Equal(1, result.Pixels.DifferentCount);
        Assert.Contains("DIFFER at 4", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void ADifferentSizeComingBack_Fails_WithoutComparingPixels()
    {
        var back = new DicomReadBack(0, 0, 0, 2, 3, (ushort[])Pixels.Clone(), Meta, null);
        var result = Run(new FakeSession { ReadOverride = back });

        Assert.False(result.Passed);
        Assert.False(result.SizeMatches);
        Assert.Null(result.Pixels);
    }

    [Theory]
    [InlineData(5, 0, 0)]
    [InlineData(0, 6, 0)]
    [InlineData(0, 0, 9)]
    public void AReadStepThatFails_FailsTheExport_AndNamesTheStep(int open, int read, int meta)
    {
        var back = new DicomReadBack(open, read, meta, 0, 0, null, null, null);
        var result = Run(new FakeSession { ReadOverride = back });

        Assert.False(result.Passed);
        Assert.False(result.ReadBackSucceeded);
        Assert.Contains($"open {open}, read {read}, metadata {meta}", result.Summary, StringComparison.Ordinal);
    }

    [Fact]
    public void AReadBackWithAProblem_FailsEvenWithTheRightPixels()
    {
        var back = new DicomReadBack(0, 0, 0, 3, 2, (ushort[])Pixels.Clone(), Meta, "the module returned pixel format 1, not 16-bit");
        var result = Run(new FakeSession { ReadOverride = back });

        Assert.False(result.Passed);
        Assert.Contains("pixel format 1", result.Summary, StringComparison.Ordinal);
    }

    [Theory]
    [InlineData("ABDOMEN", 120f, 0.15f, "body part")]
    [InlineData("CHEST", 121f, 0.15f, "kVp")]
    [InlineData("CHEST", 120f, 0.2f, "pixel pitch")]
    public void AKnownMetadataValueThatChanged_FailsTheExport_AndNamesIt(string body, float kvp, float pitch, string named)
    {
        var result = Run(new FakeSession { StoredMeta = new BaselineDicomMetadata(body, kvp, pitch) });

        Assert.False(result.Passed);
        Assert.False(result.MetadataAgrees);
        Assert.True(result.Pixels is { Identical: true });
        Assert.Contains(named, result.MetadataDetail, StringComparison.Ordinal);
    }

    [Fact]
    public void ADecimalRoundingInTheLastPlace_IsNotAMetadataDifference_ButOnePercentIs()
    {
        var near = Run(new FakeSession { StoredMeta = new BaselineDicomMetadata("CHEST", 120.00001f, 0.150001f) });
        Assert.True(near.Passed, near.Summary);

        var far = Run(new FakeSession { StoredMeta = new BaselineDicomMetadata("CHEST", 121.2f, 0.15f) });
        Assert.False(far.Passed);
    }

    [Fact]
    public void ANullMetadataBack_IsADifference()
    {
        var result = Run(new FakeSession { StoredMeta = null });
        Assert.False(result.Passed);
        Assert.Contains("none", result.MetadataDetail, StringComparison.Ordinal);
    }

    [Fact]
    public void PixelsThatDoNotMatchTheSize_AreRefusedBeforeAnythingIsWritten()
    {
        var fake = new FakeSession();
        var result = BaselineDicomExport.Export(Path.Combine(_root, "x.dcm"), Pixels, 4, 2, Meta, fake);

        Assert.False(result.Passed);
        Assert.Empty(fake.Calls);
    }

    [Fact]
    public void TheMetadataWrittenIsOnlyWhatTheGuiKnows()
    {
        // The record has no exposure, distance or time: they cannot be passed by accident, so the writer cannot invent them.
        var members = typeof(BaselineDicomMetadata).GetProperties().Select(p => p.Name).Where(n => n != "EqualityContract").Order().ToArray();
        Assert.Equal(["BodyPart", "KVp", "PixelPitchMm"], members);

        var native = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiDicomNative.cs"));
        Assert.Contains("XpeImageMetadataNative.Create(metadata.BodyPart, metadata.KVp, 0f, 0f, metadata.PixelPitchMm)", native, StringComparison.Ordinal);
    }

    [Fact]
    public void TheResolverMapsTheDicomDll()
    {
        var resolver = File.ReadAllText(BenchmarkRunnerServiceTests.ResolveRepositoryFile("gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs"));
        Assert.Contains("_ when Is(libraryName, DicomDll) => NativeModuleLibraryLocator.GetDllCandidates(DicomDll, \"image-processing\")", resolver, StringComparison.Ordinal);
    }
}
