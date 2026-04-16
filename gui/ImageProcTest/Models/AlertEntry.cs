namespace ImageProcTest.Models;

public sealed class AlertEntry
{
    public required string Severity { get; init; }

    public required string Code { get; init; }

    public required string Message { get; init; }

    public required DateTimeOffset Timestamp { get; init; }

    public override string ToString() =>
        $"[{Timestamp:HH:mm:ss.fff}] [{Severity}] {Code}: {Message}";
}
