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
#include "test_pid.h"
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "mock_scp.hpp"

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
    static std::string s_scpStartError;
    static fs::path s_scpDir;
    static xpe_test::MockScpRunner s_scp;
};

fs::path DicomNetworkTest::s_testDcm;
fs::path DicomNetworkTest::s_tempDir;
uint16_t DicomNetworkTest::s_storePort = 11112;
uint16_t DicomNetworkTest::s_findPort = 11113;
bool DicomNetworkTest::s_serverAvailable = false;
std::string DicomNetworkTest::s_scpStartError;
fs::path DicomNetworkTest::s_scpDir;
xpe_test::MockScpRunner DicomNetworkTest::s_scp;

// Reasons the mock SCP does not let these two cases run. Both are observations
// from QA-B-29, not assumptions:
//
//  * C-FIND: the MWL context is registered in the SCP profile (confirmed with
//    dumpPresentationContexts: abstract syntax 1.2.840.10008.5.1.4.31 with
//    Explicit LE / Implicit LE / J2K), yet the SCU reports "DIMSE No valid
//    Presentation Context ID" from sendFINDRequest. Deriving from DcmSCP rather
//    than DcmStorageSCP, and setAlwaysAcceptDefaultRole(OFTrue), did not change
//    it. C-STORE over the same listener negotiates and completes, so the
//    listener itself works -- the gap is specific to MWL negotiation.
//
//  * Cancel (QA-B-200 M2a): against this loopback SCP a C-STORE finishes in about
//    a millisecond, so a cancel issued 100 ms later always arrives after
//    completion. The mock peer can now be told to wait (MockScp::storeDelayMs),
//    which makes the transfer slow enough for the cancel to arrive during it --
//    CancelCStore_TerminatesOperation uses that, and test_dicom_failure_paths.cpp
//    holds the alert text and the cancel of a query.

void DicomNetworkTest::SetUpTestSuite() {
    s_tempDir = fs::temp_directory_path() / ("xpe_dicom_network_test" + xpe_test::pid_suffix());
    fs::create_directories(s_tempDir);

    // Create test DICOM file
    XpeImageBuffer img{};
    xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img);
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    s_testDcm = s_tempDir / "test_cstore.dcm";
    xpe_dicom_write(s_testDcm.string().c_str(), &img, &meta);
    xpe_free_image(&img);

    // #120 (QA-B-29): an in-process SCP is started here and stopped in
    // TearDownTestSuite, so the association tests below actually run. The
    // listener is owned by this fixture -- see mock_scp.hpp for why that is
    // the shape the lane rules permit. Both C-STORE and C-FIND go to the same
    // listener, so one port serves both.
    s_scpDir = s_tempDir / "scp_incoming";
    fs::create_directories(s_scpDir);

    const uint16_t port = s_scp.start("XPEMOCKSCP", s_scpDir.string());
    if (port == 0) {
        // No port could be bound. Tests below skip with that as the stated
        // reason -- it is an observed failure, not an assumption.
        s_serverAvailable = false;
        s_scpStartError = s_scp.listenError().empty()
            ? std::string("could not bind a loopback port for the mock SCP")
            : s_scp.listenError();
        return;
    }
    s_storePort = port;
    s_findPort  = port;
    s_serverAvailable = true;
}

void DicomNetworkTest::TearDownTestSuite() {
    // The listener must be gone before the suite ends. A failure to join is
    // reported, never swallowed: a surviving thread is a defect.
    if (s_serverAvailable) {
        EXPECT_TRUE(s_scp.stop()) << "mock SCP thread did not join within the timeout";
    }
    fs::remove_all(s_tempDir);
}

// ---------------------------------------------------------------------------
// AC-06: C-STORE success with mock PACS
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CStoreSuccess_ReturnsOK) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
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
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
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
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
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
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;

    s_scp.scp().storeDelayMs = 1500;   // a peer that is slow in the middle of the transfer
    XpeErrorCode result = XPE_OK;
    std::thread storeThread([&]() {
        result = xpe_dicom_cstore("localhost", s_storePort, "TESTSCU",
                                   s_testDcm.string().c_str(), 10000);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    xpe_dicom_cancel();
    storeThread.join();
    s_scp.scp().storeDelayMs = 0;
    xpe_clear_alerts();

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

// ---------------------------------------------------------------------------
// #120 (QA-B-27): SCU branches that run before any association, so they need
// no mock PACS (see #124 for why none is started).
//   - a non-DICOM file fails DcmFileFormat::loadFile  (DicomNetworkSCU.cpp:53-54)
//   - a cancel latched before the call returns early  (DicomNetworkSCU.cpp:101-102)
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CStoreNonDicomFile_ReturnsIoFailed) {
    auto notDicom = s_tempDir / "not_dicom_for_cstore.bin";
    {
        std::ofstream f(notDicom, std::ios::binary);
        f.write("NOT A DICOM FILE", 16);
    }
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_cstore(
        "localhost", 19999, "TESTSCU",
        notDicom.string().c_str(), 500));
}

