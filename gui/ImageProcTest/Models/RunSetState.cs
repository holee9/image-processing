using ImageProcTest.ViewModels;

namespace ImageProcTest.Models;

public sealed class RunSetState : ObservableObject
{
    private int _passed;
    private int _failed;
    private int _deferred;
    private int _total;
    private string _runId = string.Empty;
    private DateTimeOffset _startedAt;

    /// <summary>
    /// #178 (GUI-C-86): a run set starts when it is created — the app makes one per launch, and nothing
    /// else starts one (RunOnAllQueued is not implemented). The id is fixed here, once, so every piece of
    /// evidence of this run set lands in <c>evidence/&lt;RunId&gt;/</c> and its bundle in
    /// <c>bundles/&lt;RunId&gt;.zip</c>. Until GUI-C-86 nothing assigned it: the export zipped the whole
    /// <c>evidence/</c> folder into a file inside it and failed on every launch (measured GUI-C-85).
    /// </summary>
    public RunSetState()
    {
        _startedAt = DateTimeOffset.Now;
        _runId = CreateRunId(_startedAt);
    }

    /// <summary>File-name safe: <c>yyyyMMdd-HHmmss-xxxxxx</c> (local start time + 6 random hex digits).</summary>
    public static string CreateRunId(DateTimeOffset startedAt) =>
        $"{startedAt:yyyyMMdd-HHmmss}-{Random.Shared.Next(0, 0x1000000):x6}";

    public int Passed
    {
        get => _passed;
        set => SetProperty(ref _passed, value);
    }

    public int Failed
    {
        get => _failed;
        set => SetProperty(ref _failed, value);
    }

    public int Deferred
    {
        get => _deferred;
        set => SetProperty(ref _deferred, value);
    }

    public int Total
    {
        get => _total;
        set => SetProperty(ref _total, value);
    }

    public string RunId
    {
        get => _runId;
        set => SetProperty(ref _runId, value ?? string.Empty);
    }

    public DateTimeOffset StartedAt
    {
        get => _startedAt;
        set => SetProperty(ref _startedAt, value);
    }

    public int Evaluated => _passed + _failed + _deferred;
}
