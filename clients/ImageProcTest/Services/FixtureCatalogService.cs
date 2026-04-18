using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace ImageProcTest
{
    internal static class FixtureCatalogService
    {
        public static IReadOnlyList<FixtureCaseInfo> LoadCases()
        {
            var root = FindRepositoryRoot(AppContext.BaseDirectory);
            if (root is null)
            {
                return [];
            }

            var fixtureRoot = Path.Combine(root, "tests", "test_data", "calibration_cases");
            if (!Directory.Exists(fixtureRoot))
            {
                return [];
            }

            return Directory.EnumerateDirectories(fixtureRoot)
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
                .Select(CreateCase)
                .ToList();
        }

        private static FixtureCaseInfo CreateCase(string casePath)
        {
            var images = LoadRawFiles(Path.Combine(casePath, "images"));
            var calibration = LoadCalibrationFiles(Path.Combine(casePath, "calibration"));

            return new FixtureCaseInfo(
                Path.GetFileName(casePath),
                casePath,
                images,
                calibration);
        }

        private static IReadOnlyList<CalibrationFileDescriptor> LoadCalibrationFiles(string directory)
        {
            if (!Directory.Exists(directory))
            {
                return [];
            }

            return Directory.EnumerateFiles(directory, "*.raw", SearchOption.TopDirectoryOnly)
                .OrderBy(Path.GetFileName, StringComparer.OrdinalIgnoreCase)
                .Select(path =>
                {
                    var file = new RawFileDescriptor(path);
                    return new CalibrationFileDescriptor(file, InferCalibrationRole(file.Name));
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

        private static CalibrationRole InferCalibrationRole(string fileName)
        {
            var name = Path.GetFileNameWithoutExtension(fileName).ToLowerInvariant();

            if (name == "bpm" || name.EndsWith("_bpm", StringComparison.Ordinal) ||
                name.EndsWith("_bpmall", StringComparison.Ordinal) ||
                name.Contains("defect", StringComparison.Ordinal))
            {
                return CalibrationRole.Defect;
            }

            if (name.Contains("dark", StringComparison.Ordinal))
            {
                return CalibrationRole.Offset;
            }

            if (name.Contains("dif", StringComparison.Ordinal) ||
                name.Contains("reference", StringComparison.Ordinal))
            {
                return CalibrationRole.Reference;
            }

            if (name.StartsWith("calset", StringComparison.Ordinal) ||
                name.StartsWith("cbr", StringComparison.Ordinal) ||
                name.Contains("bright", StringComparison.Ordinal) ||
                name.Contains("flat", StringComparison.Ordinal) ||
                name.Contains("gain", StringComparison.Ordinal))
            {
                return CalibrationRole.Gain;
            }

            return CalibrationRole.Unknown;
        }

        private static string? FindRepositoryRoot(string startPath)
        {
            var directory = new DirectoryInfo(startPath);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, ".git")) ||
                    Directory.Exists(Path.Combine(directory.FullName, "tests", "test_data")))
                {
                    return directory.FullName;
                }

                directory = directory.Parent;
            }

            return null;
        }
    }
}
