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
    OFBool stopAfterCurrentAssociation() override { return m_stopRequested ? OFTrue : OFFalse; }
    OFBool stopAfterConnectionTimeout()  override { return m_stopRequested ? OFTrue : OFFalse; }

    OFCondition handleIncomingCommand(T_DIMSE_Message* incomingMsg,
                                      const DcmPresentationContextInfo& presInfo) override
    {
        if (incomingMsg != nullptr && incomingMsg->CommandField == DIMSE_C_FIND_RQ) {
            ++findRequests;
            T_DIMSE_C_FindRQ& req = incomingMsg->msg.CFindRQ;

            // Drain the query identifiers; the SCU always sends one.
            // receiveDIMSEDataset wants a mutable context id, so copy it out.
            T_ASC_PresentationContextID presID = presInfo.presentationContextID;
            DcmDataset* query = nullptr;
            OFCondition rc = receiveDIMSEDataset(&presID, &query);
            delete query;
            if (rc.bad()) {
                return rc;
            }

            // One fixed worklist item, then the terminating success response.
            DcmDataset item;
            item.putAndInsertString(DCM_PatientName, "MOCK^WORKLIST");
            item.putAndInsertString(DCM_PatientID, "MOCK-0001");
            item.putAndInsertString(DCM_Modality, "DX");
            item.putAndInsertString(DCM_AccessionNumber, "ACC-0001");

            rc = sendFINDResponse(presID, req.MessageID,
                                  req.AffectedSOPClassUID, &item, STATUS_Pending);
            if (rc.bad()) {
                return rc;
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