TEST_F(DicomNetworkTest, CStoreMissingFile_ReturnsIoFailed) {
    auto missing = s_tempDir / "does_not_exist.dcm";
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_cstore(
        "localhost", 19999, "TESTSCU",
        missing.string().c_str(), 500));
}

// ---------------------------------------------------------------------------
// #120 (QA-B-34, item 4): SCU branches the 7th coverage dispatch
// (34486857040) still shows uncovered. Only the reachable ones are here; the
// classification of the rest -- dead code, cancel races, fault-injection-only
// handlers -- is in .moai/reports/lane-post/QA-B-34/_scu_classification.txt.
// ---------------------------------------------------------------------------

// QA-B-210 C11 (#251, user decision): C-STORE proposes ONE transfer syntax -- the one in the file's meta header (0002,0010) --
// and does not convert. It used to offer Explicit LE, JPEG 2000 Lossless and Implicit LE whatever the file held, so a peer could
// accept a syntax the file is not in. The proposal is read from the association request the mock peer received.
TEST_F(DicomNetworkTest, CStoreProposesOnlyTheTransferSyntaxOfAnExplicitLittleEndianFile) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(s_testDcm.string().c_str()).good());
    OFString fileTs;
    ASSERT_TRUE(ff.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, fileTs).good());
    ASSERT_EQ(std::string(UID_LittleEndianExplicitTransferSyntax), std::string(fileTs.c_str())) << "precondition: the fixture is Explicit LE";

    const int before = s_scp.scp().associationRequests.load();
    ASSERT_EQ(XPE_OK, xpe_dicom_cstore("localhost", s_storePort, "TESTSCU", s_testDcm.string().c_str(), 5000));
    ASSERT_EQ(before + 1, s_scp.scp().associationRequests.load());
    const auto proposed = s_scp.scp().lastProposals();
    ASSERT_EQ(1u, proposed.size()) << "one presentation context";
    ASSERT_EQ(1u, proposed[0].transferSyntaxes.size()) << "one transfer syntax in it";
    EXPECT_EQ(std::string(UID_LittleEndianExplicitTransferSyntax), proposed[0].transferSyntaxes[0]);
}

TEST_F(DicomNetworkTest, CStoreProposesOnlyJpeg2000LosslessForAJpeg2000File) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img));
    XpeImageMetadata meta{};
    const auto j2k = s_tempDir / "test_cstore_j2k.dcm";
    const int wrc = xpe_dicom_write_j2k(j2k.string().c_str(), &img, &meta);
    xpe_free_image(&img);
    ASSERT_EQ(XPE_OK, wrc);

    const int before = s_scp.scp().associationRequests.load();
    ASSERT_EQ(XPE_OK, xpe_dicom_cstore("localhost", s_storePort, "TESTSCU", j2k.string().c_str(), 5000));
    ASSERT_EQ(before + 1, s_scp.scp().associationRequests.load());
    const auto proposed = s_scp.scp().lastProposals();
    ASSERT_EQ(1u, proposed.size());
    ASSERT_EQ(1u, proposed[0].transferSyntaxes.size()) << "the J2K file does not also offer Explicit / Implicit LE";
    EXPECT_EQ(std::string(UID_JPEG2000LosslessOnlyTransferSyntax), proposed[0].transferSyntaxes[0]);
}

// The peer does not support the file's syntax (the mock accepts Explicit LE, Implicit LE and JPEG 2000 Lossless; the file here is a
// real Explicit VR BIG Endian file, so DCMTK loads it): the context is refused, the call is NETWORK_FAILED, no C-STORE is attempted
// -- and nothing is converted to a syntax the peer would take.
TEST_F(DicomNetworkTest, CStoreOfAFileInASyntaxThePeerRefusesIsNetworkFailedAndNothingIsSent) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    const auto odd = s_tempDir / "test_cstore_big_endian.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_testDcm.string().c_str()).good());
        ASSERT_TRUE(ff.saveFile(odd.string().c_str(), EXS_BigEndianExplicit).good());   // meta (0002,0010) becomes .2
    }
    {
        DcmFileFormat chk;
        ASSERT_TRUE(chk.loadFile(odd.string().c_str()).good());
        OFString ts;
        ASSERT_TRUE(chk.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, ts).good());
        ASSERT_EQ(std::string(UID_BigEndianExplicitTransferSyntax), std::string(ts.c_str())) << "precondition: the fixture is Explicit VR Big Endian";
    }
    const int storesBefore = s_scp.scp().storeRequests.load();
    const int assocBefore = s_scp.scp().associationRequests.load();
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, xpe_dicom_cstore("localhost", s_storePort, "TESTSCU", odd.string().c_str(), 5000));
    EXPECT_EQ(assocBefore + 1, s_scp.scp().associationRequests.load()) << "the association was requested";
    const auto proposed = s_scp.scp().lastProposals();
    ASSERT_EQ(1u, proposed.size());
    ASSERT_EQ(1u, proposed[0].transferSyntaxes.size());
    EXPECT_EQ(std::string(UID_BigEndianExplicitTransferSyntax), proposed[0].transferSyntaxes[0]);
    EXPECT_EQ(storesBefore, s_scp.scp().storeRequests.load()) << "no C-STORE was attempted";
}

