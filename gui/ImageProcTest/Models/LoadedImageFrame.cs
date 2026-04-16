using System.Windows.Media.Imaging;

namespace ImageProcTest.Models;

public sealed class LoadedImageFrame
{
    public required BitmapSource Preview { get; init; }

    public BitmapSource? ProcessedPreview { get; init; }

    public required string Summary { get; init; }

    public required string MetadataText { get; init; }

    public ushort[]? RawPixels { get; init; }

    public int Width { get; init; }

    public int Height { get; init; }

    public int BitsStored { get; init; } = 16;

    public bool DisplayPipelineApplied { get; init; }

    public string DisplayPipelineSummary { get; init; } = string.Empty;
}
