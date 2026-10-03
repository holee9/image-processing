// #225 row 9 (GUI-C-196 M3): the export against the REAL xpe_dicom.dll through the gui's own binding — does the module call the file it wrote valid, and
// does it give back the same pixels?
using System.Runtime.InteropServices;
using ImageProcTest.Services;
using ImageProcTest.Services.Native;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Skipped when xpe_dicom.dll is not staged. The C# integration job's staged set (xpe-ci-post-binaries) is built with BUILD_DICOM OFF, so unless a job stages
/// xpe_dicom.dll too, this test is skipped THERE and the answer to "is the written file valid" has to come from a run that has the DLL. Like
/// <see cref="EnhanceBasicNativeTests"/>, it loads the DLLs by full path and installs no DllImport resolver on this assembly.
/// </summary>
[Trait("Category", "Functional")]
public sealed class BaselineDicomNativeTests(ITestOutputHelper output) : IDisposable
{
    private static readonly string? NativeDirectory = FindDirectory();
    private static bool _loaded;
    private static string? _loadProblem;
    private readonly string _root = Path.Combine(Path.GetTempPath(), "xpe-baseline-dicom-native-" + Guid.NewGuid().ToString("N"));

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

    private static string? FindDirectory()
    {
        var candidates = new List<string>();
        var injected = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrWhiteSpace(injected))
        {
            candidates.Add(injected);
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-dicom", "bin"));
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-common", "bin"));
            dir = dir.Parent;
        }

