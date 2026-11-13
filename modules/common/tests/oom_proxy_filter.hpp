/**
 * @file oom_proxy_filter.hpp
 * @brief QA-A-329 G1: identifies the Debug STL container-proxy allocation so the out-of-memory injection tests can
 *        exclude it from their allocation count (design v2, .moai/reports/lead/QA-A-329-design/design.md section 2).
 *
 * A Debug MSVC build (_ITERATOR_DEBUG_LEVEL > 0) gives every standard container a heap-allocated proxy, allocated
 * through std::allocator<std::_Container_proxy>::allocate. The replaced operator new in the injection tests counts it
 * like any other allocation. This header finds the start address of that one allocator function and lets operator new
 * ask whether the current allocation was made from inside it.
 *
 * Calibration (calibrate()) derives the start address twice, independently, and accepts it only when both agree:
 *  1. the explicit address of the instantiated member function, through at most two incremental-link jump thunks,
 *     resolved to its (primary, unchained) function start with the x64 unwind tables;
 *  2. a stack captured inside operator new while that allocator allocates one proxy: exactly one frame must resolve to
 *     the same start, and the replaced operator new itself must be a frame inside it (smaller index).
 * It also captures std::allocator<Pod16>::allocate(1) and ::operator new(16); the address appearing in either stack
 * means it is shared by ordinary allocations, and calibration fails.
 *
 * The filter does nothing unless a test sets g_enabled. While disabled, the replaced operator new pays one relaxed
 * atomic load and one thread-local read and behaves exactly as before. Nothing on the filter path allocates: fixed
 * stack arrays, atomics and thread-local plain values only. An allocation made while the filter is already running on
 * the same thread (re-entry) is counted normally and recorded in g_reentry; it is never excluded.
 *
 * Outside MSVC x64 with _ITERATOR_DEBUG_LEVEL > 0 the filter is unsupported: calibrate() reports Unsupported and
 * excluded() never excludes.
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <new>
#include <string>
#include <vector>

#if defined(_MSC_VER) && defined(_M_X64) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0
#define XPE_OOM_PROXY_FILTER_SUPPORTED 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#define XPE_OOM_PROXY_FILTER_SUPPORTED 0
#endif

namespace oomproxy {

enum class Calib : int { NotRun = 0, Ok = 1, Failed = 2, Unsupported = 3 };

/** Why calibration failed (g_reason). */
enum Reason : int {
    kNone = 0,
    kNoExplicitAddress = 1,   // member-function pointer did not yield an address
    kThunkTooDeep = 2,        // more than two E9 jump thunks
    kImportThunk = 3,         // FF 25 import thunk: the allocator is not in this image
    kNoFunctionEntry = 4,     // no unwind entry for the address (leaf or no unwind data)
    kChainBroken = 5,         // chained unwind entries did not end in a primary entry
    kOutsideModule = 6,       // allocator and operator new are in different images
    kNoNewEntry = 7,          // the replaced operator new could not be resolved
    kCaptureEmpty = 8,        // operator new captured no stack during the proxy allocation
    kCaptureNoMatch = 9,      // no captured frame resolved to the allocator start
    kCaptureMultiMatch = 10,  // more than one captured frame resolved to it
    kNewNotInner = 11,        // operator new was not found inside the matched frame
    kAmbiguousPod16 = 12,     // the address also appears under std::allocator<Pod16>::allocate
    kAmbiguousRawNew = 13,    // the address also appears under ::operator new(16)
    kReentry = 14,            // the filter re-entered operator new during calibration
};

constexpr int kFrames = 8;

inline std::atomic<bool> g_enabled{false};        // set only by a test that exercises the filter
inline std::atomic<std::uintptr_t> g_proxyBegin{0};
inline std::atomic<int> g_calib{static_cast<int>(Calib::NotRun)};
inline std::atomic<int> g_reason{kNone};

inline std::atomic<long> g_rawArmed{0};        // every allocation the filter was asked about
inline std::atomic<long> g_skipped{0};         // classified as proxy: not counted, not injected
inline std::atomic<long> g_reentry{0};         // allocations made from inside the filter path
inline std::atomic<long> g_unresolved{0};      // captured frames with no unwind entry
inline std::atomic<long> g_depthCapped{0};     // full capture with no match (a match could be deeper)

