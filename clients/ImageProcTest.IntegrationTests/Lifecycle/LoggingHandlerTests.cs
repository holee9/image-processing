// AC-12: Log subsystem bounds.
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Lifecycle;

/// <summary>
/// Tests for the xpe logging subsystem: set_level, set_file, flush.
/// Covers REQ-GUI-IT-029, REQ-GUI-IT-030, REQ-GUI-IT-031, AC-12.
/// </summary>
[Trait("Category", "Lifecycle")]
[Collection(NativeLibraryCollection.Name)]
public sealed class LoggingHandlerTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;
    private readonly List<string> _tempFiles = new();

    public LoggingHandlerTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>REQ-GUI-IT-031: xpe_log_flush pre-init must not throw managed exception.</summary>
    [Fact]
    public void LogFlush_PreInit_DoesNotThrow()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        // Intentionally called without xpe_init
        var ex = Record.Exception(() => XpeCommonNative.xpe_log_flush());
        Assert.Null(ex);
    }

    /// <summary>REQ-GUI-IT-029: xpe_log_set_level with valid levels {0..5} returns OK.</summary>
    [Theory]
    [InlineData(0)]
    [InlineData(1)]
    [InlineData(2)]
    [InlineData(3)]
    [InlineData(4)]
    [InlineData(5)]
    public void LogSetLevel_ValidLevels_ReturnsOk(int level)
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var result = XpeCommonNative.xpe_log_set_level(level);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
    }

    /// <summary>REQ-GUI-IT-029: xpe_log_set_level(-1) returns INVALID_INPUT.</summary>
    [Fact]
    public void LogSetLevel_NegativeOne_ReturnsInvalidInput()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var result = XpeCommonNative.xpe_log_set_level(-1);
        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, result);
    }

    /// <summary>REQ-GUI-IT-029: xpe_log_set_level(6) returns INVALID_INPUT.</summary>
    [Fact]
    public void LogSetLevel_Six_ReturnsInvalidInput()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var result = XpeCommonNative.xpe_log_set_level(6);
        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, result);
    }

    /// <summary>REQ-GUI-IT-030: xpe_log_set_file with a writable temp path returns OK.</summary>
    [Fact]
    public void LogSetFile_WritableTempPath_ReturnsOk()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var tempPath = Path.Combine(Path.GetTempPath(), $"xpe_it_log_{Guid.NewGuid():N}.log");
        _tempFiles.Add(tempPath);

        var result = XpeCommonNative.xpe_log_set_file(tempPath);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
    }

    /// <summary>REQ-GUI-IT-030: xpe_log_set_file with non-existent directory returns IO_FAILED.</summary>
    [Fact]
    public void LogSetFile_NonExistentDirectory_ReturnsIoFailed()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        XpeCommonNative.xpe_init(null);

        var invalidPath = Path.Combine(Path.GetTempPath(), $"nonexistent_{Guid.NewGuid():N}", "log.txt");
        var result = XpeCommonNative.xpe_log_set_file(invalidPath);
        Assert.Equal(XpeCommonNative.XpeErrorCode.IO_FAILED, result);
    }

    /// <summary>REQ-GUI-IT-031: xpe_log_flush post-shutdown must not throw managed exception.</summary>
    [Fact]
    public void LogFlush_PostShutdown_DoesNotThrow()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        XpeCommonNative.xpe_init(null);
        XpeCommonNative.xpe_shutdown();

        var ex = Record.Exception(() => XpeCommonNative.xpe_log_flush());
        Assert.Null(ex);
    }

    public void Dispose()
    {
        if (_fixture.IsAvailable)
            XpeCommonNative.xpe_shutdown();
        foreach (var f in _tempFiles)
            if (File.Exists(f)) File.Delete(f);
    }
}
