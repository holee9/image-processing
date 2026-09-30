/**
 * @file test_worker_protocol_conformance.cpp
 * @brief The DLL-side bridge against the real worker process (QA-B-169, #130).
 *
 * WHY THIS FILE EXISTS. QA-B-167 found that xpe_ai_worker.exe includes
 * ai_worker_protocol.h and then defines its OWN message enum in an anonymous
 * namespace, with three of the shared numbers crossed (header SHUTDOWN=3 /
 * worker HEARTBEAT=3, header HEARTBEAT=4 / worker HEARTBEAT_ACK=4, header
 * HEARTBEAT_ACK=5 / worker SHUTDOWN=5). That report had to list the mismatch
 * as UNMEASURED: nothing in the product calls the bridge, so the disagreement
 * has never had an occasion to misbehave.
 *
 * A defect with no observable symptom cannot be fixed provably -- "it works
 * now" is unfalsifiable when "it was broken" was never shown. So this file
 * connects the two halves on purpose and records what actually happens. The
 * tests are written to PASS on the conformant behaviour, so before the fix
 * they are red and they say what they saw.
 *
 * WHAT IS DELIBERATELY NOT ASSERTED. Not which of the three plausible
 * symptoms occurs (silent no-op, ERROR_RESPONSE, deadlock) -- that was the
 * open question, and encoding a guess would make the answer unfalsifiable
 * too. Each test asserts the CONFORMANT outcome and prints the observed one.
 *
 * PROCESS HYGIENE. Every launch is owned by a Worker guard that terminates
 * the child in its destructor. A trailing kill on the happy path is not
 * cleanup: an assertion failure returns early and would leave the process
 * behind (kanban-dispatch: never spawn background load).
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <string>

extern "C" {
typedef struct XpeAiIpcBridge XpeAiIpcBridge;
XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms);
XpeErrorCode    xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge);
XpeErrorCode    xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge,
                                       const XpeAiMessageHeader* header,
                                       const void* payload,
                                       uint32_t payload_size);
XpeErrorCode    xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge,
                                          XpeAiMessageHeader* header,
                                          void* payload,
                                          uint32_t payload_capacity,
                                          uint32_t* payload_size);
void            xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge);
}

namespace {

#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

/** How long to wait for the worker to exit after a shutdown request. */
constexpr DWORD kExitWaitMs = 3000;
/** How long the bridge waits for a reply before calling it silence. */
constexpr uint32_t kBridgeTimeoutMs = 1500;

std::string UniquePipeName() {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "\\\\.\\pipe\\xpe_ai_wpc_%lu_%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()),
                  static_cast<unsigned long>(GetTickCount()));
    return buf;
}

/**
 * @brief A launched worker process plus a connected bridge.
 *
 * Both halves are freed in the destructor on every exit path, including an
 * ASSERT that returns early.
 */
struct Worker {
    std::string pipe;
    PROCESS_INFORMATION pi{};
    XpeAiIpcBridge* bridge = nullptr;
    bool launched = false;

    Worker() : pipe(UniquePipeName()) {
        std::string cmd = std::string("\"") + XPE_AI_WORKER_EXE + "\" " + pipe;
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        launched = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                  CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != 0;
    }

    ~Worker() {
        if (bridge) xpe_ai_ipc_bridge_destroy(bridge);
        if (launched) {
            // The child may already be gone; TerminateProcess on an exited
            // process is a no-op that returns FALSE, which is fine here.
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;

    /** Connect the bridge; the worker needs a moment to create its pipe. */
    bool Connect() {
        bridge = xpe_ai_ipc_bridge_create(pipe.c_str(), kBridgeTimeoutMs);
        if (!bridge) return false;
        // The bridge already retries for timeout_ms on ERROR_PIPE_BUSY, but a
        // pipe that does not exist yet fails immediately, so retry the create.
        for (int i = 0; i < 40; ++i) {
            if (xpe_ai_ipc_bridge_connect(bridge) == XPE_OK) return true;
            Sleep(50);
        }
        return false;
    }

    /** True when the child has exited within @p ms. */
    bool ExitedWithin(DWORD ms) {
        return WaitForSingleObject(pi.hProcess, ms) == WAIT_OBJECT_0;
    }
};

/** A well-formed header per ai_worker_protocol.h, no payload. */
XpeAiMessageHeader Header(uint32_t type) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    h.messageType = type;
    h.requestId = 1;
    h.payloadSize = 0;
    h.flags = 0;
    h.timestamp = 0;
    return h;
}

}  // namespace

// --- the control: the harness itself works ---------------------------------
//
// Every assertion below is about a message NOT having the intended effect. If
// the worker never launched or the pipe never connected, those assertions
// would fail for a reason that has nothing to do with the protocol. This test
// separates the two.