/** Calibration diagnostics, written once by calibrate(). */
struct Diag {
    std::uintptr_t imageBase = 0;
    std::uintptr_t explicitRaw = 0;     // member-function pointer value
    std::uintptr_t explicitTarget = 0;  // after thunk resolution
    int thunksFollowed = 0;
    std::uintptr_t explicitBegin = 0;   // P_explicit
    std::uintptr_t newBegin = 0;        // start of the replaced operator new
    int captured = 0;                   // frames in the proxy capture
    int matchIndex = -1;                // index of the frame resolving to P_explicit
    int newIndex = -1;                  // index of the frame resolving to operator new
    int pod16Captured = 0;
    int rawNewCaptured = 0;
};
inline Diag g_diag;

// Calibration capture (thread-local plain values only).
inline thread_local bool t_calibrating = false;
inline thread_local bool t_inFilter = false;
inline thread_local int t_calibCount = 0;
inline thread_local std::uintptr_t t_calibFrames[kFrames] = {};

inline void resetCounters()
{
    g_rawArmed.store(0);
    g_skipped.store(0);
    g_reentry.store(0);
    g_unresolved.store(0);
    g_depthCapped.store(0);
}

#if XPE_OOM_PROXY_FILTER_SUPPORTED

/** Start of the primary function containing pc, through chained unwind entries; false when unresolvable. */
inline bool functionBegin(std::uintptr_t pc, std::uintptr_t& begin, std::uintptr_t* imageBase = nullptr)
{
    DWORD64 base = 0;
    PRUNTIME_FUNCTION f = RtlLookupFunctionEntry(static_cast<DWORD64>(pc), &base, nullptr);
    if (!f) return false;
    constexpr unsigned kChainInfo = 0x4;  // UNW_FLAG_CHAININFO
    for (int i = 0; i < 8; ++i) {
        if (f->UnwindData & 1u) {  // indirect entry: points at another RUNTIME_FUNCTION
            f = reinterpret_cast<PRUNTIME_FUNCTION>(base + (f->UnwindData & ~1u));
            continue;
        }
        const unsigned char* ui = reinterpret_cast<const unsigned char*>(base + f->UnwindData);
        if (((ui[0] >> 3) & kChainInfo) == 0) {
            begin = static_cast<std::uintptr_t>(base + f->BeginAddress);
            if (imageBase) *imageBase = static_cast<std::uintptr_t>(base);
            return true;
        }
        const unsigned codes = ui[2];
        const unsigned slots = (codes + 1u) & ~1u;  // unwind codes are padded to an even count
        f = reinterpret_cast<PRUNTIME_FUNCTION>(const_cast<unsigned char*>(ui + 4 + slots * 2));
    }
    return false;
}

/** Follows at most two E9 rel32 jump thunks. Returns kNone, kThunkTooDeep or kImportThunk. */
inline int followThunks(std::uintptr_t addr, std::uintptr_t& out, int& followed)
{
    followed = 0;
    for (;;) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(addr);
        if (p[0] == 0xFF && p[1] == 0x25) return kImportThunk;
        if (p[0] != 0xE9) break;
        if (followed == 2) return kThunkTooDeep;
        std::int32_t rel = 0;
        std::memcpy(&rel, p + 1, sizeof rel);
        addr = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(addr) + 5 + rel);
        ++followed;
    }
    out = addr;
    return kNone;
}

/** Called first thing in the replaced operator new: records the stack while this thread calibrates. */
inline void calibrationHook()
{
    if (!t_calibrating) return;
    void* frames[kFrames] = {};
    const USHORT n = RtlCaptureStackBackTrace(0, kFrames, frames, nullptr);
    t_calibCount = n;
    for (int i = 0; i < kFrames; ++i) t_calibFrames[i] = reinterpret_cast<std::uintptr_t>(frames[i]);
}

/**
 * Asked by the replaced operator new only inside its counting scope. True means: this is the container-proxy
 * allocation; do not count or inject it. Accounting holds g_rawArmed == counted + g_skipped.
 */
inline bool excluded()
{
    if (!g_enabled.load(std::memory_order_relaxed)) return false;
    g_rawArmed.fetch_add(1);
    if (t_inFilter) {
        g_reentry.fetch_add(1);
        return false;
    }
    t_inFilter = true;
    bool proxy = false;
    const std::uintptr_t target = g_proxyBegin.load(std::memory_order_acquire);
    if (target != 0) {
        void* frames[kFrames] = {};
        const USHORT n = RtlCaptureStackBackTrace(0, kFrames, frames, nullptr);
        for (USHORT i = 0; i < n && !proxy; ++i) {
            std::uintptr_t begin = 0;
            if (!functionBegin(reinterpret_cast<std::uintptr_t>(frames[i]) - 1, begin)) {
                g_unresolved.fetch_add(1);
                continue;
            }
            proxy = begin == target;
        }
        if (!proxy && n == kFrames) g_depthCapped.fetch_add(1);
    }
    t_inFilter = false;
    if (proxy) g_skipped.fetch_add(1);
    return proxy;
}

