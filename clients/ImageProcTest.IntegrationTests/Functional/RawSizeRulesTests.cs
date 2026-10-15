// GUI-C-232b (Codex #165, leader decision): the size is never guessed; a file whose length is not width x height x 2 is refused, and so is an image whose size differs from the calibration maps. No external data.
using System.Buffers.Binary;
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

public sealed class RawSizeRulesTests : IDisposable
{
    private readonly string _folder = Path.Combine(Path.GetTempPath(), "xpe_rawsize_" + Guid.NewGuid().ToString("N"));

    public RawSizeRulesTests() => Directory.CreateDirectory(_folder);

    public void Dispose() => Directory.Delete(_folder, recursive: true);

    [Fact]
    public void ALengthOfExactlyWidthTimesHeightTimesTwo_IsAccepted() => RawSizeRules.CheckLength(1024L * 1024 * 2, 1024, 1024);

    [Theory]
    [InlineData(3072L * 3072 * 2, 1024, 1024)]   // a square file under a smaller square setting: not inferred, refused
    [InlineData(1000L * 500 * 2, 1024, 1024)]    // shorter, not square
    [InlineData(2000L * 1100 * 2, 1024, 1024)]   // longer, not square
    [InlineData(1024L * 1024 * 2, 3072, 3072)]   // shorter, square
    public void AnyOtherLength_IsRefused_WithTheNeededSizeTheActualBytesAndHowToSetIt(long fileBytes, int width, int height)
    {
        var expected = (long)width * height * 2;
        var ex = Assert.Throws<InvalidDataException>(() => RawSizeRules.CheckLength(fileBytes, width, height));
        Assert.Contains(fileBytes.ToString(), ex.Message);
        Assert.Contains(expected.ToString(), ex.Message);
        Assert.Contains("rawWidth/rawHeight", ex.Message);
        Assert.Contains("--automation-width", ex.Message);
        Assert.StartsWith(fileBytes < expected ? "Raw file is too small. Expected at least" : "Raw file has", ex.Message);
    }

    [Fact]
    public void ADifferentShapeOfTheSameLength_IsAccepted_WhichIsTheDocumentedLimit()
    {
        // 3072 x 3072 and 2048 x 4608 have the same number of pixels: a length cannot prove the shape, so the setting must be right (the quick start says so)
        RawSizeRules.CheckLength(2048L * 4608 * 2, 3072, 3072);
    }

    private string WriteMap(string kind, uint width, uint height)
    {
        var directory = Path.Combine(_folder, kind);
        Directory.CreateDirectory(directory);
        var bytes = new byte[64];
        "XCAL"u8.CopyTo(bytes);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(16), width);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(20), height);
        File.WriteAllBytes(Path.Combine(directory, kind.ToLowerInvariant() + ".xcal"), bytes);
        return directory;
    }

    [Fact]
    public void AMapOfTheImageSize_IsAccepted_AndAFolderWithoutAMapIsNotAnError()
    {
        var offset = WriteMap("Offset", 1024, 1024);
        RawSizeRules.CheckMapSizes(1024, 1024, ("Offset", offset), ("Gain", Path.Combine(_folder, "nowhere")), ("Defect", string.Empty));
    }

    [Theory]
    [InlineData(1024u, 1024u, 3072, 3072)]   // maps for 1024x1024, image 3072x3072
    [InlineData(3072u, 3072u, 3072, 2048)]   // a rectangle against a square map
    public void AMapOfAnotherSize_RefusesTheImage_NamingBothSizes(uint mapWidth, uint mapHeight, int width, int height)
    {
        var gain = WriteMap("Gain", mapWidth, mapHeight);
        var ex = Assert.Throws<InvalidDataException>(() => RawSizeRules.CheckMapSizes(width, height, ("Gain", gain)));
        Assert.Contains($"{mapWidth}x{mapHeight}", ex.Message);
        Assert.Contains($"{width}x{height}", ex.Message);
        Assert.Contains("gain calibration map", ex.Message);
    }

    [Fact]
    public void AFileThatIsNotAnXcalMap_IsLeftToTheStage_NotRefusedHere()
    {
        var directory = Path.Combine(_folder, "Offset");
        Directory.CreateDirectory(directory);
        File.WriteAllBytes(Path.Combine(directory, "offset.xcal"), new byte[64]);
        RawSizeRules.CheckMapSizes(1024, 1024, ("Offset", directory));
    }
}
