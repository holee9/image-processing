/**
 * @file test_ai_label_sidecar.cpp
 * @brief The body-part label reader and the sidecar's duplicate-key check (QA-B-195c, Codex #90, #130).
 *
 * Codex #90 found two things, and this file holds both:
 *   1. LoadBodyPartLabels wrapped the whole JSON parse and the label vector in `catch (const std::exception&)`, so a
 *      std::bad_alloc became "label sidecar is not valid JSON": the worker answered -4 with model_unavailable, and the
 *      in-process path "the model is unavailable", where every other load answers XPE_ERR_OUT_OF_MEMORY (QA-B-195b). The
 *      reader now builds no JSON document (an event parser, like ParseModelSidecar), catches std::bad_alloc on its own and
 *      reports it, and LoadBodyPartModel turns it into BodyPartLoadFailure::kOutOfMemory.
 *   2. The sidecar's duplicate-key check scanned every key so far for each new key: a signed sidecar of about 90000 keys
 *      inside the 1 MiB cap cost ~4 * 10^9 comparisons. It is a hash set now, and the parse stops at the first repeat.
 *
 * What is compared with what: the new reader is compared with the OLD one (a document-based reader, kept below as
 * ReferenceLabels exactly as it was), over inputs that single out each rule -- same labels, same refusal reason. Timing
 * bounds are generous on purpose (a shared CI machine is not a quiet one) and are an upper-bound ESTIMATE, not a measurement
 * of the machine they run on: the local figure that each bound is chosen from is printed by the test.
 */

#include <gtest/gtest.h>

#include "ai_bodypart_model.h"
#include "ai_model_sidecar.h"
#include "xpe/ai/ai_onnx_session.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <new>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

#ifdef XPE_AI_TEST_HOOKS
namespace xpe::ai {
void TestSetBeforeLabelParseHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}
#endif

