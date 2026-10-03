using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Win32.SafeHandles;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-212b (#249, Codex #96): runs <see cref="XpePreprocessSyntheticOracle"/> in a child process so that its shutdown and synthetic calibration never touch the module state of THIS
    /// process (see the oracle's header). The child is this same executable started with <see cref="ModeArgument"/> -- no second binary to ship -- and answers with ONE protocol line on
    /// standard output: <see cref="ResultPrefix"/> followed by the result as JSON, in UTF-8.
    ///
    /// GUI-C-212c (Codex #98): a pass needs exit code 0, exactly one protocol line, every required field, and the success invariants (<see cref="Validate"/>).
    ///
    /// GUI-C-212d (Codex #100): (1) the invariants include each stage's EFFECT (<c>MaxAbsError &gt; 0</c>), and the stage records' own fields are required; (2) the child is created SUSPENDED,
    /// put in the kill-on-close job, and only then resumed, so there is no instant at which it runs outside the job; (3) the child's output is read as a stream under fixed limits
    /// (<see cref="OutputLimits"/>), and a child that exceeds one is killed and reported, never buffered without bound.
    /// </summary>
    internal static class XpePreprocessOracleProcess
    {
        public const string ModeArgument = "--run-preprocess-oracle";

        /// <summary>Test hook: a file the worker waits for (at most 120 s) before running the oracle, so a test holds the "checking" state open for exactly as long as it needs (see <see cref="RunWorker"/>).</summary>
        public const string TestGateVariable = "XPE_ORACLE_TEST_GATE";

        /// <summary>Test hook: a file to which each worker appends one line when it starts (see <see cref="RunWorker"/>).</summary>
        public const string TestLogVariable = "XPE_ORACLE_TEST_LOG";

        /// <summary>The marker that tells the protocol line from anything else on standard output (the native DLLs log there too).</summary>
        public const string ResultPrefix = "XPE-ORACLE-RESULT: ";

        public static readonly TimeSpan DefaultTimeout = TimeSpan.FromSeconds(60);

        /// <summary>The protocol is UTF-8 on both ends, whatever the console code page is.</summary>
        public static readonly UTF8Encoding ProtocolEncoding = new(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true);

        private static readonly Regex Sha256Hex = new("^[0-9a-f]{64}$", RegexOptions.Compiled);

        private static readonly JsonSerializerOptions Json = new()
        {
            NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
        };

        /// <summary>The app's entry: the oracle in a child process running this executable.</summary>
        public static PreprocessSyntheticOracleResult Run(string dllPath) =>
            Run(Environment.ProcessPath, [ModeArgument, dllPath], DefaultTimeout);

        /// <summary>
        /// The general form, which a test points at any executable that prints (or fails to print) the result line. <paramref name="seams"/> are test hooks at the boundaries of the child's start.
        /// </summary>
        public static PreprocessSyntheticOracleResult Run(string? exePath, IReadOnlyList<string> arguments, TimeSpan timeout, HostSeams? seams = null, OutputLimits? limits = null)
        {
            // GUI-C-219c: starting the child and reading its answer takes hundreds of milliseconds; the UI thread must never be the caller (the window installs the guard)
            OracleThreadGuard.AssertNotUiThread(nameof(XpePreprocessOracleProcess) + "." + nameof(Run));
            limits ??= OutputLimits.Default;
            if (string.IsNullOrEmpty(exePath) || !File.Exists(exePath))
            {
                return PreprocessSyntheticOracleResult.NotRun($"Oracle process executable not found: {exePath ?? "(unknown)"}");
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
                SuspendedChild child;
                try
                {
                    child = SuspendedChild.Start(exePath, arguments, job, seams);
                }
                catch (Exception ex)
                {
                    return PreprocessSyntheticOracleResult.Failed("Oracle process did not start", $"{ex.GetType().Name}: {ex.Message}");
                }

                using (child)
                {
                    try
                    {
                        var stdout = Task.Run(() => ReadStdout(child.StdOut, limits, () => child.Kill()));
                        var stderr = Task.Run(() => ReadStderr(child.StdErr, limits, () => child.Kill()));
                        var exited = child.WaitForExit(timeout);
                        if (!exited)
                        {
                            var killed = child.Kill();
                            return PreprocessSyntheticOracleResult.Failed(
                                "Oracle process timed out",
                                $"No result within {timeout.TotalSeconds:0.#} s; the child process {(killed ? "was killed" : "could not be confirmed killed")}.");
                        }

                        Task.WaitAll([stdout, stderr], TimeSpan.FromSeconds(5));
                        var outcome = stdout.IsCompletedSuccessfully ? stdout.Result : new StdoutOutcome([], 0, 0, "the standard output reader did not finish");
                        var errText = stderr.IsCompletedSuccessfully ? stderr.Result : new StderrOutcome(string.Empty, 0, "the standard error reader did not finish");
                        if (outcome.Invalid)
                        {
                            return PreprocessSyntheticOracleResult.Failed("Oracle process result invalid", $"{outcome.Exceeded}; the child was killed.");
                        }

                        if (outcome.Exceeded is not null || errText.Exceeded is not null)
                        {
                            return PreprocessSyntheticOracleResult.Failed(
                                "Oracle process output too large",
                                $"{outcome.Exceeded ?? errText.Exceeded}; the child was killed (limits: result line {limits.MaxResultLineBytes} B, standard output {limits.MaxStdoutBytes} B, standard error {limits.MaxStderrBytes} B).");
                        }

                        return Interpret(outcome.ResultLines, outcome.ResultLineCount, child.ExitCode, errText.Text);
                    }
                    catch (Exception ex)
                    {
                        var killed = child.Kill();
                        return PreprocessSyntheticOracleResult.Failed(
                            "Oracle process host failed",
                            $"{ex.GetType().Name}: {ex.Message}; the child {(killed ? "was killed" : "could not be confirmed killed")}.");
                    }
                    finally
                    {
                        if (!child.HasExited)
                        {
                            child.Kill();
                        }
                    }
                }
            }
        }

        /// <summary>The pure half of the parent for text already in memory (tests, and the shape every caller reduces to): the protocol lines of <paramref name="stdout"/>, the exit code and stderr.</summary>
        public static PreprocessSyntheticOracleResult ParseOutput(string stdout, int exitCode, string stderr)
        {
            var lines = stdout.Split('\n').Select(l => l.TrimEnd('\r')).Where(l => l.StartsWith(ResultPrefix, StringComparison.Ordinal)).ToList();
            return Interpret(lines.Take(2).ToList(), lines.Count, exitCode, stderr);
        }

        /// <summary>
        /// What a finished child's exit code, protocol lines and stderr mean. A pass needs exit code 0, exactly one protocol line, every required field (the result's and each stage's), and
        /// the success invariants; everything else is a failure naming the first thing that was wrong.
        /// </summary>
        private static PreprocessSyntheticOracleResult Interpret(IReadOnlyList<string> resultLines, int resultLineCount, int exitCode, string stderr)
        {
            var error = stderr.Trim();
            var tail = error.Length > 0 ? $" stderr: {error}" : string.Empty;
            if (exitCode != 0)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process failed", $"Exit code {exitCode}; a result line is not trusted from a child that did not exit cleanly.{tail}");
            }

            if (resultLineCount == 0)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process gave no result", $"Exit code 0; no result line on standard output.{tail}");
            }

            if (resultLineCount > 1)
            {
                return PreprocessSyntheticOracleResult.Failed("Oracle process gave an ambiguous result", $"{resultLineCount} result lines on standard output; exactly one is required.{tail}");
            }

            var json = resultLines[0][ResultPrefix.Length..];
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

        /// <summary>
        /// The names of the required fields that the JSON lacks, the result's own (constructor parameters without a default) and, for each element of <c>Stages</c>, the stage record's
        /// (<c>Stages[1].MaxAbsError</c>); null when the text is not a JSON object. A missing or null field is missing.
        /// </summary>
        private static List<string>? MissingFields(string json)
        {
            try
            {
                if (JsonNode.Parse(json) is not JsonObject root)
                {
                    return null;
                }

                var missing = RequiredNames(typeof(PreprocessSyntheticOracleResult)).Where(name => root[name] is null).ToList();
                if (root["Stages"] is JsonArray stages)
                {
                    var stageNames = RequiredNames(typeof(PreprocessSyntheticStageResult)).ToList();
                    for (var i = 0; i < stages.Count; i++)
                    {
                        if (stages[i] is not JsonObject stage)
                        {
                            missing.Add($"Stages[{i}]");
                            continue;
                        }

                        missing.AddRange(stageNames.Where(name => stage[name] is null).Select(name => $"Stages[{i}].{name}"));
                    }
                }
                else if (root["Stages"] is not null)
                {
                    missing.Add("Stages(array)");
                }

                return missing;
            }
            catch (JsonException)
            {
                return null;
            }
        }

        private static IEnumerable<string> RequiredNames(Type record) =>
            record.GetConstructors()
                .OrderByDescending(c => c.GetParameters().Length)
                .First()
                .GetParameters()
                .Where(p => !p.HasDefaultValue)
                .Select(p => p.Name!);

        /// <summary>
        /// What a result that says <c>Passed</c> must also be: executed, three stages in order each with code OK, passed, a finite non-negative latency and a POSITIVE finite effect (a stage
        /// that left its input unchanged is the "correction with no effect" the oracle exists to refuse), input preserved with equal hashes, an output hash, nothing non-finite, the two
        /// runs identical, and an output with a range. A result that says it did not pass needs only a status. Empty = consistent.
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
                if (!double.IsFinite(s.MaxAbsError) || !double.IsFinite(s.LatencyMs) || s.LatencyMs < 0) problems.Add($"stage {s.Stage} has a non-finite measurement");
                else if (!(s.MaxAbsError > 0)) problems.Add($"stage {s.Stage} had no effect (MaxAbsError {s.MaxAbsError})");
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

        // ---- the child's side

        /// <summary>The writer the worker uses on standard output: UTF-8 without a byte-order mark, LF line ends, flushed per line -- independent of the console code page.</summary>
        public static TextWriter CreateWorkerWriter() =>
            new StreamWriter(Console.OpenStandardOutput(), new UTF8Encoding(encoderShouldEmitUTF8Identifier: false), 4096) { NewLine = "\n", AutoFlush = true };

        /// <summary>Run the oracle and write the one protocol line. Any exception becomes a failed result line, so the parent never has to guess.</summary>
        /// <param name="dllPath">The (snapshot) preprocess DLL to judge.</param>
        /// <param name="output">Where the protocol line is written.</param>
        /// <param name="confineLoad">
        /// GUI-C-219d: load the DLL with its dependencies searched in its own folder only and audit the loaded modules afterwards. True in the real worker, which is a process of its own with
        /// nothing loaded before. A caller that runs the worker's logic INSIDE a process that already holds modules of the same names (a test host whose fixture loaded xpe_common.dll
        /// from somewhere else) must pass false: the loader reuses a module that is already loaded by name, and the audit would then correctly report that module, whatever the oracle did.
        /// </param>
        public static void RunWorker(string dllPath, TextWriter output, bool confineLoad = true)
        {
            PreprocessSyntheticOracleResult result;
            try
            {
                // Diagnostic hooks for the E2E suite (GUI-C-219), both unset in normal use: a line appended to a log file each time a worker runs (so the number of oracle runs a startup
                // causes can be counted from outside), and a gate file the worker waits for before running the oracle (so the "checking" state lasts exactly as long as the test needs).
                if (Environment.GetEnvironmentVariable(TestLogVariable) is { Length: > 0 } logPath)
                {
                    try { File.AppendAllText(logPath, $"worker started pid={Environment.ProcessId}{Environment.NewLine}"); } catch (IOException) { }
                }

                if (Environment.GetEnvironmentVariable(TestGateVariable) is { Length: > 0 } gatePath)
                {
                    var giveUp = DateTime.UtcNow.AddSeconds(120);
                    while (!File.Exists(gatePath) && DateTime.UtcNow < giveUp)
                    {
                        System.Threading.Thread.Sleep(50);
                    }
                }

                // GUI-C-219d: the DLL is the snapshot copy; its dependencies are searched for in that folder and System32 only, and the loaded modules are audited against it afterwards.
                result = XpePreprocessSyntheticOracle.Run(dllPath, new XpePreprocessSyntheticOracle.OracleOptions(ConfinedLoadFolder: confineLoad ? Path.GetDirectoryName(Path.GetFullPath(dllPath)) : null));
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

        // ---- bounded reading of the child's output

        /// <summary>What was read of standard output: the first two protocol lines, how many there were, the bytes seen, and the limit that was exceeded (null = none).</summary>
        private sealed record StdoutOutcome(IReadOnlyList<string> ResultLines, int ResultLineCount, long TotalBytes, string? Exceeded, bool Invalid = false);

        private sealed record StderrOutcome(string Text, int Bytes, string? Exceeded);

        /// <summary>
        /// Standard output as a stream, never as one string: a protocol line is collected only while its first bytes match <see cref="ResultPrefix"/> and only up to
        /// <see cref="OutputLimits.MaxResultLineBytes"/>; every other line is counted and dropped as it passes. Memory held is one buffer plus at most one capped line plus the (at most two)
        /// kept protocol lines. A limit that is exceeded stops the read and kills the child.
        /// </summary>
        private static StdoutOutcome ReadStdout(Stream stream, OutputLimits limits, Action kill)
        {
            var prefix = Encoding.ASCII.GetBytes(ResultPrefix);
            var buffer = new byte[8192];
            var line = new MemoryStream();
            var collecting = false;     // the current line has matched the prefix so far
            var skipping = false;       // the current line is not a protocol line: dropped
            var kept = new List<string>();
            var count = 0;
            long total = 0;

            string? Finish()
            {
                if (collecting && !skipping)
                {
                    count++;
                    if (kept.Count < 2)
                    {
                        try { kept.Add(ProtocolEncoding.GetString(line.GetBuffer(), 0, (int)line.Length).TrimEnd('\r')); }
                        catch (DecoderFallbackException) { return "a result line is not valid UTF-8"; }
                    }
                }

                line.SetLength(0);
                collecting = false;
                skipping = false;
                return null;
            }

            string? Exceed(string what)
            {
                kill();
                return what;
            }

            while (true)
            {
                int read;
                try { read = stream.Read(buffer, 0, buffer.Length); }
                catch (Exception ex) when (ex is IOException or ObjectDisposedException) { break; }
                if (read <= 0) break;

                total += read;
                if (total > limits.MaxStdoutBytes) return new StdoutOutcome(kept, count, total, Exceed($"standard output exceeded {limits.MaxStdoutBytes} bytes"));

                for (var i = 0; i < read; i++)
                {
                    var b = buffer[i];
                    if (b == (byte)'\n')
                    {
                        if (Finish() is { } invalid) return new StdoutOutcome(kept, count, total, Exceed(invalid), Invalid: true);
                        continue;
                    }

                    if (skipping) continue;
                    var position = (int)line.Length;
                    if (position < prefix.Length)
                    {
                        if (b != prefix[position]) { skipping = true; line.SetLength(0); continue; }
                        line.WriteByte(b);
                        collecting = line.Length == prefix.Length || collecting;
                        continue;
                    }

                    if (line.Length >= limits.MaxResultLineBytes) return new StdoutOutcome(kept, count, total, Exceed($"a result line exceeded {limits.MaxResultLineBytes} bytes"));
                    line.WriteByte(b);
                }
            }

            if (line.Length > 0 && Finish() is { } invalidTail) return new StdoutOutcome(kept, count, total, Exceed(invalidTail), Invalid: true);
            return new StdoutOutcome(kept, count, total, null);
        }

        /// <summary>Standard error, kept as text up to <see cref="OutputLimits.MaxStderrBytes"/>; beyond that the child is killed.</summary>
        private static StderrOutcome ReadStderr(Stream stream, OutputLimits limits, Action kill)
        {
            var buffer = new byte[4096];
            var kept = new MemoryStream();
            while (true)
            {
                int read;
                try { read = stream.Read(buffer, 0, buffer.Length); }
                catch (Exception ex) when (ex is IOException or ObjectDisposedException) { break; }
                if (read <= 0) break;

                if (kept.Length + read > limits.MaxStderrBytes)
                {
                    kill();
                    return new StderrOutcome(string.Empty, (int)kept.Length, $"standard error exceeded {limits.MaxStderrBytes} bytes");
                }

                kept.Write(buffer, 0, read);
            }

            // stderr is diagnostic text: undecodable bytes become replacement characters rather than failing the run
            return new StderrOutcome(new UTF8Encoding(false, false).GetString(kept.GetBuffer(), 0, (int)kept.Length), (int)kept.Length, null);
        }
    }

    /// <summary>
    /// The limits on a child's output, with their reasons. A real oracle run (measured, GUI-C-212d) writes ONE result line of 908 bytes, 909 bytes in all on standard output and nothing on standard error, so
    /// every default is more than 70 times above normal and still small enough that the parent holds a few hundred KB at the very most: a result line of 64 KiB (72x), a total of 1 MiB on standard
    /// output (1150x), 256 KiB on standard error.
    /// </summary>
    internal sealed record OutputLimits(int MaxResultLineBytes, long MaxStdoutBytes, int MaxStderrBytes)
    {
        public static readonly OutputLimits Default = new(64 * 1024, 1024 * 1024, 256 * 1024);
    }

    /// <summary>Test hooks at the boundaries of the child's start; each is called with the child's process id.</summary>
    internal sealed record HostSeams(Action<int>? AfterCreate = null, Action<int, KillOnCloseJob>? AfterAssign = null, Action<int, KillOnCloseJob>? AfterResume = null);

    /// <summary>
    /// A child created SUSPENDED (<c>CREATE_SUSPENDED</c>) with its standard handles on anonymous pipes, assigned to the kill-on-close job, and only then resumed: there is no instant at which it
    /// runs outside the job. Only the three standard handles are inherited (<c>PROC_THREAD_ATTRIBUTE_HANDLE_LIST</c>), so no other handle of this process leaks into it. (<c>Process.Start</c> cannot
    /// create a process suspended.)
    /// </summary>
    internal sealed class SuspendedChild : IDisposable
    {
        private const uint CreateSuspended = 0x00000004;
        private const uint CreateNoWindow = 0x08000000;
        private const uint ExtendedStartupInfoPresent = 0x00080000;
        private const uint StartfUseStdHandles = 0x00000100;
        private const uint HandleFlagInherit = 0x00000001;
        private const uint WaitObject0 = 0;
        private static readonly IntPtr ProcThreadAttributeHandleList = new(0x00020002);

        private readonly KillOnCloseJob _job;
        private IntPtr _process;

        public int ProcessId { get; }

        public Stream StdOut { get; }

        public Stream StdErr { get; }

        private SuspendedChild(IntPtr process, int pid, Stream stdout, Stream stderr, KillOnCloseJob job)
        {
            _process = process;
            ProcessId = pid;
            StdOut = stdout;
            StdErr = stderr;
            _job = job;
        }

        public static SuspendedChild Start(string exePath, IReadOnlyList<string> arguments, KillOnCloseJob job, HostSeams? seams)
        {
            var security = new SecurityAttributes { Length = Marshal.SizeOf<SecurityAttributes>(), InheritHandle = true };
            IntPtr outRead = IntPtr.Zero, outWrite = IntPtr.Zero, errRead = IntPtr.Zero, errWrite = IntPtr.Zero, nul = IntPtr.Zero;
            var attributeList = IntPtr.Zero;
            var handleList = IntPtr.Zero;
            var process = IntPtr.Zero;
            var thread = IntPtr.Zero;
            var created = false;
            try
            {
                if (!CreatePipe(out outRead, out outWrite, ref security, 0) || !CreatePipe(out errRead, out errWrite, ref security, 0))
                {
                    throw new InvalidOperationException($"CreatePipe failed ({Marshal.GetLastWin32Error()}).");
                }

                // the parent's ends are not inherited
                if (!SetHandleInformation(outRead, HandleFlagInherit, 0) || !SetHandleInformation(errRead, HandleFlagInherit, 0))
                {
                    throw new InvalidOperationException($"SetHandleInformation failed ({Marshal.GetLastWin32Error()}).");
                }

                nul = CreateFileW("NUL", 0x80000000, 3, ref security, 3, 0, IntPtr.Zero);
                if (nul == new IntPtr(-1))
                {
                    nul = IntPtr.Zero;
                    throw new InvalidOperationException($"CreateFile NUL failed ({Marshal.GetLastWin32Error()}).");
                }

                var size = IntPtr.Zero;
                InitializeProcThreadAttributeList(IntPtr.Zero, 1, 0, ref size);
                attributeList = Marshal.AllocHGlobal(size);
                if (!InitializeProcThreadAttributeList(attributeList, 1, 0, ref size))
                {
                    var error = Marshal.GetLastWin32Error();
                    Marshal.FreeHGlobal(attributeList);
                    attributeList = IntPtr.Zero;
                    throw new InvalidOperationException($"InitializeProcThreadAttributeList failed ({error}).");
                }

                handleList = Marshal.AllocHGlobal(IntPtr.Size * 3);
                Marshal.WriteIntPtr(handleList, 0, nul);
                Marshal.WriteIntPtr(handleList, IntPtr.Size, outWrite);
                Marshal.WriteIntPtr(handleList, IntPtr.Size * 2, errWrite);
                if (!UpdateProcThreadAttribute(attributeList, 0, ProcThreadAttributeHandleList, handleList, new IntPtr(IntPtr.Size * 3), IntPtr.Zero, IntPtr.Zero))
                {
                    throw new InvalidOperationException($"UpdateProcThreadAttribute failed ({Marshal.GetLastWin32Error()}).");
                }

                var startup = new StartupInfoEx
                {
                    StartupInfo = new StartupInfo { Cb = Marshal.SizeOf<StartupInfoEx>(), Flags = StartfUseStdHandles, StdInput = nul, StdOutput = outWrite, StdError = errWrite },
                    AttributeList = attributeList,
                };
                var commandLine = new StringBuilder(Quote(exePath));
                foreach (var argument in arguments)
                {
                    commandLine.Append(' ').Append(Quote(argument));
                }

                if (!CreateProcessW(exePath, commandLine, IntPtr.Zero, IntPtr.Zero, true, CreateSuspended | CreateNoWindow | ExtendedStartupInfoPresent, IntPtr.Zero, null, ref startup, out var info))
                {
                    throw new InvalidOperationException($"CreateProcess failed ({Marshal.GetLastWin32Error()}).");
                }

                created = true;
                process = info.Process;
                thread = info.Thread;

                // the child holds its own copies; the parent's copies of the child's ends must close or the read never sees the end of the stream
                CloseHandle(outWrite); outWrite = IntPtr.Zero;
                CloseHandle(errWrite); errWrite = IntPtr.Zero;
                CloseHandle(nul); nul = IntPtr.Zero;

                var child = new SuspendedChild(
                    process,
                    info.ProcessId,
                    new FileStream(new SafeFileHandle(outRead, ownsHandle: true), FileAccess.Read, 4096, isAsync: false),
                    new FileStream(new SafeFileHandle(errRead, ownsHandle: true), FileAccess.Read, 4096, isAsync: false),
                    job);
                outRead = IntPtr.Zero;
                errRead = IntPtr.Zero;
                process = IntPtr.Zero;   // owned by the child object from here

                try
                {
                    seams?.AfterCreate?.Invoke(child.ProcessId);
                    if (!job.AssignHandle(child._process))
                    {
                        throw new InvalidOperationException($"AssignProcessToJobObject failed ({Marshal.GetLastWin32Error()}).");
                    }

                    seams?.AfterAssign?.Invoke(child.ProcessId, job);
                    if (ResumeThread(thread) == uint.MaxValue)
                    {
                        throw new InvalidOperationException($"ResumeThread failed ({Marshal.GetLastWin32Error()}).");
                    }

                    CloseHandle(thread); thread = IntPtr.Zero;
                    seams?.AfterResume?.Invoke(child.ProcessId, job);
                    return child;
                }
                catch
                {
                    child.Kill();
                    child.Dispose();
                    throw;
                }
            }
            catch
            {
                if (created && process != IntPtr.Zero)
                {
                    TerminateProcess(process, 1);
                }

                throw;
            }
            finally
            {
                if (thread != IntPtr.Zero) CloseHandle(thread);
                if (process != IntPtr.Zero) CloseHandle(process);
                foreach (var h in new[] { outRead, outWrite, errRead, errWrite, nul })
                {
                    if (h != IntPtr.Zero) CloseHandle(h);
                }

                if (attributeList != IntPtr.Zero)
                {
                    DeleteProcThreadAttributeList(attributeList);
                    Marshal.FreeHGlobal(attributeList);
                }

                if (handleList != IntPtr.Zero) Marshal.FreeHGlobal(handleList);
            }
        }

        public bool HasExited => _process == IntPtr.Zero || WaitForSingleObject(_process, 0) == WaitObject0;

        public bool WaitForExit(TimeSpan timeout) => _process == IntPtr.Zero || WaitForSingleObject(_process, (uint)Math.Min(timeout.TotalMilliseconds, uint.MaxValue - 1)) == WaitObject0;

        public int ExitCode => _process != IntPtr.Zero && GetExitCodeProcess(_process, out var code) ? unchecked((int)code) : -1;

        /// <summary>Ends the child and every process in its job, and reports whether the end was confirmed.</summary>
        public bool Kill()
        {
            if (_process == IntPtr.Zero) return true;
            _job.Terminate();
            TerminateProcess(_process, 1);
            return WaitForSingleObject(_process, 5000) == WaitObject0;
        }

        public void Dispose()
        {
            StdOut.Dispose();
            StdErr.Dispose();
            var process = _process;
            _process = IntPtr.Zero;
            if (process != IntPtr.Zero) CloseHandle(process);
        }

        /// <summary>One command-line argument quoted so that the child's argv parser reads back exactly the string (the rules of CommandLineToArgvW).</summary>
        internal static string Quote(string argument)
        {
            if (argument.Length > 0 && argument.IndexOfAny([' ', '\t', '\n', '\v', '"']) < 0) return argument;
            var quoted = new StringBuilder("\"");
            var backslashes = 0;
            foreach (var c in argument)
            {
                if (c == '\\') { backslashes++; continue; }
                if (c == '"') { quoted.Append('\\', backslashes * 2 + 1).Append('"'); }
                else { quoted.Append('\\', backslashes).Append(c); }
                backslashes = 0;
            }

            return quoted.Append('\\', backslashes * 2).Append('"').ToString();
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct SecurityAttributes
        {
            public int Length;
            public IntPtr SecurityDescriptor;
            [MarshalAs(UnmanagedType.Bool)] public bool InheritHandle;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct StartupInfo
        {
            public int Cb;
            public IntPtr Reserved;
            public IntPtr Desktop;
            public IntPtr Title;
            public int X, Y, XSize, YSize, XCountChars, YCountChars, FillAttribute;
            public uint Flags;
            public short ShowWindow, Reserved2Size;
            public IntPtr Reserved2;
            public IntPtr StdInput, StdOutput, StdError;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct StartupInfoEx
        {
            public StartupInfo StartupInfo;
            public IntPtr AttributeList;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct ProcessInformation
        {
            public IntPtr Process, Thread;
            public int ProcessId, ThreadId;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CreatePipe(out IntPtr read, out IntPtr write, ref SecurityAttributes attributes, uint size);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool SetHandleInformation(IntPtr handle, uint mask, uint flags);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateFileW(string name, uint access, uint share, ref SecurityAttributes attributes, uint disposition, uint flags, IntPtr template);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool InitializeProcThreadAttributeList(IntPtr list, int count, int flags, ref IntPtr size);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool UpdateProcThreadAttribute(IntPtr list, uint flags, IntPtr attribute, IntPtr value, IntPtr size, IntPtr previous, IntPtr returnSize);

        [DllImport("kernel32.dll")]
        private static extern void DeleteProcThreadAttributeList(IntPtr list);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CreateProcessW(string application, StringBuilder commandLine, IntPtr processAttributes, IntPtr threadAttributes, [MarshalAs(UnmanagedType.Bool)] bool inheritHandles, uint flags, IntPtr environment, string? directory, ref StartupInfoEx startup, out ProcessInformation info);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint ResumeThread(IntPtr thread);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool GetExitCodeProcess(IntPtr process, out uint code);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool TerminateProcess(IntPtr process, uint code);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
    }

    /// <summary>
    /// A Windows job object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE: when the last handle to it closes -- the parent disposing it, or the parent process ending in any way -- the system
    /// terminates every process in it. A child assigned to one cannot outlive its parent. The host creates its child suspended and assigns it before it runs
    /// (<see cref="SuspendedChild"/>), so no code of the child runs outside the job.
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

        /// <summary>For a process started by <c>Process.Start</c> (tests of the job on its own).</summary>
        public void Assign(System.Diagnostics.Process process)
        {
            if (!AssignHandle(process.Handle))
            {
                throw new InvalidOperationException($"AssignProcessToJobObject failed ({Marshal.GetLastWin32Error()}).");
            }
        }

        /// <summary>Puts the process behind <paramref name="process"/> in the job; false when the system refuses.</summary>
        public bool AssignHandle(IntPtr process)
        {
            if (_handle == IntPtr.Zero)
            {
                throw new ObjectDisposedException(nameof(KillOnCloseJob));
            }

            return AssignProcessToJobObject(_handle, process);
        }

        /// <summary>Ends every process in the job now.</summary>
        public void Terminate()
        {
            if (_handle != IntPtr.Zero)
            {
                TerminateJobObject(_handle, 1);
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
        private static extern bool TerminateJobObject(IntPtr job, uint exitCode);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        private static extern bool CloseHandle(IntPtr handle);
    }
}
