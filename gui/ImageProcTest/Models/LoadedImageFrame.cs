using System.Windows.Media.Imaging;

namespace ImageProcTest.Models;

public sealed class LoadedImageFrame
{
    public required BitmapSource Preview { get; init; }

    public BitmapSource? ProcessedPreview { get; init; }

    /// <summary>
    /// Per-phase timing of the display pipeline that produced this frame (#180, GUI-C-103), e.g.
    /// <c>display: marshal-in=41 ms, native=88 ms, marshal-out=19 ms, preview=220 ms</c>.
    /// Empty when no display pipeline ran, or when the backend does not measure itself.
    /// </summary>
    public string DisplayTimings { get; init; } = string.Empty;

    public required string Summary { get; init; }

    public required string MetadataText { get; init; }

    public ushort[]? RawPixels { get; init; }

    public int Width { get; init; }

    public int Height { get; init; }

    public int BitsStored { get; init; } = 16;

    /// <summary>
    /// GUI-C-232 (B): set when the file's length did not match the size in the settings and the loader opened it at a size it worked out from the length. The window shows it where the user reads
    /// the load result, so a size the user did not ask for is never a silent one.
    /// </summary>
    public string? SizeNotice { get; init; }

    public bool DisplayPipelineApplied { get; init; }

    public string DisplayPipelineSummary { get; init; } = string.Empty;
}
