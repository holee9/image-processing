// GUI-C-232b: a save never leaves a half-written file where a good one was.
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// Writes a file by first writing a temporary file in the SAME folder and then putting it in place, so a failure while writing (disk full, a folder that vanished, a lock) leaves the previous file exactly as it was.
/// Same folder matters: a move inside one volume is a rename, a move across volumes is a copy that can itself be cut short.
/// </summary>
public static class AtomicFile
{
    public static void WriteAllBytes(string path, ReadOnlySpan<byte> bytes)
    {
        var directory = Path.GetDirectoryName(Path.GetFullPath(path)) ?? throw new IOException($"No folder in '{path}'.");
        var temp = Path.Combine(directory, "." + Path.GetFileName(path) + "." + Guid.NewGuid().ToString("N") + ".tmp");
        try
        {
            using (var stream = new FileStream(temp, FileMode.CreateNew, FileAccess.Write, FileShare.None))
            {
                stream.Write(bytes);
                stream.Flush(flushToDisk: true);
            }

            if (File.Exists(path))
            {
                File.Replace(temp, path, destinationBackupFileName: null);
            }
            else
            {
                File.Move(temp, path);
            }
        }
        catch
        {
            try { File.Delete(temp); } catch (IOException) { /* nothing more to do for a temp file */ } catch (UnauthorizedAccessException) { /* same */ }
            throw;
        }
    }
}
