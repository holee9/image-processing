using System.Runtime.InteropServices;

namespace ImageProcTest.IntegrationTests.Diagnostics;

/// <summary>
/// Platform diagnostic tests.
/// Records runtime environment details without failing baseline tests.
/// Covers REQ-GUI-IT-043.
/// </summary>
[Trait("Category", "Smoke")]
public sealed class PlatformDetectionTests
{
    /// <summary>Process architecture must be x64 on baseline CI.</summary>
    [Fact]
    public void ProcessArchitecture_IsX64()
    {
        Assert.Equal(Architecture.X64, RuntimeInformation.ProcessArchitecture);
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