namespace {

using xpe::ai::BodyPartLoadFailure;
using xpe::ai::BodyPartModel;
using xpe::ai::LoadBodyPartLabels;
using xpe::ai::LoadBodyPartModel;
using xpe::ai::ModelSidecar;
using xpe::ai::OnnxSession;
using xpe::ai::ParseModelSidecar;

const std::string kData = XPE_AI_TEST_DATA_DIR;

/** The label reader as it was before QA-B-195c: the whole text as a document, the whole thing under one catch. */
const char* ReferenceLabels(const std::string& text, std::vector<std::string>* labels) {
    try {
        nlohmann::json j = nlohmann::json::parse(text);
        if (!j.is_object() || !j.contains("labels") || !j["labels"].is_array() || j["labels"].empty()) {
            return "label sidecar has no non-empty labels array";
        }
        for (const auto& e : j["labels"]) {
            if (!e.is_string()) return "label sidecar has a label that is not a string";
            const std::string v = e.get<std::string>();
            if (v.empty() || v.size() >= XPE_AI_MAX_BODYPART_LEN) return "label sidecar has an empty or too long label";
            for (const char c : v) {
                const unsigned char u = static_cast<unsigned char>(c);
                if (c == '"' || c == '\\' || u < 0x20 || u > 0x7E) {
                    return "label sidecar has a label with a character outside printable ASCII, or a quote or backslash";
                }
            }
            labels->push_back(v);
        }
    } catch (const std::exception&) {
        labels->clear();
        return "label sidecar is not valid JSON";
    }
    return nullptr;
}

std::string Reason(const char* r) { return r == nullptr ? std::string("<ok>") : std::string(r); }

double MillisSince(const std::chrono::steady_clock::time_point& t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

}  // namespace

/* =========================================================================
 * 1. The new reader says what the old one said
 * ========================================================================= */

TEST(LabelSidecar, TheEventReaderGivesTheSameLabelsAndTheSameReasonAsTheDocumentReader) {
    const std::string tooLong = "{\"labels\":[\"" + std::string(XPE_AI_MAX_BODYPART_LEN, 'x') + "\"]}";
    const std::string justFits = "{\"labels\":[\"" + std::string(XPE_AI_MAX_BODYPART_LEN - 1, 'x') + "\"]}";
    const std::vector<std::string> texts = {
        // accepted
        "{\"labels\":[\"CHEST\",\"ABDOMEN\",\"SPINE\"]}",
        "{\"model_id\":\"x\",\"labels\":[\"A\"],\"note\":\"n\"}",
        "{\"labels\":[\"A\"],\"validation_metrics\":{\"labels\":[1,2]}}",
        "{\"a\":{\"labels\":[\"BAD\\\"X\"]},\"labels\":[\"A\",\"B\"]}",     // a nested "labels" is not the labels
        "{\"z\":[{\"labels\":[1]}],\"labels\":[\"A\"]}",
        "{\"labels\":[\"A\"],\"labels\":[\"B\",\"C\"]}",                     // a repeated key: the last one wins
        justFits,
        // no usable labels array
        "{\"labels\":\"CHEST\"}",
        "{\"labels\":[]}",
        "{\"labels\":{\"a\":1}}",
        "{\"labels\":null}",
        "{\"x\":1}",
        "{}",
        "{\"labels\":[\"A\"],\"labels\":5}",
        "{\"labels\":[\"A\"],\"labels\":[]}",
        "[1,2]",
        "\"text\"",
        "5",
        "null",
        // a label that is not acceptable
        "{\"labels\":[\"A\",5]}",
        "{\"labels\":[\"A\",null]}",
        "{\"labels\":[\"A\",true]}",
        "{\"labels\":[\"A\",[\"B\"]]}",
        "{\"labels\":[\"A\",{\"b\":1}]}",
        "{\"labels\":[\"A\",\"\"]}",
        tooLong,
        "{\"labels\":[\"caf\\u00e9\"]}",
        "{\"labels\":[\"caf\xC3\xA9\"]}",
        "{\"labels\":[\"a\\tb\"]}",
        "{\"labels\":[\"a\\\"b\"]}",
        "{\"labels\":[\"a\\\\b\"]}",
        // not JSON
        "{bad",
        "",
        "{\"labels\":[\"A\"]} trailing",
        "{\"labels\":[\"A\"],}",
        "{\"labels\":[\"A\"",
    };
    for (const std::string& t : texts) {
        std::vector<std::string> got, want;
        const std::string gotReason = Reason(LoadBodyPartLabels(&t, &got));
        const std::string wantReason = Reason(ReferenceLabels(t, &want));
        EXPECT_EQ(wantReason, gotReason) << "text: " << t;
        if (wantReason == "<ok>") {
            EXPECT_EQ(want, got) << "text: " << t;
        } else {
            // The one deliberate difference: the old reader left the labels read BEFORE a refused one in the list ({"A"}
            // for {"labels":["A",5]}); the new one leaves nothing behind. The caller drops the model either way.
            EXPECT_TRUE(got.empty()) << "a refused sidecar leaves no labels behind; text: " << t;
        }
    }
}

TEST(LabelSidecar, ControlTheComparisonSeesADifference) {
    // A comparison that cannot tell the readers apart proves nothing: a reader that accepted everything would pass it.
    std::vector<std::string> a, b;
    const std::string t = "{\"labels\":[\"A\",5]}";
    EXPECT_NE(Reason(LoadBodyPartLabels(&t, &a)), Reason(nullptr));
    EXPECT_EQ(std::string("label sidecar has a label that is not a string"), Reason(LoadBodyPartLabels(&t, &b)));
    EXPECT_TRUE(b.empty()) << "a refused sidecar leaves no labels behind";
}

TEST(LabelSidecar, NoSidecarAtAllIsNotFoundAndNeverOutOfMemory) {
    std::vector<std::string> labels;
    bool oom = true;
    EXPECT_EQ(std::string("label sidecar bodypart.json not found"), Reason(LoadBodyPartLabels(nullptr, &labels, &oom)));
    EXPECT_FALSE(oom);
}

TEST(LabelSidecar, ABadSidecarIsNeverReportedAsOutOfMemory) {
    for (const char* t : {"{bad", "", "{\"labels\":[]}", "{\"labels\":[5]}"}) {
        const std::string text = t;
        std::vector<std::string> labels;
        bool oom = true;
        EXPECT_NE(nullptr, LoadBodyPartLabels(&text, &labels, &oom)) << t;
        EXPECT_FALSE(oom) << t;
    }
}

/* =========================================================================
 * 2. A shortage of memory is "out of memory", not "not valid JSON"
 * ========================================================================= */

#ifdef XPE_AI_TEST_HOOKS
namespace {
int g_callsUntilFailure = 0;   // the k-th call of the hook fails: 1 = the start of the parse, 2.. = the labels stored
void FailTheKthCall() {
    if (--g_callsUntilFailure == 0) throw std::bad_alloc();
}
struct LabelFailureScope {
    explicit LabelFailureScope(int k) {
        g_callsUntilFailure = k;
        xpe::ai::TestSetBeforeLabelParseHook(&FailTheKthCall);
    }
    ~LabelFailureScope() { xpe::ai::TestSetBeforeLabelParseHook(nullptr); }
    LabelFailureScope(const LabelFailureScope&) = delete;
    LabelFailureScope& operator=(const LabelFailureScope&) = delete;
};
}  // namespace

TEST(LabelSidecarOom, ABadAllocAtTheParseOrAtAnyStoredLabelIsOutOfMemoryAndNeverAnException) {
    const std::string text = "{\"labels\":[\"CHEST\",\"ABDOMEN\",\"SPINE\"]}";
    // control: with no failure the sidecar is accepted and the hook was reached 1 + 3 times
    {
        std::vector<std::string> labels;
        bool oom = true;
        const LabelFailureScope never(1000);
        EXPECT_EQ(nullptr, LoadBodyPartLabels(&text, &labels, &oom));
        EXPECT_FALSE(oom);
        EXPECT_EQ(3u, labels.size());
        EXPECT_EQ(1000 - 4, g_callsUntilFailure) << "the hook is reached once for the parse and once per label";
    }
    for (int k = 1; k <= 4; ++k) {
        std::vector<std::string> labels;
        bool oom = false;
        bool escaped = false;
        const char* reason = nullptr;
        {
            const LabelFailureScope fail(k);
            try {
                reason = LoadBodyPartLabels(&text, &labels, &oom);
            } catch (...) {
                escaped = true;
            }
        }
        EXPECT_FALSE(escaped) << "call " << k << ": an exception left LoadBodyPartLabels";
        EXPECT_EQ(std::string("out of memory"), Reason(reason)) << "call " << k << ": was 'label sidecar is not valid JSON'";
        EXPECT_TRUE(oom) << "call " << k;
        EXPECT_TRUE(labels.empty()) << "call " << k << ": no half-made label list";
    }
}

TEST(LabelSidecarOom, LoadingTheModelReportsOutOfMemoryAsTheLoadFailureNotAsBadLabels) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no session is built";
    const std::string dir = kData + "/models_bodypart_a";
    {   // control: the model loads
        std::unique_ptr<BodyPartModel> m;
        BodyPartLoadFailure kind = BodyPartLoadFailure::kLabels;
        EXPECT_EQ(nullptr, LoadBodyPartModel(dir, &m, &kind));
        EXPECT_EQ(BodyPartLoadFailure::kNone, kind);
        ASSERT_NE(nullptr, m.get());
        EXPECT_EQ(3u, m->labels.size());
    }
    for (int k = 1; k <= 4; ++k) {
        std::unique_ptr<BodyPartModel> m;
        BodyPartLoadFailure kind = BodyPartLoadFailure::kNone;
        const char* reason = nullptr;
        {
            const LabelFailureScope fail(k);
            reason = LoadBodyPartModel(dir, &m, &kind);
        }
        EXPECT_EQ(BodyPartLoadFailure::kOutOfMemory, kind) << "call " << k << ": was kLabels (the model is unavailable)";
        EXPECT_EQ(std::string("out of memory"), Reason(reason)) << "call " << k;
        EXPECT_EQ(nullptr, m.get()) << "call " << k << ": no model is left half made";
    }
}
#endif  // XPE_AI_TEST_HOOKS

