/**
 * @file DicomNetworkSCU.cpp
 * @brief DICOM C-STORE and C-FIND MWL SCU implementation (SWU-4.4).
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-029..040
 */
#include "DicomNetworkSCU.h"

#include <dcmtk/dcmnet/dimse.h>
#include <dcmtk/dcmnet/diutil.h>
#include <dcmtk/dcmnet/scu.h>
#include <dcmtk/dcmdata/dctk.h>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <cstring>

namespace xpe {
namespace dicom {

// @MX:WARN: [AUTO] Global cancellation flag accessed from multiple threads
// @MX:REASON: xpe_dicom_cancel() is called from a different thread than the network operation (REQ-DICOM-040)
// @MX:SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-040
std::atomic<bool> DicomNetworkSCU::s_cancelRequested{false};

// Well-known SOP classes for DX storage
static const char* DX_SOP_CLASS_UIDS[] = {
    "1.2.840.10008.5.1.4.1.1.1.1",   // Digital X-Ray Image for Presentation
    "1.2.840.10008.5.1.4.1.1.1.1.1", // Digital X-Ray Image for Processing
    "1.2.840.10008.5.1.4.31",         // Modality Worklist Information Model - FIND
};

// @MX:ANCHOR: [AUTO] DicomNetworkSCU::cstore — C-STORE SCU exported API
// @MX:REASON: fan_in >= 3: xpe_dicom_cstore, cancel, test infrastructure; shared s_cancelRequested
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.4 REQ-DICOM-029..033
XpeErrorCode DicomNetworkSCU::cstore(const char* host,
                                      uint16_t port,
                                      const char* aet,
                                      const char* filePath,
                                      uint32_t timeoutMs) {
    spdlog::debug("[DicomNetworkSCU] cstore -> {}:{} file={}", host ? host : "(null)", port, filePath ? filePath : "(null)");
    if (!host || !aet || !filePath) return XPE_ERR_INVALID_INPUT;

    s_cancelRequested.store(false);

    // Parse host for "CALLED_AE@hostname" format
    std::string resolvedHost, calledAet;
    parseHostAet(std::string(host), resolvedHost, calledAet);

    // Load and validate the DICOM file
    DcmFileFormat dcmff;
    OFCondition loadStatus = dcmff.loadFile(filePath, EXS_Unknown, EGL_noChange, DCM_MaxReadLength);
    if (loadStatus.bad()) {
        spdlog::warn("[DicomNetworkSCU] cstore: file load failed: {}", loadStatus.text());
        return XPE_ERR_IO_FAILED;
    }

    DcmDataset* ds = dcmff.getDataset();
    if (!ds) {
        return XPE_ERR_IO_FAILED;
    }

    // Get SOP class and instance UIDs from the file
    OFString sopClassUID, sopInstanceUID;
    if (dcmff.getMetaInfo()) {
        dcmff.getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPClassUID, sopClassUID);
        dcmff.getMetaInfo()->findAndGetOFString(DCM_MediaStorageSOPInstanceUID, sopInstanceUID);
    }
    if (sopClassUID.empty()) {
        ds->findAndGetOFString(DCM_SOPClassUID, sopClassUID);
    }
    if (sopInstanceUID.empty()) {
        ds->findAndGetOFString(DCM_SOPInstanceUID, sopInstanceUID);
    }

    if (sopClassUID.empty()) {
        sopClassUID = "1.2.840.10008.5.1.4.1.1.1.1"; // DX default
    }

    // Build DcmSCU for C-STORE
    DcmSCU scu;
    scu.setAETitle(aet);
    scu.setPeerHostName(resolvedHost.c_str());
    scu.setPeerPort(port);
    scu.setPeerAETitle(calledAet.c_str());

    if (timeoutMs > 0) {
        scu.setConnectionTimeout(static_cast<Sint32>(timeoutMs / 1000));
        scu.setACSETimeout(static_cast<Sint32>(timeoutMs / 1000));
        scu.setDIMSETimeout(static_cast<Sint32>(timeoutMs / 1000));
    }

    // Add presentation context for the SOP class
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(OFString(UID_LittleEndianExplicitTransferSyntax));
    transferSyntaxes.push_back(OFString(UID_JPEG2000LosslessOnlyTransferSyntax));
    transferSyntaxes.push_back(OFString(UID_LittleEndianImplicitTransferSyntax));

