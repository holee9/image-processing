using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-212b (#249, Codex #96): runs <see cref="XpePreprocessSyntheticOracle"/> in a child process so that its shutdown and synthetic calibration never touch the module state of THIS
    /// process (see the oracle's header). The child is this same executable started with <see cref="ModeArgument"/> -- no second binary to ship -- and answers with ONE protocol line on
    /// standard output: <see cref="ResultPrefix"/> followed by the result as JSON.
    ///
    /// GUI-C-212c (Codex #98): the parent used to believe the last line that parsed as JSON, whatever the exit code, and the readiness check read one field. A child that crashed after printing
    /// <c>{"Passed":true}</c>, or a stray log line that happened to be JSON, was a pass. Now a pass needs ALL of: exit code 0; exactly one protocol line; every required field present; and the
    /// success invariants (<see cref="Validate"/>) holding. Anything else is a failure that says which. The child also lives in a kill-on-close job object, so it cannot outlive the parent
    /// however the parent ends (a crash included), and every path after the start either waits for the child or kills it and says whether the kill was confirmed.
    /// </summary>
    internal static class XpePreprocessOracleProcess
    {
        public const string ModeArgument = "--run-preprocess-oracle";

        /// <summary>The marker that tells the protocol line from anything else on standard output (the native DLLs log there too).</summary>
        public const string ResultPrefix = "XPE-ORACLE-RESULT: ";

        public static readonly TimeSpan DefaultTimeout = TimeSpan.FromSeconds(60);

        private static readonly Regex Sha256Hex = new("^[0-9a-f]{64}$", RegexOptions.Compiled);

        private static readonly JsonSerializerOptions Json = new()
        {
            NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
        };

        /// <summary>The app's entry: the oracle in a child process running this executable.</summary>
        public static PreprocessSyntheticOracleResult Run(string dllPath) =>
            Run(Environment.ProcessPath, [ModeArgument, dllPath], DefaultTimeout);

        /// <summary>The general form, which a test points at any executable that prints (or fails to print) the result line. <paramref name="onStarted"/> is a test seam called once the child is in its job.</summary>
        public static PreprocessSyntheticOracleResult Run(string? exePath, IReadOnlyList<string> arguments, TimeSpan timeout, Action<Process, KillOnCloseJob>? onStarted = null)
        {
            if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath))
            {
                return PreprocessSyntheticOracleResult.NotRun($"Oracle process executable not found: {exePath ?? "(unknown)"}");
            }

            var info = new ProcessStartInfo(exePath)
            {
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
            };
            foreach (var argument in arguments)
            {
                info.ArgumentList.Add(argument);
            }

            KillOnCloseJob job;
            try
            {
                job = KillOnCloseJob.Create();
            }
            catch (Exception ex)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process containment failed", $"The kill-on-close job object could not be created ({ex.Message}); the child was not started.");
            }

            using (job)
            {
                Process process;
                try
                {
                    process = Process.Start(info) ?? throw new InvalidOperationException("Process.Start returned null.");
                }
                catch (Exception ex)
                {
                    return PreprocessSyntheticOracleResult.Failed("Oracle process did not start", ex.Message);
                }

                using (process)
                {
                    try
                    {
                        job.Assign(process);
                    }
                    catch (Exception ex)
                    {
                        var killed = Kill(process);
                        return PreprocessSyntheticOracleResult.Failed(
                            "Oracle process containment failed",
                            $"The child could not be put in the kill-on-close job ({ex.Message}); it {(killed ? "was killed" : "could not be confirmed killed")}.");
                    }

                    try
                    {
                        onStarted?.Invoke(process, job);
                        var stdout = process.StandardOutput.ReadToEndAsync();
                        var stderr = process.StandardError.ReadToEndAsync();
                        if (!process.WaitForExit(timeout))
                        {
                            var killed = Kill(process);
                            return PreprocessSyntheticOracleResult.Failed(
                                "Oracle process timed out",
                                $"No result within {timeout.TotalSeconds:0.#} s; the child process {(killed ? "was killed" : "could not be confirmed killed")}.");
                        }

                        process.WaitForExit();   // flush the asynchronous reads
                        Task.WaitAll([stdout, stderr], TimeSpan.FromSeconds(5));
                        return ParseOutput(
                            stdout.IsCompletedSuccessfully ? stdout.Result : string.Empty,
                            process.ExitCode,
                            stderr.IsCompletedSuccessfully ? stderr.Result : string.Empty);
                    }
                    catch (Exception ex)
                    {
                        var killed = Kill(process);
                        return PreprocessSyntheticOracleResult.Failed(
                            "Oracle process host failed",
                            $"{ex.GetType().Name}: {ex.Message}; the child {(killed ? "was killed" : "could not be confirmed killed")}.");
                    }
                    finally
                    {
                        if (!process.HasExited)
                        {
                            Kill(process);
                        }
                    }
                }
            }
        }

        /// <summary>
        /// The pure half of the parent: what a finished child's exit code and standard output mean. A pass needs exit code 0, exactly one protocol line, every required field, and the
        /// success invariants; everything else is a failure naming the first thing that was wrong.
        /// </summary>
        public static PreprocessSyntheticOracleResult ParseOutput(string stdout, int exitCode, string stderr)
        {
            var error = stderr.Trim();
            var tail = error.Length > 0 ? $" stderr: {error}" : string.Empty;
            if (exitCode != 0)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process failed", $"Exit code {exitCode}; a result line is not trusted from a child that did not exit cleanly.{tail}");
            }

            var lines = stdout.Split('\n').Select(l => l.TrimEnd('\r')).Where(l => l.StartsWith(ResultPrefix, StringComparison.Ordinal)).ToList();
            if (lines.Count == 0)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process gave no result", $"Exit code 0; no result line on standard output.{tail}");
            }

            if (lines.Count > 1)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process gave an ambiguous result", $"{lines.Count} result lines on standard output; exactly one is required.{tail}");
            }

            var json = lines[0][ResultPrefix.Length..];
            var missing = MissingFields(json);
            if (missing is null)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process result invalid", "The result line is not a JSON object.");
            }

            if (missing.Count > 0)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process result invalid", $"Required field(s) missing: {string.Join(", ", missing)}.");
            }

            var parsed = TryParse(json);
            if (parsed is null)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process result invalid", "The result line could not be read as a result.");
            }

            var problems = Validate(parsed);
            return problems.Count == 0
                ? parsed
                : PreprocessSyntheticOracleResult.Failed("Oracle process result invalid", $"The result does not hold together: {string.Join("; ", problems)}.");
        }

        /// <summary>The names of the required result fields (the record's constructor parameters without a default) that the JSON lacks; null when the text is not a JSON object.</summary>
        private static List<string>? MissingFields(string json)
        {
            try
            {
                using var document = JsonDocument.Parse(json);
                if (document.RootElement.ValueKind != JsonValueKind.Object)
                {
                    return null;
                }

                return typeof(PreprocessSyntheticOracleResult).GetConstructors()
                    .OrderByDescending(c => c.GetParameters().Length)
                    .First()
                    .GetParameters()
                    .Where(p => !p.HasDefaultValue)
                    .Select(p => p.Name!)
                    .Where(name => !document.RootElement.TryGetProperty(name, out var value) || value.ValueKind == JsonValueKind.Null)
                    .ToList();
            }
            catch (JsonException)
            {
                return null;
            }
        }

        /// <summary>
        /// What a result that says <c>Passed</c> must also be: executed, three stages in order each with code OK and Passed, input preserved with equal hashes, an output hash, nothing non-finite,
        /// the two runs identical, and an output with a range. A result that says it did not pass needs only a status. Empty = consistent.
        /// </summary>
        internal static List<string> Validate(PreprocessSyntheticOracleResult r)
        {
            var problems = new List<string>();
            if (string.IsNullOrWhiteSpace(r.Status)) problems.Add("Status is empty");
            if (r.Stages is null) { problems.Add("Stages is missing"); return problems; }
            if (!r.Passed) return problems;

            if (!r.Executed) problems.Add("Passed but not Executed");
            if (r.Status != "Synthetic oracle pass") problems.Add($"Passed but Status is '{r.Status}'");
            var names = r.Stages.Select(s => s.Stage).ToArray();
            if (!names.SequenceEqual(["offset", "gain", "defect"])) problems.Add($"Stages are [{string.Join(", ", names)}], expected [offset, gain, defect]");
            foreach (var s in r.Stages)
            {
                if (s.ErrorCode != "OK") problems.Add($"stage {s.Stage} has code {s.ErrorCode}");
                if (!s.Passed) problems.Add($"stage {s.Stage} did not pass");
                if (!double.IsFinite(s.MaxAbsError) || !double.IsFinite(s.LatencyMs)) problems.Add($"stage {s.Stage} has a non-finite measurement");
            }

            if (!r.InputPreserved) problems.Add("the input was not preserved");
            if (!Sha256Hex.IsMatch(r.RawSha256Before ?? string.Empty) || !Sha256Hex.IsMatch(r.RawSha256After ?? string.Empty)) problems.Add("an input hash is missing or malformed");
            else if (r.RawSha256Before != r.RawSha256After) problems.Add("the input hash changed");
            if (!Sha256Hex.IsMatch(r.OutputSha256 ?? string.Empty)) problems.Add("the output hash is missing or malformed");
            if (r.NaNInfCount != 0) problems.Add($"{r.NaNInfCount} non-finite output value(s)");
            if (!double.IsFinite(r.DeterminismRmse) || r.DeterminismRmse != 0) problems.Add($"the two runs differ (RMSE {r.DeterminismRmse})");
            if (!double.IsFinite(r.OutputMin) || !double.IsFinite(r.OutputMax) || !(r.OutputMax > r.OutputMin)) problems.Add($"the output has no finite range ({r.OutputMin}..{r.OutputMax})");
            if (!double.IsFinite(r.TotalLatencyMs) || r.TotalLatencyMs < 0) problems.Add("the latency is not a finite non-negative number");
            return problems;
        }

        /// <summary>The child's side: run the oracle and write the one protocol line. Any exception becomes a failed result line, so the parent never has to guess.</summary>
        public static void RunWorker(string dllPath, TextWriter output)
        {
            PreprocessSyntheticOracleResult result;
            try
            {
                result = XpePreprocessSyntheticOracle.Run(dllPath);
            }
            catch (Exception ex)
            {
                result = PreprocessSyntheticOracleResult.Failed("Synthetic oracle exception", ex.Message);
            }

            output.WriteLine(FormatResultLine(result));
            output.Flush();
        }

        public static string FormatResultLine(PreprocessSyntheticOracleResult result) => ResultPrefix + Serialize(result);

        public static string Serialize(PreprocessSyntheticOracleResult result) => JsonSerializer.Serialize(result, Json);

        public static PreprocessSyntheticOracleResult? TryParse(string json)
        {
            try
            {
                return JsonSerializer.Deserialize<PreprocessSyntheticOracleResult>(json, Json);
            }
            catch (JsonException)
            {
                return null;
            }
        }

        private static bool Kill(Process process)
        {
            try
            {
                process.Kill(entireProcessTree: true);
                return process.WaitForExit(5000);
            }
            catch (Exception ex) when (ex is InvalidOperationException or System.ComponentModel.Win32Exception)
            {
                return process.HasExited;
            }
        }
    }

    /// <summary>
    /// A Windows job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE: when the last handle to it closes -- the parent disposing it, or the parent process ending in any way -- the system
    /// terminates every process in it. A child assigned to one cannot outlive its parent. (A child is assigned just after it starts, so for that instant it is not yet covered; the oracle child
    /// starts no processes of its own in that time.)
    /// </summary>
    internal sealed class KillOnCloseJob : IDisposable
    {
        private const int JobObjectExtendedLimitInformation = 9;
        private const uint JobObjectLimitKillOnJobClose = 0x2000;

        private IntPtr _handle;

        private KillOnCloseJob(IntPtr handle) => _handle = handle;

        public static KillOnCloseJob Create()
        {
            var handle = CreateJobObjectW(IntPtr.Zero, null);
            if (handle == IntPtr.Zero)
            {
                throw new InvalidOperationException($"CreateJobObject failed ({Marshal.GetLastWin32Error()}).");
            }

            var info = new ExtendedLimitInformation { BasicLimitInformation = { LimitFlags = JobObjectLimitKillOnJobClose } };
            var size = Marshal.SizeOf<ExtendedLimitInformation>();
            var buffer = Marshal.AllocHGlobal(size);
            try
            {
                Marshal.StructureToPtr(info, buffer, fDeleteOld: false);
                if (!SetInformationJobObject(handle, JobObjectExtendedLimitInformation, buffer, (uint)size))
                {
                    var error = Marshal.GetLastWin32Error();
                    CloseHandle(handle);
                    throw new InvalidOperationException($"SetInformationJobObject failed ({error}).");
                }
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }

            return new KillOnCloseJob(handle);
        }

        public void Assign(Process process)
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(KillOnCloseJob));
            }

            if (!AssignProcessToJobObject(_handle, process.Handle))
            {
                throw new InvalidOperationException($"AssignProcessToJobObject failed ({Marshal.GetLastWin32Error()}).");
            }
        }

        public void Dispose()
        {
            var handle = _handle;
            _handle = IntPtr.Zero;
            if (handle != IntPtr.Zero)
            {
                CloseHandle(handle);
            }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct BasicLimitInformationStruct
        {
            public long PerProcessUserTimeLimit;
            public long PerJobUserTimeLimit;
            public uint LimitFlags;
            public UIntPtr MinimumWorkingSetSize;
            public UIntPtr MaximumWorkingSetSize;
            public uint ActiveProcessLimit;
            public UIntPtr Affinity;
            public uint PriorityClass;
            public uint SchedulingClass;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct IoCountersStruct
        {
            public ulong ReadOperationCount;
            public ulong WriteOperationCount;
            public ulong OtherOperationCount;
            public ulong ReadTransferCount;
            public ulong WriteTransferCount;
            public ulong OtherTransferCount;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ExtendedLimitInformation
        {
            public BasicLimitInformationStruct BasicLimitInformation;
            public IoCountersStruct IoInfo;
            public UIntPtr ProcessMemoryLimit;
            public UIntPtr JobMemoryLimit;
            public UIntPtr PeakProcessMemoryUsed;
            public UIntPtr PeakJobMemoryUsed;
        }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateJobObjectW(IntPtr attributes, string? name);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SetInformationJobObject(IntPtr job, int informationClass, IntPtr information, uint length);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool AssignProcessToJobObject(IntPtr job, IntPtr process);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
    }
}