/* =========================================================================
 * 3. The sidecar's duplicate-key check is linear (Codex #90, medium)
 * ========================================================================= */

namespace {

/** A sidecar that satisfies REQ-AI-008, followed by @p tail (more members, each with its leading comma), closed. */
std::string SidecarWith(const std::string& tail) {
    return "{\"model_id\":\"m\",\"version\":\"1.0.0\",\"pccp_scope\":\"s\",\"training_data_hash\":\"h\","
           "\"validation_metrics\":{\"a\":1}" + tail + "}";
}

/** Members `,"k<i>":0` with distinct keys, as many as keep the whole text just inside the sidecar cap. */
std::string DistinctKeys(size_t* count) {
    std::string tail;
    size_t n = 0;
    while (SidecarWith(tail).size() + 16 < xpe::ai::kMaxSidecarBytes) {
        tail += ",\"k" + std::to_string(n++) + "\":0";
    }
    *count = n;
    return SidecarWith(tail);
}

/** Members `,"x":0`, the same key again and again, up to the cap. */
std::string RepeatedKey(size_t* count) {
    std::string tail;
    size_t n = 0;
    while (SidecarWith(tail).size() + 16 < xpe::ai::kMaxSidecarBytes) {
        tail += ",\"x\":0";
        ++n;
    }
    *count = n;
    return SidecarWith(tail);
}

// UPPER-BOUND ESTIMATE, not a measurement of the CI machine: chosen far above the local figure (printed by each test)
// and far below what the quadratic scan costs, so that neither a slow runner nor a regression to the scan can hide.
constexpr double kBoundMs = 3000.0;

}  // namespace