    scu.addPresentationContext(sopClassUID, transferSyntaxes);

    // Check for cancel before attempting connection
    if (s_cancelRequested.load()) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Initialize network and negotiate association
    OFCondition cond = scu.initNetwork();
    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] initNetwork failed: {}", cond.text());
        return XPE_ERR_NETWORK_FAILED;
    }

    cond = scu.negotiateAssociation();
    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] negotiateAssociation failed: {}", cond.text());
        return XPE_ERR_NETWORK_FAILED;
    }

    // Check for cancel after association
    if (s_cancelRequested.load()) {
        scu.releaseAssociation();
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Send the DICOM file via C-STORE
    Uint16 rspStatus = 0;

    cond = scu.sendSTORERequest(
        0,                         // presentation context ID (0 = auto-select)
        OFFilename(filePath),      // DICOM file path
        nullptr,                   // override dataset (nullptr = use file)
        rspStatus
    );

    scu.releaseAssociation();

    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] sendSTORERequest failed: {}", cond.text());
        return XPE_ERR_NETWORK_FAILED;
    }

    // Check C-STORE response status (0x0000 = Success)
    if (rspStatus != STATUS_Success) {
        spdlog::warn("[DicomNetworkSCU] C-STORE response status: 0x{:04X}", rspStatus);
        return XPE_ERR_NETWORK_FAILED;
    }

    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] DicomNetworkSCU::cfindMwl — C-FIND MWL SCU exported API
