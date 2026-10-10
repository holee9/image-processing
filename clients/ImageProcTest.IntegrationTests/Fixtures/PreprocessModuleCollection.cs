namespace ImageProcTest.IntegrationTests.Fixtures;

/// <summary>
/// GUI-C-233g (Codex #176, finding 2): xpe_preprocess.dll keeps ONE process-wide state (initialised flag, loaded maps). A second <c>xpe_preprocess_init</c> while it is initialised answers
/// XPE_ERR_INVALID_INPUT (modules/preprocess/src/preprocess.cpp), and <c>xpe_preprocess_shutdown</c> clears the maps another test just loaded. xUnit runs test classes in different
/// collections IN PARALLEL, so seven classes that initialise this module could overlap and fail each other (observed: PreprocessHandshakeTests expecting init == OK, twice in about ten full runs).
/// Every class that initialises, shuts down or runs the module is in this one collection, so they run one after the other.
/// </summary>
[CollectionDefinition(Name)]
public sealed class PreprocessModuleCollection
{
    public const string Name = "PreprocessModuleState";
}