TEST(SidecarDuplicateKeys, AFullSizeSidecarOfDistinctKeysIsParsedInLinearTime) {
    size_t n = 0;
    const std::string text = DistinctKeys(&n);
    ASSERT_GT(n, 50000u) << "the sidecar is full size (control)";
    ModelSidecar sc;
    std::string why;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = ParseModelSidecar(&text, &sc, &why);
    const double ms = MillisSince(t0);
    EXPECT_TRUE(ok) << why << " (distinct keys are not a fault)";
    std::printf("[ measured ] %zu distinct keys, %zu bytes: %.1f ms (bound %.0f ms)\n", n, text.size(), ms, kBoundMs);
    EXPECT_LT(ms, kBoundMs);
}

TEST(SidecarDuplicateKeys, AFullSizeSidecarOfOneRepeatedKeyIsRefusedQuicklyWithTheReasonKept) {
    size_t n = 0;
    const std::string text = RepeatedKey(&n);
    ASSERT_GT(n, 100000u) << "the sidecar is full size (control)";
    ModelSidecar sc;
    std::string why;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = ParseModelSidecar(&text, &sc, &why);
    const double ms = MillisSince(t0);
    EXPECT_FALSE(ok);
    EXPECT_EQ(std::string("the key x appears more than once"), why);
    std::printf("[ measured ] %zu repeats of one key, %zu bytes: %.1f ms (bound %.0f ms)\n", n, text.size(), ms, kBoundMs);
    EXPECT_LT(ms, kBoundMs);
}

TEST(SidecarDuplicateKeys, TheFirstRepeatedKeyIsNamedAndARepeatedRequiredKeyIsRefusedToo) {
    ModelSidecar sc;
    std::string why;
    const std::string a = SidecarWith(",\"p\":1,\"q\":2,\"p\":3,\"q\":4");
    EXPECT_FALSE(ParseModelSidecar(&a, &sc, &why));
    EXPECT_EQ(std::string("the key p appears more than once"), why) << "the first repeat, not the last";
    const std::string b = "{\"model_id\":\"m\",\"model_id\":\"n\",\"version\":\"1.0.0\",\"pccp_scope\":\"s\","
                          "\"training_data_hash\":\"h\",\"validation_metrics\":{\"a\":1}}";
    EXPECT_FALSE(ParseModelSidecar(&b, &sc, &why));
    EXPECT_EQ(std::string("the key model_id appears more than once"), why);
    const std::string c = SidecarWith(",\"n\":{\"x\":1,\"x\":2}");
    EXPECT_TRUE(ParseModelSidecar(&c, &sc, &why)) << why << " (a repeat INSIDE a member is not a top-level repeat)";
}

