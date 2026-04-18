/**
 * @file DicomValidator.cpp
 * @brief DICOM DX IOD conformance validator implementation (SWU-4.3).
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-023..028
 */
#include "DicomValidator.h"

#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <regex>
#include <cstring>

namespace xpe {
namespace dicom {

// @MX:ANCHOR: [AUTO] DicomValidator::validate — exported API boundary for DICOM conformance check
// @MX:REASON: Called by xpe_dicom_validate; stateless; results written to caller buffer
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.3

// Required Type 1 tags for DX IOD (tag, keyword)
static const std::pair<DcmTagKey, const char*> s_requiredTags[] = {
    { DCM_PatientName,      "0010,0010" },
    { DCM_PatientID,        "0010,0020" },
    { DCM_StudyInstanceUID, "0020,000D" },
    { DCM_SeriesInstanceUID,"0020,000E" },
    { DCM_SOPInstanceUID,   "0008,0018" },
    { DCM_Modality,         "0008,0060" },
    { DCM_Rows,             "0028,0010" },
    { DCM_Columns,          "0028,0011" },
    { DCM_BitsAllocated,    "0028,0100" },
    { DCM_BitsStored,       "0028,0101" },
    { DCM_PixelData,        "7FE0,0010" },
};

XpeErrorCode DicomValidator::validate(const char* filePath,
                                       char* outBuf,
                                       uint32_t bufLen) {
    spdlog::debug("[DicomValidator] validate: {}", filePath ? filePath : "(null)");
    if (!filePath || !outBuf) return XPE_ERR_INVALID_INPUT;

    // Try to parse the file — if it fails completely, it's not a DICOM
    ValidationResult result;
    bool parseOk = false;

    DcmFileFormat dcmff;
    OFCondition status = dcmff.loadFile(
        filePath,
        EXS_Unknown,
        EGL_noChange,
        DCM_MaxReadLength
    );

    if (status.bad()) {
        result.valid = false;
        nlohmann::json errEntry;
        errEntry["tag"] = "0008,0000";
        errEntry["message"] = std::string("File cannot be parsed as DICOM: ") + status.text();
        nlohmann::json errors = nlohmann::json::array();
        errors.push_back(errEntry);
        result.errorsJson = errors.dump();

        std::string report = buildReport(result);

        // Even for DICOM_INVALID, write report before returning
        if (bufLen < static_cast<uint32_t>(report.size() + 1)) {
            uint32_t required = static_cast<uint32_t>(report.size() + 1);
            std::memcpy(outBuf, &required, sizeof(uint32_t));
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        std::strncpy(outBuf, report.c_str(), bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return XPE_ERR_DICOM_INVALID;
    }

    // Check for meta-information header (DICOM Part 10 preamble)
    DcmMetaInfo* meta = dcmff.getMetaInfo();
    if (!meta) {
        result.valid = false;
        nlohmann::json errEntry;
        errEntry["tag"] = "0002,0000";
        errEntry["message"] = "Missing DICOM meta information header";
        nlohmann::json errors = nlohmann::json::array();
        errors.push_back(errEntry);
        result.errorsJson = errors.dump();

        std::string report = buildReport(result);
        if (bufLen < static_cast<uint32_t>(report.size() + 1)) {
            uint32_t required = static_cast<uint32_t>(report.size() + 1);
            std::memcpy(outBuf, &required, sizeof(uint32_t));
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        std::strncpy(outBuf, report.c_str(), bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return XPE_ERR_DICOM_INVALID;
    }

    DcmDataset* ds = dcmff.getDataset();
    if (!ds) {
        result.valid = false;
        nlohmann::json errEntry;
        errEntry["tag"] = "0000,0000";
        errEntry["message"] = "No dataset found in DICOM file";
        nlohmann::json errors = nlohmann::json::array();
        errors.push_back(errEntry);
        result.errorsJson = errors.dump();

        std::string report = buildReport(result);
        if (bufLen < static_cast<uint32_t>(report.size() + 1)) {
            uint32_t required = static_cast<uint32_t>(report.size() + 1);
            std::memcpy(outBuf, &required, sizeof(uint32_t));
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        std::strncpy(outBuf, report.c_str(), bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return XPE_ERR_DICOM_INVALID;
    }

    // Check all required Type 1 tags
    nlohmann::json errors = nlohmann::json::array();
    nlohmann::json warnings = nlohmann::json::array();

    for (const auto& tagPair : s_requiredTags) {
        DcmElement* elem = nullptr;
        OFCondition findStatus = ds->findAndGetElement(tagPair.first, elem);
        if (findStatus.bad() || !elem) {
            result.valid = false;
            nlohmann::json errEntry;
            errEntry["tag"] = tagPair.second;
            errEntry["message"] = std::string("Missing required Type 1 tag: ") + tagPair.second;
            errors.push_back(errEntry);
        }
    }

    // Validate UID formats for known UID tags
    auto checkUID = [&](DcmTagKey key, const char* tagStr) {
        OFString uidVal;
        if (ds->findAndGetOFString(key, uidVal).good()) {
            if (!isValidUID(std::string(uidVal.c_str()))) {
                nlohmann::json warnEntry;
                warnEntry["tag"] = tagStr;
                warnEntry["message"] = std::string("Invalid UID format: ") + uidVal.c_str();
                warnings.push_back(warnEntry);
                result.valid = false;
                errors.push_back(warnEntry);
            }
        }
    };

    checkUID(DCM_StudyInstanceUID,  "0020,000D");
    checkUID(DCM_SeriesInstanceUID, "0020,000E");
    checkUID(DCM_SOPInstanceUID,    "0008,0018");

    result.errorsJson   = errors.dump();
    result.warningsJson = warnings.dump();

    std::string report = buildReport(result);

    // Check buffer size
    if (bufLen < static_cast<uint32_t>(report.size() + 1)) {
        uint32_t required = static_cast<uint32_t>(report.size() + 1);
        std::memcpy(outBuf, &required, sizeof(uint32_t));
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    std::strncpy(outBuf, report.c_str(), bufLen - 1);
    outBuf[bufLen - 1] = '\0';

    return XPE_OK;
}

DicomValidator::ValidationResult DicomValidator::checkConformance(const std::string& filePath) {
    ValidationResult result;

    DcmFileFormat dcmff;
    OFCondition status = dcmff.loadFile(filePath.c_str(), EXS_Unknown, EGL_noChange, DCM_MaxReadLength);
    if (status.bad()) {
        result.valid = false;
        nlohmann::json errors = nlohmann::json::array();
        nlohmann::json err;
        err["tag"] = "0000,0000";
        err["message"] = std::string("Parse error: ") + status.text();
        errors.push_back(err);
        result.errorsJson = errors.dump();
        return result;
    }

    DcmDataset* ds = dcmff.getDataset();
    if (!ds) {
        result.valid = false;
        return result;
    }

    nlohmann::json errors = nlohmann::json::array();
    for (const auto& tagPair : s_requiredTags) {
        DcmElement* elem = nullptr;
        if (ds->findAndGetElement(tagPair.first, elem).bad() || !elem) {
            result.valid = false;
            nlohmann::json err;
            err["tag"] = tagPair.second;
            err["message"] = std::string("Missing tag: ") + tagPair.second;
            errors.push_back(err);
        }
    }
    result.errorsJson = errors.dump();
    return result;
}

bool DicomValidator::isValidUID(const std::string& uid) {
    // DICOM UID: dot-separated numeric components, max 64 chars
    if (uid.empty() || uid.size() > 64) return false;
    static const std::regex uidRegex(R"(^[0-9]+(\.[0-9]+)*$)");
    return std::regex_match(uid, uidRegex);
}

std::string DicomValidator::buildReport(const ValidationResult& result) {
    nlohmann::json report;
    report["valid"] = result.valid;

    if (result.errorsJson.empty()) {
        report["errors"] = nlohmann::json::array();
    } else {
        try {
            report["errors"] = nlohmann::json::parse(result.errorsJson);
        } catch (...) {
            report["errors"] = nlohmann::json::array();
        }
    }

    if (result.warningsJson.empty()) {
        report["warnings"] = nlohmann::json::array();
    } else {
        try {
            report["warnings"] = nlohmann::json::parse(result.warningsJson);
        } catch (...) {
            report["warnings"] = nlohmann::json::array();
        }
    }

    return report.dump();
}

} // namespace dicom
} // namespace xpe
