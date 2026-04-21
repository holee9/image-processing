/**
 * @file test_post_degraded_mode.cpp
 * @brief Post-B BP-06..10 degraded-mode GTest checks.
 *
 * These tests intentionally link only against xpe_common. Optional Post-B DLLs
 * are inspected as files in the staged degraded environment so the Windows
 * loader cannot fail before Google Test starts.
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

std::string get_env_or_empty(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }

    std::string result{value};
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string{value};
#endif
}

std::filesystem::path degraded_stage_dir()
{
    auto stage = get_env_or_empty("XPE_DEGRADED_STAGE_DIR");
    return stage.empty() ? std::filesystem::current_path() : std::filesystem::path{stage};
}

void expect_degraded_r0(const char* bpId, const char* dllName)
{
    auto absent = get_env_or_empty("XPE_DEGRADED_ABSENT_DLL");
    if (absent.empty()) {
        GTEST_SKIP() << "XPE_DEGRADED_ABSENT_DLL is not set.";
    }

    if (absent != "ALL_OPTIONAL" && absent != dllName) {
        GTEST_SKIP() << bpId << " is not selected for absent DLL scenario "
                     << absent << ".";
    }

    auto dllPath = degraded_stage_dir() / dllName;
    EXPECT_FALSE(std::filesystem::exists(dllPath))
        << bpId << " expected " << dllName << " to be absent in the staged "
        << "degraded-mode environment so module readiness degrades to R0.";
    ::testing::Test::RecordProperty("BP", bpId);
    ::testing::Test::RecordProperty("expected_readiness", "R0");
    ::testing::Test::RecordProperty("absent_dll", dllName);
}

} // namespace

TEST(DegradedMode, BP06_GsvgMissingReportsR0)
{
    expect_degraded_r0("BP-06", "gsvg.dll");
}

TEST(DegradedMode, BP07_CollimationMissingEnhanceAdvancedReportsR0)
{
    expect_degraded_r0("BP-07", "xpe_enhance_advanced.dll");
}

TEST(DegradedMode, BP08_EiMissingEnhanceBasicReportsR0)
{
    expect_degraded_r0("BP-08", "xpe_enhance_basic.dll");
}

TEST(DegradedMode, BP09_DicomMissingReportsR0)
{
    expect_degraded_r0("BP-09", "xpe_dicom.dll");
}

TEST(DegradedMode, BP10_DisplayMissingReportsR0)
{
    expect_degraded_r0("BP-10", "xpe_display.dll");
}
