/**
 * @file simd_dispatch.cpp
 * @brief Runtime SIMD dispatch layer for XPE pre-processing algorithms
 *        Supports: AVX-512F → AVX2 → NEON → Scalar fallback hierarchy
 * SPEC: SPEC-XPE-P1A-SIMD-PARITY v2.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_internal.h"

// Platform-specific intrinsics headers
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

#include <immintrin.h>  // AVX2/AVX-512
#if defined(__aarch64__)
#include <arm_neon.h>   // NEON
#endif

#include <cstring>
#include <cstdlib>

/* =========================================================================
 * SIMD Feature Detection
 * REQ-P1A-040: CPUID-based dispatch with hierarchical fallback
 * ========================================================================= */

namespace {

/**
 * @brief Supported SIMD architectures
 */
enum class SimdArchitecture {
    Scalar,   // Portable C implementation
    AVX2,     // x86-64 AVX2 (256-bit)
    AVX512,   // x86-64 AVX-512F (512-bit)
    NEON      // ARM64 NEON (128-bit)
};

/**
 * @brief Global SIMD capability cache (initialized once at startup)
 */
struct SimdCapabilities {
    SimdArchitecture arch = SimdArchitecture::Scalar;
    bool has_avx2 = false;
    bool has_avx512f = false;
    bool has_neon = false;
    bool force_scalar = false;    // Override via env var or config
    bool force_avx2 = false;      // Override via env var
    bool force_avx512 = false;    // Override via env var
    bool force_neon = false;      // Override via env var