struct Pod16 {
    unsigned char b[16];
};

/** Captures the stack operator new sees for one call of f (disarmed, this thread only). */
template <class F>
inline int captureDuring(F&& f, std::uintptr_t (&out)[kFrames])
{
    t_calibCount = 0;
    t_calibrating = true;
    f();
    t_calibrating = false;
    for (int i = 0; i < kFrames; ++i) out[i] = t_calibFrames[i];
    return t_calibCount;
}

inline bool anyFrameBegins(const std::uintptr_t (&frames)[kFrames], int n, std::uintptr_t target)
{
    for (int i = 0; i < n; ++i) {
        std::uintptr_t b = 0;
        if (functionBegin(frames[i] - 1, b) && b == target) return true;
    }
    return false;
}

inline Calib fail(int reason)
{
    g_proxyBegin.store(0, std::memory_order_release);
    g_reason.store(reason);
    g_calib.store(static_cast<int>(Calib::Failed));
    return Calib::Failed;
}

/**
 * Calibrates once per process. Must run disarmed, before any worker thread exists, with g_enabled false.
 * Publishes g_proxyBegin (release) only when every check of design v2 section 2-1 passes.
 */
inline Calib calibrate()
{
    if (g_calib.load() != static_cast<int>(Calib::NotRun)) return static_cast<Calib>(g_calib.load());
    Diag d;

    // 1. explicit address of the instantiated allocator member.
    using ProxyAlloc = std::allocator<std::_Container_proxy>;
    using Pm = std::_Container_proxy* (ProxyAlloc::*)(std::size_t);
    Pm pm = &ProxyAlloc::allocate;
    static_assert(sizeof(pm) == sizeof(void*), "member-function pointer is not a single code address");
    std::memcpy(&d.explicitRaw, &pm, sizeof pm);
    if (d.explicitRaw == 0) { g_diag = d; return fail(kNoExplicitAddress); }

    // 2. incremental-link thunks.
    int r = followThunks(d.explicitRaw, d.explicitTarget, d.thunksFollowed);
    if (r != kNone) { g_diag = d; return fail(r); }

    // 3. primary function start.
    std::uintptr_t imgAlloc = 0;
    if (!functionBegin(d.explicitTarget, d.explicitBegin, &imgAlloc)) { g_diag = d; return fail(kNoFunctionEntry); }

    void* (*pn)(std::size_t) = &::operator new;
    std::uintptr_t newRaw = 0, newTarget = 0;
    std::memcpy(&newRaw, &pn, sizeof pn);
    int newThunks = 0;
    if (followThunks(newRaw, newTarget, newThunks) != kNone || !functionBegin(newTarget, d.newBegin, &d.imageBase)) {
        g_diag = d;
        return fail(kNoNewEntry);
    }
    if (imgAlloc != d.imageBase) { g_diag = d; return fail(kOutsideModule); }

    // 4. capture during a real proxy allocation.
    const long reentryBefore = g_reentry.load();
    std::uintptr_t frames[kFrames] = {};
    d.captured = captureDuring([] {
        ProxyAlloc a;
        std::_Container_proxy* p = a.allocate(1);
        a.deallocate(p, 1);
    }, frames);
    if (d.captured <= 0) { g_diag = d; return fail(kCaptureEmpty); }
    int matches = 0;
    for (int i = 0; i < d.captured; ++i) {
        std::uintptr_t b = 0;
        if (!functionBegin(frames[i] - 1, b)) continue;
        if (b == d.explicitBegin) { ++matches; d.matchIndex = i; }
        if (b == d.newBegin && d.newIndex < 0) d.newIndex = i;
    }
    if (matches == 0) { g_diag = d; return fail(kCaptureNoMatch); }
    if (matches > 1) { g_diag = d; return fail(kCaptureMultiMatch); }
    if (d.newIndex < 0 || d.newIndex >= d.matchIndex) { g_diag = d; return fail(kNewNotInner); }

    // 5. the address must not appear under ordinary allocations.
    std::uintptr_t podFrames[kFrames] = {};
    d.pod16Captured = captureDuring([] {
        std::allocator<Pod16> a;
        Pod16* p = a.allocate(1);
        a.deallocate(p, 1);
    }, podFrames);
    if (d.pod16Captured <= 0 || anyFrameBegins(podFrames, d.pod16Captured, d.explicitBegin)) {
        g_diag = d;
        return fail(kAmbiguousPod16);
    }
    std::uintptr_t rawFrames[kFrames] = {};
    d.rawNewCaptured = captureDuring([] { ::operator delete(::operator new(16)); }, rawFrames);
    if (d.rawNewCaptured <= 0 || anyFrameBegins(rawFrames, d.rawNewCaptured, d.explicitBegin)) {
        g_diag = d;
        return fail(kAmbiguousRawNew);
    }
    if (g_reentry.load() != reentryBefore) { g_diag = d; return fail(kReentry); }

    // 6. publish.
    g_diag = d;
    g_reason.store(kNone);
    g_proxyBegin.store(d.explicitBegin, std::memory_order_release);
    g_calib.store(static_cast<int>(Calib::Ok));
    t_calibCount = 0;
    for (auto& f : t_calibFrames) f = 0;
    resetCounters();
    return Calib::Ok;
}

