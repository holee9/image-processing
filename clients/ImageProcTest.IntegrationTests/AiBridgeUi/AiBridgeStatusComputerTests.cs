// SPEC: SPEC-XPE-P3-AI (Issue #83) - AI IPC Bridge UI C5/C6 graceful degraded mode.
// These tests pin the contract of AiBridgeStatusComputer, the pure helper that
// the WPF MainWindow uses to derive AI panel state. They run in the headless
// IntegrationTests project (net8.0, no WPF) via linked source files.
using System.Collections.Generic;
using ImageProcTest;
using Xunit;

namespace ImageProcTest.IntegrationTests.AiBridgeUi
{
    public sealed class AiBridgeStatusComputerTests
    {
        [Fact]
        public void Compute_NullReadiness_ReturnsNotConnectedSnapshot()
        {
            var snapshot = AiBridgeStatusComputer.Compute(null);

            Assert.Equal(AiBridgeConnectionStatus.NotConnected, snapshot.Status);
            Assert.Equal("AI Engine: Not Connected", snapshot.HeaderText);
            Assert.Contains("xpe_ai.dll not detected", snapshot.DetailText);
            Assert.Contains("No AI analysis available", snapshot.ResultsText);
            Assert.False(snapshot.ConnectButtonEnabled);
            Assert.False(snapshot.ConnectionInputsEnabled);
            Assert.Equal(AiBridgeStatusComputer.Phase3Tooltip, snapshot.Tooltip);
        }

        [Fact]
        public void Compute_EmptyReadiness_ReturnsNotConnectedSnapshot()
        {
            var snapshot = AiBridgeStatusComputer.Compute(new List<ModuleReadinessSnapshot>());

            Assert.Equal(AiBridgeConnectionStatus.NotConnected, snapshot.Status);
            Assert.Equal("AI Engine: Not Connected", snapshot.HeaderText);
        }

        [Fact]
        public void Compute_ReadinessWithoutAiModule_ReturnsNotConnected()
        {
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_common", "R3", "Ready", "evidence", "next", true)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules);

            Assert.Equal(AiBridgeConnectionStatus.NotConnected, snapshot.Status);
            Assert.False(snapshot.ConnectButtonEnabled);
        }

        [Fact]
        public void Compute_AiModuleR0_ReturnsNotConnected()
        {
            // R0 = not ready / DLL absent. UI must not even allow Connect.
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_ai", "R0", "Not ready", "no evidence", "deploy worker", false)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules);

            Assert.Equal(AiBridgeConnectionStatus.NotConnected, snapshot.Status);
            Assert.False(snapshot.ConnectButtonEnabled);
            Assert.False(snapshot.ConnectionInputsEnabled);
        }

        [Fact]
        public void Compute_AiModuleR1WithoutConnectAttempt_ReturnsAvailable()
        {
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_ai", "R1", "Binary present", "version=0.0.1", "next", false)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules);

            Assert.Equal(AiBridgeConnectionStatus.Available, snapshot.Status);
            Assert.True(snapshot.ConnectButtonEnabled);
            Assert.True(snapshot.ConnectionInputsEnabled);
            Assert.Contains("Connect", snapshot.DetailText);
        }

        [Fact]
        public void Compute_AiModuleR2UserRequestedConnect_ReturnsConnecting()
        {
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_ai", "R2", "ABI smoke", "evidence", "next", false)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules, userRequestedConnect: true);

            Assert.Equal(AiBridgeConnectionStatus.Connecting, snapshot.Status);
            Assert.False(snapshot.ConnectButtonEnabled);
            Assert.True(snapshot.DisconnectButtonEnabled);
            Assert.Contains("Connecting", snapshot.HeaderText);
        }

        [Fact]
        public void Compute_AiModuleR3Enabled_ReturnsConnected()
        {
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_ai", "R3", "Worker heartbeat OK", "evidence", "ready", true)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules);

            Assert.Equal(AiBridgeConnectionStatus.Connected, snapshot.Status);
            Assert.Equal("AI Engine: Connected", snapshot.HeaderText);
            Assert.False(snapshot.ConnectButtonEnabled);
            Assert.True(snapshot.DisconnectButtonEnabled);
        }

        [Fact]
        public void Compute_LastErrorMessage_OverridesToErrorState()
        {
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("xpe_ai", "R0", "Not ready", "no evidence", "deploy worker", false)
            };

            var snapshot = AiBridgeStatusComputer.Compute(
                modules,
                userRequestedConnect: true,
                lastErrorMessage: "xpe_ai.dll absent (Phase 3 not yet deployed)");

            Assert.Equal(AiBridgeConnectionStatus.Error, snapshot.Status);
            Assert.Contains("Connection Error", snapshot.HeaderText);
            Assert.Contains("xpe_ai.dll absent", snapshot.DetailText);
            Assert.True(snapshot.ConnectButtonEnabled);
            Assert.Contains("No AI analysis available", snapshot.ResultsText);
        }

        [Fact]
        public void Compute_NeverThrowsOnUnusualReadinessShapes()
        {
            // Defensive: empty strings, weird casing, null collection - none should throw.
            var modules = new List<ModuleReadinessSnapshot>
            {
                new("XPE_AI", "", "", "", "", false),
                new("Xpe_Ai", "Rfoo", "weird", "weird", "weird", false)
            };

            var snapshot = AiBridgeStatusComputer.Compute(modules);

            Assert.NotNull(snapshot);
            Assert.False(string.IsNullOrEmpty(snapshot.HeaderText));
            Assert.False(string.IsNullOrEmpty(snapshot.ResultsText));
        }
    }
}
