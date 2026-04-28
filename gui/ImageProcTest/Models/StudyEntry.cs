using ImageProcTest.ViewModels;

namespace ImageProcTest.Models;

public sealed class StudyEntry : ObservableObject
{
    private StudyStatus _status = StudyStatus.Queued;
    private string _deltaSummary = "—";

    public required string Id { get; init; }
    public required string Name { get; init; }
    public required string BodyPart { get; init; }
    public required string RawPath { get; init; }

    public StudyStatus Status
    {
        get => _status;
        set => SetProperty(ref _status, value);
    }

    public string DeltaSummary
    {
        get => _deltaSummary;
        set => SetProperty(ref _deltaSummary, value);
    }
}

public enum StudyStatus
{
    Queued,
    Active,
    Pass,
    Fail,
    Defer
}
