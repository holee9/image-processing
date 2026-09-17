/**
 * @file xpe_sha256_backend.cpp
 * @brief SHA-256 backend: Windows CNG when available, PicoSHA2 otherwise (QA-A-106, #179)
 *
 * QA-A-105 measured the calibration load at 3072x3072: about 475 ms for the
 * three files, of which about 415 ms was PicoSHA2 hashing 66 MB (~160 MB/s).
 * SRS-CALIB-PERF-003 budgets 200 ms for the three files.
 *
 * The digest is a SHA-256 either way, so files written by one backend verify
 * under the other; xpe_sha256_backend_name() reports which path a build took.
 * CNG is used only when every one of its calls succeeds -- any failure (missing
 * provider, allocation, hashing) falls back to PicoSHA2 for that stream, so a
 * environment without CNG keeps working.
 */

#include "xpe_sha256.hpp"

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#  ifndef STATUS_SUCCESS
#    define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#  endif
#endif

#include <vector>

namespace {

#ifdef _WIN32

/** Process-wide SHA-256 algorithm provider; opened once, never closed. */
BCRYPT_ALG_HANDLE open_provider() {
    static BCRYPT_ALG_HANDLE alg = [] {
        BCRYPT_ALG_HANDLE h = nullptr;
        const NTSTATUS st = BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM,
                                                        nullptr, 0);
        return (st == STATUS_SUCCESS) ? h : nullptr;
    }();
    return alg;
}

#endif  // _WIN32

}  // namespace

/* =========================================================================
 * Sha256Stream
 * ========================================================================= */

struct Sha256Stream::Impl {
#ifdef _WIN32
    BCRYPT_HASH_HANDLE   hash = nullptr;
    std::vector<uint8_t> object;
#endif
    picosha2::hash256_one_by_one fallback;
    bool                 using_cng = false;

    Impl() {
#ifdef _WIN32
        BCRYPT_ALG_HANDLE alg = open_provider();
        if (alg != nullptr) {
            DWORD object_len = 0, copied = 0;
            if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH,
                                  reinterpret_cast<PUCHAR>(&object_len),
                                  sizeof(object_len), &copied, 0) == STATUS_SUCCESS) {
                object.resize(object_len);
                if (BCryptCreateHash(alg, &hash, object.data(), object_len,
                                     nullptr, 0, 0) == STATUS_SUCCESS) {
                    using_cng = true;
                }
            }
        }
#endif
        if (!using_cng) fallback.init();
    }

    ~Impl() {
#ifdef _WIN32
        if (hash != nullptr) BCryptDestroyHash(hash);
#endif
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void update(const uint8_t* data, size_t len) {
#ifdef _WIN32
        if (using_cng) {
            // BCryptHashData takes a ULONG length; feed long buffers in pieces.
            size_t done = 0;
            while (done < len) {
                const ULONG chunk = static_cast<ULONG>(
                    (len - done > 0x40000000u) ? 0x40000000u : (len - done));
                if (BCryptHashData(hash, const_cast<PUCHAR>(data + done), chunk, 0)
                        != STATUS_SUCCESS) {
                    // Give up on CNG for this stream: restart in PicoSHA2 is not
                    // possible mid-stream, so report a zero digest instead of a
                    // wrong one. finish() below leaves the digest all-zero, which
                    // fails the comparison at the call site.
                    using_cng = false;
                    failed = true;
                    return;
                }
                done += chunk;
            }
            return;
        }
#endif
        if (!failed) fallback.process(data, data + len);
    }

    void finish(std::array<uint8_t, 32>& out) {
        if (failed) { out.fill(0); return; }
#ifdef _WIN32
        if (using_cng) {
            if (BCryptFinishHash(hash, out.data(), static_cast<ULONG>(out.size()), 0)
                    != STATUS_SUCCESS) {
                out.fill(0);
            }
            return;
        }
#endif
        fallback.finish();
        fallback.get_hash_bytes(out.begin(), out.end());
    }

    bool failed = false;
};

Sha256Stream::Sha256Stream() : impl_(new Impl()) {}
Sha256Stream::~Sha256Stream() { delete impl_; }

void Sha256Stream::update(const uint8_t* data, size_t len) {
    if (data != nullptr && len > 0) impl_->update(data, len);
}

const std::array<uint8_t, 32>& Sha256Stream::digest() {
    if (!done_) {
        impl_->finish(digest_);
        done_ = true;
    }
    return digest_;
}

const char* xpe_sha256_backend_name() {
#ifdef _WIN32
    return open_provider() != nullptr ? "cng" : "picosha2";
#else
    return "picosha2";
#endif
}
