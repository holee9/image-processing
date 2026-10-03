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

// Attributes the DX IOD requires, with the Type the standard gives them (QA-B-206 M2b, Codex #111).
//
//   Type 1  the attribute must be present AND carry a value (a zero-length or all-padding value is an error);
//   Type 2  the attribute must be present, and an empty value is allowed (PS3.5: "zero length if unknown").
//
// Patient's Name and Patient ID are Type 2 (PS3.3 Table C.7-1, Patient Module, included in the DX IOD by A.26.3), so an
// anonymized file that keeps both elements with no value is conformant. Every other entry is Type 1 in its module: Study
// Instance UID (C.7.2.1), Series Instance UID (C.7.3.1), SOP Instance UID (C.12.1), Modality (DX Series, C.8.11.1), and Rows,
// Columns, Bits Allocated, Bits Stored (the Image Pixel Description Macro, Table C.7-11c). Pixel Data is Type 1C in the
// Image Pixel module ("Required if Pixel Data Provider URL (0028,7FE0) is not present"): it is required unless the file's
// transfer syntax is JPIP Referenced and it gives a URL with a value, and then it is a warning; Pixel Data and the URL together
// are an error (QA-B-206 M2c, M2d). The table with the clauses is in the QA-B-206 M2b report.
struct RequiredTag {
    DcmTagKey   key;
    const char* tag;
    bool        needsValue;   // true: Type 1 (and Type 1C where required), false: Type 2
};
static const RequiredTag s_requiredTags[] = {
    { DCM_PatientName,      "0010,0010", false },   // Type 2
    { DCM_PatientID,        "0010,0020", false },   // Type 2
    { DCM_StudyInstanceUID, "0020,000D", true  },
    { DCM_SeriesInstanceUID,"0020,000E", true  },
    { DCM_SOPInstanceUID,   "0008,0018", true  },
    { DCM_Modality,         "0008,0060", true  },
    { DCM_Rows,             "0028,0010", true  },
    { DCM_Columns,          "0028,0011", true  },
    { DCM_BitsAllocated,    "0028,0100", true  },
    { DCM_BitsStored,       "0028,0101", true  },
    { DCM_PixelData,        "7FE0,0010", true  },   // Type 1C: required when there is no Pixel Data Provider URL
};

// A string with nothing in it but the padding DICOM adds to reach even length.
static bool isAllPadding(const OFString& text) {
    for (size_t k = 0; k < text.length(); ++k) {
        if (text[k] != ' ' && text[k] != '\0') return false;
    }
    return true;
}

// The element is there and carries a value (not zero length, not only padding).
static bool elementHasValue(DcmDataset* ds, const DcmTagKey& key) {
    DcmElement* e = nullptr;
    if (ds->findAndGetElement(key, e).bad() || !e) return false;
    if (e->getLength(ds->getOriginalXfer(), EET_ExplicitLength) == 0) return false;
    OFString text;
    if (e->getOFString(text, 0).good()) return !isAllPadding(text);
    return true;
}

// #142 (QA-B-42): report the required size through the caller's buffer, but
// only when it can actually hold the 4-byte value. The unguarded memcpy this
// replaces wrote past any buffer shorter than a uint32_t -- observed for a
// 2-byte buffer in QA-B-42's RED run. A buffer too short even for the report
// still returns BUFFER_TOO_SMALL; the caller simply learns no size from it.
static void report_required_size(char* outBuf, uint32_t bufLen, size_t reportSize) {
    if (bufLen < sizeof(uint32_t)) return;
    const uint32_t required = static_cast<uint32_t>(reportSize + 1);
    std::memcpy(outBuf, &required, sizeof(uint32_t));
}

