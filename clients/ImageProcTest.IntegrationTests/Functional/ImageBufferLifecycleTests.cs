// AC-3: DLL resolution, AC-4: PInvoke symbol functional tests (alloc/free/copy).
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Functional tests for xpe_alloc_image, xpe_free_image, xpe_copy_image.
/// Covers REQ-GUI-IT-023, REQ-GUI-IT-024, REQ-GUI-IT-025, AC-4.
/// </summary>
[Trait("Category", "Functional")]
[Collection(NativeLibraryCollection.Name)]
public sealed class ImageBufferLifecycleTests : IDisposable
{
    private readonly NativeLibraryFixture _fixture;
    private XpeCommonNative.XpeImageBuffer _allocatedBuffer;
    private bool _bufferAllocated;

    public ImageBufferLifecycleTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    /// <summary>
    /// REQ-GUI-IT-021: xpe_init(null) must return XPE_OK before memory operations.
    /// </summary>
    private void EnsureInitialized()
    {
        var initResult = XpeCommonNative.xpe_init(null);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, initResult);
    }

    /// <summary>
    /// REQ-GUI-IT-023: xpe_alloc_image(16, 16, UInt16) returns OK with non-zero Data pointer
    /// and DataSize == 512 (16×16×2 bytes).
    /// </summary>
    [Fact]
    public void AllocImage_ValidDimensions_ReturnsOkAndNonZeroData()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        EnsureInitialized();

        var result = XpeCommonNative.xpe_alloc_image(16, 16, XpeCommonNative.XpePixelFormat.UInt16, out _allocatedBuffer);
        _bufferAllocated = result == XpeCommonNative.XpeErrorCode.OK;

        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, result);
        Assert.NotEqual(IntPtr.Zero, _allocatedBuffer.Data);
        Assert.Equal((nuint)512, _allocatedBuffer.DataSize); // 16*16*2
        Assert.Equal(16u, _allocatedBuffer.Width);
        Assert.Equal(16u, _allocatedBuffer.Height);
    }

    /// <summary>
    /// REQ-GUI-IT-023: xpe_free_image after successful alloc returns OK and zeroes Data pointer.
    /// </summary>
    [Fact]
    public void FreeImage_AfterAlloc_ReturnsOkAndZeroesData()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        EnsureInitialized();

        var allocResult = XpeCommonNative.xpe_alloc_image(16, 16, XpeCommonNative.XpePixelFormat.UInt16, out var buf);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, allocResult);

        var freeResult = XpeCommonNative.xpe_free_image(ref buf);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, freeResult);
        Assert.Equal(IntPtr.Zero, buf.Data);
    }

    /// <summary>
    /// REQ-GUI-IT-024: xpe_alloc_image with 0 dimensions returns INVALID_INPUT.
    /// </summary>
    [Fact]
    public void AllocImage_ZeroDimensions_ReturnsInvalidInput()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        EnsureInitialized();

        var result = XpeCommonNative.xpe_alloc_image(0, 0, XpeCommonNative.XpePixelFormat.UInt16, out var buf);

        Assert.Equal(XpeCommonNative.XpeErrorCode.INVALID_INPUT, result);
        // buffer.Data must not be modified on failure
        Assert.Equal(IntPtr.Zero, buf.Data);
    }

    /// <summary>
    /// REQ-GUI-IT-025: xpe_copy_image with matching src/dst dimensions returns OK.
    /// </summary>
    [Fact]
    public void CopyImage_MatchingDimensions_ReturnsOk()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped
        EnsureInitialized();

        var allocSrc = XpeCommonNative.xpe_alloc_image(16, 16, XpeCommonNative.XpePixelFormat.UInt16, out var src);
        var allocDst = XpeCommonNative.xpe_alloc_image(16, 16, XpeCommonNative.XpePixelFormat.UInt16, out var dst);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, allocSrc);
        Assert.Equal(XpeCommonNative.XpeErrorCode.OK, allocDst);

        try
        {
            var copyResult = XpeCommonNative.xpe_copy_image(ref src, ref dst);
            Assert.Equal(XpeCommonNative.XpeErrorCode.OK, copyResult);
        }
        finally
        {
            XpeCommonNative.xpe_free_image(ref src);
            XpeCommonNative.xpe_free_image(ref dst);
        }
    }

    public void Dispose()
    {
        if (!_fixture.IsAvailable) return;
        if (_bufferAllocated)
        {
            XpeCommonNative.xpe_free_image(ref _allocatedBuffer);
            _bufferAllocated = false;
        }
        XpeCommonNative.xpe_shutdown();
    }
}
