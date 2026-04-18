namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// Provides deterministic test patterns used across multiple test classes.
/// All patterns are seed=0, reproducible without any external file I/O.
/// </summary>
public static class TestDataLoader
{
    /// <summary>
    /// Returns a ushort array of length <paramref name="count"/> filled with
    /// the canonical pattern: (ushort)(1000 + (index % 97)).
    /// </summary>
    public static ushort[] SyntheticUInt16(int count)
    {
        var data = new ushort[count];
        for (var i = 0; i < count; i++)
            data[i] = (ushort)(1000 + (i % 97));
        return data;
    }

    /// <summary>63-character ASCII body part string for boundary testing.</summary>
    public static string BodyPart63Chars { get; } = new string('A', 63);

    /// <summary>Valid JSON configuration accepted by xpe_configure.</summary>
    public static string ValidConfigJson { get; } = """{"log":{"level":2}}""";

    /// <summary>Malformed JSON that xpe_configure must reject with CONFIG_INVALID.</summary>
    public static string MalformedConfigJson { get; } = "{not json";

}
