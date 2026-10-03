using System.Runtime.InteropServices;
using Xunit.Abstractions;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// Platform diagnostic tests.
/// Records runtime environment details without failing baseline tests.
/// Covers REQ-GUI-IT-043.
/// </summary>
[Trait("Category", "Smoke")]
public sealed class PlatformDetectionTests(ITestOutputHelper output)
{
    /// <summary>
    /// REQ-GUI-IT-043: the architecture the suite runs on is RECORDED, on every architecture, and recording it fails nothing. GUI-C-208 (D7): the requirement asked for a diagnostic
    /// that "records the architecture without failing other tests"; what stood here was <c>ProcessArchitecture_IsX64</c>, an assertion that FAILS on any other architecture (and a
    /// twin in DllLoadSmokeTests that accepted X64 or Arm64 and recorded nothing). The record goes to the test output and to <c>platform-diagnostic.txt</c> in the output folder,
    /// and the test reads the file back.
    /// </summary>
    [Fact]
    public void ProcessArchitecture_IsRecordedForDiagnostics()
    {
        var arch = RuntimeInformation.ProcessArchitecture;
        var record = $"ProcessArchitecture={arch}; OSArchitecture={RuntimeInformation.OSArchitecture}; IntPtrSize={IntPtr.Size}; OS={RuntimeInformation.OSDescription}; Framework={RuntimeInformation.FrameworkDescription}";
        output.WriteLine(record);
        if (arch != Architecture.X64)
        {
            output.WriteLine("non-x64 host: the x64-only native DLLs may not load here; this is recorded, not a failure (REQ-GUI-IT-043).");
        }

        var path = Path.Combine(AppContext.BaseDirectory, "platform-diagnostic.txt");
        if (File.Exists(path)) File.Delete(path);   // a record left by an earlier run must not satisfy the read-back below
        File.WriteAllText(path, record + Environment.NewLine);

        var back = File.ReadAllText(path);
        Assert.Contains($"ProcessArchitecture={arch};", back, StringComparison.Ordinal);
        Assert.Contains("OSArchitecture=", back, StringComparison.Ordinal);
    }

    /// <summary>IntPtr.Size must be 8 on x64 process.</summary>
    [Fact]
    public void IntPtrSize_IsEight()
    {
        Assert.Equal(8, IntPtr.Size);
    }

    /// <summary>.NET version must be 8.x or higher.</summary>
    [Fact]
    public void DotNetVersion_Is8OrHigher()
    {
        var version = Environment.Version;
        Assert.True(version.Major >= 8,
            $"Expected .NET 8+, found {version}");
    }

    /// <summary>OS description is Windows (required for native DLL P/Invoke path).</summary>
    [Fact]
    public void OperatingSystem_IsWindows()
    {
        Assert.True(RuntimeInformation.IsOSPlatform(OSPlatform.Windows),
            $"Expected Windows, found: {RuntimeInformation.OSDescription}");
    }
}
