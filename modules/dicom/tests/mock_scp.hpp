/**
 * @file mock_scp.hpp
 * @brief In-process DICOM SCP for SCU association tests (#120 #124, QA-B-29).
 *
 * Why this exists
 * ---------------
 * `test_dicom_network_scu.cpp` had four cases that never ran: they need a peer
 * that accepts an association, and none was started (#124 recorded that the
 * skip message claimed a probe that never happened). Everything the SCU does
 * after `negotiateAssociation()` was therefore unexecuted.
 *
 * Lifetime contract (the reason this is allowed)
 * ----------------------------------------------
 * The lane rule against background load targets processes whose cleanup is not
 * guaranteed. This listener is bound to the gtest fixture: `start()` is called
 * from `SetUpTestSuite` and `stop()` from `TearDownTestSuite`, and `stop()`
 * joins the thread. It is not a detached thread, not a separate executable, and
 * it does not use a fixed port. If the thread does not join within the timeout,
 * `stop()` reports failure to the caller rather than returning quietly -- a
 * listener that outlives the suite is a defect, not an acceptable cost.
 */
#ifndef XPE_DICOM_TESTS_MOCK_SCP_HPP
#define XPE_DICOM_TESTS_MOCK_SCP_HPP

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmnet/scp.h>
#include <dcmtk/dcmdata/dctk.h>

#include <atomic>
#include <memory>
#include <chrono>
#include <string>
#include <thread>

namespace xpe_test {

/**
 * @brief Minimal SCP answering C-STORE and Modality Worklist C-FIND.
 *
 * Derives from DcmSCP rather than DcmStorageSCP on purpose: DcmStorageSCP only
 * negotiates storage SOP classes, so a C-FIND against it fails on the SCU side
 * with "DIMSE No valid Presentation Context ID" even though the MWL context is
 * registered in the profile (observed, QA-B-29). DcmSCP negotiates whatever the
 * profile carries, so both services work from one listener.
 */
class MockScp : public DcmSCP {
public:
    /// Association negotiation trace (#124, QA-B-30). Each association appends
    /// what the SCU proposed and what negotiation left accepted, so a C-STORE
    /// association and a C-FIND association can be compared side by side.
    std::string negotiationLog;

    /// Requests answered; readable by tests as a liveness check.
    std::atomic<int> findRequests{0};
    std::atomic<int> storeRequests{0};

    /**
     * @brief Ask the listen loop to exit.
     *
     * DcmSCP does not expose a stop *command*: stopAfterCurrentAssociation()
     * and stopAfterConnectionTimeout() are protected predicates the base class
     * polls to decide whether to keep going. So the stop request is a flag here
     * and the predicates below report it.
     */
    void requestStop() { m_stopRequested = true; }

protected:
    /// Record what the SCU proposed, before this SCP decides anything.
    void notifyAssociationRequest(const T_ASC_Parameters& params,
                                  DcmSCPActionType& desiredAction) override;

    /// Record what survived negotiation (accepted vs refused, with reasons).
    OFCondition negotiateAssociation() override;

    OFBool stopAfterCurrentAssociation() override { return m_stopRequested ? OFTrue : OFFalse; }
    OFBool stopAfterConnectionTimeout()  override { return m_stopRequested ? OFTrue : OFFalse; }