// A file with no Part 10 meta header and no SOPClassUID/SOPInstanceUID in the
// dataset drives all three UID fallbacks in cstore (DicomNetworkSCU.cpp:69, 72,
// 76). Those run before initNetwork, so no peer is needed -- the call is aimed
// at a dead port and the network failure afterwards is the expected outcome.
TEST_F(DicomNetworkTest, CStoreFileWithoutSopUids_FallsBackToDxDefault) {
    auto path = s_tempDir / "cstore_no_sop_uids.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_testDcm.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ASSERT_NE(nullptr, ds);
        ds->findAndDeleteElement(DCM_SOPClassUID);
        ds->findAndDeleteElement(DCM_SOPInstanceUID);
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dataset).good());
    }
    // Nothing listens on 19999; reaching the connection attempt at all means
    // the UID fallbacks above it ran.
    EXPECT_EQ(XPE_ERR_NETWORK_FAILED, xpe_dicom_cstore(
        "localhost", 19999, "TESTSCU", path.string().c_str(), 500));
}

// "CALLED_AE@host" splits into called AE title + hostname
// (DicomNetworkSCU.cpp:300-302). Sent to the real listener so the parse is
// shown to produce a usable association, not just to execute the branch.
TEST_F(DicomNetworkTest, CStoreCalledAeInHost_NegotiatesAndStores) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    const int before = s_scp.scp().storeRequests.load();
    EXPECT_EQ(XPE_OK, xpe_dicom_cstore(
        ("XPEMOCKSCP@localhost"), s_storePort, "TESTSCU",
        s_testDcm.string().c_str(), 5000));
    EXPECT_GT(s_scp.scp().storeRequests.load(), before)
        << "the called-AE form must reach the SCP, not just parse";
}

// A query body that is not JSON fails in buildFindRequest
// (DicomNetworkSCU.cpp:337-339) after the association is already up, so the
// caller sees the release-and-fail path (:213-214).
// KnownDivergence_ (QA-B-43): this records what the current implementation
// does, not what any requirement asks for. No SPEC, api-spec or header
// sentence states this value; if the implementation changed it, that would
// be a change, not a defect. The prefix keeps the distinction visible in the
// ctest listing, where a reader sees only the name.
// Specifically: PROCESSING_FAILED here comes from buildFindRequest failing
// deep inside cfindMwl. INVALID_INPUT would be a defensible answer too --
// nothing decides between them, which is exactly the point.
TEST_F(DicomNetworkTest, KnownDivergence_CFindMalformedQueryJson_ReturnsProcessingFailed) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    char outJson[256] = {};
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        "{not json",
        outJson, sizeof(outJson), 5000));
}

// PatientName and AccessionNumber are the two query keys no existing case
// sends (DicomNetworkSCU.cpp:328, 334). The mock matches on PatientID only, so
// the full worklist comes back -- the assertion is that the call succeeds with
// these keys present, not that they filter.
TEST_F(DicomNetworkTest, CFindQueryWithNameAndAccession_ReturnsJsonArray) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    char outJson[4096] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        R"({"PatientName":"MOCK^WORKLIST","AccessionNumber":"ACC-0001"})",
        outJson, sizeof(outJson), 5000));
    auto j = json::parse(outJson);
    EXPECT_TRUE(j.is_array());
    EXPECT_EQ(3u, j.size());
}

