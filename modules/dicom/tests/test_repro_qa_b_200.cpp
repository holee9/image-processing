/**
 * @file test_repro_qa_b_200.cpp
 * @brief QA-B-200 M1: reproductions of the DICOM candidates of QA-B-199 (C1, C7, C9). NOT part of the suite.
 *
 * The reproductions are DISABLED_: built, never run by ctest. Run by hand with --gtest_also_run_disabled_tests; the
 * OUTPUT is the evidence (.moai/reports/lane-post/QA-B-200/). A test that FAILS reproduces a defect; one that passes shows
 * the candidate was not one. The tests of a confirmed candidate are enabled by the M2 that fixes it.
 *
 * One test is ACTIVE, the control ControlTheModuleReadsBackWhatItWrote: a test executable in which every test is
 * DISABLED_ runs nothing, and CI's single-process step refuses "a run that executes nothing" (QA-B-203). The control is
 * the precondition the reproductions share: without it a reproduction that fails could be a harness that cannot see the
 * module.
 *
 * C1  REQ-DICOM-009: "(0018,1152) Exposure (mAs) --> outMeta->mAs". The module writes and reads (0018,9332) instead.
 *     Measured with a file made by DCMTK directly (not by the module), so the writer and the reader cannot agree with each
 *     other by sharing the choice.
 * C9  REQ-DICOM-038: "IF the C-FIND operation fails or times out, THEN the system SHALL return XPE_ERR_NETWORK_FAILED".
 *     The in-process peer (mock_scp.hpp, forcedFindStatus) answers the query with a failure status.
 * C7  REQ-DICOM-032 / REQ-DICOM-039: a failed C-STORE returns NETWORK_FAILED AND posts a WARNING alert with the reason; a
 *     cancelled operation returns PROCESSING_FAILED with a cancel indicator in the alert message.
 */

#include <gtest/gtest.h>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include "mock_scp.hpp"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"
#include "xpe/common/xpe_types.h"
#include "xpe/dicom/dicom_api.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

namespace {

namespace fs = std::filesystem;

struct TempDir {
    fs::path path;
    explicit TempDir(const char* name) {
        path = fs::temp_directory_path() / (std::string("xpe_repro200_") + name + "_" + std::to_string(GetCurrentProcessId()));
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};

/** A 64x64 UINT16 file written by the MODULE with mAs = 100. */
bool WriteWithTheModule(const fs::path& file, float mAs) {
    XpeImageBuffer img{};
    if (xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img) != XPE_OK) return false;
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    meta.kVp = 80.0f;
    meta.mAs = mAs;
    meta.SID_mm = 1000.0f;
    meta.pixelPitch_mm = 0.15f;
    const XpeErrorCode rc = xpe_dicom_write(file.string().c_str(), &img, &meta);
    xpe_free_image(&img);
    return rc == XPE_OK;
}

/** The mAs the module reads from @p file; negative when it could not read. */
float ReadMas(const fs::path& file) {
    XpeDicomHandle* h = nullptr;
    if (xpe_dicom_open(file.string().c_str(), &h) != XPE_OK) return -1.0f;
    XpeImageMetadata m{};
    const XpeErrorCode rc = xpe_dicom_get_metadata(h, &m);
    xpe_dicom_close(h);
    return rc == XPE_OK ? m.mAs : -2.0f;
}

std::string AlertsText() {
    std::string all;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) {
            all += "[severity " + std::to_string(sev) + "] " + msg + "\n";
        }
    }
    return all;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------
// The active control
// ---------------------------------------------------------------------------------------------------------------------

TEST(ReproQaB200Dicom, ControlTheModuleReadsBackWhatItWrote) {
    const TempDir t("control");
    const fs::path f = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(f, 50.0f));
    EXPECT_NEAR(50.0f, ReadMas(f), 0.01f) << "the round trip every existing test runs: the harness can see the module";
    EXPECT_EQ(-1.0f, ReadMas(t.path / "no_such_file.dcm")) << "and a file that is not there is reported as not read (-1)";
}

// ---------------------------------------------------------------------------------------------------------------------
// C1
// ---------------------------------------------------------------------------------------------------------------------

TEST(ReproQaB200Dicom, DISABLED_C1_AFileFromAnotherSystemWithExposure1152ReadsAsMas100) {
    const TempDir t("c1_read");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 50.0f));
    // control: what the module wrote, the module reads back (the round trip every existing test runs)
    ASSERT_NEAR(50.0f, ReadMas(mine), 0.01f) << "control: the module reads back what it wrote";

    // Turn it into the file another system would have written: (0018,9332) gone, (0018,1152) IS = "100".
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_ExposureInmAs);
    ASSERT_TRUE(ds->putAndInsertString(DCM_Exposure, "100").good());
    const fs::path theirs = t.path / "other_system.dcm";
    ASSERT_TRUE(ff.saveFile(theirs.string().c_str()).good());

    const float mas = ReadMas(theirs);
    std::printf("C1 READ: file with (0018,1152) Exposure = IS \"100\" and no (0018,9332) -> module mAs = %.3f\n", mas);
    EXPECT_NEAR(100.0f, mas, 0.01f) << "REQ-DICOM-009: (0018,1152) Exposure (mAs) --> outMeta->mAs";
}

