using System.IO;
using System.Text.Json;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>
/// What a load produced, and — when the stored file could not be read — where the original was put
/// (#173, GUI-C-119).
///
/// <para><b>Why the return type changed.</b> <see cref="AppSettingsService.Load"/> used to return
/// <see cref="AppSettings"/> alone, so a corrupt file and an absent file produced the same object and
/// nothing downstream could tell them apart. GUI-C-118 measured what that costs: all three corruption
/// kinds (truncated, wrong type, wrong encoding) lose EVERY stored setting — including the keys in the
/// same file that were perfectly good — and nothing on screen says so. The failure had no place to
/// live in the old signature; this record is that place.</para>
/// </summary>
/// <param name="Settings">The settings to run with — the stored ones, or defaults when they could not be read.</param>
/// <param name="PreservedOriginalPath">
/// Where the unreadable original was moved, or null when the file loaded (or when there was no file).
/// </param>
public sealed record SettingsLoadResult(AppSettings Settings, string? PreservedOriginalPath)
{
    /// <summary>True when the stored file existed but could not be read.</summary>
    public bool FailedToRead => PreservedOriginalPath is not null;
}

public sealed class AppSettingsService
{
    private readonly JsonSerializerOptions _serializerOptions = new()
    {
        WriteIndented = true
    };

    public AppSettingsService(string? filePath = null)
    {
        FilePath = filePath ?? Path.Combine(AppContext.BaseDirectory, "appsettings.json");
    }

    public string FilePath { get; }

    /// <summary>
    /// Reads the stored settings, or falls back to defaults.
    ///
    /// <para><b>An unreadable file is moved aside before the fallback, not after.</b> The next Save
    /// writes the whole defaults object over this path (measured in GUI-C-118: 89/105/190 bytes became
    /// 1482, with no backup anywhere), so without this the original is gone the first time the user
    /// tidies up the settings they just found reset — believing they are restoring them.</para>
    ///
    /// <para>Moving aside rather than refusing to save is deliberate: refusing leaves the user with no
    /// way out, while a move costs nothing and makes the loss reversible. This happens once, at the
    /// failed read — it is not a rolling backup of good files.</para>
    ///
    /// <para>Still no exception leaves this method. An app that dies at startup over a settings file is
    /// worse than one that starts at defaults: people come here to look at images.</para>
    /// </summary>
    public SettingsLoadResult Load()
    {
        if (!File.Exists(FilePath))
        {
            return new SettingsLoadResult(new AppSettings(), null);
        }

        try
        {
            var json = File.ReadAllText(FilePath);
            var settings = JsonSerializer.Deserialize<AppSettings>(json, _serializerOptions);
            if (settings is not null)
            {
                return new SettingsLoadResult(settings, null);
            }
        }
        catch
        {
            // Fall through to preservation. The reason is not reported: every reason has the same
            // consequence for the user, and the file itself is kept so the detail is not lost.
        }

        return new SettingsLoadResult(new AppSettings(), PreserveUnreadable());
    }

    /// <summary>
    /// Moves the unreadable file aside and returns its new path, or null when even that failed.
    ///
    /// <para>A failure here must not stop the app starting, so it is swallowed like the read failure —
    /// but it returns null rather than a path, so the caller never tells the user the original is
    /// somewhere it is not.</para>
    /// </summary>
    private string? PreserveUnreadable()
    {
        try
        {
            var preserved = $"{FilePath}.unreadable-{DateTime.Now:yyyyMMdd-HHmmss}";
            for (var attempt = 1; File.Exists(preserved); attempt++)
            {
                preserved = $"{FilePath}.unreadable-{DateTime.Now:yyyyMMdd-HHmmss}-{attempt}";
            }

            File.Move(FilePath, preserved);
            return preserved;
        }
        catch
        {
            return null;
        }
    }

    public void Save(AppSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        File.WriteAllText(FilePath, JsonSerializer.Serialize(settings, _serializerOptions));
    }
}
