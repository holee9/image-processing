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
 *     integer string, rounded half up) beside (0018,1153) Exposure in uAs (an integer string, mAs * 1000 rounded half up,
 *     which keeps three decimals) -- both Type 3 attributes of the DX X-Ray Acquisition Dose module -- and NOT (0018,9332),
 *     which that module does not list. It reads (0018,1153), else (0018,9332), else (0018,1152). The files used to prove the
 *     read are made by DCMTK directly, not by the module, so a writer and a reader that share a mistake cannot agree.
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

    // The file another system would have written: only (0018,1152) Exposure, IS = "100".
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_ExposureInuAs);
    ds->findAndDeleteElement(DCM_ExposureInmAs);
    ASSERT_TRUE(ds->putAndInsertString(DCM_Exposure, "100").good());
    const fs::path theirs = t.path / "other_system.dcm";
    ASSERT_TRUE(ff.saveFile(theirs.string().c_str()).good());

    EXPECT_NEAR(100.0f, ReadMas(theirs), 0.01f) << "was 0: only (0018,9332) was read";
}

TEST(DicomExposureTag, TheReadOrderIs1153Then9332Then1152) {
    const TempDir t("c1_order");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 2.5f));   // (0018,1152) "3" and (0018,1153) "2500"
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    OFString v1152, v1153;
    ASSERT_TRUE(ds->findAndGetOFString(DCM_Exposure, v1152).good());
    ASSERT_TRUE(ds->findAndGetOFString(DCM_ExposureInuAs, v1153).good());
    ASSERT_STREQ("3", v1152.c_str());
    ASSERT_STREQ("2500", v1153.c_str());

    auto save = [&](const char* name) {
        const fs::path f = t.path / name;
        EXPECT_TRUE(ff.saveFile(f.string().c_str()).good());
        return f;
    };
    // all three present and disagreeing: (0018,1153) wins
    ASSERT_TRUE(ds->putAndInsertString(DCM_Exposure, "999").good());
    ASSERT_TRUE(ds->putAndInsertString(DCM_ExposureInmAs, "888").good());
    EXPECT_NEAR(2.5f, ReadMas(save("all_three.dcm")), 1e-4f) << "(0018,1153) uAs is the most precise: it wins";
    // (0018,1153) gone: (0018,9332) wins over (0018,1152)
    ds->findAndDeleteElement(DCM_ExposureInuAs);
    EXPECT_NEAR(888.0f, ReadMas(save("no_1153.dcm")), 1e-3f) << "then (0018,9332) Exposure in mAs";
    // only (0018,1152) left
    ds->findAndDeleteElement(DCM_ExposureInmAs);
    EXPECT_NEAR(999.0f, ReadMas(save("only_1152.dcm")), 1e-3f) << "then (0018,1152) Exposure";
}

TEST(DicomExposureTag, AFileThatOnlyHas9332StillReadsAsThatValue) {
    // The files this module wrote before M2a2 carry (0018,9332) and, since M2a, (0018,1152); older ones only (0018,9332).
    const TempDir t("c1_old");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 50.0f));
    DcmFileFormat ff;
    ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_Exposure);
    ds->findAndDeleteElement(DCM_ExposureInuAs);
    ASSERT_TRUE(ds->putAndInsertString(DCM_ExposureInmAs, "12.5000").good());
    const fs::path f = t.path / "old.dcm";
    ASSERT_TRUE(ff.saveFile(f.string().c_str()).good());
    EXPECT_NEAR(12.5f, ReadMas(f), 1e-4f);
}

