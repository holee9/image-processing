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

    /// <summary>
    /// XpePixelFormat: every member the header has, with its value (XPE_PIXEL_UINT16 = 0, XPE_PIXEL_FLOAT32 = 1, XPE_PIXEL_UINT8 = 2). The mirror lacked UInt8 until GUI-C-208 M1 found it;
    /// the member count is held so a member added to either side fails here.
    /// </summary>
    [Fact]
    public void XpePixelFormat_EnumValues_MatchNativeAbi()
    {
        Assert.Equal(0u, (uint)XpeCommonNative.XpePixelFormat.UInt16);
        Assert.Equal(1u, (uint)XpeCommonNative.XpePixelFormat.Float32);
        Assert.Equal(2u, (uint)XpeCommonNative.XpePixelFormat.UInt8);
        Assert.Equal(3, Enum.GetValues<XpeCommonNative.XpePixelFormat>().Length);
    }

    /// <summary>
    /// XpeErrorCode: all 18 codes, each with the value xpe_error.h gives it (GUI-C-208, D13: this pinned three of them, -11 .. -17 were not held by any value assertion).
    /// The list is written out on purpose: ErrorCodeHeaderParityTests compares the enum to the header's text, and this is the independent ABI tripwire — changing a value in the
    /// enum AND in the header together still fails here until someone edits this table too.
    /// </summary>
    [Fact]
    public void XpeErrorCode_EnumValues_MatchNativeAbi()
    {
        var expected = new (string Name, int Value)[]
        {
            ("OK", 0), ("INVALID_INPUT", -1), ("OUT_OF_MEMORY", -2), ("PROCESSING_FAILED", -3), ("CONFIG_INVALID", -4), ("CALIBRATION_EXPIRED", -5),
            ("NOT_INITIALIZED", -6), ("UNSUPPORTED_FORMAT", -7), ("BUFFER_TOO_SMALL", -8), ("IO_FAILED", -9), ("NETWORK_FAILED", -10), ("SAFETY_VIOLATION", -11),
            ("INTERNAL", -12), ("DICOM_INVALID", -13), ("DICOM_CONFORMANCE", -14), ("NOT_IMPLEMENTED", -15), ("CALIB_NOT_LOADED", -16), ("INVALID_CALIB_DATA", -17),
        };

        var actual = Enum.GetValues<XpeCommonNative.XpeErrorCode>().Select(c => (Name: c.ToString(), Value: (int)c)).OrderByDescending(x => x.Value).ToArray();
        Assert.Equal(expected.Length, actual.Length);
        for (var i = 0; i < expected.Length; i++)
        {
            Assert.Equal(expected[i], actual[i]);
        }
    }
}
