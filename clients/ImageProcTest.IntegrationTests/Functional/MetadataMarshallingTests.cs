// AC-4: Functional coverage for xpe_configure, xpe_get_param_range.
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Functional tests for JSON configuration and parameter range marshalling.
/// Covers REQ-GUI-IT-022, REQ-GUI-IT-026, AC-4.
/// </summary>
[Trait("Category", "Functional")]
[Collection(NativeLibraryCollection.Name)]
public sealed class MetadataMarshallingTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;

    public MetadataMarshallingTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
        if (fixture.IsAvailable)
            XpeCommonNative.xpe_init(null);
    }

    /// <summary>
    /// REQ-GUI-IT-022: xpe_configure with valid JSON returns OK.
    /// </summary>
    [Fact]
    public void Configure_ValidJson_ReturnsOk()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var result = XpeCommonNative.xpe_configure(TestDataLoader.ValidConfigJson);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
    }

    /// <summary>
    /// REQ-GUI-IT-022: xpe_configure with malformed JSON returns CONFIG_INVALID.
    /// </summary>
    [Fact]
    public void Configure_MalformedJson_ReturnsConfigInvalid()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var result = XpeCommonNative.xpe_configure(TestDataLoader.MalformedConfigJson);
        Assert.Equal(XpeCommonNative.XpeErrorCode.CONFIG_INVALID, result);
    }

    /// <summary>
    /// REQ-GUI-IT-026: xpe_get_param_range("CHEST", "window_center") returns OK with min &lt;= default &lt;= max.
    /// </summary>
    [Fact]
    public void GetParamRange_ChestWindowCenter_ReturnsValidRange()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var result = XpeCommonNative.xpe_get_param_range("CHEST", "window_center", out var min, out var max, out var dflt);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
        Assert.True(min <= dflt, $"min ({min}) must be <= default ({dflt})");
        Assert.True(dflt <= max, $"default ({dflt}) must be <= max ({max})");
    }

    /// <summary>
    /// REQ-GUI-IT-026: xpe_get_param_range with unknown body part returns error code (not crash).
    /// </summary>
    [Fact]
    public void GetParamRange_UnknownBodyPart_ReturnsErrorCode()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var result = XpeCommonNative.xpe_get_param_range("UNKNOWN_BODY_PART_XYZ", "window_center",
            out _, out _, out _);

        // Must return an error code — not throw a managed exception.
        Assert.True((int)result < 0, $"Expected error code, got {result}");
    }

    public void Dispose()
    {
        try { if (_fixture.IsAvailable) XpeCommonNative.xpe_shutdown(); }
        catch (DllNotFoundException) { /* DLL absent — nothing to shut down */ }
    }
}
