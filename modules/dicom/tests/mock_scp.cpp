/**
 * @file mock_scp.cpp
 * @brief Implementation of the fixture-owned in-process SCP (#120 #124, QA-B-29).
 */
#include "mock_scp.hpp"

#include <sstream>

#include <dcmtk/dcmnet/dcasccff.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace xpe_test {
namespace {

/**
 * @brief Ask the OS for a free TCP port by binding port 0 and reading it back.
 * @return the port, or 0 on failure.
 *
 * DcmSCP wants a concrete port number, so the probe-then-release shape is used
 * rather than handing it a listening socket.
 */
uint16_t probe_free_port()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 0;
    }
#endif
    uint16_t port = 0;
    int sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
    if (sock >= 0) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // let the OS choose
        if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            sockaddr_in bound{};
            socklen_t len = sizeof(bound);
            if (::getsockname(sock, reinterpret_cast<sockaddr*>(&bound), &len) == 0) {
                port = ntohs(bound.sin_port);
            }
        }
#ifdef _WIN32
        ::closesocket(static_cast<SOCKET>(sock));
#else
        ::close(sock);
#endif
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return port;
}

/**
 * @brief Poll until a TCP connect to the loopback port succeeds.
 * @return true if the listener accepted a probe connection before the deadline.
 */
bool wait_until_accepting(uint16_t port, std::chrono::milliseconds timeout)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
#endif
    bool ok = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!ok && std::chrono::steady_clock::now() < deadline) {
        int sock = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
        if (sock >= 0) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            addr.sin_port = htons(port);
            ok = (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
#ifdef _WIN32
            ::closesocket(static_cast<SOCKET>(sock));
#else
            ::close(sock);
#endif
        }
        if (!ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return ok;
}

}  // namespace

uint16_t MockScpRunner::start(const std::string& aeTitle, const std::string& storageDir)
{
    const uint16_t port = probe_free_port();
    if (port == 0) {
        return 0;
    }

    m_scp.setPort(port);
    m_scp.setAETitle(OFString(aeTitle.c_str()));
    // The SCU under test sends called AE "ANY-SCP" whenever the host carries no
    // "AE@host" prefix (DicomNetworkSCU.cpp:287). A DcmSCP that insists on its
    // own AE title would reject the association before any SCU code under test
    // ran, so accept the called title the SCU chose and echo it back.
    m_scp.setRespondWithCalledAETitle(OFTrue);
    (void)storageDir;  // objects are consumed in memory, not written to disk

    // Presentation contexts the SCU under test proposes (DicomNetworkSCU.cpp:
    // cstore uses the file's SOP class, defaulting to DX; cfindMwl uses MWL).
    OFList<OFString> ts;
    ts.push_back(OFString(UID_LittleEndianExplicitTransferSyntax));
    ts.push_back(OFString(UID_LittleEndianImplicitTransferSyntax));
    ts.push_back(OFString(UID_JPEG2000LosslessOnlyTransferSyntax));

    // Every add is checked: an ignored failure here leaves the profile empty and
    // surfaces later as "Invalid or non-existing SCP Association Profile", which
    // is a much harder message to trace back to its cause.
    const char* const kAbstractSyntaxes[] = {
        UID_DigitalXRayImageStorageForPresentation,
        UID_SecondaryCaptureImageStorage,
        UID_FINDModalityWorklistInformationModel,
    };
    for (const char* as : kAbstractSyntaxes) {
        const OFCondition rc = m_scp.getConfig().addPresentationContext(OFString(as), ts);
        if (rc.bad()) {
            m_listenError = std::string("addPresentationContext(") + as + "): " + rc.text();
            return 0;
        }
    }

    {
        std::ostringstream dump;
        m_scp.getConfig().dumpPresentationContexts(dump);
        // Kept for diagnosis: this is what the SCP believes it has registered.
        m_diag = dump.str();
    }
    // Non-blocking accept with a short timeout, so the stop predicates are
    // polled promptly instead of only after a connection arrives.
    // Accept the default role for every proposed context. Without this the MWL
    // C-FIND context is not accepted and the SCU reports
    // "DIMSE No valid Presentation Context ID" (observed, QA-B-29).
    m_scp.getConfig().setAlwaysAcceptDefaultRole(OFTrue);

    m_scp.setConnectionBlockingMode(DUL_NOBLOCK);
    m_scp.setConnectionTimeout(1);

    m_running = true;
    m_thread = std::thread([this]() {
        // listen() returns once the stop predicates report true, or immediately
        // if it could not bind. Keep the condition text: a silent failure here
        // surfaces downstream as an unexplained skip.
        const OFCondition rc = m_scp.listen();
        if (rc.bad()) {
            m_listenError = rc.text();
        }
        m_running = false;
    });

    // Wait until the port actually accepts connections. Returning as soon as
    // the thread is spawned would let a test connect before listen() has bound,
    // which shows up as an association failure that looks like a product defect.
    if (!wait_until_accepting(port, std::chrono::seconds(5))) {
        if (m_listenError.empty()) {
            m_listenError = "listen() did not accept a connection within 5s";
        }
        m_scp.requestStop();
        if (m_thread.joinable()) {
            m_thread.join();
        }
        m_running = false;
        return 0;
    }

    return port;
}

bool MockScpRunner::stop(std::chrono::milliseconds timeout)
{
    if (!m_thread.joinable()) {
        return true;
    }

    m_scp.requestStop();

    // Poll for the listen loop to exit, then join. A join without this wait
    // would block indefinitely if listen() never noticed the stop request; the
    // caller is told about that instead of hanging the suite.
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (m_running && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (m_running) {
        return false;  // still spinning: report, do not join and hang
    }

    m_thread.join();
    return true;
}

}  // namespace xpe_test