TEST(SidecarDuplicateKeys, TheLabelReaderIsLinearToo) {
    // The label reader keeps no set of keys, so it has no quadratic scan to fall into; this holds it to that.
    std::string text = "{\"labels\":[";
    size_t n = 0;
    while (text.size() + 16 < xpe::ai::kMaxSidecarBytes) {
        text += (n == 0 ? "" : ",");
        text += "\"L" + std::to_string(n++) + "\"";
    }
    text += "]}";
    std::vector<std::string> labels;
    const auto t0 = std::chrono::steady_clock::now();
    const char* reason = LoadBodyPartLabels(&text, &labels);
    const double ms = MillisSince(t0);
    EXPECT_EQ(nullptr, reason);
    EXPECT_EQ(n, labels.size());
    std::printf("[ measured ] %zu labels, %zu bytes: %.1f ms (bound %.0f ms)\n", n, text.size(), ms, kBoundMs);
    EXPECT_LT(ms, kBoundMs);
}

/* =========================================================================
 * 4. Keys chosen to collide in a hash set (Codex #92, medium)
 * =========================================================================
 * A hash set made the duplicate-key check linear for ORDINARY keys, but the keys of a signed sidecar are chosen by whoever
 * signed it, and MSVC's std::hash<std::string> is a fixed FNV-1a: keys can be chosen that all fall in one bucket, which makes
 * an unordered set O(n^2) again. The check is an ordered set now (worst case O(n log n) for the whole sidecar). The corpus
 * below is built FROM the hash function: FNV-1a's low k bits depend only on the low k bits of its state, so the strings
 * whose hash ends in the same 20 bits are found by meeting in the middle -- a 4-character prefix and a 3-character suffix.
 * It is verified against the real std::hash; where the standard library hashes differently the corpus cannot be built and the
 * tests SKIP (they say so) instead of passing on keys that collide nowhere.
 */

namespace {

constexpr unsigned kCollideBits = 20;
constexpr uint64_t kCollideMask = (uint64_t{1} << kCollideBits) - 1;

/** Up to @p want distinct 7-character keys whose std::hash<std::string> has the same low 20 bits (0). Empty when it cannot be built. */
std::vector<std::string> BuildCollidingKeys(size_t want) {
    const uint64_t prime = 1099511628211ull;               // FNV-1a 64-bit, as MSVC's x64 std::hash uses
    const uint64_t basis = 14695981039346656037ull;
    auto step = [&](uint64_t s, unsigned char b) { return ((s ^ b) * prime) & kCollideMask; };
    uint64_t pinv = prime & kCollideMask;                   // modular inverse of the (odd) prime, Newton's iteration
    for (int i = 0; i < 6; ++i) pinv = (pinv * (2 - (prime & kCollideMask) * pinv)) & kCollideMask;
    auto unstep = [&](uint64_t t, unsigned char b) { return (((t * pinv) & kCollideMask) ^ b) & kCollideMask; };

    std::string alphabet;
    for (char c = 'a'; c <= 'z'; ++c) alphabet += c;
    for (char c = '0'; c <= '9'; ++c) alphabet += c;
    for (char c = 'A'; c <= 'L'; ++c) alphabet += c;      // 48 characters
    const size_t a = alphabet.size();

    // every 4-character prefix, grouped by the low 20 bits of its state
    std::vector<uint32_t> count((size_t{1} << kCollideBits) + 1, 0);
    auto prefixState = [&](size_t code) {
        uint64_t st = basis & kCollideMask;
        size_t c = code;
        for (int i = 0; i < 4; ++i) { st = step(st, static_cast<unsigned char>(alphabet[c % a])); c /= a; }
        return st;
    };
    const size_t prefixes = a * a * a * a;
    for (size_t code = 0; code < prefixes; ++code) ++count[prefixState(code) + 1];
    for (size_t i = 1; i < count.size(); ++i) count[i] += count[i - 1];
    std::vector<uint32_t> byState(prefixes);
    {
        std::vector<uint32_t> fill(count.begin(), count.end() - 1);
        for (size_t code = 0; code < prefixes; ++code) byState[fill[prefixState(code)]++] = static_cast<uint32_t>(code);
    }

    std::vector<std::string> keys;
    const std::hash<std::string> h;
    for (size_t suffix = 0; suffix < a * a * a && keys.size() < want; ++suffix) {
        const unsigned char c1 = static_cast<unsigned char>(alphabet[suffix % a]);
        const unsigned char c2 = static_cast<unsigned char>(alphabet[(suffix / a) % a]);
        const unsigned char c3 = static_cast<unsigned char>(alphabet[(suffix / a / a) % a]);
        uint64_t need = 0;                                  // the state the prefix must leave, so that the suffix ends at 0
        need = unstep(need, c3);
        need = unstep(need, c2);
        need = unstep(need, c1);
        for (uint32_t i = count[need]; i < count[need + 1] && keys.size() < want; ++i) {
            std::string key;
            size_t c = byState[i];
            for (int k = 0; k < 4; ++k) { key += alphabet[c % a]; c /= a; }
            key += static_cast<char>(c1);
            key += static_cast<char>(c2);
            key += static_cast<char>(c3);
            if ((h(key) & kCollideMask) == 0) keys.push_back(key);   // verified against the REAL hash
        }
    }
    return keys;
}

std::string SidecarOfKeys(const std::vector<std::string>& keys, size_t from, size_t to) {
    std::string tail;
    for (size_t i = from; i < to; ++i) tail += ",\"" + keys[i] + "\":0";
    return SidecarWith(tail);
}

}  // namespace

