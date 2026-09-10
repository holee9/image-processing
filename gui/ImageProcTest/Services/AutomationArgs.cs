// #136: command-line parsing for automation runs, kept out of the WPF Application class so it can
// be tested. Same move as GUI-C-24 made for the alert-drain wrapper.
using System.IO;

namespace ImageProcTest.Services;

/// <summary>
/// The automation switches, parsed and validated.
///
/// This lived inside <c>App.ParseAutomationArgs</c>, which is a WPF <c>Application</c> member and so
/// unreachable from the test assembly — GUI-C-26 shipped the <c>--automation-backend</c> switch with
/// no regression at all, and said so. Parsing has no WPF dependency, so it moves here.
///
/// <see cref="Error"/> is what makes an unusable value visible: an unrecognised backend name used to
/// flow into settings and be quietly downgraded to Mock by XpeBackendFactory, so a run could report
/// "arg" as the source while exercising a backend the caller never asked for.
/// </summary>
public sealed record AutomationArgs(
    string? RawPath,
    string? ReportPath,
    string? BackendMode,
    string? CalibrationDirectory,
    int? RawWidth,
    int? RawHeight,
    string? Error)
{
    /// <summary>Backend names the automation accepts, in their canonical spelling.</summary>
    public static readonly string[] AcceptedBackendModes = ["Mock", "Native"];

    /// <summary>An automation run needs both a raw image and somewhere to write its report.</summary>
    public bool IsAutomationMode =>
        !string.IsNullOrWhiteSpace(RawPath) && !string.IsNullOrWhiteSpace(ReportPath);

    /// <summary>True when nothing in the command line was rejected.</summary>
    public bool IsValid => Error is null;

    /// <summary>
    /// Parses the switches. A recognised switch missing its value, or a backend name outside
    /// <see cref="AcceptedBackendModes"/>, produces an <see cref="Error"/> rather than being skipped:
    /// silently ignoring it is how a typo turns into a run that measures the wrong thing.
    /// </summary>
    public static AutomationArgs Parse(string[] args)
    {
        ArgumentNullException.ThrowIfNull(args);

        string? rawPath = null, reportPath = null, backendMode = null, calibrationDirectory = null, error = null;
        int? rawWidth = null, rawHeight = null;

        for (var i = 0; i < args.Length; i++)
        {
            var switchName = args[i];
            if (!switchName.StartsWith("--automation-", StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            if (i + 1 >= args.Length)
            {
                error ??= $"{switchName} was given without a value.";
                break;
            }

            var value = args[++i];

            if (Is(switchName, "--automation-raw"))
            {
                rawPath = Path.GetFullPath(value);
            }
            else if (Is(switchName, "--automation-report"))
            {
                reportPath = Path.GetFullPath(value);
            }
            else if (Is(switchName, "--automation-backend"))
            {
                var canonical = AcceptedBackendModes.FirstOrDefault(
                    m => string.Equals(m, value, StringComparison.OrdinalIgnoreCase));

                if (canonical is null)
                {
                    error ??= $"--automation-backend '{value}' is not one of " +
                              $"{string.Join(" | ", AcceptedBackendModes)}.";
                    continue;
                }

                backendMode = canonical;
            }
            else if (Is(switchName, "--automation-calib"))
            {
                // #141: the XCal set is generated at run time (xpe_calib_fixture_gen), so the
                // directory cannot be a compiled-in default — the caller names it.
                calibrationDirectory = Path.GetFullPath(value);
            }
            else if (Is(switchName, "--automation-width"))
            {
                if (!int.TryParse(value, out var width))
                {
                    error ??= $"--automation-width '{value}' is not an integer.";
                    continue;
                }

                rawWidth = width;
            }
            else if (Is(switchName, "--automation-height"))
            {
                if (!int.TryParse(value, out var height))
                {
                    error ??= $"--automation-height '{value}' is not an integer.";
                    continue;
                }

                rawHeight = height;
            }
            else
            {
                // #136: an --automation-* switch nobody recognises is refused, not skipped. A typo in
                // the SWITCH name is the same hazard as one in its value: the run starts, the report
                // looks like the requested one, and the option silently did nothing.
                error ??= $"{switchName} is not a recognised automation switch.";
            }
        }

        // A rejection keeps only the report destination: writing the reason somewhere the caller
        // already named is the point, while carrying the other half-applied values forward would
        // start a run that looks like the requested one. When the rejection happened before
        // --automation-report was seen, there is nowhere to write and the exit code is the signal.
        return error is null
            ? new AutomationArgs(rawPath, reportPath, backendMode, calibrationDirectory, rawWidth, rawHeight, Error: null)
            : new AutomationArgs(
                RawPath: null, reportPath, BackendMode: null, CalibrationDirectory: null,
                RawWidth: null, RawHeight: null, error);
    }

    private static bool Is(string argument, string switchName) =>
        string.Equals(argument, switchName, StringComparison.OrdinalIgnoreCase);
}
