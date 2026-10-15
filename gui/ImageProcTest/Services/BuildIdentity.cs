using System.Reflection;

namespace ImageProcTest.Services;

/// <summary>
/// GUI-C-206: which build the running app is — the commit it was built from and the configuration — as the build stamped them into the assembly
/// (<c>ImageProcTest.csproj</c>, target <c>XpeStampGitRevision</c>: the informational version carries "+&lt;sha&gt;", assembly metadata carries
/// <c>BuildConfiguration</c>). "unknown" is what a build without git says; it is never an empty string and never a made-up value.
/// </summary>
public sealed record BuildIdentity(string Revision, string Configuration)
{
    public const string Unknown = "unknown";

    /// <summary>The identity of the assembly that is running (the entry assembly, the app itself).</summary>
    public static BuildIdentity Current { get; } = FromAssembly(Assembly.GetEntryAssembly() ?? Assembly.GetExecutingAssembly());

    public static BuildIdentity FromAssembly(Assembly assembly) =>
        From(
            assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion,
            assembly.GetCustomAttributes<AssemblyMetadataAttribute>().FirstOrDefault(a => a.Key == "BuildConfiguration")?.Value);

    /// <summary>Reads "1.0.0+abc1234567" as revision "abc1234567"; anything without a usable revision is <see cref="Unknown"/>.</summary>
    public static BuildIdentity From(string? informationalVersion, string? configuration)
    {
        var plus = informationalVersion?.IndexOf('+') ?? -1;
        var revision = plus >= 0 ? informationalVersion![(plus + 1)..].Trim() : string.Empty;
        if (revision.Length == 0) revision = Unknown;
        return new BuildIdentity(revision, string.IsNullOrWhiteSpace(configuration) ? Unknown : configuration.Trim());
    }

    /// <summary>The line the About dialog shows.</summary>
    public string Describe() => $"Build: {Revision} ({Configuration})";
}
