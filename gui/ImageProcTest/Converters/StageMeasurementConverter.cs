using System;
using System.Globalization;
using System.Windows.Data;
using ImageProcTest.Models;

namespace ImageProcTest.Converters;

/// <summary>
/// #225 row 13 (GUI-C-168): renders one stage's elapsed time, and renders NOTHING when there is no
/// measurement to render.
///
/// <para><b>The whole reason this converter exists.</b> <see cref="StageOutcome.ElapsedMs"/> is
/// documented as "Zero for a stage that was not requested" — so a stage that never ran carries the
/// value <c>0.0</c>, and <see cref="ChainResult.Timings"/> already prints it as
/// <c>preprocess=0 ms</c>. On a status bar that string sits next to the status, which supplies the
/// missing half; on a panel that lists times, <c>0 ms</c> alone reads as "this stage was
/// instantaneous". That is the failure the card names: not a wrong value, an ABSENT value shown as a
/// number. This converter refuses to print a number that is not a measurement.</para>
///
/// <para>It suppresses; it never invents. There is no branch here that produces a time where the
/// model had none — the only outputs are the model's own number, or the em dash that means "no
/// measurement". A converter that filled in a default would be the defect, not the fix.</para>
/// </summary>
public sealed class StageMeasurementConverter : IValueConverter
{
    /// <summary>What the panel shows when the model holds no measurement.</summary>
    public const string NoMeasurement = "—";

    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        if (value is not StageOutcome stage) return NoMeasurement;

        // NotRequested is the documented zero: the stage was in the list and switched off, so its
        // ElapsedMs is 0.0 by construction rather than by measurement.
        return stage.Status == StageStatus.NotRequested
            ? NoMeasurement
            : $"{stage.ElapsedMs:0} ms";
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture) =>
        throw new NotSupportedException("Diagnostics are read-only.");
}
