// #225 row 9 (GUI-C-196 M3): the baseline's DICOM write, its validation, and the read-back that proves what was stored. Free of WPF and of native code.
using System.Globalization;
using System.IO;
using System.Text.Json;

namespace ImageProcTest.Services;

/// <summary>
/// The acquisition metadata the gui actually knows (design D6): body part, tube voltage and pixel pitch. Exposure (mAs), source-to-image distance and the
/// acquisition time are not known to the gui and are written as 0 = "unknown" (the module's own "unspecified"), never as an invented value.
/// </summary>
internal sealed record BaselineDicomMetadata(string BodyPart, float KVp, float PixelPitchMm);

/// <summary>What the module returned when the file was opened and read back: each step's code, then the content.</summary>
internal sealed record DicomReadBack(
    int OpenCode,
    int ReadCode,
    int MetadataCode,
    int Width,
    int Height,
    ushort[]? Pixels,
    BaselineDicomMetadata? Metadata,
    string? Problem);

/// <summary>The module's DICOM session, behind an interface so the verdict logic is tested without the module.</summary>
internal interface IDicomSession
{
    /// <summary>xpe_dicom_write of 16-bit pixels with the metadata. Returns the module's return code (0 = OK).</summary>
    int Write(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata);

    /// <summary>xpe_dicom_validate: the return code and the JSON report the module produced (empty when it produced none).</summary>
    (int Code, string Json) Validate(string path);

    /// <summary>xpe_dicom_open + read_image + get_metadata + close.</summary>
    DicomReadBack ReadBack(string path);
}

/// <summary>What one export did, step by step. <see cref="Passed"/> is the conjunction; the fields say which part failed.</summary>
internal sealed record BaselineDicomResult(
    string Path,
    bool Written,
    bool ReportProduced,
    bool Valid,
    string ReportJson,
    bool ReadBackSucceeded,
    bool SizeMatches,
    PixelDifference? Pixels,
    bool MetadataAgrees,
    string MetadataDetail,
    string Summary)
{
    public bool Passed => Written && ReportProduced && Valid && ReadBackSucceeded && SizeMatches && Pixels is { Identical: true } && MetadataAgrees;
}

/// <summary>
/// Writes the baseline output as a DICOM file, asks the module whether the file it just wrote is conformant, reads it back, and compares (design D6):
/// the pixels bit-for-bit and the three known metadata values. A success of the write call alone proves nothing about the file, and a valid report alone proves
/// nothing about the pixels, so all of them have to agree.
///
/// <para>The metadata comparison uses a relative tolerance of 1e-4: kVp and the pixel pitch are stored as DICOM decimal strings, so a float that is not
/// exactly representable in the decimal form can come back one unit in the last place away. The pixels have no tolerance.</para>
/// </summary>
internal static class BaselineDicomExport
{
    private const double MetadataRelativeTolerance = 1e-4;

    public static BaselineDicomResult Export(string path, ushort[] pixels, int width, int height, BaselineDicomMetadata metadata, IDicomSession session)
    {
        ArgumentNullException.ThrowIfNull(pixels);
        ArgumentNullException.ThrowIfNull(metadata);
        ArgumentNullException.ThrowIfNull(session);

        if (width <= 0 || height <= 0 || pixels.Length != checked(width * height))
        {
            return Failed(path, $"DICOM export refused: {pixels.Length} pixels for {width} x {height}.");
        }

        try
        {
            var directory = System.IO.Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(directory))
            {
                Directory.CreateDirectory(directory);
            }
        }
        catch (Exception ex)
        {
            return Failed(path, $"DICOM export: could not create the folder: {ex.Message}");
        }

        var writeCode = Guarded(() => session.Write(path, pixels, width, height, metadata), out var writeFault);
        if (writeFault is not null || writeCode != 0)
        {
            return Failed(path, writeFault is null ? $"xpe_dicom_write returned {writeCode}; no file was validated." : $"xpe_dicom_write threw: {writeFault}");
        }

        var (validateCode, json, validateFault) = ValidateGuarded(session, path);
        var reportProduced = json.Length > 0 && validateCode is 0;
        var valid = reportProduced && ReportSaysValid(json, out _);