        return candidates.FirstOrDefault(c => File.Exists(Path.Combine(c, "xpe_dicom.dll")) && File.Exists(Path.Combine(c, "xpe_common.dll")));
    }

    private static bool EnsureLoaded()
    {
        if (_loaded)
        {
            return true;
        }

        try
        {
            ImageProcTest.IntegrationTests.Fixtures.SharedCommonModule.Load(NativeDirectory!);   // GUI-C-199: never a second copy of xpe_common.dll
            NativeLibrary.Load(Path.Combine(NativeDirectory!, "xpe_dicom.dll"));
            _loaded = true;
        }
        catch (Exception ex) when (ex is DllNotFoundException or BadImageFormatException)
        {
            _loadProblem = "the staged DLLs could not be loaded: " + ex.Message;
        }

        return _loaded;
    }

    private void RequireNative()
    {
        Skip.If(NativeDirectory is null, "xpe_dicom.dll and xpe_common.dll are not staged together (XPE_NATIVE_DIR, build/ci-dicom/bin).");
        Skip.If(!EnsureLoaded(), _loadProblem ?? "the DLLs could not be loaded");
        output.WriteLine($"native directory: {NativeDirectory}");
    }

    private static ushort[] Image(int width, int height)
    {
        var random = new Random(777);
        var pixels = new ushort[width * height];
        for (var i = 0; i < pixels.Length; i++)
        {
            pixels[i] = (ushort)((i * 31 % 60000) + random.Next(0, 5000));   // spans most of the 16-bit range, including values above 32767
        }

        pixels[0] = 0;
        pixels[^1] = ushort.MaxValue;
        return pixels;
    }

    [SkippableFact]
    public void TheExport_OnTheRealModule_IsValid_AndTheReadBackIsBitIdentical()
    {
        RequireNative();
        var pixels = Image(128, 96);
        var path = Path.Combine(_root, "baseline-1", "output.dcm");

        var result = BaselineDicomExport.Export(path, pixels, 128, 96, new BaselineDicomMetadata("CHEST", 120f, 0.15f), new NativeDicomSession());
        Assert.True(result.Passed, result.Summary);
        Assert.Null(BaselineDicomExport.Promote(result));   // the file has its final name only after it passed
        output.WriteLine("SUMMARY: " + result.Summary);
        output.WriteLine("REPORT: " + result.ReportJson);
        output.WriteLine($"file bytes: {(File.Exists(path) ? new FileInfo(path).Length : -1)}");
        Assert.True(File.Exists(path), "no file under the final name after Promote");
        Assert.False(File.Exists(path + BaselineDicomExport.PartialSuffix), "the partial file is still there after Promote");

        Assert.True(result.Written, result.Summary);
        Assert.True(result.ReportProduced, result.Summary);
        Assert.True(result.Valid, "the module's own validator does not call the file it just wrote valid: " + result.ReportJson);
        Assert.True(result.ReadBackSucceeded, result.Summary);
        Assert.True(result.Pixels is { Identical: true }, result.Summary);
        Assert.True(result.MetadataAgrees, result.MetadataDetail);
        Assert.True(result.Passed, result.Summary);
    }

    /// <summary>
    /// MEASUREMENT (Codex #73 recommendation 4): the export under an evidence path that contains Hangul. The path is marshalled as ANSI, so what happens depends on the
    /// system code page and on how the DICOM module opens files; this records each step instead of assuming. The test fails only when the export reports a state that
    /// contradicts itself (a pass with no final file, or a failure that left a file under the final name).
    /// </summary>
    [SkippableTheory]
    [InlineData("증거-한글경로")]          // inside the Korean ANSI code page (949)
    [InlineData("หลักฐาน-ไทย")]      // Thai: outside code page 949, so the ANSI conversion cannot represent it
    public void TheExport_UnderANonAsciiPath_IsMeasuredStepByStep(string folderName)
    {
        RequireNative();
        var folder = Path.Combine(_root, folderName, "baseline-1");
        var path = Path.Combine(folder, "baseline.dcm");
        var pixels = Image(64, 64);

        var result = BaselineDicomExport.Export(path, pixels, 64, 64, new BaselineDicomMetadata("CHEST", 120f, 0.15f), new NativeDicomSession());
        output.WriteLine($"HANGUL PATH ({folderName}): {path}");
        output.WriteLine($"HANGUL written={result.Written} reportProduced={result.ReportProduced} valid={result.Valid} readBack={result.ReadBackSucceeded} sizeMatches={result.SizeMatches} pixelsIdentical={result.Pixels is { Identical: true }} metadataAgrees={result.MetadataAgrees}");
        output.WriteLine("HANGUL summary: " + result.Summary);
        output.WriteLine($"HANGUL partial exists={File.Exists(path + BaselineDicomExport.PartialSuffix)} final exists={File.Exists(path)}; system ANSI code page = {System.Globalization.CultureInfo.CurrentCulture.TextInfo.ANSICodePage}");

        if (result.Passed)
        {
            Assert.Null(BaselineDicomExport.Promote(result));
            Assert.True(File.Exists(path));
        }
        else
        {
            Assert.False(File.Exists(path), "a failed export left a file under the final name");
            Assert.False(File.Exists(path + BaselineDicomExport.PartialSuffix), "a failed export left its partial file");
        }
    }

    [SkippableFact]
    public void TheSameImageWrittenTwice_GivesTheSameFileBytes_OrTheDifferenceIsNamed()
    {
        RequireNative();
        var pixels = Image(64, 64);
        var meta = new BaselineDicomMetadata("CHEST", 120f, 0.15f);
        var a = Path.Combine(_root, "a.dcm");
        var b = Path.Combine(_root, "b.dcm");

        var first = BaselineDicomExport.Export(a, pixels, 64, 64, meta, new NativeDicomSession());
        var second = BaselineDicomExport.Export(b, pixels, 64, 64, meta, new NativeDicomSession());
        Assert.True(first.Passed && second.Passed);
        Assert.Null(BaselineDicomExport.Promote(first));
        Assert.Null(BaselineDicomExport.Promote(second));

        var bytesA = File.ReadAllBytes(a);
        var bytesB = File.ReadAllBytes(b);
        var differing = bytesA.Length == bytesB.Length ? Enumerable.Range(0, bytesA.Length).Count(i => bytesA[i] != bytesB[i]) : -1;
        output.WriteLine($"MEASURED two writes of the same pixels: {bytesA.Length} / {bytesB.Length} bytes, {differing} bytes differ " +
                         "(where the differing bytes sit was not located; a measurement, not a gate)");
        Assert.True(bytesA.Length > 0 && bytesB.Length > 0);
    }
}
