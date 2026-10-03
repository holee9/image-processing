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

    /// <summary>
    /// All XpeErrorCode members, taken from the enum itself rather than a hand-written list.
    /// A code added to the enum joins this run automatically; GUI-C-12's drift test keeps the
    /// enum itself honest against xpe_error.h.
    /// </summary>
    public static IEnumerable<object[]> AllErrorCodes() =>
        Enum.GetValues<XpeCommonNative.XpeErrorCode>().Select(code => new object[] { code });

    /// <summary>
    /// REQ-GUI-IT-009: xpe_error_string returns a real message — non-NULL, non-empty, and not the
    /// "Unknown error" fallback — for every code the enum declares. The fallback is what an
    /// unmapped code returns, so accepting it here would let a missing mapping pass silently.
    /// </summary>
    [SkippableTheory]
    [MemberData(nameof(AllErrorCodes))]
    public void ErrorString_ForAllDefinedCodes_IsNonNullAndNotFallback(XpeCommonNative.XpeErrorCode code)
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        // GUI-C-208 (D1): the NOT_IMPLEMENTED row used to be skipped ("no mapping yet, #126 / QA-A-23"). The native maps it now
        // (xpe_common.cpp: "Function not implemented in this version"), and no gate noticed that the one skipped row had started to pass.
        //
        // The fallback is read from the DLL — what an unmapped code returns — rather than written here as a literal: a reworded native fallback
        // would otherwise leave "not the fallback" true for every code.
        var fallback = Marshal.PtrToStringAnsi(XpeCommonNative.xpe_error_string((XpeCommonNative.XpeErrorCode)(-999)));
        Assert.False(string.IsNullOrEmpty(fallback), "the native fallback text must exist for the comparison below to mean anything");

        var ptr = XpeCommonNative.xpe_error_string(code);
        Assert.NotEqual(IntPtr.Zero, ptr);

        var text = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(text);
        Assert.NotEmpty(text);
        Assert.NotEqual(fallback, text);
    }

    /// <summary>REQ-GUI-IT-009: Unknown code (-999) returns non-NULL fallback string.</summary>
    [SkippableFact]
    public void ErrorString_ForUnknownCode_ReturnsFallbackNonNull()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var ptr = XpeCommonNative.xpe_error_string((XpeCommonNative.XpeErrorCode)(-999));
        Assert.NotEqual(IntPtr.Zero, ptr);

        var text = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(text);
        // Must be non-empty (may be "Unknown error" or similar)
        Assert.NotEmpty(text);
    }

    /// <summary>REQ-GUI-IT-005: xpe_error_string returns same pointer for same code (static storage).</summary>
    [SkippableFact]
    public void ErrorString_CalledTwice_ReturnsSamePointer()
    {
        SkipHelper.SkipIf(!_fixture.IsAvailable, _fixture.SkipReason);

        var ptr1 = XpeCommonNative.xpe_error_string(XpeCommonNative.XpeErrorCode.INVALID_INPUT);
        var ptr2 = XpeCommonNative.xpe_error_string(XpeCommonNative.XpeErrorCode.INVALID_INPUT);
        Assert.Equal(ptr1, ptr2);
    }
}
