// REQ-GUI-IT-053: Version pin — xpe_version() major must match pinned_major in expected-versions.json.
using System.Runtime.InteropServices;
using System.Text.Json;
using ImageProcTest.IntegrationTests.Fixtures;
using ImageProcTest.IntegrationTests.PInvoke;

namespace ImageProcTest.IntegrationTests.Smoke;

/// <summary>
/// Verifies that the runtime xpe_common.dll version matches the repository's
/// pinned major version contract (Resources/expected-versions.json). This
/// catches accidental ABI-breaking releases that forget to update the pin.
/// Covers REQ-GUI-IT-053.
/// </summary>
[Trait("Category", "Smoke")]
[Collection(NativeLibraryCollection.Name)]
public sealed class VersionPinTests
{
    private const string ExpectedVersionsResource = "Resources/expected-versions.json";
    private readonly NativeLibraryFixture _fixture;

    public VersionPinTests(NativeLibraryFixture fixture)
    {
        _fixture = fixture;
    }

    [Fact]
    public void XpeVersion_MajorMatchesPinnedVersion()
    {
        if (!_fixture.IsAvailable) return; // DLL not available — test skipped

        var pinnedMajor = LoadPinnedMajor();

        var ptr = XpeCommonNative.xpe_version();
        Assert.NotEqual(IntPtr.Zero, ptr);

        var version = Marshal.PtrToStringAnsi(ptr);
        Assert.NotNull(version);

        var dot = version!.IndexOf('.');
        Assert.True(dot > 0, $"xpe_version() must be semver-formatted (got '{version}')");

        var majorText = version.Substring(0, dot);
        Assert.True(int.TryParse(majorText, out var runtimeMajor),
            $"xpe_version() major segment must parse as integer (got '{majorText}')");

        Assert.Equal(pinnedMajor, runtimeMajor);
    }

    private static int LoadPinnedMajor()
    {
        var path = Path.Combine(AppContext.BaseDirectory, ExpectedVersionsResource);
        Assert.True(File.Exists(path), $"Expected pin file missing: {path}");

        using var stream = File.OpenRead(path);
        using var doc = JsonDocument.Parse(stream);
        var root = doc.RootElement;

        Assert.True(root.TryGetProperty("xpe_common", out var xpeCommon),
            "expected-versions.json must contain 'xpe_common' object");
        Assert.True(xpeCommon.TryGetProperty("pinned_major", out var pinned),
            "xpe_common.pinned_major is required");

        return pinned.GetInt32();
    }
}
