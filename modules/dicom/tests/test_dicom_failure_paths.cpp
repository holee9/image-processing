/**
 * @file test_dicom_failure_paths.cpp
 * @brief The failure and cancel paths of the network SCU, and the mAs tag (QA-B-200 M2a, #251; found by QA-B-199).
 *
 * These began as the M1 reproductions (QA-B-200 M1, every one DISABLED_ and failing); the fixes turned them into tests.
 *
 * C9  REQ-DICOM-038: a C-FIND the server answers with a failure status is XPE_ERR_NETWORK_FAILED, not XPE_OK with "[]".
 *     (PS3.4 Table K.4-1: 0xA700, 0xA900, 0xCxxx are failures; an SCU shall recognise any status in the failure range.)
 * C7  REQ-DICOM-032: a failed C-STORE posts a WARNING alert with the reason. REQ-DICOM-039: a cancel is reported as a
 *     cancel (XPE_ERR_PROCESSING_FAILED, "cancelled" in the alert), never as success. DCMTK's send is one blocking call,
 *     so a cancel that arrives during a transfer is reported when the transfer returns; the test pins that, and NOT a
 *     promptness the module cannot give.
 * C1  REQ-DICOM-009/015: (0018,1152) Exposure is the attribute another system writes for mAs. The module writes it (an
 *     integer string, rounded half up) beside (0018,9332) (FD, the exact value) and reads (0018,9332) when present, else
 *     (0018,1152). The files used to prove the read are made by DCMTK directly, not by the module, so a writer and a
 *     reader that share a mistake cannot agree with each other.
 *
 * The mock peer (mock_scp.hpp) answers with a failure status or waits when a test asks it to.
 */

#include <gtest/gtest.h>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include "mock_scp.hpp"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"
#include "xpe/common/xpe_types.h"
#include "xpe/dicom/dicom_api.h"

#include <algorithm>
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
        path = fs::temp_directory_path() / (std::string("xpe_failpaths_") + name + "_" + std::to_string(GetCurrentProcessId()));
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

/** A 64x64 UINT16 file written by the MODULE with the given mAs. */
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

struct Alert {
    int32_t severity;
    std::string text;
};

std::vector<Alert> Alerts() {
    std::vector<Alert> all;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) all.push_back({sev, msg});
    }
    return all;
}

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

// ---------------------------------------------------------------------------------------------------------------------
// C1 -- the mAs tag
// ---------------------------------------------------------------------------------------------------------------------

TEST(DicomExposureTag, AFileFromAnotherSystemWithOnlyExposure1152ReadsAsThatValue) {
    const TempDir t("c1_read");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 50.0f));
    ASSERT_NEAR(50.0f, ReadMas(mine), 0.01f) << "control: the module reads back what it wrote";

    // The file another system would have written: (0018,9332) gone, (0018,1152) IS = "100".
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_ExposureInmAs);
    ASSERT_TRUE(ds->putAndInsertString(DCM_Exposure, "100").good());
    const fs::path theirs = t.path / "other_system.dcm";
    ASSERT_TRUE(ff.saveFile(theirs.string().c_str()).good());

    EXPECT_NEAR(100.0f, ReadMas(theirs), 0.01f) << "was 0: only (0018,9332) was read";
}

TEST(DicomExposureTag, WhenBothAreInTheFileTheExactValueOf9332IsUsed) {
    const TempDir t("c1_both");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 2.5f));
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ASSERT_TRUE(ds->putAndInsertString(DCM_Exposure, "999").good()) << "a disagreeing (0018,1152)";
    const fs::path both = t.path / "both.dcm";
    ASSERT_TRUE(ff.saveFile(both.string().c_str()).good());
    EXPECT_NEAR(2.5f, ReadMas(both), 0.001f) << "(0018,9332) is the exact value and takes precedence";
}

