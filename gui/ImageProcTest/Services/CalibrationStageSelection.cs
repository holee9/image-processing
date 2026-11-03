using ImageProcTest.Models;

namespace ImageProcTest.Services;

/// <summary>Auto uses an available map, On requires one, Off never loads one. Load validation is authoritative.</summary>
public static class CalibrationStageSelection
{
    public static bool ShouldLoad(string mode, bool exists) => CalibrationStageMode.Normalize(mode) switch
    { CalibrationStageMode.Off => false, CalibrationStageMode.On => true, _ => exists };

    public static string[] ExecutedFromReason(string? reason)
    {
        const string marker = "executed=[";
        var start = reason?.IndexOf(marker, StringComparison.Ordinal) ?? -1;
        if (start < 0) return [];
        start += marker.Length;
        var end = reason!.IndexOf(']', start);
        return end < start ? [] : reason[start..end].Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
    }
}
