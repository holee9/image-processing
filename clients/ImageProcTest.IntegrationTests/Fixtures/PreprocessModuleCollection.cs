namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// GUI-C-233g/h (Codex #176, #177): xpe_preprocess.dll keeps ONE process-wide state (initialised flag, loaded maps). A second <c>xpe_preprocess_init</c> while it is initialised answers
/// XPE_ERR_INVALID_INPUT (modules/preprocess/src/preprocess.cpp), and <c>xpe_preprocess_shutdown</c> clears the maps another test just loaded.
///
/// <para>xunit.runner.json already runs everything serially (<c>parallelizeTestCollections: false</c>, one thread), so this collection is NOT what stops a concurrent overlap today: it is a
/// protective device for the day parallelisation is switched on, and it keeps the classes that share the module declared as one group. The intermittent failure of
/// PreprocessHandshakeTests was not a race in a serial run; see PreprocessModuleLeakTests for what was found.</para>
/// </summary>
[CollectionDefinition(Name)]
public sealed class PreprocessModuleCollection
{
    public const string Name = "PreprocessModuleState";
}
