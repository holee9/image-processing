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
/// Where the user's original settings are kept, or null when the file loaded (or when there was no
/// file, or when preserving it failed).
/// </param>
/// <param name="PreservedIsFromAnEarlierFailure">
/// True when this path was rescued by an EARLIER failed read rather than this one — see
/// <see cref="AppSettingsService.Load"/> for why the earlier rescue is the valuable one.
/// </param>
public sealed record SettingsLoadResult(
    AppSettings Settings,
    string? PreservedOriginalPath,
    bool PreservedIsFromAnEarlierFailure = false)
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
    /// <para><b>An existing rescue is never replaced (#173, GUI-C-120).</b> The file rescued at the
    /// FIRST failure is the user's real settings; by the time a second failure happens this path has
    /// been rewritten from defaults (GUI-C-118 measured the next Save putting 1482 bytes over it), so
    /// a later rescue holds something closer to defaults than to what the user had. Keeping the newest
    /// would discard exactly the thing worth keeping, and keeping both would accumulate files with
    /// nobody to remove them. So the count stays at one, and that one is the earliest.</para>
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

        var earlier = ExistingRescue();
        if (earlier is not null)
        {
            // Leave both this unreadable file and the earlier rescue where they are. The caller says
            // where the earlier one is, so the user can still find what they had.
            return new SettingsLoadResult(new AppSettings(), earlier, PreservedIsFromAnEarlierFailure: true);
        }

        var rescued = PreserveUnreadable();
        var defaults = new AppSettings();

        if (rescued is not null)
        {
            // The original is safe, so the path it left empty is filled with defaults and the next
            // launch is quiet (#173, GUI-C-121). Leaving it unreadable would repeat this warning on
            // every start, and a warning that always appears is one nobody reads.
            //
            // Only a run that actually rescued may do this. When the rescue failed the file below is
            // still the user's only copy, so it is left exactly as it is — writing defaults over it
            // would undo what GUI-C-119 set out to keep.
            TryWriteDefaults(defaults);
        }

        return new SettingsLoadResult(defaults, rescued);
    }

    /// <summary>
    /// Moves the unreadable file aside and returns its new path, or null when even that failed.
    ///
    /// <para>A failure here must not stop the app starting, so it is swallowed like the read failure —
    /// but it returns null rather than a path, so the caller never tells the user the original is
    /// somewhere it is not.</para>
    /// </summary>
    /// <summary>
    /// The rescue an earlier failed read left behind, or null when there is none.
    ///
    /// <para>The earliest is returned when several exist — a tree carrying more than one predates this
    /// rule, and the earliest is still the one closest to what the user had. The names are timestamped
    /// in a sortable format, so ordinal order is chronological order.</para>
    /// </summary>
    private string? ExistingRescue()
    {
        try
        {
            var directory = Path.GetDirectoryName(FilePath);
            if (string.IsNullOrEmpty(directory)) return null;

            return Directory
                .GetFiles(directory, Path.GetFileName(FilePath) + ".unreadable-*")
                .OrderBy(path => path, StringComparer.Ordinal)
                .FirstOrDefault();
        }
        catch
        {
            return null;
        }
    }

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

    /// <summary>
    /// Writes a defaults file over the path a successful rescue emptied.
    ///
    /// <para>A failure here is swallowed for the same reason every other failure on this path is: the
    /// app must start. The cost of failing is only that the next launch warns again — the original is
    /// already safe by the time this runs.</para>
    /// </summary>
    private void TryWriteDefaults(AppSettings defaults)
    {
        try
        {
            Save(defaults);
        }
        catch
        {
            // Next launch repeats the warning. Nothing is lost.
        }
    }

    public void Save(AppSettings settings)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        File.WriteAllText(FilePath, JsonSerializer.Serialize(settings, _serializerOptions));
    }
}
