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
    string? Error,
    string? SettingsPath = null,
    string? RenderDumpPath = null,
    string? SelfCheckExePath = null
#if XPE_TEST_FAULTS
    , int? DisplayPipelineFailAfter = null,
    bool AiWorkerDisabled = false,
    int? AiWorkerSilentAfter = null
#endif
    )
{
#if XPE_TEST_FAULTS
    // GUI-C-193: everything about the two fault switches is compiled only in a test build (XPE_TEST_FAULTS, Debug by default). A shipped
    // build has no such members, no such strings and no such branch below: the switch is "not a recognised automation switch".

    /// <summary>
    /// #171 (GUI-C-79): the first accepted fault. <c>display-pipeline-after:N</c> lets the first N display
    /// pipeline calls succeed and makes every later one throw, so the failure path can be tested end to
    /// end. Command line only, off unless given, and an unknown fault is refused like any other switch.
    /// </summary>
    public const string DisplayPipelineFaultPrefix = "display-pipeline-after:";

    /// <summary>
    /// GUI-C-191b: the second accepted fault. <c>ai-worker-disabled</c> makes the AI worker status read answer "switched off, 3 of 3", so
    /// the mark (banner + Restart AI) can be put on screen WITHOUT a native module and looked at by a UI test at the window sizes users
    /// have. Same terms as <see cref="DisplayPipelineFaultPrefix"/>: command line only, inert without the argument, loud when armed.
    /// </summary>
    public const string AiWorkerDisabledFault = "ai-worker-disabled";

    /// <summary>
    /// GUI-C-192c: the third accepted fault. <c>ai-worker-silent</c> makes the AI worker status read answer "active" once and then never answer
    /// again (a worker that stopped replying), so the status-unconfirmed notice can be put on screen without a native module. Same terms as
    /// the others: command line only, inert without the argument, loud when armed.
    /// </summary>
    public const string AiWorkerSilentFault = "ai-worker-silent";

    /// <summary>GUI-C-192d: <c>ai-worker-silent:0</c> is silent from the very first read (no answer is ever given), the never-confirmed case.</summary>
    public const string AiWorkerSilentFromStartFault = "ai-worker-silent:0";
#endif

    /// <summary>
    /// #225 (GUI-C-159): where the Run Self-Check command should look for its runner, overriding the
    /// path derived from the repository root.
    ///
    /// <para>It exists for the same reason the test fault switches do (#171): the FAILING
    /// path has to be observable. Without it a test can only watch the self-check succeed, and
    /// "reports success correctly" and "reports everything as success" look identical — the shape
    /// #205, #207 and #212 each turned out to be. A test points this at a copy of the runner staged
    /// outside the repository, where it genuinely fails, and then asserts that the app says so.</para>
    ///
    /// <para>Command line only and off unless given, so a normal launch resolves the runner the usual
    /// way.</para>
    /// </summary>
    public const string SelfCheckExeSwitch = "--automation-selfcheck-exe";

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
        string? settingsPath = null;
        string? renderDumpPath = null;
        string? selfCheckExePath = null;
        int? rawWidth = null, rawHeight = null;
#if XPE_TEST_FAULTS
        int? displayPipelineFailAfter = null;
        var aiWorkerDisabled = false;
        int? aiWorkerSilentAfter = null;
#endif

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
            else if (Is(switchName, SelfCheckExeSwitch))
            {
                selfCheckExePath = Path.GetFullPath(value);
            }
            else if (Is(switchName, "--automation-export-render"))
            {
                // #200 (GUI-C-138): writes the drawn pixels of the processed layer to this path on every
                // render. The automation surface reports a hash and a mean; the pixels are what a
                // statistic study needs, and a screenshot is the composited control rather than the
                // frame the pipeline produced.
                renderDumpPath = Path.GetFullPath(value);
            }
            else if (Is(switchName, "--automation-settings"))
            {
                // #173 (GUI-C-119): names the settings file this run reads and writes. It exists so an
                // E2E case can put a corrupt file in front of a real launch — the unreadable-file path
                // is otherwise reachable only by reading the code.
                settingsPath = Path.GetFullPath(value);
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
#if XPE_TEST_FAULTS
            else if (Is(switchName, "--automation-fault") && string.Equals(value, AiWorkerDisabledFault, StringComparison.Ordinal))
            {
                aiWorkerDisabled = true;
            }
            else if (Is(switchName, "--automation-fault") && string.Equals(value, AiWorkerSilentFault, StringComparison.Ordinal))
            {
                aiWorkerSilentAfter = 1;
            }
            else if (Is(switchName, "--automation-fault") && string.Equals(value, AiWorkerSilentFromStartFault, StringComparison.Ordinal))
            {
                aiWorkerSilentAfter = 0;
            }
            else if (Is(switchName, "--automation-fault"))
            {
                if (!value.StartsWith(DisplayPipelineFaultPrefix, StringComparison.Ordinal)
                    || !int.TryParse(value[DisplayPipelineFaultPrefix.Length..], System.Globalization.NumberStyles.None,
                        System.Globalization.CultureInfo.InvariantCulture, out var failAfter))
                {
                    error ??= $"--automation-fault '{value}' is not a recognised fault " +
                              $"(expected {DisplayPipelineFaultPrefix}<non-negative integer>, {AiWorkerDisabledFault}, {AiWorkerSilentFault} or {AiWorkerSilentFromStartFault}).";
                    continue;
                }

                displayPipelineFailAfter = failAfter;
            }
#endif
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
            ? new AutomationArgs(rawPath, reportPath, backendMode, calibrationDirectory, rawWidth, rawHeight, Error: null,
                settingsPath, renderDumpPath, selfCheckExePath
#if XPE_TEST_FAULTS
                , displayPipelineFailAfter, aiWorkerDisabled, aiWorkerSilentAfter
#endif
                )
            : new AutomationArgs(
                RawPath: null, reportPath, BackendMode: null, CalibrationDirectory: null,
                RawWidth: null, RawHeight: null, error);
    }

    private static bool Is(string argument, string switchName) =>
        string.Equals(argument, switchName, StringComparison.OrdinalIgnoreCase);
}