#else  // unsupported configuration: the filter never excludes

inline void calibrationHook() {}
inline bool excluded()
{
    if (g_enabled.load(std::memory_order_relaxed)) g_rawArmed.fetch_add(1);  // keeps raw == count + skipped
    return false;
}
inline Calib calibrate()
{
    g_calib.store(static_cast<int>(Calib::Unsupported));
    return Calib::Unsupported;
}

#endif

/** One count-only control's observations, read after disarming. */
struct Sample {
    long count = 0;
    long raw = 0;
    long skipped = 0;
    long reentry = 0;
    long unresolved = 0;
    long depthCapped = 0;
};

/**
 * Runs body inside the caller's counting scope with the filter enabled, in count-only mode (the caller's arm must
 * not be able to reach an injection). Everything is read before the filter is disabled; nothing is asserted here.
 */
template <class Arm, class Disarm, class Count, class Body>
inline Sample measure(Arm arm, Disarm disarm, Count count, Body body)
{
    resetCounters();
    g_enabled.store(true);
    arm();
    body();
    const long c = count();
    disarm();
    g_enabled.store(false);
    Sample s;
    s.count = c;
    s.raw = g_rawArmed.load();
    s.skipped = g_skipped.load();
    s.reentry = g_reentry.load();
    s.unresolved = g_unresolved.load();
    s.depthCapped = g_depthCapped.load();
    return s;
}

inline void sink(const void* p)
{
    static const void* volatile v = nullptr;
    v = p;
}

/** The count-only controls of design v2 section 4 (P1-P3, N1, N3). Each allocates and frees inside the scope. */
inline void controlP1() { std::string s; sink(&s); }
inline void controlP2() { std::vector<int> v; sink(&v); }
inline void controlP3() { std::filesystem::path p; sink(&p); }
inline void controlN1()
{
#if XPE_OOM_PROXY_FILTER_SUPPORTED
    // The real 16-byte payload is the point of N1 only where the filter runs (MSVC x64); other ABIs keep compiling
    // with the filter off (Reviewer F329f-2).
    static_assert(sizeof(std::exception_ptr) == 16, "N1 expects a 16-byte payload on x64");
#endif
    std::vector<std::exception_ptr> v(1);
    sink(v.data());
}
struct Pod16Payload {
    unsigned char b[16];
};
inline void controlN3a()
{
    std::allocator<Pod16Payload> a;
    Pod16Payload* p = a.allocate(1);
    sink(p);
    a.deallocate(p, 1);
}
inline void controlN3b()
{
    void* p = ::operator new(16);
    sink(p);
    ::operator delete(p);
}

/** Prints the calibration diagnostics (after calibration; printf only, outside any counting scope). */
inline void printCalibration(const char* exe)
{
    const Diag& d = g_diag;
    std::printf("[proxy-filter G1] exe=%s calib=%d reason=%d imageBase=0x%llx explicitRaw=0x%llx explicitTarget=0x%llx "
                "thunks=%d P_explicit=0x%llx newBegin=0x%llx captured=%d matchIndex=%d newIndex=%d pod16Captured=%d "
                "rawNewCaptured=%d supported=%d\n",
                exe, g_calib.load(), g_reason.load(), static_cast<unsigned long long>(d.imageBase),
                static_cast<unsigned long long>(d.explicitRaw), static_cast<unsigned long long>(d.explicitTarget),
                d.thunksFollowed, static_cast<unsigned long long>(d.explicitBegin),
                static_cast<unsigned long long>(d.newBegin), d.captured, d.matchIndex, d.newIndex, d.pod16Captured,
                d.rawNewCaptured, XPE_OOM_PROXY_FILTER_SUPPORTED);
    std::fflush(stdout);
}

