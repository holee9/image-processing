// #173 (GUI-C-119): a launch that reads and writes a settings file the caller names.
namespace ImageProcTest.E2ETests.Fixtures;

/// <summary>
/// Launches the app with <c>--automation-settings &lt;path&gt;</c>, so a case can decide what the
/// stored settings are before the process starts.
///
/// <para>No raw image is loaded. That is deliberate: the status bar is the surface these cases read,
/// and an automation image load writes to it after startup — the message under test would be gone by
/// the time UIA looked. With no image, the last thing said at startup stays said.</para>
///
/// <para>Not a collection fixture. Settings are read once, at startup, so each case needs its own
/// process against its own file.</para>
/// </summary>
internal sealed class SettingsFileApplicationFixture(string settingsPath)
    : ApplicationFixture(rawImageRelativePath: null, ["--automation-settings", settingsPath])
{
}