    const char* arch_name() const noexcept {
        switch (arch) {
            case SimdArchitecture::AVX512: return "AVX-512F";
            case SimdArchitecture::AVX2:   return "AVX2";
            case SimdArchitecture::NEON:   return "NEON";
            case SimdArchitecture::Scalar: return "scalar";
            default:                       return "unknown";
        }
    }
};

// Global singleton (initialized at first call)
SimdCapabilities g_simd_caps;

/* =========================================================================
 * Platform-Specific CPUID Detection
 * ========================================================================= */

/**
 * @brief Detect AVX-512F support (x86-64 only)
 * @return true if CPU supports AVX-512F foundation instructions
 */
bool detect_avx512f() noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // Step 1: Check OSXSAVE (CPUID leaf 1, ECX bit 27)
    int cpuInfo[4];
#if defined(_MSC_VER)
    __cpuid(cpuInfo, 1);
#else
    __cpuid(1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
    if ((cpuInfo[2] & (1 << 27)) == 0) return false;  // OSXSAVE not enabled

    // Step 2: Check AVX-512F (CPUID leaf 7, subleaf 0, EBX bit 16)
#if defined(_MSC_VER)
    __cpuidex(cpuInfo, 7, 0);
#else
    __cpuid_count(7, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
    if ((cpuInfo[1] & (1 << 16)) == 0) return false;  // AVX-512F not supported

    // Step 3: Check OS saves ZMM registers (XGETBV bits 7:5, 2:1, 0)
#if defined(_MSC_VER)
    unsigned long long xcr_mask = _xgetbv(0);
#else
    unsigned int xcr0_lo, xcr0_hi;
    __asm__ __volatile__("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
    unsigned long long xcr_mask = ((unsigned long long)xcr0_hi << 32) | xcr0_lo;
#endif
    // ZMM state (bits 7:5), YMM state (bits 2:1), XMM state (bit 0)
    return (xcr_mask & 0xE6ULL) == 0xE6ULL;
#else
    // Not x86/x64
    return false;
#endif
}

/**
 * @brief Detect AVX2 support (x86-64 only)
 * @return true if CPU supports AVX2 instructions
 */
bool detect_avx2() noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    // Step 1: Check OSXSAVE (CPUID leaf 1, ECX bit 27)
    int cpuInfo[4];
#if defined(_MSC_VER)
    __cpuid(cpuInfo, 1);
#else
    __cpuid(1, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
    if ((cpuInfo[2] & (1 << 27)) == 0) return false;  // OSXSAVE not enabled

    // Step 2: Check AVX2 (CPUID leaf 7, subleaf 0, EBX bit 5)
#if defined(_MSC_VER)
    __cpuidex(cpuInfo, 7, 0);
#else
    __cpuid_count(7, 0, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
#endif
    if ((cpuInfo[1] & (1 << 5)) == 0) return false;  // AVX2 not supported

    // Step 3: Check OS saves YMM registers (XGETBV bits 2:1, 0)
#if defined(_MSC_VER)
    unsigned long long xcr_mask = _xgetbv(0);
#else
    unsigned int xcr0_lo, xcr0_hi;
    __asm__ __volatile__("xgetbv" : "=a"(xcr0_lo), "=d"(xcr0_hi) : "c"(0));
    unsigned long long xcr_mask = ((unsigned long long)xcr0_hi << 32) | xcr0_lo;
#endif
    // YMM state (bits 2:1), XMM state (bit 0)
    return (xcr_mask & 0x06ULL) == 0x06ULL;
#else
    // Not x86/x64
    return false;
#endif
}

/**
 * @brief Detect NEON support (ARM64 only)
 * @return true if CPU supports NEON (always true on ARM64)
 */
bool detect_neon() noexcept
{
#if defined(__aarch64__)
    // ARM64 always has NEON as part of the base architecture
    return true;
#else
    return false;
#endif
}

/**
 * @brief Read environment variable for SIMD override
 * @param var_name Environment variable name
 * @return true if variable is set to "1"
 */
bool env_var_enabled(const char* var_name) noexcept
{
#if defined(_MSC_VER)
    char buf[16];
    size_t len = 0;
    if (getenv_s(&len, buf, sizeof(buf), var_name) != 0) return false;
    return (len > 0 && (buf[0] == '1' || _stricmp(buf, "true") == 0));
#else
    const char* val = std::getenv(var_name);
    return (val && (val[0] == '1' || strcmp(val, "true") == 0));
#endif
}

/* =========================================================================
 * SIMD Initialization (called once at module startup)
 * ========================================================================= */

/**
 * @brief Initialize SIMD capabilities cache
 * Called by xpe_preprocess_init() during module startup
 */
void simd_dispatch_init() noexcept
{
    // Detect hardware capabilities
    g_simd_caps.has_avx512f = detect_avx512f();
    g_simd_caps.has_avx2 = detect_avx2();
    g_simd_caps.has_neon = detect_neon();

    // Check environment variable overrides (highest priority)
    g_simd_caps.force_scalar = env_var_enabled("XPE_FORCE_SCALAR");
    g_simd_caps.force_avx2 = env_var_enabled("XPE_FORCE_AVX2");
    g_simd_caps.force_avx512 = env_var_enabled("XPE_FORCE_AVX512");
    g_simd_caps.force_neon = env_var_enabled("XPE_FORCE_NEON");

    // Determine selected architecture based on priority
    if (g_simd_caps.force_scalar) {
        g_simd_caps.arch = SimdArchitecture::Scalar;
    } else if (g_simd_caps.force_avx512 && g_simd_caps.has_avx512f) {
        g_simd_caps.arch = SimdArchitecture::AVX512;
    } else if (g_simd_caps.force_avx2 && g_simd_caps.has_avx2) {
        g_simd_caps.arch = SimdArchitecture::AVX2;
    } else if (g_simd_caps.force_neon && g_simd_caps.has_neon) {
        g_simd_caps.arch = SimdArchitecture::NEON;
    } else {
        // Default: hardware-based hierarchical fallback
        if (g_simd_caps.has_avx512f) {
            g_simd_caps.arch = SimdArchitecture::AVX512;
        } else if (g_simd_caps.has_avx2) {
            g_simd_caps.arch = SimdArchitecture::AVX2;
        } else if (g_simd_caps.has_neon) {
            g_simd_caps.arch = SimdArchitecture::NEON;
        } else {
            g_simd_caps.arch = SimdArchitecture::Scalar;
        }
    }
}

} // anonymous namespace

/* =========================================================================
 * Public API: SIMD Dispatch Query
 * REQ-P1A-040: Query function for logging and telemetry
 * ========================================================================= */

extern "C" {

/**
 * @brief Get the currently active SIMD architecture name
 * @return String constant ("scalar", "AVX2", "AVX-512F", "NEON")
 */
XPE_EXPORT const char* xpe_simd_get_arch_name() noexcept
{
    return g_simd_caps.arch_name();
}

/**
 * @brief Check if AVX-512 path is active
 * @return true if using AVX-512 implementation
 */
XPE_EXPORT bool xpe_simd_is_avx512() noexcept
{
    return g_simd_caps.arch == SimdArchitecture::AVX512;
}

/**
 * @brief Check if AVX2 path is active
 * @return true if using AVX2 implementation
 */
XPE_EXPORT bool xpe_simd_is_avx2() noexcept
{
    return g_simd_caps.arch == SimdArchitecture::AVX2;
}

/**
 * @brief Check if NEON path is active
 * @return true if using NEON implementation
 */
XPE_EXPORT bool xpe_simd_is_neon() noexcept
{
    return g_simd_caps.arch == SimdArchitecture::NEON;
}

/**
 * @brief Check if scalar path is active
 * @return true if using scalar implementation
 */
XPE_EXPORT bool xpe_simd_is_scalar() noexcept
{
    return g_simd_caps.arch == SimdArchitecture::Scalar;
}

/**
 * @brief Force scalar mode (for testing parity)
 * @param enable true to force scalar, false to restore auto-detection
 *
 * This function is primarily for testing purposes.
 * Production code should use XPE_FORCE_SCALAR environment variable instead.
 */
XPE_EXPORT void xpe_simd_force_scalar(bool enable) noexcept
{
    g_simd_caps.force_scalar = enable;
    if (enable) {
        g_simd_caps.arch = SimdArchitecture::Scalar;
    } else {
        // Re-detect hardware
        simd_dispatch_init();
    }
}

/**
 * @brief Get SIMD capabilities as a bitmask
 * @return Bitmask with flags: 0x01=AVX2, 0x02=AVX-512, 0x04=NEON
 */
XPE_EXPORT int xpe_simd_get_capabilities() noexcept
{
    int caps = 0;
    if (g_simd_caps.has_avx2)    caps |= 0x01;
    if (g_simd_caps.has_avx512f) caps |= 0x02;
    if (g_simd_caps.has_neon)    caps |= 0x04;
    return caps;
}

} // extern "C"

/* =========================================================================
 * Internal Helpers for Algorithm Implementations
 * These functions are called by offset_correct.cpp, gain_correct.cpp, etc.
 * ========================================================================= */

namespace xpe::simd {

/**
 * @brief Call this function at module startup to initialize SIMD dispatch
 * Automatically called by xpe_preprocess_init()
 */
void init_dispatch() noexcept
{
    simd_dispatch_init();
}

/**
 * @brief Get the selected architecture (internal use)
 */
SimdArchitecture get_arch() noexcept
{
    return g_simd_caps.arch;
}

} // namespace xpe::simd