inline void printSample(const char* exe, const char* name, const Sample& s)
{
    std::printf("[proxy-filter G1] exe=%s control=%s count=%ld raw=%ld skipped=%ld reentry=%ld unresolved=%ld "
                "depthCapped=%ld\n",
                exe, name, s.count, s.raw, s.skipped, s.reentry, s.unresolved, s.depthCapped);
    std::fflush(stdout);
}

/** Expected (count, skipped) for each control, by configuration: proxies exist only when the filter is supported. */
struct Expect {
    const char* name;
    void (*body)();
    long count;
    long skippedMin;  // supported: at least this many proxies skipped; unsupported: exactly 0
    bool skippedExact;
};

inline const Expect* controls(int& n)
{
    static const Expect k[] = {
        {"P1_string", &controlP1, 0, 1, false},
        {"P2_vector_int", &controlP2, 0, 1, false},
        {"P3_fs_path", &controlP3, 0, 1, false},
        {"N1_vector_exception_ptr_1", &controlN1, 1, 1, true},
        {"N3a_allocator_Pod16", &controlN3a, 1, 0, true},
        {"N3b_operator_new_16", &controlN3b, 1, 0, true},
    };
    n = static_cast<int>(sizeof k / sizeof k[0]);
    return k;
}

/* -------------------------------------------------------------------------
 * G2: injection controls (design v2 section 4, Reviewer F329f-1)
 * ------------------------------------------------------------------------- */

/** One injection control's observations. Everything is captured before any assertion or message. */
struct InjectResult {
    Sample sample;
    bool threwBadAlloc = false;
    bool threwOther = false;
    bool injected = false;       // the caller's injected flag, read by its disarm
    bool restored = false;       // see injectRun
    bool probeOk = false;        // an allocation after the scope succeeded
    long rawAfterProbe = -1;     // filter counters must stay untouched by it (filter off)
};

/**
 * Exception-safe injection scope. Arms the caller's allocator (arm(1) shaped) with the filter on, runs body, and on
 * every exit path - normal return, std::bad_alloc, any other exception - first disarms, turns the filter off and clears
 * the filter's thread-local state, and only then records anything. No allocation, message or assertion happens before
 * the disarm. Separate from the count-only measure() on purpose.
 */
template <class Arm, class Disarm, class Count, class Armed, class Body>
inline InjectResult injectRun(Arm arm, Disarm disarm, Count count, Armed armedNow, Body body)
{
    InjectResult r;
    resetCounters();
    struct Scope {
        Disarm& disarm;
        Count& count;
        InjectResult& r;
        bool open = true;
        void close() noexcept
        {
            if (!open) return;
            open = false;
            r.sample.count = count();
            r.injected = disarm();         // injection off before anything else
            g_enabled.store(false);        // filter off
            t_inFilter = false;            // filter thread-local state cleared
            t_calibrating = false;
            r.sample.raw = g_rawArmed.load();
            r.sample.skipped = g_skipped.load();
            r.sample.reentry = g_reentry.load();
            r.sample.unresolved = g_unresolved.load();
            r.sample.depthCapped = g_depthCapped.load();
        }
        ~Scope() { close(); }
    };
    {
        Scope scope{disarm, count, r};
        g_enabled.store(true);
        arm();
        try {
            body();
        } catch (const std::bad_alloc&) {
            scope.close();
            r.threwBadAlloc = true;
        } catch (...) {
            scope.close();
            r.threwOther = true;
        }
        scope.close();
    }
    // State restore: not armed, filter off, filter thread state clear, a following allocation succeeds and is not seen
    // by the (now off) filter.
    resetCounters();
    try {
        void* p = ::operator new(16);
        sink(p);
        ::operator delete(p);
        r.probeOk = true;
    } catch (...) {
        r.probeOk = false;
    }
    r.rawAfterProbe = g_rawArmed.load();
    r.restored = !armedNow() && !g_enabled.load() && !t_inFilter && !t_calibrating && r.probeOk && r.rawAfterProbe == 0;
    return r;
}