TEST(DicomExposureTag, TheModuleWritesBothAttributesWithTheValuesTheRulesGive) {
    struct Row {
        float mAs;
        const char* expect1152;   // IS, rounded half up; nullptr = the attribute is not written
    };
    const Row rows[] = {{100.0f, "100"}, {2.5f, "3"}, {2.4f, "2"}, {0.4f, "0"}, {0.5f, "1"}, {3.0e9f, nullptr}};
    for (const Row& r : rows) {
        const TempDir t("c1_write");
        const fs::path f = t.path / "w.dcm";
        ASSERT_TRUE(WriteWithTheModule(f, r.mAs)) << r.mAs;
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(f.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        OFString v1152;
        const bool has1152 = ds->findAndGetOFString(DCM_Exposure, v1152).good();
        Float64 v9332 = -1;
        ASSERT_TRUE(ds->findAndGetFloat64(DCM_ExposureInmAs, v9332).good()) << "(0018,9332) keeps the exact value, mAs " << r.mAs;
        EXPECT_NEAR(static_cast<double>(r.mAs), v9332, 1e-3 * (std::max)(1.0, static_cast<double>(r.mAs))) << "mAs " << r.mAs;
        if (r.expect1152 == nullptr) {
            EXPECT_FALSE(has1152) << "mAs " << r.mAs << " does not fit an IS (above 2^31-1): (0018,1152) is left out";
        } else {
            ASSERT_TRUE(has1152) << "mAs " << r.mAs << ": REQ-DICOM-015 embeds (0018,1152) Exposure";
            EXPECT_STREQ(r.expect1152, v1152.c_str()) << "mAs " << r.mAs;
        }
        DcmElement* e = nullptr;
        ASSERT_TRUE(ds->findAndGetElement(DCM_Exposure, e).good() == has1152);
        if (has1152) EXPECT_STREQ("IS", DcmVR(e->getVR()).getValidVRName()) << "(0018,1152) is an Integer String";
        // the module's own round trip still gives the exact value back
        EXPECT_NEAR(r.mAs, ReadMas(f), 1e-3 * (std::max)(1.0f, r.mAs)) << "mAs " << r.mAs;
    }
}

TEST(DicomExposureTag, ANonNumericOrNegative1152IsNotAMasValue) {
    const TempDir t("c1_garbage");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 50.0f));
    for (const char* bad : {"abc", "-7", ""}) {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ds->findAndDeleteElement(DCM_ExposureInmAs);
        ds->putAndInsertString(DCM_Exposure, bad);
        const fs::path f = t.path / "bad.dcm";
        ASSERT_TRUE(ff.saveFile(f.string().c_str()).good());
        EXPECT_FLOAT_EQ(0.0f, ReadMas(f)) << "(0018,1152) = '" << bad << "': mAs stays at its default 0";
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// C9 -- a C-FIND the server fails
// ---------------------------------------------------------------------------------------------------------------------

TEST(DicomFailurePaths, ACFindAnsweredWithAFailureStatusIsNetworkFailedForEveryStatusOfTheFailureRange) {
    Peer peer;
    ASSERT_NE(0, peer.port) << "no loopback port for the mock SCP";
    char control[4096] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", control, sizeof(control), 5000));
    ASSERT_NE(std::string("[]"), std::string(control)) << "control: the unforced peer returns its three entries";

    for (const unsigned status : {0xA700u, 0xA900u, 0xC001u, 0xFE00u}) {
        peer.runner.scp().forcedFindStatus = status;
        char out[4096];
        std::memset(out, 'x', sizeof(out));
        xpe_clear_alerts();
        const XpeErrorCode rc =
            xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", out, sizeof(out), 5000);
        EXPECT_EQ(XPE_ERR_NETWORK_FAILED, rc) << "status 0x" << std::hex << status << " (was XPE_OK with [])";
        EXPECT_EQ('x', out[0]) << "a failed query writes nothing to the output buffer";
        bool named = false;
        for (const Alert& a : Alerts()) named = named || a.text.find("C-FIND failed") != std::string::npos;
        EXPECT_TRUE(named) << "an alert says the query failed (status 0x" << std::hex << status << ")";
    }
    peer.runner.scp().forcedFindStatus = 0;
    char after[4096] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", after, sizeof(after), 5000))
        << "nothing is remembered: the same peer, unforced, works again";
}

TEST(DicomFailurePaths, AQueryWithNoMatchIsStillAnEmptyListAndOk) {
    // The line the fix must not move: "no worklist entries" (final status Success, nothing pending) is XPE_OK with [].
    Peer peer;
    ASSERT_NE(0, peer.port);
    char out[256] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"PatientID":"NONEXISTENT_9999"})", out, sizeof(out), 5000));
    EXPECT_STREQ("[]", out);
}

// ---------------------------------------------------------------------------------------------------------------------
// C7 -- a C-STORE that fails, and one that is cancelled
// ---------------------------------------------------------------------------------------------------------------------

