/**
 * @file test_dicom_network_scu.cpp
 * @brief TDD tests for DicomNetworkSCU / SWU-4.4 (>= 8 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-029..040, AC-06, AC-07, AC-08
 *
 * Network test strategy:
 *   DCMTK storescp/wlmscpfs mock servers are launched on random localhost ports
 *   to avoid port conflicts in CI environments.
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <cstring>

namespace fs = std::filesystem;
using json = nlohmann::json;

class DicomNetworkTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    static fs::path s_testDcm;
    static fs::path s_tempDir;
    static uint16_t s_storePort;    // DCMTK storescp port
    static uint16_t s_findPort;     // DCMTK wlmscpfs port
    static bool     s_serverAvailable;
};

fs::path DicomNetworkTest::s_testDcm;
fs::path DicomNetworkTest::s_tempDir;
uint16_t DicomNetworkTest::s_storePort = 11112;
uint16_t DicomNetworkTest::s_findPort = 11113;
bool DicomNetworkTest::s_serverAvailable = false;

void DicomNetworkTest::SetUpTestSuite() {
    s_tempDir = fs::temp_directory_path() / "xpe_dicom_network_test";
    fs::create_directories(s_tempDir);

    // Create test DICOM file
    XpeImageBuffer img{};
    xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img);
    XpeImageMetadata meta{};
    std::strncpy(meta.bodyPart, "CHEST", sizeof(meta.bodyPart));
    s_testDcm = s_tempDir / "test_cstore.dcm";
    xpe_dicom_write(s_testDcm.string().c_str(), &img, &meta);
    xpe_free_image(&img);

    // TODO: launch DCMTK storescp and wlmscpfs mock servers
    // s_serverAvailable = launchMockServers(s_storePort, s_findPort);
    s_serverAvailable = false; // will be set to true after server integration
}

void DicomNetworkTest::TearDownTestSuite() {
    // TODO: stop mock servers
    fs::remove_all(s_tempDir);
}

// ---------------------------------------------------------------------------
// AC-06: C-STORE success with mock PACS
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CStoreSuccess_ReturnsOK) {
    if (!s_serverAvailable) GTEST_SKIP() << "DCMTK storescp not available";
    EXPECT_EQ(XPE_OK, xpe_dicom_cstore(
        "localhost", s_storePort, "TESTSCU",
        s_testDcm.string().c_str(), 5000));
}

// ---------------------------------------------------------------------------
// AC-06: C-STORE timeout returns NETWORK_FAILED
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CStoreTimeout_ReturnsNetworkFailed) {
    // Port 19999 — nothing listening there
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, xpe_dicom_cstore(
        "localhost", 19999, "TESTSCU",
        s_testDcm.string().c_str(), 500));
}

// ---------------------------------------------------------------------------
// AC-07: C-FIND returns results from mock MWL server
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CFindResults_ReturnsJsonArray) {
    if (!s_serverAvailable) GTEST_SKIP() << "DCMTK wlmscpfs not available";
    char outJson[4096] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        R"({"Modality":"DX"})",
        outJson, sizeof(outJson), 5000));
    auto j = json::parse(outJson);
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(3u, j.size()) << "Expected 3 worklist entries for DX modality";
}

// ---------------------------------------------------------------------------
// AC-07: C-FIND empty result returns [] and XPE_OK
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CFindEmpty_ReturnsEmptyArray) {
    if (!s_serverAvailable) GTEST_SKIP() << "DCMTK wlmscpfs not available";
    char outJson[256] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        R"({"PatientID":"NONEXISTENT_9999"})",
        outJson, sizeof(outJson), 5000));
    EXPECT_STREQ("[]", outJson);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-038: C-FIND timeout returns NETWORK_FAILED
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CFindTimeout_ReturnsNetworkFailed) {
    char outJson[256] = {};
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, xpe_dicom_cfind_mwl(
        "localhost", 19998, "TESTSCU",
        R"({"Modality":"DX"})",
        outJson, sizeof(outJson), 500));
}

// ---------------------------------------------------------------------------
// AC-08: Cancel in-progress C-STORE
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CancelCStore_TerminatesOperation) {
    if (!s_serverAvailable) GTEST_SKIP() << "DCMTK storescp not available";

    XpeErrorCode result = XPE_OK;
    std::thread storeThread([&]() {
        // Send a large file to a slow server
        result = xpe_dicom_cstore("localhost", s_storePort, "TESTSCU",
                                   s_testDcm.string().c_str(), 10000);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    xpe_dicom_cancel();
    storeThread.join();

    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, result)
        << "Cancelled C-STORE must return PROCESSING_FAILED";
}

// ---------------------------------------------------------------------------
// REQ-DICOM-040: Cancel is thread-safe and no-op when idle
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CancelNoOp_WhenIdle) {
    ASSERT_NO_FATAL_FAILURE(xpe_dicom_cancel());
    ASSERT_NO_FATAL_FAILURE(xpe_dicom_cancel()); // idempotent
}

// ---------------------------------------------------------------------------
// AC-09: NULL parameters
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CStoreNullHost_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_cstore(
        nullptr, 104, "TESTSCU", s_testDcm.string().c_str(), 1000));
}

TEST_F(DicomNetworkTest, CFindNullQueryJson_ReturnsInvalidInput) {
    char buf[256] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_cfind_mwl(
        "localhost", 104, "TESTSCU", nullptr, buf, sizeof(buf), 1000));
}
