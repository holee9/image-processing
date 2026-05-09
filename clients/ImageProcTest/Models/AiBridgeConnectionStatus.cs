// @MX:NOTE: Pure status enum for AI IPC Bridge UI (C5/C6).
// @MX:REASON: Decouples UI binding from WPF runtime so logic can be unit-tested
// without instantiating MainWindow or referencing PresentationFramework.
// SPEC: SPEC-XPE-P3-AI (Issue #83) — graceful degraded mode when xpe_ai.dll absent.
namespace ImageProcTest
{
    /// <summary>
    /// Represents the AI IPC Bridge UI panel state.
    /// All transitions originate from <see cref="AiBridgeStatusComputer"/>
    /// based on <see cref="ModuleReadinessSnapshot"/>; the UI never invents states locally.
    /// </summary>
    internal enum AiBridgeConnectionStatus
    {
        /// <summary>xpe_ai.dll absent or readiness probe never executed.</summary>
        NotConnected,

        /// <summary>xpe_ai readiness reached binary/ABI tier but worker handshake not yet attempted.</summary>
        Available,

        /// <summary>User initiated connect attempt; awaiting worker heartbeat / IPC response.</summary>
        Connecting,

        /// <summary>Worker heartbeat OK and adapter smoke passed; inference branches may execute.</summary>
        Connected,

        /// <summary>Connect attempted but failed (DLL missing, version mismatch, worker timeout, etc.).</summary>
        Error,
    }
}
