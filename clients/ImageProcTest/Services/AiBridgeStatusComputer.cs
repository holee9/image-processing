using System;
using System.Collections.Generic;
using System.Linq;

namespace ImageProcTest
{
    // @MX:ANCHOR: Single function that converts module readiness into AI panel display state.
    // @MX:REASON: Fan-in >= 3 (MainWindow header bind, AI tab bind, unit tests). Any logic drift
    // here breaks Phase 3 graceful-degradation contract from SPEC-XPE-P3-AI / Issue #83.
    /// <summary>
    /// Pure logic helper that builds an <see cref="AiBridgePanelSnapshot"/> from a
    /// <see cref="ModuleReadinessSnapshot"/> list. WPF-free so it can be unit-tested
    /// without WPF dispatcher or window construction.
    /// </summary>
    internal static class AiBridgeStatusComputer
    {
        internal const string ModuleName = "xpe_ai";
        internal const string Phase3Tooltip =
            "Requires xpe_ai.dll - Phase 3 feature (SPEC-XPE-P3-AI).";

        /// <summary>
        /// Compute the panel snapshot. <paramref name="moduleReadiness"/> may be empty
        /// (e.g. before readiness probe runs) - we still return a stable NotConnected snapshot.
        /// <paramref name="userRequestedConnect"/> tracks whether the user clicked Connect since
        /// the last refresh; this lets the UI show <c>Connecting</c> even when readiness has
        /// not yet flipped, so the button does not feel unresponsive.
        /// </summary>
        public static AiBridgePanelSnapshot Compute(
            IReadOnlyList<ModuleReadinessSnapshot>? moduleReadiness,
            bool userRequestedConnect = false,
            string? lastErrorMessage = null)
        {
            var snapshot = FindAiSnapshot(moduleReadiness);
            var rank = snapshot?.LevelRank ?? 0;

            if (!string.IsNullOrEmpty(lastErrorMessage))
            {
                return new AiBridgePanelSnapshot(
                    Status: AiBridgeConnectionStatus.Error,
                    HeaderText: "AI Engine: Connection Error",
                    DetailText: $"Last connect attempt failed: {lastErrorMessage}. Phase 3 worker not yet available; UI remains in degraded mode.",
                    ResultsText: "No AI analysis available - retry connect or wait for xpe_ai.dll deployment.",
                    ConnectButtonEnabled: true,
                    DisconnectButtonEnabled: false,
                    ConnectionInputsEnabled: true,
                    Tooltip: Phase3Tooltip);
            }

            if (snapshot is null || rank < 1)
            {
                return new AiBridgePanelSnapshot(
                    Status: AiBridgeConnectionStatus.NotConnected,
                    HeaderText: "AI Engine: Not Connected",
                    DetailText: "xpe_ai.dll not detected. Phase 3 AI features remain disabled; deterministic preprocess/enhance pipeline continues unaffected.",
                    ResultsText: "No AI analysis available - inference score and anomaly map will appear here once the AI worker connects.",
                    ConnectButtonEnabled: false,
                    DisconnectButtonEnabled: false,
                    ConnectionInputsEnabled: false,
                    Tooltip: Phase3Tooltip);
            }

            if (userRequestedConnect && rank < 3)
            {
                return new AiBridgePanelSnapshot(
                    Status: AiBridgeConnectionStatus.Connecting,
                    HeaderText: "AI Engine: Connecting...",
                    DetailText: $"xpe_ai readiness {snapshot.Level} {snapshot.Status}; awaiting worker heartbeat and adapter smoke.",
                    ResultsText: "No AI analysis available - waiting for worker handshake.",
                    ConnectButtonEnabled: false,
                    DisconnectButtonEnabled: true,
                    ConnectionInputsEnabled: false,
                    Tooltip: Phase3Tooltip);
            }

            if (snapshot.ProcessingEnabled && rank >= 3)
            {
                return new AiBridgePanelSnapshot(
                    Status: AiBridgeConnectionStatus.Connected,
                    HeaderText: "AI Engine: Connected",
                    DetailText: $"xpe_ai {snapshot.Level} {snapshot.Status}. AI assistive branches (body-part, AI collimation, bone suppression, DL denoise, stitching) available as sidecar outputs.",
                    ResultsText: "AI ready - inference score and anomaly map will populate after the next pipeline run.",
                    ConnectButtonEnabled: false,
                    DisconnectButtonEnabled: true,
                    ConnectionInputsEnabled: false,
                    Tooltip: Phase3Tooltip);
            }

            return new AiBridgePanelSnapshot(
                Status: AiBridgeConnectionStatus.Available,
                HeaderText: "AI Engine: Available (not connected)",
                DetailText: $"xpe_ai readiness {snapshot.Level} {snapshot.Status}; click Connect to attempt worker handshake.",
                ResultsText: "No AI analysis available - press Connect to start the IPC bridge.",
                ConnectButtonEnabled: true,
                DisconnectButtonEnabled: false,
                ConnectionInputsEnabled: true,
                Tooltip: Phase3Tooltip);
        }

        private static ModuleReadinessSnapshot? FindAiSnapshot(IReadOnlyList<ModuleReadinessSnapshot>? moduleReadiness)
        {
            if (moduleReadiness is null || moduleReadiness.Count == 0)
            {
                return null;
            }

            return moduleReadiness.FirstOrDefault(module =>
                string.Equals(module.ModuleName, ModuleName, StringComparison.OrdinalIgnoreCase));
        }
    }
}
