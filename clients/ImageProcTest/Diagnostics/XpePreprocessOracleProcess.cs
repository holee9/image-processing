using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;

namespace ImageProcTest
{
    /// <summary>
    /// GUI-C-212b (#249, Codex #96): runs <see cref="XpePreprocessSyntheticOracle"/> in a child process so that its shutdown and synthetic calibration never touch the module state of THIS
    /// process (see the oracle's header). The child is this same executable started with <see cref="ModeArgument"/> -- no second binary to ship -- and answers with ONE line of JSON on
    /// standard output. The parent waits at most <see cref="DefaultTimeout"/>, then kills the child and its descendants; a child that exits without a result line, or prints something
    /// that is not one, is reported as a failure that says so, never as a pass.
    /// </summary>
    internal static class XpePreprocessOracleProcess
    {
        public const string ModeArgument = "--run-preprocess-oracle";

        public static readonly TimeSpan DefaultTimeout = TimeSpan.FromSeconds(60);

        private static readonly JsonSerializerOptions Json = new()
        {
            NumberHandling = JsonNumberHandling.AllowNamedFloatingPointLiterals,
        };

        /// <summary>The app's entry: the oracle in a child process running this executable.</summary>
        public static PreprocessSyntheticOracleResult Run(string dllPath) =>
            Run(Environment.ProcessPath, [ModeArgument, dllPath], DefaultTimeout);

        /// <summary>The general form, which a test points at any executable that prints (or fails to print) the result line.</summary>
        public static PreprocessSyntheticOracleResult Run(string? exePath, IReadOnlyList<string> arguments, TimeSpan timeout)
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
                var line = (stdout.IsCompletedSuccessfully ? stdout.Result : string.Empty)
                    .Split('\n', StringSplitOptions.RemoveEmptyEntries)
                    .Select(l => l.Trim())
                    .LastOrDefault(l => l.StartsWith('{'));
                var parsed = line is null ? null : TryParse(line);
                if (parsed is not null)
                {
                    return parsed;
                }

                var error = stderr.IsCompletedSuccessfully ? stderr.Result.Trim() : string.Empty;
                return PreprocessSyntheticOracleResult.Failed(
                    "Oracle process gave no result",
                    $"Exit code {process.ExitCode}; no result line on standard output." + (error.Length > 0 ? $" stderr: {error}" : string.Empty));
            }
        }

        /// <summary>The child's side: run the oracle and write the one result line. Any exception becomes a failed result line, so the parent never has to guess.</summary>
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

            output.WriteLine(Serialize(result));
            output.Flush();
        }

        public static string Serialize(PreprocessSyntheticOracleResult result) => JsonSerializer.Serialize(result, Json);

        public static PreprocessSyntheticOracleResult? TryParse(string line)
        {
            try
            {
                return JsonSerializer.Deserialize<PreprocessSyntheticOracleResult>(line, Json);
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
}
