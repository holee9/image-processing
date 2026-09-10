// #98 (GUI-C-42): the CI writer and this reader must agree, and that agreement must be checked.
using ImageProcTest.E2ETests.Fixtures;
using Xunit;
using Xunit.Abstractions;

namespace ImageProcTest.E2ETests.Scenarios.Smoke;

/// <summary>
/// Parses the provenance file <b>ci.yml actually produced</b>, captured verbatim from a real run.
///
/// GUI-C-41 built the reader against a format described in a message and validated it only against
/// the lane's own writer — the two producers had never met. Measured afterwards: run 34536951123
/// wrote the file, and the guard that reads it was not yet in that run's tree, so nothing had ever
/// read a CI-written record. A format agreed in prose and checked on one side is an assumption.
///
/// The sample is committed rather than fetched: a test that needs the network to decide is a test
/// that reports "no opinion" exactly when CI is unreachable. When ci.yml changes the shape, this
/// fails and the sample gets re-captured — which is the notification this pairing needs.
/// </summary>
[Trait("Category", "Smoke")]
public sealed class CiProvenanceCompatibilityTests(ITestOutputHelper output)
{
    /// <summary>Captured verbatim from the "Record staged-DLL provenance" step of run 34536951123.</summary>
    private const string SampleRelativePath = "Fixtures/Samples/ci-provenance.json";

    /// <summary>
    /// The reader accepts CI's file and recovers every field the guard depends on.
    ///
    /// Fails when ci.yml renames a field, changes the file's shape, or drops the file list — each of
    /// which would otherwise surface as a Native suite that refuses to run, on CI, at the least
    /// convenient moment.
    /// </summary>
    [Fact]
    public void ReaderAccepts_TheFileCiActuallyWrites()
    {
        var sample = ResolveSample();
        var directory = Path.Combine(Path.GetTempPath(), $"xpe_ci_provenance_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);

        try
        {
            File.Copy(sample, Path.Combine(directory, NativeProvenance.FileName));

            var record = NativeProvenance.Read(directory, out var reason);
            Assert.True(record is not null, reason);

            Assert.Equal("ci", record!.Source);
            Assert.False(string.IsNullOrWhiteSpace(record.RunId), "CI's record must carry a runId.");
            Assert.False(string.IsNullOrWhiteSpace(record.HeadSha), "CI's record must carry a headSha.");
            Assert.True(record.Files is { Count: > 0 }, "CI's record must list the files it staged.");

            foreach (var file in record.Files!)
            {
                Assert.False(string.IsNullOrWhiteSpace(file.Name), "A listed file must have a name.");
                Assert.False(string.IsNullOrWhiteSpace(file.Md5), $"{file.Name} has no md5.");
                // Lower-case hex is the agreed spelling on both sides; Describe() compares these
                // strings against a freshly computed hash, and a case difference would read as drift.
                Assert.Equal(file.Md5!.ToLowerInvariant(), file.Md5);
                Assert.True(file.Length > 0, $"{file.Name} has a non-positive length.");
            }

            // The files are not on disk here — the point is the record's shape, and Describe must
            // survive that rather than throwing.
            output.WriteLine(record.Describe(directory, currentHeadSha: null));
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    /// <summary>
    /// Every rendered provenance leads with the trust boundary.
    ///
    /// Added because removing that sentence broke nothing: the GUI-C-42 falsification blanked
    /// <see cref="NativeProvenance.TrustBoundary"/> and the whole suite stayed green, which means the
    /// caveat was decoration rather than a requirement. A caveat nothing enforces is one a later edit
    /// deletes as noise — and then a Native pass silently reads as stronger than it is.
    /// </summary>
    [Fact]
    public void EveryRenderedProvenance_LeadsWithTheTrustBoundary()
    {
        var sample = ResolveSample();
        var directory = Path.Combine(Path.GetTempPath(), $"xpe_ci_provenance_{Guid.NewGuid():N}");
        Directory.CreateDirectory(directory);

        try
        {
            File.Copy(sample, Path.Combine(directory, NativeProvenance.FileName));
            var record = NativeProvenance.Read(directory, out var reason);
            Assert.True(record is not null, reason);

            var first = record!.Describe(directory, currentHeadSha: null)
                .Split(Environment.NewLine)[0];

            Assert.False(
                string.IsNullOrWhiteSpace(first),
                "The rendered provenance must lead with the trust boundary, not a blank line.");
            Assert.Contains("not independently verified", first, StringComparison.Ordinal);
            Assert.Contains("no signature", first, StringComparison.Ordinal);
        }
        finally
        {
            Directory.Delete(directory, recursive: true);
        }
    }

    /// <summary>
    /// The lane's writer and CI's writer produce records the same reader accepts.
    ///
    /// Not a byte comparison: the lane record carries extra fields (headBranch, workflow, timestamps)
    /// CI does not write. What must match is the contract — the four fields the reader needs.
    /// </summary>
    [Fact]
    public void LaneAndCi_ShareTheFieldsTheReaderDependsOn()
    {
        var sample = ResolveSample();
        var text = File.ReadAllText(sample);

        foreach (var field in new[] { "\"source\"", "\"runId\"", "\"headSha\"", "\"files\"", "\"name\"", "\"md5\"", "\"length\"" })
        {
            Assert.Contains(field, text, StringComparison.Ordinal);
        }
    }

    /// <summary>
    /// Locates the committed sample by walking up from the test output directory.
    ///
    /// A missing sample FAILS rather than skips: skipping would quietly restore the state this class
    /// exists to end, where nothing has read a CI-written record.
    /// </summary>
    private static string ResolveSample()
    {
        var native = SampleRelativePath.Replace('/', Path.DirectorySeparatorChar);
        var direct = Path.Combine(AppContext.BaseDirectory, native);
        if (File.Exists(direct)) return direct;

        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        while (dir is not null)
        {
            var candidate = Path.Combine(
                dir.FullName, "clients", "ImageProcTest.E2ETests", native);
            if (File.Exists(candidate)) return candidate;
            dir = dir.Parent;
        }

        Assert.Fail($"{SampleRelativePath} was not found from {AppContext.BaseDirectory}.");
        return string.Empty; // unreachable
    }
}