TEST(WorkerProtocolConformance, TheWorkerLaunchesAndTheBridgeConnects) {
    Worker w;
    ASSERT_TRUE(w.launched) << "could not start " << XPE_AI_WORKER_EXE;
    EXPECT_TRUE(w.Connect()) << "bridge never connected to " << w.pipe;
    EXPECT_FALSE(w.ExitedWithin(200))
        << "the worker exited on its own before any message was sent; every "
           "'did not act on the message' assertion below would then pass for "
           "the wrong reason";
}

// --- what the worker actually says ------------------------------------------
//
// The first measurement (QA-B-169, before the worker was changed) showed rc=-1
// from the bridge. That is XPE_ERR_INVALID_INPUT, not silence: the worker DID
// reply, the bridge read a full header-sized chunk (40 bytes) and rejected it
// because the first word was not the protocol magic. The worker was sending
// its own private struct, whose first word is its own message type. So the
// helper below keeps the first word of whatever came back, and every failure
// message prints it -- "no reply" and "a reply the bridge could not use" are
// different bugs and must not read alike.

namespace {

struct Reply {
    XpeErrorCode rc = XPE_OK;
    XpeAiMessageHeader header{};
    std::string payload;
};

Reply Receive(Worker& w) {
    Reply r;
    uint8_t buf[1024] = {0};
    uint32_t got = 0;
    r.rc = xpe_ai_ipc_bridge_receive(w.bridge, &r.header, buf, sizeof(buf) - 1, &got);
    if (r.rc == XPE_OK && r.header.payloadSize > 0 && got >= r.header.payloadSize) {
        r.payload.assign(reinterpret_cast<char*>(buf), r.header.payloadSize);
    }
    return r;
}

Reply SendAndReceive(Worker& w, uint32_t type, uint32_t request_id) {
    XpeAiMessageHeader h = Header(type);
    h.requestId = request_id;
    EXPECT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(w.bridge, &h, nullptr, 0));
    return Receive(w);
}

::testing::AssertionResult IsFramedReply(const Reply& r, uint32_t expected_type,
                                         uint32_t expected_request_id) {
    if (r.rc != XPE_OK) {
        // snprintf, not std::hex: a manipulator does not persist across the
        // AssertionResult stream, and an earlier version of this message
        // printed a decimal 99 as "0x99" and the decimal magic as "0x1481655617"
        // -- evidence that reads wrong is worse than none.
        char words[96];
        std::snprintf(words, sizeof(words), "first word of the reply = %u (decimal), protocol magic = 0x%08X",
                      static_cast<unsigned>(r.header.magic),
                      static_cast<unsigned>(XPE_AI_MSG_MAGIC));
        return ::testing::AssertionFailure()
            << "the bridge rejected the worker reply: rc=" << static_cast<int>(r.rc)
            << " (-1 is XPE_ERR_INVALID_INPUT, i.e. not a protocol frame); "
            << words << ". A first word of 99 is the old worker's own ERROR_RESPONSE type.";
    }
    if (r.header.messageType != expected_type) {
        return ::testing::AssertionFailure()
            << "reply type " << r.header.messageType << ", expected " << expected_type;
    }
    if (r.header.requestId != expected_request_id) {
        return ::testing::AssertionFailure()
            << "reply requestId " << r.header.requestId << ", expected the request's "
            << expected_request_id << " echoed back";
    }
    return ::testing::AssertionSuccess();
}

}  // namespace

TEST(WorkerProtocolConformance, ShutdownPerTheHeaderActuallyShutsTheWorkerDown) {
    Worker w;
    ASSERT_TRUE(w.launched);
    ASSERT_TRUE(w.Connect());

    const XpeAiMessageHeader h = Header(XPE_AI_MSG_SHUTDOWN);   // 3 per the header
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(w.bridge, &h, nullptr, 0));

    // Before the fix the worker read its own struct's first word -- the magic
    // -- as the message type, so no header-framed message of ANY type was
    // understood, SHUTDOWN included. It answered with an error frame and kept
    // running.
    EXPECT_TRUE(w.ExitedWithin(kExitWaitMs))
        << "the worker did not exit within " << kExitWaitMs << " ms of a protocol "
           "SHUTDOWN (type " << XPE_AI_MSG_SHUTDOWN << "). A shutdown that is "
           "silently not a shutdown leaves the process resident, which is the "
           "failure REQ-AI-003 process isolation exists to avoid.";
}

TEST(WorkerProtocolConformance, InitPerTheHeaderGetsAnInitResponse) {
    Worker w;
    ASSERT_TRUE(w.launched);
    ASSERT_TRUE(w.Connect());

    // INIT is the one lifecycle number the two old enums agreed on (1 == 1).
    // It still failed before the fix, which is how the envelope -- not only the
    // numbering -- was shown to be a different shape.
    const Reply r = SendAndReceive(w, XPE_AI_MSG_INIT, 41);
    ASSERT_TRUE(IsFramedReply(r, XPE_AI_MSG_INIT_RESPONSE, 41));
    EXPECT_NE(std::string::npos, r.payload.find("\"success\":true")) << r.payload;
}

