// GUI-C-229e: the real-keystroke step of R02 with a diagnostic read that throws. No input is sent and no application is started: the reads and the key sending are stand-ins.
using System.Diagnostics;
using ImageProcTest.E2ETests.Fixtures;
using Xunit;

namespace ImageProcTest.E2ETests.Scenarios.Workflows;

public sealed class KeystrokeStepTests
{
    private const int App = 100;
    private const string Typed = "4321";
    private const string Box = "VoiWindowCenterInput";

    private static Observation Obs(string center) => new(center, "1000", Box, App, App);

    /// <summary>A step whose pieces are stand-ins. Each piece can be made to throw. The events list records the order in which they ran.</summary>
    private sealed class Rig
    {
        public string Center = Typed;
        public bool InFront = true;
        public Exception? FrontThrows;
        public Exception? ObserveBeforeThrows;
        public Exception? ObserveAfterThrows;
        public Exception? ObserveLaterThrows;
        public Exception? DescribeFrontThrows;
        public readonly List<string> Events = [];
        public readonly List<string> Log = [];

        public KeystrokeStep.Seams Seams() => new(
            AppInFront: () =>
            {
                Events.Add("front-check");
                return FrontThrows is null ? InFront : throw FrontThrows;
            },
            DescribeFront: () =>
            {
                Events.Add("describe-front");
                return DescribeFrontThrows is null ? "In front: something." : throw DescribeFrontThrows;
            },
            ObserveBefore: () =>
            {
                Events.Add("observe-before");
                return ObserveBeforeThrows is null ? Obs("300") : throw ObserveBeforeThrows;
            },
            SendKeys: () => Events.Add("keys-sent"),
            TakeJudgment: _ =>
            {
                Events.Add("judgment");
                return (Center, 604, 616);
            },
            ObserveAfterJudgment: center =>
            {
                Events.Add("observe-after");
                return ObserveAfterThrows is null ? Obs(center) : throw ObserveAfterThrows;
            },
            ObserveLater: () =>
            {
                Events.Add("observe-later");
                return ObserveLaterThrows is null ? Obs(Center) : throw ObserveLaterThrows;
            },
            AppProcessId: App);

        public KeystrokeStep.Result Run() => KeystrokeStep.Run(Typed, Seams(), line =>
        {
            Events.Add("log");
            Log.Add(line);
        });
    }

    // ---- (a) the Width lookup throws, Center is 4321: the verdict is a pass and the record is there -------------------------------------------------------------------------

    [Fact]
    public void A_ADiagnosticReadThatThrows_DoesNotFailARunWhoseCenterIs4321_AndTheRecordIsStillWritten()
    {
        var rig = new Rig { ObserveAfterThrows = new InvalidOperationException("the Width box could not be found") };

        var run = rig.Run();

        Assert.Null(run.Failure);                       // the verdict
        Assert.Equal(Typed, run.Center);
        Assert.True(run.KeysSent);
        Assert.StartsWith("R02 judgment read: started +604 ms, completed +616 ms", rig.Log[0]);   // the record, first
        Assert.Contains("diagnostic read failed: InvalidOperationException: the Width box could not be found", rig.Log[1]);   // named as a fact
        Assert.True(rig.Events.IndexOf("log") < rig.Events.IndexOf("observe-after"), "the record must be written before any diagnostic read");
    }

    // ---- (b) the Width lookup throws, Center is 43: the original loss failure, with the record ---------------------------------------------------------------------------------

    [Fact]
    public void B_ADiagnosticReadThatThrows_DoesNotHideALoss_AndTheMessageStillHasTheRecord()
    {
        var rig = new Rig { Center = "43", ObserveAfterThrows = new InvalidOperationException("the Width box could not be found") };

        var run = rig.Run();

        Assert.NotNull(run.Failure);
        Assert.Contains("typed '4321' but the center box held '43'", run.Failure);
        Assert.Contains("judgment read: started +604 ms, completed +616 ms", run.Failure);
        Assert.Contains("diagnostic read failed: InvalidOperationException: the Width box could not be found", run.Failure);
        Assert.Contains("Unclassified", run.Failure);
        Assert.StartsWith("R02 judgment read:", rig.Log[0]);
    }

    // ---- (c) the Width lookup before the keys throws: the keys are still sent ----------------------------------------------------------------------------------------------------

    [Fact]
    public void C_ADiagnosticReadBeforeTheKeysThatThrows_DoesNotStopTheKeys()
    {
        var rig = new Rig { ObserveBeforeThrows = new InvalidOperationException("the Width box could not be found") };

        var run = rig.Run();

        Assert.True(run.KeysSent);
        Assert.Contains("keys-sent", rig.Events);
        Assert.Null(run.Failure);
        Assert.Contains("before the keys: diagnostic read failed: InvalidOperationException", rig.Log[1]);
    }

    // ---- (d) the foreground check throws or says no: no key is sent and the step fails ---------------------------------------------------------------------------------------------

    [Fact]
    public void D_AForegroundCheckThatThrows_SendsNoKey_AndFails()
    {
        var rig = new Rig { FrontThrows = new InvalidOperationException("could not ask Windows") };

        var run = rig.Run();

        Assert.False(run.KeysSent);
        Assert.DoesNotContain("keys-sent", rig.Events);
        Assert.NotNull(run.Failure);
        Assert.Contains("could not be checked", run.Failure);
        Assert.Contains("NO key was sent", run.Failure);
        Assert.Empty(rig.Log);
    }

    [Fact]
    public void D_AnApplicationNotInFront_SendsNoKey_AndFails()
    {
        var rig = new Rig { InFront = false };

        var run = rig.Run();

        Assert.False(run.KeysSent);
        Assert.DoesNotContain("keys-sent", rig.Events);
        Assert.Contains("NO key was sent", run.Failure);
        Assert.Contains("In front: something.", run.Failure);
    }

    // ---- the other diagnostic reads are isolated the same way ---------------------------------------------------------------------------------------------------------------------

    [Fact]
    public void TheReReadAndTheDescriptionOfWhatIsInFront_AreIsolatedToo()
    {
        var rig = new Rig
        {
            Center = "43",
            ObserveLaterThrows = new TimeoutException("UI Automation did not answer"),
            DescribeFrontThrows = new InvalidOperationException("no foreground window"),
        };

        var run = rig.Run();

        Assert.Contains("typed '4321' but the center box held '43'", run.Failure);
        Assert.Contains("1.5 s later: diagnostic read failed: TimeoutException: UI Automation did not answer", run.Failure);
        Assert.Contains("(diagnostic read failed: InvalidOperationException: no foreground window)", run.Failure);
    }

    [Fact]
    public void ThePassingPath_ReadsNothingElse_AfterTheRecord_ThanTheOneObservation()
    {
        var rig = new Rig();

        rig.Run();

        Assert.Equal(["observe-before", "front-check", "keys-sent", "judgment", "log", "observe-after", "log"], rig.Events);
    }
}