/** G2 controls: P4 must not be injected (its only armed allocation is the proxy); N2/N3 must be. */
struct InjectExpect {
    const char* name;
    void (*body)();
    bool badAlloc;     // supported configuration
    long count;
    long skipped;      // supported configuration; 0 when unsupported
};

inline void controlP4() { controlP1(); }  // std::string s; under arm(1)

inline const InjectExpect* injectControls(int& n)
{
    static const InjectExpect k[] = {
        {"P4_string_arm1", &controlP4, false, 0, 1},
        {"N2_vector_exception_ptr_1_arm1", &controlN1, true, 1, 1},
        {"N3a_allocator_Pod16_arm1", &controlN3a, true, 1, 0},
        {"N3b_operator_new_16_arm1", &controlN3b, true, 1, 0},
    };
    n = static_cast<int>(sizeof k / sizeof k[0]);
    return k;
}

inline void printInject(const char* exe, const char* name, const InjectResult& r)
{
    std::printf("[proxy-filter G2] exe=%s control=%s threw=%s injected=%d count=%ld raw=%ld skipped=%ld reentry=%ld "
                "unresolved=%ld depthCapped=%ld restored=%d probeOk=%d rawAfterProbe=%ld\n",
                exe, name, r.threwBadAlloc ? "std::bad_alloc" : (r.threwOther ? "other" : "none"),
                r.injected ? 1 : 0, r.sample.count, r.sample.raw, r.sample.skipped, r.sample.reentry,
                r.sample.unresolved, r.sample.depthCapped, r.restored ? 1 : 0, r.probeOk ? 1 : 0, r.rawAfterProbe);
    std::fflush(stdout);
}

}  // namespace oomproxy

/** G2 checks, evaluated after injectRun returned (state already restored). */
#define XPE_OOM_PROXY_G2_CHECK(exe, result, expect)                                                               \
    do {                                                                                                          \
        const ::oomproxy::InjectResult& r_ = (result);                                                            \
        const ::oomproxy::InjectExpect& e_ = (expect);                                                            \
        ::oomproxy::printInject((exe), e_.name, r_);                                                              \
        EXPECT_FALSE(r_.threwOther) << e_.name << ": an exception other than std::bad_alloc";                     \
        EXPECT_EQ(r_.threwBadAlloc, e_.badAlloc) << e_.name << ": std::bad_alloc observed";                       \
        EXPECT_EQ(r_.injected, e_.badAlloc) << e_.name << ": injected flag";                                      \
        EXPECT_EQ(r_.sample.count, e_.count) << e_.name << ": counted allocations";                               \
        EXPECT_EQ(r_.sample.skipped, XPE_OOM_PROXY_FILTER_SUPPORTED ? e_.skipped : 0L) << e_.name << ": skipped";  \
        EXPECT_EQ(r_.sample.raw, r_.sample.count + r_.sample.skipped) << e_.name << ": raw == count + skipped";    \
        EXPECT_EQ(r_.sample.reentry, 0) << e_.name << ": filter re-entered operator new";                          \
        EXPECT_TRUE(r_.restored) << e_.name << ": state not restored after the scope";                             \
    } while (0)

/** gtest checks shared by the three executables; evaluated after the scope is disarmed. */
#define XPE_OOM_PROXY_G1_CHECK(exe, sample, expect)                                                                \
    do {                                                                                                          \
        const ::oomproxy::Sample& s_ = (sample);                                                                  \
        const ::oomproxy::Expect& e_ = (expect);                                                                  \
        ::oomproxy::printSample((exe), e_.name, s_);                                                              \
        EXPECT_EQ(s_.count, e_.count) << e_.name << ": counted allocations";                                      \
        if (XPE_OOM_PROXY_FILTER_SUPPORTED) {                                                                     \
            if (e_.skippedExact) EXPECT_EQ(s_.skipped, e_.skippedMin) << e_.name << ": proxies skipped";          \
            else EXPECT_GE(s_.skipped, e_.skippedMin) << e_.name << ": proxies skipped";                          \
        } else {                                                                                                  \
            EXPECT_EQ(s_.skipped, 0) << e_.name << ": no proxies without iterator debugging";                     \
        }                                                                                                         \
        EXPECT_EQ(s_.raw, s_.count + s_.skipped) << e_.name << ": raw == count + skipped";                        \
        EXPECT_EQ(s_.reentry, 0) << e_.name << ": filter re-entered operator new";                                \
    } while (0)
