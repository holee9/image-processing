using System.IO;

namespace ImageProcTest
{
    internal sealed class RawFileDescriptor
    {
        public RawFileDescriptor(string path)
        {
            Path = path;
            var file = new FileInfo(path);
            Name = file.Name;
            Length = file.Length;
            DisplayName = $"{file.Name} ({FormatBytes(file.Length)})";
        }

        public string Name { get; }

        public string Path { get; }

        public long Length { get; }

        public string DisplayName { get; }

        public static string FormatBytes(long bytes)
        {
            string[] units = ["B", "KB", "MB", "GB"];
            var value = (double)bytes;
            var unit = 0;

            while (value >= 1024 && unit < units.Length - 1)
            {
                value /= 1024;
                unit++;
            }

            return $"{value:0.##} {units[unit]}";
        }
    }
}