TEST(SidecarCollidingKeys, TheCorpusReallyCollidesInAnUnorderedSetOfThisStandardLibrary) {
    // The control for the next test: keys that collide nowhere would let it pass for any implementation.
    const std::vector<std::string> keys = BuildCollidingKeys(20000);
    if (keys.size() < 20000) GTEST_SKIP() << "only " << keys.size() << " keys could be built: this standard library hashes strings differently";
    std::unordered_set<std::string> hashed;
    std::set<std::string> ordered;
    auto t0 = std::chrono::steady_clock::now();
    for (const std::string& k : keys) ordered.insert(k);
    const double orderedMs = MillisSince(t0);
    t0 = std::chrono::steady_clock::now();
    for (const std::string& k : keys) hashed.insert(k);
    const double hashedMs = MillisSince(t0);
    std::printf("[ measured ] %zu colliding keys: ordered set %.1f ms, unordered set %.1f ms\n", keys.size(), orderedMs, hashedMs);
    EXPECT_GT(hashed.bucket_size(hashed.bucket(keys[0])), keys.size() * 9 / 10) << "the keys share one bucket";
    EXPECT_GT(hashedMs, orderedMs * 10) << "and that is what makes a hash set slow on them";
}

TEST(SidecarCollidingKeys, AFullSizeSidecarOfKeysThatShareOneHashBucketIsParsedInTimeToo) {
    const std::vector<std::string> keys = BuildCollidingKeys(80000);
    if (keys.size() < 70000) GTEST_SKIP() << "only " << keys.size() << " colliding keys could be built: this standard library hashes strings differently";
    // as many as fit under the sidecar cap
    size_t n = keys.size();
    while (n > 0 && SidecarOfKeys(keys, 0, n).size() + 16 >= xpe::ai::kMaxSidecarBytes) --n;
    ASSERT_GT(n, 60000u) << "the sidecar is full size (control)";
    const std::string text = SidecarOfKeys(keys, 0, n);
    ModelSidecar sc;
    std::string why;
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = ParseModelSidecar(&text, &sc, &why);
    const double ms = MillisSince(t0);
    EXPECT_TRUE(ok) << why << " (distinct keys, whatever they hash to, are not a fault)";
    std::printf("[ measured ] %zu keys of one hash bucket, %zu bytes: %.1f ms (bound %.0f ms)\n", n, text.size(), ms, kBoundMs);
    EXPECT_LT(ms, kBoundMs);
}