// @MX:REASON: fan_in >= 3: xpe_dicom_cfind_mwl, cancel, test infrastructure
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.4 REQ-DICOM-034..038
XpeErrorCode DicomNetworkSCU::cfindMwl(const char* host,
                                        uint16_t port,
                                        const char* aet,
                                        const char* queryJson,
                                        char* outJson,
                                        uint32_t outBufLen,
                                        uint32_t timeoutMs) {
    spdlog::debug("[DicomNetworkSCU] cfindMwl -> {}:{}", host ? host : "(null)", port);
    if (!host || !aet || !queryJson || !outJson) return XPE_ERR_INVALID_INPUT;

    s_cancelRequested.store(false);

    // Parse host for "CALLED_AE@hostname" format
    std::string resolvedHost, calledAet;
    parseHostAet(std::string(host), resolvedHost, calledAet);

    // Build DcmSCU for C-FIND
    DcmSCU scu;
    scu.setAETitle(aet);
    scu.setPeerHostName(resolvedHost.c_str());
    scu.setPeerPort(port);
    scu.setPeerAETitle(calledAet.c_str());

    if (timeoutMs > 0) {
        scu.setConnectionTimeout(static_cast<Sint32>(timeoutMs / 1000));
        scu.setACSETimeout(static_cast<Sint32>(timeoutMs / 1000));
        scu.setDIMSETimeout(static_cast<Sint32>(timeoutMs / 1000));
    }

    // Add Modality Worklist presentation context
    OFList<OFString> transferSyntaxes;
    transferSyntaxes.push_back(OFString(UID_LittleEndianExplicitTransferSyntax));
    transferSyntaxes.push_back(OFString(UID_LittleEndianImplicitTransferSyntax));

    scu.addPresentationContext(
        OFString(UID_FINDModalityWorklistInformationModel),
        transferSyntaxes
    );

    // Check for cancel before attempting connection
    if (s_cancelRequested.load()) {
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Initialize network and negotiate association
    OFCondition cond = scu.initNetwork();
    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] cfindMwl initNetwork failed: {}", cond.text());
        return XPE_ERR_NETWORK_FAILED;
    }

    cond = scu.negotiateAssociation();
    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] cfindMwl negotiateAssociation failed: {}", cond.text());
        return XPE_ERR_NETWORK_FAILED;
    }

    // Build the C-FIND request dataset from JSON
    DcmDataset requestDS;
    if (!buildFindRequest(std::string(queryJson), &requestDS)) {
        scu.releaseAssociation();
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Check for cancel after association
    if (s_cancelRequested.load()) {
        scu.releaseAssociation();
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Send C-FIND request and collect responses
    OFList<QRResponse*> responses;

    cond = scu.sendFINDRequest(0, &requestDS, &responses);
    scu.releaseAssociation();

    if (cond.bad()) {
        spdlog::warn("[DicomNetworkSCU] sendFINDRequest failed: {}", cond.text());
        // Clean up responses
        for (auto* r : responses) delete r;
        return XPE_ERR_NETWORK_FAILED;
    }

    // Serialize responses to JSON
    nlohmann::json resultArray = nlohmann::json::array();
    for (auto* r : responses) {
        if (r && r->m_dataset) {
            nlohmann::json entry = nlohmann::json::object();

            // Extract common MWL fields
            auto extractString = [&](DcmTagKey key, const char* jsonKey) {
                OFString val;
                if (r->m_dataset->findAndGetOFString(key, val).good()) {
                    entry[jsonKey] = std::string(val.c_str());
                }
            };

            extractString(DCM_PatientID,          "PatientID");
            extractString(DCM_PatientName,         "PatientName");
            extractString(DCM_StudyInstanceUID,    "StudyInstanceUID");
            extractString(DCM_AccessionNumber,     "AccessionNumber");
            extractString(DCM_Modality,            "Modality");

            resultArray.push_back(entry);
        }
        delete r;
    }

    // Write result to output buffer
    std::string resultStr = resultArray.dump();
    if (static_cast<uint32_t>(resultStr.size() + 1) > outBufLen) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    std::strncpy(outJson, resultStr.c_str(), outBufLen - 1);
    outJson[outBufLen - 1] = '\0';

    return XPE_OK;
}

void DicomNetworkSCU::cancel() {
    spdlog::debug("[DicomNetworkSCU] cancel requested");
    s_cancelRequested.store(true);
}

void DicomNetworkSCU::parseHostAet(const std::string& host,
                                    std::string& outHost,
                                    std::string& outCalledAet) {
    auto pos = host.find('@');
    if (pos != std::string::npos) {
        outCalledAet = host.substr(0, pos);
        outHost = host.substr(pos + 1);
    } else {
        outHost = host;
        outCalledAet = "ANY-SCP";
    }
}

bool DicomNetworkSCU::buildFindRequest(const std::string& queryJson, void* outDataset) {
    DcmDataset* ds = static_cast<DcmDataset*>(outDataset);
    if (!ds) return false;

    // Add Scheduled Procedure Step Sequence as required by MWL
    // Start with universal match (empty) keys for all standard fields
    ds->putAndInsertString(DCM_PatientID, "");
    ds->putAndInsertString(DCM_PatientName, "");
    ds->putAndInsertString(DCM_StudyInstanceUID, "");
    ds->putAndInsertString(DCM_AccessionNumber, "");
    ds->putAndInsertString(DCM_Modality, "");

    // Parse the JSON query and override defaults
    try {
        auto j = nlohmann::json::parse(queryJson);

        if (j.contains("PatientID")) {
            ds->putAndInsertString(DCM_PatientID, j["PatientID"].get<std::string>().c_str());
        }
        if (j.contains("PatientName")) {
            ds->putAndInsertString(DCM_PatientName, j["PatientName"].get<std::string>().c_str());
        }
        if (j.contains("Modality")) {
            ds->putAndInsertString(DCM_Modality, j["Modality"].get<std::string>().c_str());
        }
        if (j.contains("AccessionNumber")) {
            ds->putAndInsertString(DCM_AccessionNumber, j["AccessionNumber"].get<std::string>().c_str());
        }
    } catch (const std::exception& e) {
        spdlog::warn("[DicomNetworkSCU] buildFindRequest: JSON parse error: {}", e.what());
        return false;
    }

    return true;
}

std::string DicomNetworkSCU::responseToJson(void* responseList) {
    if (!responseList) return "[]";

    OFList<QRResponse*>* list = static_cast<OFList<QRResponse*>*>(responseList);
    nlohmann::json resultArray = nlohmann::json::array();

    for (const auto* r : *list) {
        if (r && r->m_dataset) {
            nlohmann::json entry = nlohmann::json::object();
            OFString val;
            if (r->m_dataset->findAndGetOFString(DCM_PatientID, val).good()) {
                entry["PatientID"] = std::string(val.c_str());
            }
            if (r->m_dataset->findAndGetOFString(DCM_PatientName, val).good()) {
                entry["PatientName"] = std::string(val.c_str());
            }
            if (r->m_dataset->findAndGetOFString(DCM_Modality, val).good()) {
                entry["Modality"] = std::string(val.c_str());
            }
            resultArray.push_back(entry);
        }
    }

    return resultArray.dump();
}

} // namespace dicom
} // namespace xpe
