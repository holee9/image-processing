using System.Collections.Generic;

namespace ImageProcTest
{
    internal sealed class FixtureCaseInfo
    {
        public FixtureCaseInfo(
            string name,
            string rootPath,
            IReadOnlyList<RawFileDescriptor> images,
            IReadOnlyList<RawFileDescriptor> calibrationFiles)
        {
            Name = name;
            RootPath = rootPath;
            Images = images;
            CalibrationFiles = calibrationFiles;
            DisplayName = $"{name} ({images.Count} images, {calibrationFiles.Count} calibration)";
        }

        public string Name { get; }

        public string RootPath { get; }

        public IReadOnlyList<RawFileDescriptor> Images { get; }

        public IReadOnlyList<RawFileDescriptor> CalibrationFiles { get; }

        public string DisplayName { get; }
    }
}
