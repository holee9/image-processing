// AC-2: ABI struct size parity.
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Smoke;

/// <summary>
/// Verifies C# struct layout parity with the native ABI (Pack=8, x64).
/// Covers REQ-GUI-IT-002, REQ-GUI-IT-003, REQ-GUI-IT-004, AC-2.
/// </summary>
[Trait("Category", "Smoke")]
public sealed class AbiLayoutTests
{
    /// <summary>REQ-GUI-IT-002: Marshal.SizeOf&lt;XpeImageBuffer&gt;() == 40.</summary>
    [Fact]
    public void XpeImageBuffer_MarshalSize_Is40Bytes()
    {
        var size = Marshal.SizeOf<XpeCommonNative.XpeImageBuffer>();
        Assert.Equal(40, size);
    }

    /// <summary>REQ-GUI-IT-002: Marshal.SizeOf&lt;XpeImageMetadata&gt;() == 96.</summary>
    [Fact]
    public void XpeImageMetadata_MarshalSize_Is96Bytes()
    {
        var size = Marshal.SizeOf<XpeCommonNative.XpeImageMetadata>();
        Assert.Equal(96, size);
    }

    /// <summary>
    /// REQ-GUI-IT-003: Field offsets of XpeImageBuffer match C++ layout.
    /// C++ (Pack=8, x64): width=0, height=4, bitsAllocated=8, bitsStored=12,
    /// format=16, [pad 4B], data=24, dataSize=32.
    /// </summary>
    [Fact]
    public void XpeImageBuffer_FieldOffsets_MatchNativeLayout()
    {
        Assert.Equal(0, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.Width)));
        Assert.Equal(4, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.Height)));
        Assert.Equal(8, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.BitsAllocated)));
        Assert.Equal(12, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.BitsStored)));
        Assert.Equal(16, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.Format)));
        // Data starts at 24 (4 bytes Format + 4 bytes pad = 8, aligned to 8-byte boundary)
        Assert.Equal(24, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.Data)));
        Assert.Equal(32, (int)Marshal.OffsetOf<XpeCommonNative.XpeImageBuffer>(nameof(XpeCommonNative.XpeImageBuffer.DataSize)));
    }

    /// <summary>
    /// REQ-GUI-IT-004: XpeImageMetadata.BodyPart is ANSI ByValTStr SizeConst=64.
    /// A 63-char ASCII string round-trips correctly (no buffer overflow, null-terminated).
    /// </summary>
    [Fact]
    public void XpeImageMetadata_BodyPart_63CharAscii_RoundTrips()
    {
        var input63 = TestDataLoader.BodyPart63Chars; // 63 × 'A'
        Assert.Equal(63, input63.Length);

        var meta = new XpeCommonNative.XpeImageMetadata { BodyPart = input63 };

        // Pin and marshal to unmanaged memory, then read back.
        var size = Marshal.SizeOf<XpeCommonNative.XpeImageMetadata>();
        var ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(meta, ptr, false);
            var back = Marshal.PtrToStructure<XpeCommonNative.XpeImageMetadata>(ptr);
            Assert.Equal(input63, back.BodyPart);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    /// <summary>
    /// REQ-GUI-IT-004: BodyPart field is 64 bytes starting at offset 0 of XpeImageMetadata.
    /// </summary>
    [Fact]
    public void XpeImageMetadata_BodyPart_FieldOffset_IsZero()
    {
        var offset = (int)Marshal.OffsetOf<XpeCommonNative.XpeImageMetadata>(nameof(XpeCommonNative.XpeImageMetadata.BodyPart));
        Assert.Equal(0, offset);
    }

    /// <summary>IntPtr.Size must be 8 on x64 process.</summary>
    [Fact]
    public void IntPtrSize_IsEight_OnX64()
    {
        Assert.Equal(8, IntPtr.Size);
    }
}
