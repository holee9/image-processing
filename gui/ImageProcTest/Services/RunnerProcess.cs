using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace ImageProcTest.Services;

/// <summary>
/// What became of one attempt to run a runner process (#225 rows 15, 16 and 17, GUI-C-177).
///
/// <para>Two different things used to share one answer. "The process could not be started" (the file is not
/// there, ctest is not on PATH, the file is not an executable) means NOTHING ran, so there is no verdict to
/// report; "the process ran and exited non-zero" means something ran and its verdict is a failure. Reporting both
/// as "failed" sends an operator whose PATH is wrong to look for a defect in the code under test.</para>
/// </summary>
/// <param name="Started">False when no process was started at all.</param>
/// <param name="ExitCode">Meaningful only when <paramref name="Started"/>.</param>
/// <param name="LastLine">The line the verdict quotes; empty when nothing started.</param>
/// <param name="ElapsedMs">Wall time from just before the start to the exit; 0 when nothing started.</param>
/// <param name="StartError">Why nothing started; null when something did.</param>
public sealed record RunnerOutcome(bool Started, int ExitCode, string LastLine, double ElapsedMs, string? StartError)
{
    /// <summary>
    /// True/false once a process ran and exited (<c>== 0</c> and nothing else); null when none was started —
    /// "did not run", the same third state the runners' callers already keep apart from "failed".
    /// </summary>
    public bool? Verdict => Started ? ExitCode == 0 : null;

    public static RunnerOutcome DidNotStart(string reason) => new(false, 0, string.Empty, 0, reason);
}

/// <summary>
/// The one place a runner process is started and its outcome put into words (#225 rows 15, 16 and 17). Everything
/// in the app that runs a child process for a menu command goes through <see cref="Run"/>; the status line for it is
/// <see cref="Describe"/>'s, so the three commands cannot word the same situation differently.
/// </summary>
public static class RunnerProcess
{
    /// <summary>
    /// Starts <paramref name="exePath"/> with <paramref name="arguments"/> (an argument list, never a command line:
    /// nothing is parsed by a shell, so a pattern with '|' stays one argument), waits for it, and returns what happened.
    /// A start that fails — any exception from the start itself — is <see cref="RunnerOutcome.DidNotStart"/>; an
    /// exception AFTER the process started (reading its output) is not caught here: something ran, and the caller
    /// must not read that as "did not run".
    /// </summary>
    public static RunnerOutcome Run(
        string exePath, IReadOnlyList<string>? arguments = null, string? workingDirectory = null,
        Func<string, string?>? summarize = null)
    {
        var start = new ProcessStartInfo(exePath)
        {
            WorkingDirectory = workingDirectory ?? Path.GetDirectoryName(exePath)!,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        };

        foreach (var argument in arguments ?? Array.Empty<string>())
        {
            start.ArgumentList.Add(argument);
        }

        var stopwatch = Stopwatch.StartNew();
        Process process;
        try
        {
            process = Process.Start(start)
                      ?? throw new InvalidOperationException($"Process.Start returned null for '{exePath}'.");
        }
        catch (Exception ex)
        {
            return RunnerOutcome.DidNotStart(StartFailureReason(ex));
        }

        using (process)
        {
            var stdout = process.StandardOutput.ReadToEnd();
            var stderr = process.StandardError.ReadToEnd();
            process.WaitForExit();
            stopwatch.Stop();

            // Which line to report, measured rather than guessed (GUI-C-158): a failing runner throws, so
            // stderr begins with "Unhandled exception. ...: <reason>" and CONTINUES with stack frames. The
            // first attempt reported the LAST line and produced "at Program...line 62" — true, and useless.
            // The reason is the FIRST stderr line; on success there is no stderr and the runner's verdict
            // is the last stdout line.
            var fallback = string.IsNullOrWhiteSpace(stderr)
                ? stdout.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                    .Select(line => line.Trim()).LastOrDefault(line => line.Length > 0)
                : stderr.Split('\n', StringSplitOptions.RemoveEmptyEntries)
                    .Select(line => line.Trim()).FirstOrDefault(line => line.Length > 0);

            var reported = summarize?.Invoke(stdout) ?? fallback;

            return new RunnerOutcome(true, process.ExitCode, reported ?? "(no output)", stopwatch.Elapsed.TotalMilliseconds, null);
        }
    }

    /// <summary>
    /// The status-bar line for an outcome. "did not run" and "FAILED" never share wording, and each names the
    /// command by <paramref name="label"/>, so the same text cannot be produced for a start failure and an exit code.
    /// <paramref name="quotesSummary"/> is true when the caller chose the line the verdict quotes (row 17's ctest
    /// summary): a pass then shows it too, where the self-check and E2E runners keep their short pass line.
    /// </summary>
    public static string Describe(string label, string exePath, RunnerOutcome outcome, bool quotesSummary)
    {
        if (!outcome.Started)
        {
            return $"{label} did not run: '{exePath}' could not be started ({outcome.StartError}). Nothing was executed.";
        }

        return outcome.ExitCode == 0
            ? $"{label} passed in {outcome.ElapsedMs:0} ms." + (quotesSummary ? $" Exit 0. {outcome.LastLine}" : string.Empty)
            : $"{label} FAILED (exit {outcome.ExitCode}): {outcome.LastLine}";
    }

    /// <summary>The reason a start failed, with the likely cause when the OS says the file is not there (error 2 or 3).</summary>
    private static string StartFailureReason(Exception ex) =>
        ex is Win32Exception { NativeErrorCode: 2 or 3 }
            ? $"the file was not found; a bare name such as 'ctest' has to be on PATH: {ex.Message}"
            : ex.Message;
}
