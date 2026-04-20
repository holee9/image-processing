using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace ImageProcTest
{
    internal static class FixtureCatalogService
    {
        private static readonly string[] CalibrationCaseRoots =
        [
            Path.Combine("tests", "test_data", "calibration_cases"),
            Path.Combine("tests", "test_data", "CalData_6"),
            Path.Combine("tests", "test_data", "Grid_abnormal"),
            Path.Combine("tests", "test_data", "cyan_test")
        ];

        private static readonly HashSet<string> CalibrationFileExtensions = new(
            [".raw", ".map"],
            StringComparer.OrdinalIgnoreCase);

        public static IReadOnlyList<FixtureCaseInfo> LoadCases()
        {
            var root = FindRepositoryRoot(AppContext.BaseDirectory);
            if (root is null)
            {
                return [];
            }

            return GetRepositoryAndSiblingRoots(root, "image-processing", "xpe-pre")
                .SelectMany(EnumerateCalibrationCases)
                .Select(CreateCase)
                .GroupBy(item => item.Name, StringComparer.OrdinalIgnoreCase)
                .Select(group => group
                    .OrderByDescending(item => item.Images.Count + item.CalibrationFiles.Count)
                    .ThenBy(item => item.RootPath, StringComparer.OrdinalIgnoreCase)
                    .First())
                .OrderBy(item => item.Name, StringComparer.OrdinalIgnoreCase)
                .ToList();
        }

        public static FixtureCaseInfo LoadCalibrationFolder(string selectedPath)
        {
            if (string.IsNullOrWhiteSpace(selectedPath))
            {
                throw new ArgumentException("Calibration folder path is required.", nameof(selectedPath));
            }

            if (!Directory.Exists(selectedPath))
            {
                throw new DirectoryNotFoundException($"Calibration folder was not found: {selectedPath}");
            }

            var calibrationPath = ResolveCalibrationDirectory(selectedPath);
            var rootPath = selectedPath;
            var caseName = Path.GetFileName(rootPath.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar));
            if (string.IsNullOrWhiteSpace(caseName))
            {
                caseName = "manual-calibration";
            }

            var imagesPath = Path.Combine(rootPath, "images");
            var manifestRoles = LoadManifestRoles(rootPath);
            return new FixtureCaseInfo(
                caseName,
                rootPath,
                LoadRawFiles(Directory.Exists(imagesPath) ? imagesPath : rootPath),
                LoadCalibrationFiles(calibrationPath, manifestRoles),
                calibrationPath);
        }

        private static FixtureCaseInfo CreateCase(string casePath)
        {
            var calibrationPath = Path.Combine(casePath, "calibration");
            var imagesPath = Path.Combine(casePath, "images");
            var isFlatStructure = !Directory.Exists(imagesPath);
            var manifestRoles = LoadManifestRoles(casePath);
            var images = LoadRawFiles(isFlatStructure ? casePath : imagesPath);
            var calibration = LoadCalibrationFiles(isFlatStructure ? casePath : calibrationPath, manifestRoles);

            return new FixtureCaseInfo(
                Path.GetFileName(casePath),
                casePath,
                images,
                calibration,
                calibrationPath);
        }

        private static IReadOnlyList<CalibrationFileDescriptor> LoadCalibrationFiles(
            string directory,
            IReadOnlyDictionary<string, CalibrationRole>? manifestRoles = null)
        {
            if (!Directory.Exists(directory))
            {
                return [];
            }

            return Directory.EnumerateFiles(directory, "*", SearchOption.TopDirectoryOnly)
                .Where(path => CalibrationFileExtensions.Contains(Path.GetExtension(path)))
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
                .Select(path =>
                {
                    var file = new RawFileDescriptor(path);
                    var role = manifestRoles is not null &&
                        manifestRoles.TryGetValue(file.Name, out var manifestRole)
                            ? manifestRole
                            : InferCalibrationRole(file.Name);
                    return new CalibrationFileDescriptor(file, role);
                })
                .ToList();
        }

        private static IReadOnlyList<RawFileDescriptor> LoadRawFiles(string directory)
        {
            if (!Directory.Exists(directory))
            {
                return [];
            }

            return Directory.EnumerateFiles(directory, "*.raw", SearchOption.TopDirectoryOnly)
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
                .Select(path => new RawFileDescriptor(path))
                .ToList();
        }

        private static string ResolveCalibrationDirectory(string selectedPath)
        {
            var childCalibrationPath = Path.Combine(selectedPath, "calibration");
            return Directory.Exists(childCalibrationPath)
                ? childCalibrationPath
                : selectedPath;
        }

        internal static CalibrationRole InferCalibrationRole(string fileName)
        {
            var name = Path.GetFileNameWithoutExtension(fileName).ToLowerInvariant();

            if (name == "defect_oracle" ||
                name == "bpmap" ||
                name.EndsWith("_oracle", StringComparison.Ordinal) ||
                name.EndsWith("_ground_truth", StringComparison.Ordinal))
            {
                return CalibrationRole.DefectOracle;
            }

            if (name == "bpm" || name.EndsWith("_bpm", StringComparison.Ordinal) ||
                name.EndsWith("_bpmall", StringComparison.Ordinal) ||
                name.Contains("defect", StringComparison.Ordinal))
            {
                return CalibrationRole.Defect;
            }

            if (name.StartsWith("masterdark", StringComparison.Ordinal) ||
                name.StartsWith("dark", StringComparison.Ordinal))
            {
                return CalibrationRole.Offset;
            }

            if (name.Contains("dark", StringComparison.Ordinal))
            {
                return CalibrationRole.Offset;
            }

            if (name.EndsWith("_pre", StringComparison.Ordinal) ||
                name.EndsWith("_nonpre", StringComparison.Ordinal) ||
                name.EndsWith("_result", StringComparison.Ordinal) ||
                name.Contains("dif", StringComparison.Ordinal) ||
                name.Contains("reference", StringComparison.Ordinal))
            {
                return CalibrationRole.Reference;
            }

            if (Regex.IsMatch(name, "^calset_\\d+$", RegexOptions.CultureInvariant) ||
                Regex.IsMatch(name, "^bright\\d{2}$", RegexOptions.CultureInvariant) ||
                name.StartsWith("masterbright", StringComparison.Ordinal) ||
                name.StartsWith("calset", StringComparison.Ordinal) ||
                name.StartsWith("cbr", StringComparison.Ordinal) ||
                name.Contains("bright", StringComparison.Ordinal) ||
                name.Contains("flat", StringComparison.Ordinal) ||
                name.Contains("gain", StringComparison.Ordinal))
            {
                return CalibrationRole.Gain;
            }

            return CalibrationRole.Unknown;
        }

        private static IEnumerable<string> EnumerateCalibrationCases(string repoRoot)
        {
            foreach (var relativeRoot in CalibrationCaseRoots)
            {
                var candidate = Path.Combine(repoRoot, relativeRoot);
                if (!Directory.Exists(candidate))
                {
                    continue;
                }

                if (Path.GetFileName(candidate).Equals("calibration_cases", StringComparison.OrdinalIgnoreCase))
                {
                    foreach (var caseDirectory in Directory.EnumerateDirectories(candidate))
                    {
                        yield return caseDirectory;
                    }
                }
                else
                {
                    yield return candidate;
                }
            }
        }

        private static IReadOnlyDictionary<string, CalibrationRole> LoadManifestRoles(string casePath)
        {
            var manifestPath = Path.Combine(casePath, "fixture.json");
            if (!File.Exists(manifestPath))
            {
                return new Dictionary<string, CalibrationRole>(StringComparer.OrdinalIgnoreCase);
            }

            try
            {
                using var stream = File.OpenRead(manifestPath);
                using var document = JsonDocument.Parse(stream);
                if (!document.RootElement.TryGetProperty("roles", out var roles) ||
                    roles.ValueKind != JsonValueKind.Object)
                {
                    return new Dictionary<string, CalibrationRole>(StringComparer.OrdinalIgnoreCase);
                }

                var mappedRoles = new Dictionary<string, CalibrationRole>(StringComparer.OrdinalIgnoreCase);
                foreach (var role in roles.EnumerateObject())
                {
                    mappedRoles[role.Name] = ParseCalibrationRole(role.Value.GetString());
                }

                return mappedRoles;
            }
            catch (JsonException)
            {
                return new Dictionary<string, CalibrationRole>(StringComparer.OrdinalIgnoreCase);
            }
        }

        private static CalibrationRole ParseCalibrationRole(string? value)
        {
            return value?.Trim().ToLowerInvariant() switch
            {
                "offset" or "dark" => CalibrationRole.Offset,
                "gain" or "flat" or "bright" => CalibrationRole.Gain,
                "defect" or "bpm" => CalibrationRole.Defect,
                "defect_oracle" or "defectoracle" or "oracle" => CalibrationRole.DefectOracle,
                "reference" => CalibrationRole.Reference,
                _ => CalibrationRole.Unknown
            };
        }

        private static string? FindRepositoryRoot(string startPath)
        {
            var directory = new DirectoryInfo(startPath);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, ".git")) ||
                    File.Exists(Path.Combine(directory.FullName, ".git")) ||
                    Directory.Exists(Path.Combine(directory.FullName, "tests", "test_data")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }

        private static IEnumerable<string> GetRepositoryAndSiblingRoots(
            string repoRoot,
            params string[] siblingNames)
        {
            var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            if (seen.Add(repoRoot))
            {
                yield return repoRoot;
            }

            var parent = Directory.GetParent(repoRoot);
            if (parent is null)
            {
                yield break;
            }

            foreach (var siblingName in siblingNames)
            {
                var siblingRoot = Path.Combine(parent.FullName, siblingName);
                if (Directory.Exists(siblingRoot) && seen.Add(siblingRoot))
                {
                    yield return siblingRoot;
                }
            }
        }
    }
}
