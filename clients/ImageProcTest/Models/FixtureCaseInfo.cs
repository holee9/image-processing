using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    internal enum CalibrationRole
    {
        Offset,
        Gain,
        Defect,
        DefectOracle,
        Reference,
        Unknown
    }

    internal sealed class CalibrationFileDescriptor
    {
        public CalibrationFileDescriptor(RawFileDescriptor file, CalibrationRole role)
        {
            File = file;
            Role = role;
            DisplayName = $"{Role}: {file.DisplayName}";
        }

        public RawFileDescriptor File { get; }

        public CalibrationRole Role { get; }

        public string Name => File.Name;

        public string Path => File.Path;

        public long Length => File.Length;

        public string DisplayName { get; }
    }

    internal sealed class FixtureCaseInfo
    {
        public FixtureCaseInfo(
            string name,
            string rootPath,
            IReadOnlyList<RawFileDescriptor> images,
            IReadOnlyList<CalibrationFileDescriptor> calibrationFiles,
            string? calibrationDirectoryPath = null)
        {
            Name = name;
            RootPath = rootPath;
            Images = images;
            CalibrationFiles = calibrationFiles;
            CalibrationDirectoryPath = calibrationDirectoryPath ?? System.IO.Path.Combine(rootPath, "calibration");
            CalibrationSummary = string.Join(", ", calibrationFiles
                .GroupBy(file => file.Role)
                .OrderBy(group => group.Key)
                .Select(group => $"{group.Key}={group.Count()}"));
            DisplayName = string.IsNullOrWhiteSpace(CalibrationSummary)
                ? $"{name} ({images.Count} images, no calibration)"
                : $"{name} ({images.Count} images, {CalibrationSummary})";
        }

        public string Name { get; }

        public string RootPath { get; }

        public string CalibrationDirectoryPath { get; }

        public IReadOnlyList<RawFileDescriptor> Images { get; }

        public IReadOnlyList<CalibrationFileDescriptor> CalibrationFiles { get; }

        public string CalibrationSummary { get; }

        public string DisplayName { get; }
    }
}
