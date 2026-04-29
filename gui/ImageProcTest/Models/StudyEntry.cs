using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;
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

public sealed class StudyStatusToGlyphConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        return value switch
        {
            StudyStatus.Pass => "✓",
            StudyStatus.Fail => "✗",
            StudyStatus.Defer => "⏸",
            StudyStatus.Active => "●",
            _ => "○"
        };
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => Binding.DoNothing;
}

public sealed class StudyStatusToColorConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        var hex = value switch
        {
            StudyStatus.Pass => "#86efac",
            StudyStatus.Fail => "#fca5a5",
            StudyStatus.Defer => "#fcd34d",
            StudyStatus.Active => "#7dd3fc",
            _ => "#5a5f6a"
        };
        return new SolidColorBrush((Color)ColorConverter.ConvertFromString(hex));
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
        => Binding.DoNothing;
}