        var readBack = ReadBackGuarded(session, path);
        var readBackOk = readBack is { OpenCode: 0, ReadCode: 0, MetadataCode: 0, Pixels: not null, Problem: null };
        var sizeMatches = readBackOk && readBack!.Width == width && readBack.Height == height;
        PixelDifference? difference = readBackOk && sizeMatches ? BaselineDeterminism.Compare(pixels, readBack!.Pixels!) : null;
        var (metadataAgrees, metadataDetail) = readBackOk ? CompareMetadata(metadata, readBack!.Metadata) : (false, "not read back");

        var parts = new List<string>
        {
            "write ok",
            reportProduced ? (valid ? "validator: valid" : "validator: NOT valid " + Truncate(json)) : $"validator produced no report (return code {validateCode}{(validateFault is null ? "" : "; threw " + validateFault)})",
            readBackOk
                ? $"read back {readBack!.Width}x{readBack.Height}: pixels {(difference is { Identical: true } ? "identical" : difference is null ? "size differs" : $"DIFFER at {difference.FirstIndex} ({difference.DifferentCount} px)")}"
                : $"read back failed (open {readBack.OpenCode}, read {readBack.ReadCode}, metadata {readBack.MetadataCode}{(readBack.Problem is null ? "" : "; " + readBack.Problem)})",
            "metadata " + (readBackOk ? metadataDetail : "not compared"),
        };

        return new BaselineDicomResult(path, true, reportProduced, valid, json, readBackOk, sizeMatches, difference, metadataAgrees, metadataDetail, string.Join("; ", parts));
    }

    /// <summary>True when the report's top-level <c>"valid"</c> is the JSON value true. A report that is not JSON, or has no such member, is not valid.</summary>
    internal static bool ReportSaysValid(string json, out string errors)
    {
        errors = string.Empty;
        try
        {
            using var document = JsonDocument.Parse(json);
            if (document.RootElement.ValueKind != JsonValueKind.Object || !document.RootElement.TryGetProperty("valid", out var valid))
            {
                return false;
            }

            if (document.RootElement.TryGetProperty("errors", out var list) && list.ValueKind == JsonValueKind.Array)
            {
                errors = list.GetRawText();
            }

            return valid.ValueKind == JsonValueKind.True;
        }
        catch (JsonException)
        {
            return false;
        }
    }

    private static (bool Agrees, string Detail) CompareMetadata(BaselineDicomMetadata written, BaselineDicomMetadata? read)
    {
        if (read is null)
        {
            return (false, "the module returned none");
        }

        var problems = new List<string>();
        if (!string.Equals(written.BodyPart, read.BodyPart, StringComparison.Ordinal))
        {
            problems.Add($"body part '{written.BodyPart}' -> '{read.BodyPart}'");
        }

        if (!Close(written.KVp, read.KVp))
        {
            problems.Add(string.Create(CultureInfo.InvariantCulture, $"kVp {written.KVp} -> {read.KVp}"));
        }

        if (!Close(written.PixelPitchMm, read.PixelPitchMm))
        {
            problems.Add(string.Create(CultureInfo.InvariantCulture, $"pixel pitch {written.PixelPitchMm} -> {read.PixelPitchMm}"));
        }

        return problems.Count == 0 ? (true, "agrees (body part, kVp, pixel pitch)") : (false, "DIFFERS: " + string.Join(", ", problems));
    }

    private static bool Close(float a, float b) => Math.Abs((double)a - b) <= MetadataRelativeTolerance * Math.Max(1.0, Math.Abs((double)a));

    private static int Guarded(Func<int> call, out string? fault)
    {
        try
        {
            fault = null;
            return call();
        }
        catch (Exception ex)
        {
            fault = $"{ex.GetType().Name}: {ex.Message}";
            return -1;
        }
    }

    private static (int Code, string Json, string? Fault) ValidateGuarded(IDicomSession session, string path)
    {
        try
        {
            var (code, json) = session.Validate(path);
            return (code, json ?? string.Empty, null);
        }
        catch (Exception ex)
        {
            return (-1, string.Empty, $"{ex.GetType().Name}: {ex.Message}");
        }
    }

    private static DicomReadBack ReadBackGuarded(IDicomSession session, string path)
    {
        try
        {
            return session.ReadBack(path);
        }
        catch (Exception ex)
        {
            return new DicomReadBack(-1, -1, -1, 0, 0, null, null, $"{ex.GetType().Name}: {ex.Message}");
        }
    }

    private static string Truncate(string text) => text.Length <= 300 ? text : text[..300] + "...";

    private static BaselineDicomResult Failed(string path, string summary) =>
        new(path, false, false, false, string.Empty, false, false, null, false, "not compared", summary);
}