TEST(WorkerProtocolConformance, TheInitResponseNamesTheBuildTheWorkerReallyIs) {
    Worker w;
    ASSERT_TRUE(w.launched);
    ASSERT_TRUE(w.Connect());

    const Reply r = SendAndReceive(w, XPE_AI_MSG_INIT, 42);
    ASSERT_TRUE(IsFramedReply(r, XPE_AI_MSG_INIT_RESPONSE, 42));

    // The worker and this test binary are configured by the same CMake
    // condition, so the mode the worker reports has to equal the mode this
    // binary was built in. This is the assertion that DIFFERS between the two
    // configurations: it is "stub" in one and "full" in the other. The old
    // constant answered "0.1.0-stub" from both.
    const bool stub = xpe::ai::OnnxSession::IsStubBuild();
    EXPECT_NE(std::string::npos,
              r.payload.find(stub ? "\"mode\":\"stub\"" : "\"mode\":\"full\""))
        << "this test binary is a " << (stub ? "STUB" : "FULL")
        << " build; the worker said: " << r.payload;
    EXPECT_EQ(stub, r.payload.find("-stub") != std::string::npos)
        << "the worker version string disagrees with its own mode: " << r.payload;
}

TEST(WorkerProtocolConformance, HeartbeatPerTheHeaderIsAnsweredAsAHeartbeat) {
    Worker w;
    ASSERT_TRUE(w.launched);
    ASSERT_TRUE(w.Connect());

    // Header HEARTBEAT is 4; the old worker enum's 4 was HEARTBEAT_ACK, so the
    // DLL's ping would have been read as the worker's own acknowledgement.
    const Reply r = SendAndReceive(w, XPE_AI_MSG_HEARTBEAT, 43);
    ASSERT_TRUE(IsFramedReply(r, XPE_AI_MSG_HEARTBEAT_ACK, 43));
    EXPECT_NE(std::string::npos, r.payload.find("\"state\":0")) << r.payload;
}

TEST(WorkerProtocolConformance, AMessageTheWorkerDoesNotHandleGetsAnErrorThatSaysSo) {
    Worker w;
    ASSERT_TRUE(w.launched);
    ASSERT_TRUE(w.Connect());

    // 10 is defined by the protocol (BODYPART_RECOGNIZE) and not implemented by
    // the worker yet (QA-B-170): the reply must be an ERROR frame that names
    // the type, not silence and not a lifecycle acknowledgement.
    const Reply defined = SendAndReceive(w, XPE_AI_MSG_BODYPART_RECOGNIZE, 44);
    ASSERT_TRUE(IsFramedReply(defined, XPE_AI_MSG_ERROR, 44));
    EXPECT_NE(std::string::npos, defined.payload.find("message type 10 is not handled"))
        << defined.payload;

    // 77 is not in the protocol at all. The worker must survive it and go on
    // answering: a stream that decodes one bad type must not desynchronise.
    const Reply undefined = SendAndReceive(w, 77, 45);
    ASSERT_TRUE(IsFramedReply(undefined, XPE_AI_MSG_ERROR, 45));

    const Reply after = SendAndReceive(w, XPE_AI_MSG_HEARTBEAT, 46);
    EXPECT_TRUE(IsFramedReply(after, XPE_AI_MSG_HEARTBEAT_ACK, 46))
        << "the worker stopped answering after an unsupported message";
}

// --- the numbering itself, read from the source -----------------------------

TEST(WorkerProtocolConformance, TheWorkerSourceDeclaresNoPrivateMessageEnum) {
    // The worker's private enum is a source-level fact, and the assertion that
    // it is gone has to read the source: the worker is a separate binary and
    // nothing of its anonymous namespace is linkable from here.
    const std::string src = std::string(XPE_AI_WORKER_SOURCE);
    FILE* f = nullptr;
    ASSERT_EQ(0, fopen_s(&f, src.c_str(), "rb")) << "cannot read " << src;
    ASSERT_NE(nullptr, f);
    std::string text;
    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);

    // A control first: the file really was read, and it really is the worker.
    ASSERT_NE(std::string::npos, text.find("WorkerServer"))
        << "read " << src << " but it does not look like the worker source; "
           "every 'not found' below would be vacuous";

    EXPECT_EQ(std::string::npos, text.find("enum class MessageType"))
        << "the worker still defines its own MessageType. The shared header is "
           "the single definition; a second one drifts silently because "
           "nothing compares them (QA-B-167 found three crossed numbers).";
}
