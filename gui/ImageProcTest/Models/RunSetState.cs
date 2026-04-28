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
