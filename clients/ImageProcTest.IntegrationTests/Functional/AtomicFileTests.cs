// GUI-C-232b (Codex #165): a save writes beside the target and puts the file in place, so a failure keeps the previous file. Needs no external data, so CI runs it.
using ImageProcTest.Services;

namespace ImageProcTest.IntegrationTests.Functional;

public sealed class AtomicFileTests : IDisposable
{
    private readonly string _folder = Path.Combine(Path.GetTempPath(), "xpe_atomic_" + Guid.NewGuid().ToString("N"));

    public AtomicFileTests() => Directory.CreateDirectory(_folder);

    public void Dispose() => Directory.Delete(_folder, recursive: true);

    [Fact]
    public void ANewFile_IsWrittenWithExactlyTheBytes_AndNoTempFileIsLeft()
    {
        var path = Path.Combine(_folder, "new.raw");
        AtomicFile.WriteAllBytes(path, [1, 2, 3, 4]);

        Assert.Equal(new byte[] { 1, 2, 3, 4 }, File.ReadAllBytes(path));
        Assert.Equal([path], Directory.GetFileSystemEntries(_folder));
    }

    [Fact]
    public void AnExistingFile_IsReplacedWhole()
    {
        var path = Path.Combine(_folder, "old.raw");
        File.WriteAllBytes(path, new byte[100]);
        AtomicFile.WriteAllBytes(path, [9, 9]);

        Assert.Equal(new byte[] { 9, 9 }, File.ReadAllBytes(path));
        Assert.Single(Directory.GetFileSystemEntries(_folder));
    }

    [Fact]
    public void AFailedWrite_KeepsThePreviousFile_AndLeavesNoTempFile()
    {
        var path = Path.Combine(_folder, "keep.raw");
        File.WriteAllBytes(path, [7, 7, 7]);

        // the previous file is held open without sharing: putting a new one in place must fail, and the old content must survive
        using (new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None))
        {
            Assert.ThrowsAny<IOException>(() => AtomicFile.WriteAllBytes(path, new byte[1000]));
        }

        Assert.Equal(new byte[] { 7, 7, 7 }, File.ReadAllBytes(path));
        Assert.Equal([path], Directory.GetFileSystemEntries(_folder));
    }

    [Fact]
    public void AFolderThatDoesNotExist_Fails_WithoutCreatingAnything()
    {
        var path = Path.Combine(_folder, "missing", "x.raw");
        Assert.ThrowsAny<IOException>(() => AtomicFile.WriteAllBytes(path, [1]));
        Assert.Empty(Directory.GetDirectories(_folder));
    }
}