    OFCondition handleIncomingCommand(T_DIMSE_Message* incomingMsg,
                                      const DcmPresentationContextInfo& presInfo) override
    {
        // QA-B-30: record every command that actually reaches a handler, with
        // the context it arrived on. A service whose association was refused
        // never appears here -- absence is the signal.
        if (incomingMsg != nullptr) {
            negotiationLog += "handleIncomingCommand: CommandField=0x"
                            + to_hex(incomingMsg->CommandField)
                            + " presID=" + std::to_string(presInfo.presentationContextID)
                            + " as=" + presInfo.abstractSyntax.c_str()
                            + " ts=" + presInfo.acceptedTransferSyntax.c_str() + "\n";
        }

        if (incomingMsg != nullptr && incomingMsg->CommandField == DIMSE_C_FIND_RQ) {
            ++findRequests;
            T_DIMSE_C_FindRQ& req = incomingMsg->msg.CFindRQ;

            // Drain the query identifiers; the SCU always sends one.
            // receiveDIMSEDataset wants a mutable context id, so copy it out.
            T_ASC_PresentationContextID presID = presInfo.presentationContextID;
            DcmDataset* query = nullptr;
            OFCondition rc = receiveDIMSEDataset(&presID, &query);
            const std::unique_ptr<DcmDataset> queryOwned(query);
            DcmDataset* const queryCopy = queryOwned.get();
            if (rc.bad()) {
                return rc;
            }

            // A worklist the tests can distinguish: three DX entries for a
            // general query, none when the query names a patient that is not
            // on it. Matching is deliberately minimal -- only PatientID is
            // honoured, which is what the empty-result case needs.
            OFString queryPatientId;
            if (queryCopy != nullptr) {
                queryCopy->findAndGetOFString(DCM_PatientID, queryPatientId);
            }
            const bool patientIdRequested =
                !queryPatientId.empty() && queryPatientId != "*";
            const bool patientIdOnWorklist =
                patientIdRequested && (queryPatientId == "MOCK-0001" ||
                                       queryPatientId == "MOCK-0002" ||
                                       queryPatientId == "MOCK-0003");

            if (!patientIdRequested || patientIdOnWorklist) {
                static const char* const kIds[]   = {"MOCK-0001", "MOCK-0002", "MOCK-0003"};
                static const char* const kNames[] = {"MOCK^WORKLIST", "MOCK^SECOND", "MOCK^THIRD"};
                static const char* const kAcc[]   = {"ACC-0001", "ACC-0002", "ACC-0003"};

                for (int n = 0; n < 3; ++n) {
                    if (patientIdRequested && queryPatientId != kIds[n]) {
                        continue;
                    }
                    DcmDataset item;
                    item.putAndInsertString(DCM_PatientName, kNames[n]);
                    item.putAndInsertString(DCM_PatientID, kIds[n]);
                    item.putAndInsertString(DCM_Modality, "DX");
                    item.putAndInsertString(DCM_AccessionNumber, kAcc[n]);

                    rc = sendFINDResponse(presID, req.MessageID,
                                          req.AffectedSOPClassUID, &item, STATUS_Pending);
                    if (rc.bad()) {
                        return rc;
                    }
                }
            }

            return sendFINDResponse(presID, req.MessageID,
                                    req.AffectedSOPClassUID, nullptr,
                                    STATUS_FIND_Success_MatchingIsComplete);
        }

        if (incomingMsg != nullptr && incomingMsg->CommandField == DIMSE_C_STORE_RQ) {
            ++storeRequests;
            T_DIMSE_C_StoreRQ& req = incomingMsg->msg.CStoreRQ;
            DcmDataset* received = nullptr;
            const OFCondition rc =
                handleSTORERequest(req, presInfo.presentationContextID, received);
            delete received;   // the object itself is not inspected by these tests
            return rc;
        }

        return DcmSCP::handleIncomingCommand(incomingMsg, presInfo);
    }

private:
    static std::string to_hex(unsigned v) {
        static const char* d = "0123456789abcdef";
        std::string out(4, '0');
        for (int i = 3; i >= 0; --i) { out[i] = d[v & 0xF]; v >>= 4; }
        return out;
    }

    std::atomic<bool> m_stopRequested{false};
};

/**
 * @brief Owns the listener thread and guarantees it is joined.
 */
class MockScpRunner {
public:
    /**
     * @brief Bind and start listening.
     * @return the port actually in use, or 0 if the listener could not start.
     *
     * The port is chosen by probing rather than hardcoded: an ephemeral socket
     * is bound, its number read back, then released. The window between release
     * and the SCP's own bind is a known race (§ Residual-risk in the report);
     * it is preferred over a fixed port, which collides across parallel runs.
     */
    uint16_t start(const std::string& aeTitle, const std::string& storageDir);

    /**
     * @brief Stop listening and join the thread.
     * @return true if the thread joined within the timeout.
     */
    bool stop(std::chrono::milliseconds timeout = std::chrono::seconds(10));

    MockScp& scp() { return m_scp; }

    /// Text of the listen() failure, empty if listen() has not failed.
    const std::string& listenError() const { return m_listenError; }

    /// Diagnostic dump of the registered presentation contexts.
    const std::string& diag() const { return m_diag; }

    ~MockScpRunner() {
        // Last-resort: never leave the thread running past the object.
        if (m_thread.joinable()) {
            m_scp.requestStop();
            m_thread.join();
        }
    }

private:
    MockScp                m_scp;
    std::thread            m_thread;
    std::atomic<bool>      m_running{false};
    std::string            m_listenError;
    std::string            m_diag;
};

}  // namespace xpe_test

#endif  // XPE_DICOM_TESTS_MOCK_SCP_HPP
