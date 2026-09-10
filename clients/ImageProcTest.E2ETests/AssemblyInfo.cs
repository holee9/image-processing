// UI automation cannot run collections in parallel.
using Xunit;

// GUI-C-36: xUnit runs distinct collections in parallel by default. This suite drives the SAME
// application executable from two collections (smoke and workflow), and the leftover-instance sweep
// added in the same card kills every process running that exe — including the one the other
// collection is driving. Measured: a Native run failed all four workflow scenarios at 1 ms with
// "E_FAIL from a COM component" inside GetMainWindow, because its window died mid-attach.
//
// Serialising the assembly is the right fix rather than narrowing the sweep: two instances of the
// same GUI competing for focus and for the settings file is not a state this suite should observe.
[assembly: CollectionBehavior(DisableTestParallelization = true)]