TEST(DicomFailurePaths, AFailedCStoreIsNetworkFailedAndPostsAWarningWithTheReason) {
    Peer peer;
    ASSERT_NE(0, peer.port);
    const TempDir t("c7_file");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 5000));
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "control: a successful send posts no alert";

    peer.runner.scp().forcedStoreStatus = 0xA700;
    const XpeErrorCode rc = xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 5000);
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, rc);
    const std::vector<Alert> alerts = Alerts();
    ASSERT_EQ(1u, alerts.size()) << "was 0: nothing told the operator";
    EXPECT_EQ(XPE_ALERT_WARNING, alerts[0].severity);
    EXPECT_NE(std::string::npos, alerts[0].text.find("C-STORE failed")) << alerts[0].text;
    EXPECT_NE(std::string::npos, alerts[0].text.find("0xA700")) << "the reason: the status the peer gave: " << alerts[0].text;
    EXPECT_EQ(std::string::npos, alerts[0].text.find("cancel")) << "a failure text never says 'cancel'";
    peer.runner.scp().forcedStoreStatus = 0;
    xpe_clear_alerts();
}

TEST(DicomFailurePaths, ACStoreToAPortNobodyServesPostsAWarningWithTheReason) {
    const TempDir t("c7_noserver");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, xpe_dicom_cstore("localhost", 19999, "TESTSCU", file.string().c_str(), 500));
    const std::vector<Alert> alerts = Alerts();
    ASSERT_EQ(1u, alerts.size());
    EXPECT_EQ(XPE_ALERT_WARNING, alerts[0].severity);
    EXPECT_NE(std::string::npos, alerts[0].text.find("C-STORE failed")) << alerts[0].text;
    xpe_clear_alerts();
}

TEST(DicomFailurePaths, ACancelDuringATransferIsReportedAsACancelAndNeverAsSuccess) {
    Peer peer;
    ASSERT_NE(0, peer.port);
    const TempDir t("c7_cancel");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_clear_alerts();
    peer.runner.scp().storeDelayMs = 2000;   // the peer is slow in the middle of the transfer
    XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
    std::thread sender([&] { rc = xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 10000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(700));   // the association is up, the request is with the peer
    xpe_dicom_cancel();
    sender.join();
    peer.runner.scp().storeDelayMs = 0;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << "was XPE_OK: the cancel was reported as a success";
    const std::vector<Alert> alerts = Alerts();
    ASSERT_EQ(1u, alerts.size());
    EXPECT_EQ(XPE_ALERT_WARNING, alerts[0].severity);
    EXPECT_NE(std::string::npos, alerts[0].text.find("cancelled")) << alerts[0].text;
    EXPECT_NE(std::string::npos, alerts[0].text.find("may have received")) << "the text admits the transfer ran to its end";
    xpe_clear_alerts();
}

TEST(DicomFailurePaths, ACancelDuringAQueryIsReportedAsACancel) {
    Peer peer;
    ASSERT_NE(0, peer.port);
    xpe_clear_alerts();
    peer.runner.scp().findDelayMs = 2000;
    XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
    char out[4096] = {};
    std::thread sender([&] { rc = xpe_dicom_cfind_mwl("localhost", peer.port, "TESTSCU", R"({"Modality":"DX"})", out, sizeof(out), 10000); });
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    xpe_dicom_cancel();
    sender.join();
    peer.runner.scp().findDelayMs = 0;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc);
    const std::vector<Alert> alerts = Alerts();
    ASSERT_EQ(1u, alerts.size());
    EXPECT_NE(std::string::npos, alerts[0].text.find("C-FIND cancelled")) << alerts[0].text;
    xpe_clear_alerts();
}

TEST(DicomFailurePaths, ACancelThatArrivesWhileNothingRunsIsForgottenByTheNextCall) {
    // REQ-DICOM-039 cancels an IN-PROGRESS operation; a cancel with nothing running must not poison the next call.
    Peer peer;
    ASSERT_NE(0, peer.port);
    const TempDir t("c7_idle_cancel");
    const fs::path file = t.path / "to_send.dcm";
    ASSERT_TRUE(WriteWithTheModule(file, 100.0f));
    xpe_dicom_cancel();
    xpe_dicom_cancel();
    EXPECT_EQ(XPE_OK, xpe_dicom_cstore("localhost", peer.port, "TESTSCU", file.string().c_str(), 5000));
}
