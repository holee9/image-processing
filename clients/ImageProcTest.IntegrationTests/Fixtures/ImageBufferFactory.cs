using System.Runtime.InteropServices;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// Factory that allocates synthetic <see cref="XpeCommonNative.XpeImageBuffer"/> instances
/// filled with a deterministic pattern and frees them on disposal.
/// Seed=0, pattern: (ushort)(1000 + (index % 97)) per research.md §7.4
/// </summary>
public sealed class ImageBufferFactory : IDisposable
{
    private readonly List<GCHandle> _handles = new();

    /// <summary>Creates a 16x16 UInt16 buffer filled with the canonical synthetic pattern.</summary>
    public XpeCommonNative.XpeImageBuffer CreateSynthetic16x16()
    {
        const uint width = 16;
        const uint height = 16;
        const int count = (int)(width * height);

        var pixels = new ushort[count];
        for (var i = 0; i < count; i++)
            pixels[i] = (ushort)(1000 + (i % 97));

        var handle = GCHandle.Alloc(pixels, GCHandleType.Pinned);
        _handles.Add(handle);

        return new XpeCommonNative.XpeImageBuffer
        {
            Width = width,
            Height = height,
            BitsAllocated = 16,
            BitsStored = 16,
            Format = XpeCommonNative.XpePixelFormat.UInt16,
            Data = handle.AddrOfPinnedObject(),
            DataSize = (nuint)(count * sizeof(ushort)),
        };
    }

    /// <summary>Creates an empty buffer with all fields zeroed.</summary>
    public static XpeCommonNative.XpeImageBuffer CreateEmpty() =>
        default;

    public void Dispose()
    {
        foreach (var h in _handles)
            if (h.IsAllocated) h.Free();
        _handles.Clear();
    }
}
