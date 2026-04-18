// AC-5: xpe_error_string parity for all XpeErrorCode values.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Functional tests for xpe_error_string: non-NULL, non-empty for every enum value.
/// Covers REQ-GUI-IT-009, REQ-GUI-IT-053, AC-5, AC-6.
/// </summary>
[Trait("Category", "Functional")]
[Collection(NativeLibraryCollection.Name)]
public sealed class ErrorCodeMappingTests
{
    private readonly NativeLibraryFixture _fixture;

    public ErrorCodeMappingTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>REQ-GUI-IT-009: xpe_error_string returns non-NULL non-empty for all defined error codes.</summary>
    [Theory]
    [InlineData(XpeCommonNative.XpeErrorCode.OK)]
    [InlineData(XpeCommonNative.XpeErrorCode.INVALID_INPUT)]
    [InlineData(XpeCommonNative.XpeErrorCode.OUT_OF_MEMORY)]
    [InlineData(XpeCommonNative.XpeErrorCode.PROCESSING_FAILED)]
    [InlineData(XpeCommonNative.XpeErrorCode.CONFIG_INVALID)]
    [InlineData(XpeCommonNative.XpeErrorCode.CALIBRATION_EXPIRED)]
    [InlineData(XpeCommonNative.XpeErrorCode.NOT_INITIALIZED)]
    [InlineData(XpeCommonNative.XpeErrorCode.UNSUPPORTED_FORMAT)]
    [InlineData(XpeCommonNative.XpeErrorCode.BUFFER_TOO_SMALL)]
    [InlineData(XpeCommonNative.XpeErrorCode.IO_FAILED)]
    [InlineData(XpeCommonNative.XpeErrorCode.NETWORK_FAILED)]
    public void ErrorString_ForAllDefinedCodes_IsNonNullAndNonEmpty(XpeCommonNative.XpeErrorCode code)
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var ptr = XpeCommonNative.xpe_error_string(code);
        Assert.NotEqual(IntPtr.Zero, ptr);

        var text = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(text);
        Assert.NotEmpty(text);
    }

    /// <summary>REQ-GUI-IT-009: Unknown code (-999) returns non-NULL fallback string.</summary>
    [Fact]
    public void ErrorString_ForUnknownCode_ReturnsFallbackNonNull()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var ptr = XpeCommonNative.xpe_error_string((XpeCommonNative.XpeErrorCode)(-999));
        Assert.NotEqual(IntPtr.Zero, ptr);

        var text = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(text);
        // Must be non-empty (may be "Unknown error" or similar)
        Assert.NotEmpty(text);
    }

    /// <summary>REQ-GUI-IT-005: xpe_error_string returns same pointer for same code (static storage).</summary>
    [Fact]
    public void ErrorString_CalledTwice_ReturnsSamePointer()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var ptr1 = XpeCommonNative.xpe_error_string(XpeCommonNative.XpeErrorCode.INVALID_INPUT);
        var ptr2 = XpeCommonNative.xpe_error_string(XpeCommonNative.XpeErrorCode.INVALID_INPUT);
        Assert.Equal(ptr1, ptr2);
    }
}