TEST(DicomExposureTag, TheModuleWritesExposure1152AndExposureInMicroAsAndNot9332) {
    struct Row {
        float mAs;
        const char* expect1152;   // IS, mAs rounded half up; nullptr = the attribute is not written
        const char* expect1153;   // IS, mAs * 1000 rounded half up; nullptr = not written
        float readBack;           // what the module reads back from its own file
    };
    const Row rows[] = {{100.0f, "100", "100000", 100.0f}, {2.5f, "3", "2500", 2.5f},      {2.4f, "2", "2400", 2.4f},
                        {0.4f, "0", "400", 0.4f},          {0.5f, "1", "500", 0.5f},        {0.0004f, "0", "0", 0.0f},
                        {3000.0f, "3000", "3000000", 3000.0f},
                        {3.0e6f, "3000000", nullptr, 3.0e6f}, {3.0e9f, nullptr, nullptr, 0.0f}};
    for (const Row& r : rows) {
        const TempDir t("c1_write");
        const fs::path f = t.path / "w.dcm";
        ASSERT_TRUE(WriteWithTheModule(f, r.mAs)) << r.mAs;
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(f.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        EXPECT_FALSE(ds->tagExists(DCM_ExposureInmAs)) << "(0018,9332) is not in the DX X-Ray Acquisition Dose module: mAs " << r.mAs;
        struct Want {
            DcmTagKey key;
            const char* expect;
            const char* name;
        } wants[] = {{DCM_Exposure, r.expect1152, "(0018,1152)"}, {DCM_ExposureInuAs, r.expect1153, "(0018,1153)"}};
        for (const Want& w : wants) {
            OFString v;
            const bool has = ds->findAndGetOFString(w.key, v).good();
            if (w.expect == nullptr) {
                EXPECT_FALSE(has) << w.name << " does not fit an IS and is left out: mAs " << r.mAs;
            } else {
                ASSERT_TRUE(has) << w.name << ": mAs " << r.mAs;
                EXPECT_STREQ(w.expect, v.c_str()) << w.name << ": mAs " << r.mAs;
                DcmElement* e = nullptr;
                ASSERT_TRUE(ds->findAndGetElement(w.key, e).good());
                EXPECT_STREQ("IS", DcmVR(e->getVR()).getValidVRName()) << w.name << " is an Integer String";
            }
        }
        EXPECT_NEAR(r.readBack, ReadMas(f), 5e-4f) << "mAs " << r.mAs << ": the module's own round trip";
    }
}

TEST(DicomExposureTag, ANonNumericOrNegativeAttributeIsSkippedAndTheNextOneIsUsed) {
    const TempDir t("c1_garbage");
    const fs::path mine = t.path / "module_written.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 50.0f));
    struct Case {
        const char* v1153;   // nullptr = absent
        const char* v1152;
        float expect;
    };
    const Case cases[] = {{nullptr, "abc", 0.0f}, {nullptr, "-7", 0.0f}, {nullptr, "", 0.0f},
                          {"abc", "7", 7.0f},     {"-7000", "7", 7.0f},  {"", "7", 7.0f}};
    for (const Case& c : cases) {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(mine.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ds->findAndDeleteElement(DCM_ExposureInmAs);
        ds->findAndDeleteElement(DCM_ExposureInuAs);
        ds->putAndInsertString(DCM_Exposure, c.v1152);
        if (c.v1153) ds->putAndInsertString(DCM_ExposureInuAs, c.v1153);
        const fs::path f = t.path / "bad.dcm";
        ASSERT_TRUE(ff.saveFile(f.string().c_str()).good());
        EXPECT_NEAR(c.expect, ReadMas(f), 1e-4f) << "(0018,1153) '" << (c.v1153 ? c.v1153 : "(absent)") << "', (0018,1152) '"
                                                  << c.v1152 << "'";
    }
}

/** A copy of a module-written file whose three exposure attributes are replaced: nullptr leaves the attribute out. */
fs::path WithExposureAttributes(const TempDir& t, const char* name, const char* v1153, const char* v9332, const char* v1152) {
    const fs::path mine = t.path / (std::string(name) + "_base.dcm");
    EXPECT_TRUE(WriteWithTheModule(mine, 50.0f));
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(mine.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_ExposureInuAs);
    ds->findAndDeleteElement(DCM_ExposureInmAs);
    ds->findAndDeleteElement(DCM_Exposure);
    if (v1153) EXPECT_TRUE(ds->putAndInsertString(DCM_ExposureInuAs, v1153).good());
    if (v9332) EXPECT_TRUE(ds->putAndInsertString(DCM_ExposureInmAs, v9332).good());
    if (v1152) EXPECT_TRUE(ds->putAndInsertString(DCM_Exposure, v1152).good());
    const fs::path out = t.path / (std::string(name) + ".dcm");
    EXPECT_TRUE(ff.saveFile(out.string().c_str()).good());
    return out;
}

std::vector<Alert> DisagreementAlerts() {
    std::vector<Alert> found;
    for (const Alert& a : Alerts())
        if (a.text.find("exposure attributes disagree") != std::string::npos) found.push_back(a);
    return found;
}

TEST(DicomExposureTag, AnIntegerStringMustMatchTheGrammarCompletelyAndNotJustByItsPrefix) {
    // PS3.5 Table 6.2-1, IS: optional sign, decimal digits, leading/trailing spaces allowed, no embedded spaces,
    // at most 12 bytes, -2^31..2^31-1. strtod accepted a prefix ("2500junk" as 2500) and decimals/hex/exponents.
    const TempDir t("c1_is");
    // (0018,1153) holds the junk, (0018,1152) holds 7: a junk 1153 is skipped and 1152 is used.
    const char* junk[] = {"2500junk", "7.5", "0x10", "1e3", "12 34", "+", "-", "1234567890123", "0000000000007", "2147483648", "25\\00", "abc"};
    for (const char* v : junk) {
        const fs::path f = WithExposureAttributes(t, "junk1153", v, nullptr, "7");
        EXPECT_NEAR(7.0f, ReadMas(f), 1e-4f) << "(0018,1153) '" << v << "' is not an IS: it must be skipped";
    }
    // the same junk in (0018,1152), nothing else in the file: no mAs
    for (const char* v : junk) {
        const fs::path f = WithExposureAttributes(t, "junk1152", nullptr, nullptr, v);
        EXPECT_EQ(0.0f, ReadMas(f)) << "(0018,1152) '" << v << "' is not an IS: no value was accepted";
    }
    // valid spellings: padding and an explicit plus sign
    struct Row {
        const char* v1153;
        float expect;
    } valid[] = {{" 2500 ", 2.5f}, {"+2500", 2.5f}, {"2500", 2.5f}, {"0", 0.0f}, {"  100000", 100.0f}};
    for (const Row& r : valid) {
        const fs::path f = WithExposureAttributes(t, "valid1153", r.v1153, nullptr, nullptr);
        EXPECT_NEAR(r.expect, ReadMas(f), 1e-4f) << "(0018,1153) '" << r.v1153 << "' is a valid IS";
    }
}

TEST(DicomExposureTag, ThreeExposureAttributesThatAgreeWithinTheirRoundingPostNoAlertAndThatDisagreePostOne) {
    const TempDir t("c1_alert");
    xpe_clear_alerts();
    // control: what the module writes for 2.5 mAs is "3" in (0018,1152) and "2500" in (0018,1153) -- they agree
    const fs::path mine = t.path / "mine.dcm";
    ASSERT_TRUE(WriteWithTheModule(mine, 2.5f));
    EXPECT_NEAR(2.5f, ReadMas(mine), 1e-4f);
    EXPECT_EQ(0u, DisagreementAlerts().size()) << "rounding alone is not a disagreement";
    // agreement inside the tolerances: 9332 2.5 with 1152 "3" or "2" (rounded to 1 mAs: up to 0.5 apart)
    for (const char* v1152 : {"3", "2"}) {
        xpe_clear_alerts();
        const fs::path f = WithExposureAttributes(t, "agree", "2500", "2.5000", v1152);
        EXPECT_NEAR(2.5f, ReadMas(f), 1e-4f);
        EXPECT_EQ(0u, DisagreementAlerts().size()) << "1152 '" << v1152 << "' is within 0.5 mAs of 2.5";
    }

    // conflict: (0018,1152) says 999 mAs while (0018,1153) says 2500 uAs = 2.5 mAs
    xpe_clear_alerts();
    const fs::path conflict = WithExposureAttributes(t, "conflict", "2500", "2.5000", "999");
    EXPECT_NEAR(2.5f, ReadMas(conflict), 1e-4f) << "the priority is unchanged: (0018,1153) wins";
    std::vector<Alert> alerts = DisagreementAlerts();
    ASSERT_EQ(1u, alerts.size()) << "exactly one alert per read";
    EXPECT_EQ(XPE_ALERT_WARNING, alerts[0].severity);
    for (const char* part : {"(0018,1153) = 2500 uAs", "(0018,9332) = 2.5 mAs", "(0018,1152) = 999 mAs", "using (0018,1153) = 2.5000 mAs"})
        EXPECT_NE(std::string::npos, alerts[0].text.find(part)) << part << " | " << alerts[0].text;

    // conflict without (0018,1153): (0018,9332) against (0018,1152), 9332 wins
    xpe_clear_alerts();
    const fs::path conflict2 = WithExposureAttributes(t, "conflict2", nullptr, "12.5", "80");
    EXPECT_NEAR(12.5f, ReadMas(conflict2), 1e-4f);
    alerts = DisagreementAlerts();
    ASSERT_EQ(1u, alerts.size());
    EXPECT_NE(std::string::npos, alerts[0].text.find("using (0018,9332) = 12.5000 mAs")) << alerts[0].text;
    EXPECT_EQ(std::string::npos, alerts[0].text.find("(0018,1153)")) << "an attribute the file does not have is not named";

    // a single attribute can disagree with nothing; an invalid attribute is not a value to disagree with
    xpe_clear_alerts();
    EXPECT_NEAR(7.0f, ReadMas(WithExposureAttributes(t, "single", nullptr, nullptr, "7")), 1e-4f);
    EXPECT_NEAR(7.0f, ReadMas(WithExposureAttributes(t, "invalid_other", "junk", nullptr, "7")), 1e-4f);
    EXPECT_EQ(0u, DisagreementAlerts().size());
    xpe_clear_alerts();
}

TEST(DicomModuleVersion, TheModuleExportsAVersionStringLikeTheOtherModules) {
    // REQ-P0-033: every module exports a version function; xpe_display_version, xpe_enhance_basic_version and the
    // others return a non-empty "major.minor.patch" string whose lifetime is the process.
    const char* v = xpe_dicom_version();
    ASSERT_NE(nullptr, v);
    EXPECT_GT(std::strlen(v), 0u);
    int major = -1, minor = -1, patch = -1;
    char tail = 0;
    EXPECT_EQ(3, std::sscanf(v, "%d.%d.%d%c", &major, &minor, &patch, &tail)) << "major.minor.patch and nothing after it: '" << v << "'";
    EXPECT_EQ(v, xpe_dicom_version()) << "the same pointer each call: the lifetime is the process";
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
