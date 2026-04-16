using System.IO;
using System.Security.Cryptography;
using System.Text.Json;
using ImageProcTest.Models;

namespace ImageProcTest.Services;

public static class GuiFixtureManifestService
{
    public static string FindRepositoryRoot(string startDirectory)
    {
        var current = new DirectoryInfo(startDirectory);
        while (current is not null)
        {
            var marker = Path.Combine(current.FullName, "docs", "project", "sprint-plan.md");
            if (File.Exists(marker))
            {
                return current.FullName;
            }

            current = current.Parent;
        }

        throw new InvalidOperationException("Repository root could not be located.");
    }

    public static string GetRepositoryFixtureRoot(string repositoryRoot)
    {
        return Path.Combine(repositoryRoot, "gui", "ImageProcTest", "fixtures", "gui-s0");
    }

    public static string GetRuntimeFixtureRoot(string runtimeDirectory)
    {
        return Path.Combine(runtimeDirectory, "fixtures", "gui-s0");
    }

    public static GuiFixtureManifest LoadFromRepository(string repositoryRoot)
    {
        return Load(Path.Combine(GetRepositoryFixtureRoot(repositoryRoot), "fixture-manifest.json"));
    }

    public static GuiFixtureManifest LoadFromRuntime(string runtimeDirectory)
    {
        return Load(Path.Combine(GetRuntimeFixtureRoot(runtimeDirectory), "fixture-manifest.json"));
    }

    public static string ResolveFixturePath(string fixtureRoot, string relativePath)
    {
        var normalizedRelativePath = relativePath
            .Replace('/', Path.DirectorySeparatorChar)
            .Replace('\\', Path.DirectorySeparatorChar);
        return Path.GetFullPath(Path.Combine(fixtureRoot, normalizedRelativePath));
    }

    public static AppSettings CreateFixtureSettings(GuiFixtureManifest manifest, string fixtureRoot, string lastRawDirectory)
    {
        return new AppSettings
        {
            BackendMode = manifest.BackendMode,
            RawWidth = manifest.RawSample.Width,
            RawHeight = manifest.RawSample.Height,
            RawPixelFormat = manifest.RawSample.PixelFormat,
            OffsetCalibrationDirectory = ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Offset),
            GainCalibrationDirectory = ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Gain),
            DefectCalibrationDirectory = ResolveFixturePath(fixtureRoot, manifest.CalibrationDirectories.Defect),
            LastRawDirectory = lastRawDirectory
        };
    }

    public static string ComputeSha256(string filePath)
    {
        using var stream = File.OpenRead(filePath);
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static GuiFixtureManifest Load(string manifestPath)
    {
        if (!File.Exists(manifestPath))
        {
            throw new FileNotFoundException("Fixture manifest not found.", manifestPath);
        }

        var json = File.ReadAllText(manifestPath);
        return JsonSerializer.Deserialize<GuiFixtureManifest>(json) ??
               throw new InvalidOperationException($"Fixture manifest could not be deserialized: {manifestPath}");
    }
}
