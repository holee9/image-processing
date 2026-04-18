/**
 * @file DicomValidator.h
 * @brief Internal C++ class for DICOM DX IOD conformance validation (SWU-4.3).
 *
 * Checks required Type 1 tags, UID format, preamble, and pixel consistency.
 * Produces a JSON report string.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-023..028
 */
#pragma once

#include <string>
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

namespace xpe {
namespace dicom {

// @MX:NOTE: [AUTO] Validator is stateless — all methods static; no thread-safety concern
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.3
class DicomValidator {
public:
    /**
     * Validate filePath for DX IOD conformance.
     * Writes null-terminated JSON report into outBuf (size bufLen).
     * REQ-DICOM-023..028
     */
    static XpeErrorCode validate(const char* filePath,
                                  char* outBuf,
                                  uint32_t bufLen);

private:
    struct ValidationResult {
        bool valid{true};
        std::string errorsJson;    // JSON array fragment
        std::string warningsJson;  // JSON array fragment
    };

    static ValidationResult checkConformance(const std::string& filePath);
    static bool isValidUID(const std::string& uid);
    static std::string buildReport(const ValidationResult& result);
};

} // namespace dicom
} // namespace xpe
