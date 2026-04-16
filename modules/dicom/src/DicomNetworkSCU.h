/**
 * @file DicomNetworkSCU.h
 * @brief Internal C++ class for DICOM C-STORE and C-FIND MWL SCU operations (SWU-4.4).
 *
 * Wraps DCMTK storescu and wlmscuscu functionality with cancellation support.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-029..040
 */
#pragma once

#include <string>
#include <atomic>
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

namespace xpe {
namespace dicom {

// @MX:WARN: [AUTO] Global cancellation flag accessed from multiple threads
// @MX:REASON: xpe_dicom_cancel() is called from a different thread than the network operation (REQ-DICOM-040)
// @MX:SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-040

// @MX:ANCHOR: [AUTO] Public API boundary — cstore, cfind_mwl, cancel
// @MX:REASON: Three exported API functions share cancellation state; singleton network context
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.4
class DicomNetworkSCU {
public:
    /**
     * Send a DICOM file via C-STORE.
     * REQ-DICOM-029..033
     */
    static XpeErrorCode cstore(const char* host,
                                uint16_t port,
                                const char* aet,
                                const char* filePath,
                                uint32_t timeoutMs);

    /**
     * C-FIND Modality Worklist query.
     * REQ-DICOM-034..038
     */
    static XpeErrorCode cfindMwl(const char* host,
                                  uint16_t port,
                                  const char* aet,
                                  const char* queryJson,
                                  char* outJson,
                                  uint32_t outBufLen,
                                  uint32_t timeoutMs);

    /**
     * Signal cancellation to in-progress network operation.
     * Thread-safe. REQ-DICOM-039..040
     */
    static void cancel();

private:
    // Thread-safe cancellation flag
    static std::atomic<bool> s_cancelRequested;

    /** Parse "CALLED_AE@hostname" format or default to "ANY-SCP". */
    static void parseHostAet(const std::string& host,
                              std::string& outHost,
                              std::string& outCalledAet);

    /** Convert queryJson to DCMTK C-FIND request dataset. */
    static bool buildFindRequest(const std::string& queryJson, void* outDataset);

    /** Serialize C-FIND response dataset list to JSON array string. */
    static std::string responseToJson(void* responseList);
};

} // namespace dicom
} // namespace xpe
