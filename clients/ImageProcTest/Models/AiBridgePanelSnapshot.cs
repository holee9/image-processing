// @MX:NOTE: Immutable snapshot bound to the AI Module tab UI (C5 connection + C6 results).
// @MX:REASON: Single source-of-truth for the AI panel display so a single computation
// drives both header status and results placeholder, ensuring degraded-mode parity.
namespace ImageProcTest
{
    /// <summary>
    /// Display-ready snapshot of the AI IPC Bridge UI for one render frame.
    /// Produced by <see cref="AiBridgeStatusComputer"/>.
    /// </summary>
    internal sealed record AiBridgePanelSnapshot(
        AiBridgeConnectionStatus Status,
        string HeaderText,
        string DetailText,
        string ResultsText,
        bool ConnectButtonEnabled,
        bool DisconnectButtonEnabled,
        bool ConnectionInputsEnabled,
        string Tooltip);
}
