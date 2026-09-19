// #198 / #194 (GUI-C-129): an app launched against a POLYNOMIAL gain calibration.
using System.Diagnostics;

namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Launches the app with a calibration set whose <c>gain.xcal</c> is a polynomial.
///
/// <para><b>Why a fixture of its own.</b> The shared set every other fixture uses is a SCALAR gain, and
/// the clamp alert (#194) exists only on the polynomial path. Generating a second set costs a
/// generator run — GUI-C-48 measured 53 s for 1024×1024 — so this fixture is used by the one scenario
/// that needs it rather than shared.</para>
///
/// <para><b>The swap.</b> <c>xpe_calib_fixture_gen --gain-poly</c> writes the polynomial as
/// <c>gain_poly.xcal</c> while the gui loads <c>gain.xcal</c> by name, so the file is copied over.
/// Nothing in the product is changed to make the measurement possible: the gui loads whatever
/// <c>gain.xcal</c> holds.</para>
///
/// <para>A missing or too-old generator leaves <see cref="ApplicationFixture.CalibrationDirectory"/>
/// null and the scenario skips with the reason — "not measured", never "measured and fine".</para>
/// </summary>
internal sealed class PolynomialCalibrationApplicationFixture : ApplicationFixture
{
    private const string RawImageRelativePath = @"fixtures\gui-s0\raw\synthetic_1024x1024.raw";

    public PolynomialCalibrationApplicationFixture()
        : base(RawImageRelativePath, BuildArguments(out var directory, out var note))
    {
        PolynomialCalibrationDirectory = directory;
        PolynomialNote = note;
    }

    /// <summary>Where the polynomial set was written, or null when it could not be produced.</summary>
    public string? PolynomialCalibrationDirectory { get; }

    /// <summary>What happened while producing it — the skip reason when it failed.</summary>
    public string PolynomialNote { get; }

    private static IReadOnlyList<string> BuildArguments(out string? directory, out string note)
    {
        directory = Generate(out note);
        return directory is null
            ? []
            : ["--automation-calib", directory];
    }

    private static string? Generate(out string note)
    {
        var generator = FindGenerator(out var searched);
        if (generator is null)
        {
            note = $"xpe_calib_fixture_gen.exe was not found. Looked in: {string.Join(" ; ", searched)}.";
            return null;
        }

        var directory = Path.Combine(Path.GetTempPath(), $"xpe-c129-e2e-{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);

        var startInfo = new ProcessStartInfo(generator)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };

        foreach (var argument in new[]
                 {
                     "--out", directory, "--width", "1024", "--height", "1024", "--seed", "0", "--gain-poly",
                 })
        {
            startInfo.ArgumentList.Add(argument);
        }

        using var process = Process.Start(startInfo);
        if (process is null)
        {
            note = $"Could not start {generator}.";
            return null;
        }

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit(180_000);

        var poly = Path.Combine(directory, "gain_poly.xcal");
        if (process.ExitCode != 0 || !File.Exists(poly))
        {
            note = $"The generator did not write gain_poly.xcal (exit {process.ExitCode}). " +
                   $"A staged copy predating QA-A-136 does not accept --gain-poly. stderr: {stderr.Trim()}";
            return null;
        }

        // The gui loads gain.xcal; this run's gain.xcal is the polynomial.
        File.Copy(poly, Path.Combine(directory, "gain.xcal"), overwrite: true);

        note = $"polynomial set in {directory}. {stdout.Trim()}";
        return directory;
    }

    private static string? FindGenerator(out string[] searched)
    {
        var candidates = new List<string>();
        var nativeDir = Environment.GetEnvironmentVariable("XPE_NATIVE_DIR");
        if (!string.IsNullOrWhiteSpace(nativeDir))
        {
            candidates.Add(Path.Combine(nativeDir, "xpe_calib_fixture_gen.exe"));
        }

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            candidates.Add(Path.Combine(dir.FullName, "build", "ci-common", "bin", "xpe_calib_fixture_gen.exe"));
            candidates.Add(Path.Combine(dir.FullName, "build", "e2e-native-dlls", "xpe_calib_fixture_gen.exe"));
            dir = dir.Parent;
        }

        searched = candidates.Distinct().ToArray();
        return searched.FirstOrDefault(File.Exists);
    }
}
