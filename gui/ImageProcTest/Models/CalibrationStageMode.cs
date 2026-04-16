namespace ImageProcTest.Models;

/// <summary>
/// Defines the stage-control vocabulary used by the Test GUI calibration evaluation workflow.
/// </summary>
public static class CalibrationStageMode
{
    public const string Auto = "Auto";
    public const string On = "On";
    public const string Off = "Off";

    public static readonly string[] Options = { Auto, On, Off };

    public static string Normalize(string? value)
    {
        if (string.Equals(value, On, StringComparison.OrdinalIgnoreCase))
        {
            return On;
        }

        if (string.Equals(value, Off, StringComparison.OrdinalIgnoreCase))
        {
            return Off;
        }

        return Auto;
    }
}