TEST(ReproQaB200Dicom, DISABLED_C1_AFileTheModuleWritesCarriesExposure1152) {
    const TempDir t("c1_write");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 100.0f));
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    OFString v1152, v9332;
    const bool has1152 = ds->findAndGetOFString(DCM_Exposure, v1152).good();
    const bool has9332 = ds->findAndGetOFString(DCM_ExposureInmAs, v9332).good();
    DcmElement* e = nullptr;
    const char* vr9332 = "(absent)";
    if (ds->findAndGetElement(DCM_ExposureInmAs, e).good() && e != nullptr) vr9332 = DcmVR(e->getVR()).getValidVRName();
    std::printf("C1 WRITE: module wrote mAs=100 -> (0018,1152) present: %d value '%s'; (0018,9332) present: %d value '%s' VR %s\n",
                has1152, v1152.c_str(), has9332, v9332.c_str(), vr9332);
    EXPECT_TRUE(has1152) << "REQ-DICOM-015 embeds mAs as (0018,1152) Exposure";
}

// ---------------------------------------------------------------------------------------------------------------------
// C9 and C7 -- a peer that fails
// ---------------------------------------------------------------------------------------------------------------------

namespace {

struct Peer {
    xpe_test::MockScpRunner runner;
    TempDir dir{"peer"};
    uint16_t port = 0;
    Peer() {
        fs::create_directories(dir.path / "incoming");
        port = runner.start("XPEMOCKSCP", (dir.path / "incoming").string());
    }
    ~Peer() {
        if (port != 0) runner.stop();
    }
    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;
};

}  // namespace

TEST(ReproQaB200Dicom, DISABLED_C9_ACFindThatTheServerAnswersWithAFailureStatusIsNetworkFailed) {
    Peer peer;
    ASSERT_NE(0, peer.port) << "no loopback port for the mock SCP";
    char out[4096] = {};
    // control: the same peer, no forced status -> three worklist entries
    ASSERT_EQ(XPE_OK, xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", out, sizeof(out), 5000));
    ASSERT_NE(std::string("[]"), std::string(out)) << "control: the unforced peer returns its three entries";

    peer.runner.scp().forcedFindStatus = 0xA700;   // Refused: out of resources (a failure status)
    char out2[4096] = {};
    const XpeErrorCode rc = xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", out2, sizeof(out2), 5000);
    std::printf("C9 OBSERVED: C-FIND answered with status 0xA700 -> rc %d (%s), outJson '%s'\n", static_cast<int>(rc),
                xpe_error_string(rc), out2);
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, rc) << "REQ-DICOM-038: a C-FIND that fails returns XPE_ERR_NETWORK_FAILED";
    peer.runner.scp().forcedFindStatus = 0;
}

TEST(ReproQaB200Dicom, DISABLED_C7_AFailedCStoreIsNetworkFailedAndPostsAWarningWithTheReason) {
    Peer peer;
    ASSERT_NE(0, peer.port);
    const TempDir t("c7_file");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_clear_alerts();
    // control: success sends and posts no alert of its own
    ASSERT_EQ(XPE_OK, xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 5000));
    xpe_clear_alerts();

    peer.runner.scp().forcedStoreStatus = 0xA700;   // Refused: out of resources
    const XpeErrorCode rc = xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 5000);
    const std::string alerts = AlertsText();
    std::printf("C7 STORE FAILURE: peer answered 0xA700 -> rc %d (%s); alerts posted: %d\n%s", static_cast<int>(rc),
                xpe_error_string(rc), xpe_get_pending_alert_count(), alerts.c_str());
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, rc) << "REQ-DICOM-032";
    EXPECT_GE(xpe_get_pending_alert_count(), 1) << "REQ-DICOM-032: a WARNING alert with the failure reason string";
    peer.runner.scp().forcedStoreStatus = 0;
    xpe_clear_alerts();
}

TEST(ReproQaB200Dicom, DISABLED_C7_ACancelledCStoreIsProcessingFailedWithACancelIndicatorInTheAlert) {
    Peer peer;
    ASSERT_NE(0, peer.port);
    const TempDir t("c7_cancel");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_clear_alerts();
    peer.runner.scp().storeDelayMs = 3000;   // the peer is slow in the middle of the transfer
    XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
    long long ms = 0;
    std::thread sender([&] {
        const auto start = std::chrono::steady_clock::now();
        rc = xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 10000);
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(800));   // the association is up, the request is with the peer
    xpe_dicom_cancel();
    sender.join();
    peer.runner.scp().storeDelayMs = 0;
    const std::string alerts = AlertsText();
    std::printf("C7 CANCEL: cancel sent 800 ms into a transfer the peer delays by 3000 ms -> call returned after %lld ms with "
                "rc %d (%s); alerts: %d\n%s",
                ms, static_cast<int>(rc), xpe_error_string(rc), xpe_get_pending_alert_count(), alerts.c_str());
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << "REQ-DICOM-039: the cancelled operation returns XPE_ERR_PROCESSING_FAILED";
    EXPECT_LT(ms, 2000) << "the cancel interrupts the transfer; it does not wait for the peer";
    EXPECT_NE(std::string::npos, alerts.find("cancel")) << "REQ-DICOM-039: a cancel indicator in the alert message";
    xpe_clear_alerts();
}