// QA-B-209 C8 (#251): the SHAPE of the query. PS3.4 K.6.1 (Modality Worklist Information Model): the scheduling keys --
// Modality (0008,0060), Scheduled Station AE Title (0040,0001), Scheduled Procedure Step Start Date (0040,0002) -- live
// INSIDE the Scheduled Procedure Step Sequence (0040,0100), one item; the patient keys and Accession Number are top level.
// The mock answers with a canned worklist whatever it is asked, so the captured identifier is what proves the shape.
TEST_F(DicomNetworkTest, CFindQueryShape_SchedulingKeysAreInsideTheScheduledProcedureStepSequence) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    char outJson[4096] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        R"({"PatientID":"MOCK-0001","AccessionNumber":"ACC-0001","Modality":"DX","ScheduledStationAETitle":"STATION1","ScheduledProcedureStepStartDate":"20260101-20261231"})",
        outJson, sizeof(outJson), 5000));
    DcmDataset q = s_scp.scp().lastFindQuery();

    OFString v;
    EXPECT_TRUE(q.findAndGetOFString(DCM_PatientID, v).good());
    EXPECT_EQ("MOCK-0001", std::string(v.c_str()));
    EXPECT_TRUE(q.findAndGetOFString(DCM_AccessionNumber, v).good());
    EXPECT_EQ("ACC-0001", std::string(v.c_str())) << "AccessionNumber stays a top-level key (leader decision on #251)";
    EXPECT_FALSE(q.tagExists(DCM_Modality)) << "Modality is a Scheduled Procedure Step attribute: not a top-level key";

    DcmItem* sps = nullptr;
    ASSERT_TRUE(q.findAndGetSequenceItem(DCM_ScheduledProcedureStepSequence, sps, 0).good())
        << "no Scheduled Procedure Step Sequence (0040,0100) in the C-FIND identifier";
    ASSERT_NE(nullptr, sps);
    DcmSequenceOfItems* seq = nullptr;
    ASSERT_TRUE(q.findAndGetElement(DCM_ScheduledProcedureStepSequence, reinterpret_cast<DcmElement*&>(seq)).good());
    EXPECT_EQ(1u, seq->card()) << "exactly one sequence item";
    EXPECT_TRUE(sps->findAndGetOFString(DCM_Modality, v).good());
    EXPECT_EQ("DX", std::string(v.c_str()));
    EXPECT_TRUE(sps->findAndGetOFString(DCM_ScheduledStationAETitle, v).good());
    EXPECT_EQ("STATION1", std::string(v.c_str()));
    EXPECT_TRUE(sps->findAndGetOFString(DCM_ScheduledProcedureStepStartDate, v).good());
    EXPECT_EQ("20260101-20261231", std::string(v.c_str())) << "a DICOM date RANGE is passed as given";
}

// Keys the caller does not give are universal-match (empty) keys, inside the sequence item for the scheduling ones.
TEST_F(DicomNetworkTest, CFindQueryShape_AbsentSchedulingKeysAreUniversalMatchInsideTheItem) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    char outJson[4096] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_cfind_mwl("localhost", s_findPort, "TESTSCU", "{}", outJson, sizeof(outJson), 5000));
    DcmDataset q = s_scp.scp().lastFindQuery();
    EXPECT_FALSE(q.tagExists(DCM_Modality));
    DcmItem* sps = nullptr;
    ASSERT_TRUE(q.findAndGetSequenceItem(DCM_ScheduledProcedureStepSequence, sps, 0).good());
    ASSERT_NE(nullptr, sps);
    OFString v;
    EXPECT_TRUE(sps->tagExists(DCM_Modality));
    EXPECT_TRUE(sps->tagExists(DCM_ScheduledStationAETitle));
    EXPECT_TRUE(sps->tagExists(DCM_ScheduledProcedureStepStartDate));
    EXPECT_TRUE(sps->findAndGetOFString(DCM_Modality, v).good() ? v.empty() : true);
}

// The serialized worklist does not fit the caller's buffer
// (DicomNetworkSCU.cpp:281). Three DX entries are far larger than 8 bytes.
TEST_F(DicomNetworkTest, CFindOutBufferTooSmall_ReturnsBufferTooSmall) {
    if (!s_serverAvailable) GTEST_SKIP() << "mock SCP unavailable: " << s_scpStartError;
    char outJson[8] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_dicom_cfind_mwl(
        "localhost", s_findPort, "TESTSCU",
        R"({"Modality":"DX"})",
        outJson, sizeof(outJson), 5000));
}

// ---------------------------------------------------------------------------
// #142 (QA-B-42): the output-buffer contract on the C-FIND result buffer.
// A declared length of 0 means the argument does not exist -- INVALID_INPUT,
// not BUFFER_TOO_SMALL. Checked before the association is attempted, so a
// caller with a broken buffer does not cost a network round trip.
// ---------------------------------------------------------------------------
TEST_F(DicomNetworkTest, CFindOutBufferZeroLength_ReturnsInvalidInput) {
    char outJson[64] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_cfind_mwl(
        "localhost", 19998, "TESTSCU",
        R"({"Modality":"DX"})",
        outJson, 0, 500));
}
