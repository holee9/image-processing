// AC-2: Struct layout parity (additional functional coverage complement to AbiLayoutTests).
using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Functional;

/// <summary>
/// Functional struct layout verification.
/// Covers REQ-GUI-IT-002, REQ-GUI-IT-003, REQ-GUI-IT-004.
/// </summary>
[Trait("Category", "Functional")]
public sealed class StructLayoutParityTests
{
    /// <summary>Blittable nature: StructureToPtr roundtrip preserves all numeric fields of XpeImageBuffer.</summary>
    [Fact]
    public void XpeImageBuffer_RoundTrip_PreservesAllFields()
    {
        var buf = new XpeCommonNative.XpeImageBuffer
        {
            Width = 640,
            Height = 480,
            BitsAllocated = 16,
            BitsStored = 12,
            Format = XpeCommonNative.XpePixelFormat.UInt16,
            Data = new IntPtr(0xDEADBEEF),
            DataSize = (nuint)614400,
        };

        var size = Marshal.SizeOf<XpeCommonNative.XpeImageBuffer>();
        var ptr = Marshal.AllocHGlobal(size);
        try
        {
            Marshal.StructureToPtr(buf, ptr, false);
            var back = Marshal.PtrToStructure<XpeCommonNative.XpeImageBuffer>(ptr);

            Assert.Equal(buf.Width, back.Width);
            Assert.Equal(buf.Height, back.Height);
            Assert.Equal(buf.BitsAllocated, back.BitsAllocated);
            Assert.Equal(buf.BitsStored, back.BitsStored);
            Assert.Equal(buf.Format, back.Format);
            Assert.Equal(buf.Data, back.Data);
            Assert.Equal(buf.DataSize, back.DataSize);
        }
        finally
        {
            Marshal.FreeHGlobal(ptr);
        }
    }

    /// <summary>XpePixelFormat enum values match native ABI (UInt16=0, Float32=1).</summary>
    [Fact]
    public void XpePixelFormat_EnumValues_MatchNativeAbi()
    {
        Assert.Equal(0u, (uint)XpeCommonNative.XpePixelFormat.UInt16);
        Assert.Equal(1u, (uint)XpeCommonNative.XpePixelFormat.Float32);
    }

    /// <summary>XpeErrorCode enum range matches ABI (-10 through 0).</summary>
    [Fact]
    public void XpeErrorCode_EnumValues_MatchNativeAbi()
    {
        Assert.Equal(0, (int)XpeCommonNative.XpeErrorCode.OK);
        Assert.Equal(-1, (int)XpeCommonNative.XpeErrorCode.INVALID_INPUT);
        Assert.Equal(-10, (int)XpeCommonNative.XpeErrorCode.NETWORK_FAILED);
    }
}
