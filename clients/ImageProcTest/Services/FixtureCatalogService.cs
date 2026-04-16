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
            var calibration = LoadRawFiles(Path.Combine(casePath, "calibration"));

            return new FixtureCaseInfo(
                Path.GetFileName(casePath),
                casePath,
                images,
                calibration);
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