XpeErrorCode DicomValidator::validate(const char* filePath,
                                       char* outBuf,
                                       uint32_t bufLen) {
    spdlog::debug("[DicomValidator] validate: {}", filePath ? filePath : "(null)");
    if (!filePath || !outBuf) return XPE_ERR_INVALID_INPUT;
    // #142 (QA-B-42): a declared length of 0 means the output argument does not
    // exist. That is INVALID_INPUT, not BUFFER_TOO_SMALL -- "too small" is for a
    // buffer that is real but short. Checked first so the two cannot overlap.
    if (bufLen == 0u) return XPE_ERR_INVALID_INPUT;

    // Try to parse the file — if it fails completely, it's not a DICOM
    ValidationResult result;

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
            report_required_size(outBuf, bufLen, report.size());
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
            report_required_size(outBuf, bufLen, report.size());
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
            report_required_size(outBuf, bufLen, report.size());
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        std::strncpy(outBuf, report.c_str(), bufLen - 1);
        outBuf[bufLen - 1] = '\0';
        return XPE_ERR_DICOM_INVALID;
    }

    // Check all required Type 1 tags
    nlohmann::json errors = nlohmann::json::array();
    nlohmann::json warnings = nlohmann::json::array();

    auto addError = [&](const char* tag, const std::string& message) {
        result.valid = false;
        nlohmann::json errEntry;
        errEntry["tag"] = tag;
        errEntry["message"] = message;
        errors.push_back(errEntry);
    };

    // #139: the Part 10 file meta information group (0002) is required, and its
    // content must agree with the dataset.
    //
    // The null guard at :78 does not catch a missing meta header: DCMTK
    // synthesises a DcmMetaInfo object during loadFile even for a dataset-only
    // file, so getMetaInfo() is non-null there (observed, QA-B-34). What
    // distinguishes the two is that the synthesised object carries no elements,
    // so card() is the observation this check keys on -- not the pointer.
    if (meta->card() == 0) {
        addError("0002,0000",
                 "Missing DICOM Part 10 file meta information group (0002)");
    } else {
        auto metaString = [&](const DcmTagKey& key) {
            OFString value;
            return meta->findAndGetOFString(key, value).good()
                 ? std::string(value.c_str()) : std::string();
        };
        auto datasetString = [&](const DcmTagKey& key) {
            OFString value;
            return ds->findAndGetOFString(key, value).good()
                 ? std::string(value.c_str()) : std::string();
        };

        const std::string metaTs = metaString(DCM_TransferSyntaxUID);
        if (metaTs.empty()) {
            addError("0002,0010", "File meta information has no TransferSyntaxUID");
        } else if (!isValidUID(metaTs)) {
            addError("0002,0010",
                     "Invalid UID format in file meta TransferSyntaxUID: " + metaTs);
        }

        // Media Storage SOP Class/Instance UID must equal the dataset's own
        // SOPClassUID/SOPInstanceUID (PS3.10). A mismatch means the file
        // describes itself as something its content is not. When the dataset
        // tag is absent the required-Type-1 loop below already reports it, so
        // only the meta side is judged here.
        struct { DcmTagKey metaKey; DcmTagKey dsKey; const char* tag; const char* name; }
        const kPairs[] = {
            { DCM_MediaStorageSOPClassUID,    DCM_SOPClassUID,    "0002,0002", "SOPClassUID"    },
            { DCM_MediaStorageSOPInstanceUID, DCM_SOPInstanceUID, "0002,0003", "SOPInstanceUID" },
        };
        for (const auto& p : kPairs) {
            const std::string metaValue = metaString(p.metaKey);
            if (metaValue.empty()) {
                addError(p.tag, std::string("File meta information has no MediaStorage")
                              + p.name);
                continue;
            }
            const std::string dsValue = datasetString(p.dsKey);
            if (!dsValue.empty() && metaValue != dsValue) {
                addError(p.tag, std::string("File meta MediaStorage") + p.name
                              + " (" + metaValue + ") does not match dataset "
                              + p.name + " (" + dsValue + ")");
            }
        }
    }

    // QA-B-206 M2d (Codex #114): the Pixel Data Provider URL belongs to the JPIP Referenced transfer syntaxes alone (.94 and
    // .95, PS3.5 8.2), so what a URL means depends on the transfer syntax the file declares in its meta group. Empty when the
    // meta group or the element is missing -- that is reported above, and "no JPIP syntax" is the safe reading.
    std::string transferSyntax;
    {
        OFString value;
        if (meta->findAndGetOFString(DCM_TransferSyntaxUID, value).good()) transferSyntax = value.c_str();
    }
    const bool jpipReferenced = transferSyntax == "1.2.840.10008.1.2.4.94" || transferSyntax == "1.2.840.10008.1.2.4.95";

    for (const auto& req : s_requiredTags) {
        DcmElement* elem = nullptr;
        OFCondition findStatus = ds->findAndGetElement(req.key, elem);
        if (findStatus.bad() || !elem) {
            // QA-B-206 M2c/M2d (Codex #113, #114): Pixel Data is Type 1C -- required unless the pixels are given by reference.
            // Only under a JPIP Referenced transfer syntax does a Provider URL stand in for it: such a file has no Pixel
            // Data and is not wrong for that; this module cannot read pixels by reference, so it says so once, as a warning
            // that leaves `valid` alone. The URL has to carry a value to count (an empty one is not a provider). Under any
            // other transfer syntax a URL replaces nothing and the missing Pixel Data is reported as such.
            const bool urlGiven = req.key == DCM_PixelData && elementHasValue(ds, DCM_PixelDataProviderURL);
            if (urlGiven && jpipReferenced) {
                nlohmann::json warnEntry;
                warnEntry["tag"] = req.tag;
                warnEntry["message"] = "Pixel data by reference (Pixel Data Provider URL) is not supported by this module";
                warnings.push_back(warnEntry);
                continue;
            }
            result.valid = false;
            nlohmann::json errEntry;
            errEntry["tag"] = req.tag;
            errEntry["message"] = std::string(req.needsValue ? "Missing required Type 1 tag: " : "Missing required Type 2 tag: ") + req.tag +
                                  (urlGiven ? " (a Pixel Data Provider URL stands in for it only under a JPIP Referenced transfer syntax)" : "");
            errors.push_back(errEntry);
            continue;
        }
        // QA-B-206 M2d: Pixel Data and the Provider URL are mutually exclusive (PS3.5 8.2) whatever the transfer syntax. Judged
        // by the element being there, as the standard words it; the value check below still applies to Pixel Data itself.
        if (req.key == DCM_PixelData && ds->tagExists(DCM_PixelDataProviderURL)) {
            addError("0028,7FE0", "Pixel Data and Pixel Data Provider URL are mutually exclusive (PS3.5 8.2): both are present");
        }
        // QA-B-206 M2b (Codex #111): a Type 2 attribute only has to be there. C5 below judged every required attribute
        // by its value, which made an anonymized file with an empty Patient Name / Patient ID DICOM_INVALID.
        if (!req.needsValue) continue;
        // QA-B-206 C5: REQ-DICOM-024 says Type 1 tags are "present AND non-empty". An element that is there with no value
        // (zero length) or a string that is only padding (spaces / NULs) carries nothing, and a Type 1 attribute must have a
        // value. Pixel Data and the numeric attributes are judged by their length; the strings also by their trimmed value.
        bool hasValue = elem->getLength(ds->getOriginalXfer(), EET_ExplicitLength) != 0;
        if (hasValue) {
            OFString text;
            if (elem->getOFString(text, 0).good()) {
                bool allPadding = true;
                for (size_t k = 0; k < text.length(); ++k) {
                    if (text[k] != ' ' && text[k] != '\0') { allPadding = false; break; }
                }
                hasValue = !allPadding;
            }
        }
        if (!hasValue) {
            result.valid = false;
            nlohmann::json errEntry;
            errEntry["tag"] = req.tag;
            errEntry["message"] = std::string("Required Type 1 tag has no value: ") + req.tag;
            errors.push_back(errEntry);
        }
    }

    // Validate UID formats for known UID tags
    auto checkUID = [&](DcmTagKey key, const char* tagStr) {
        OFString uidVal;
        if (ds->findAndGetOFString(key, uidVal).good()) {
            // QA-B-206 M2c (Codex #113): a UID with no value was already reported by the required-attribute loop as "has no
            // value"; the empty string is not a UID either, so judging its format as well put one defect in the report twice.
            // Only a UID that has a value is judged for format.
            if (isAllPadding(uidVal)) return;
            if (!isValidUID(std::string(uidVal.c_str()))) {
                // QA-B-206 C6: a UID that is not in the dot-separated numeric form is a FAILED check (REQ-DICOM-024 lists
                // "UID format correct" among the conformance criteria), so it is an error and valid is false. It used to
                // be pushed to BOTH arrays, so the same problem read as a failure and as a "non-critical issue"
                // (REQ-DICOM-025 reserves warnings for those). Errors only.
                nlohmann::json errEntry;
                errEntry["tag"] = tagStr;
                errEntry["message"] = std::string("Invalid UID format: ") + uidVal.c_str();
                result.valid = false;
                errors.push_back(errEntry);
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
        report_required_size(outBuf, bufLen, report.size());
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    std::strncpy(outBuf, report.c_str(), bufLen - 1);
    outBuf[bufLen - 1] = '\0';

    return XPE_OK;
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
