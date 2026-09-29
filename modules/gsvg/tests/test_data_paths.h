#ifndef XPE_GSVG_TESTS_TEST_DATA_PATHS_H
#define XPE_GSVG_TESTS_TEST_DATA_PATHS_H

/**
 * @file test_data_paths.h
 * @brief Absolute fixture paths, baked in by CMake (#229, QA-B-163).
 *
 * WHY THIS EXISTS. Every gsvg test used to name its fixture relatively
 * ("tests/data/virtual_grid_synthetic_table.csv"), which resolves only when
 * the cwd is modules/gsvg. Two things arranged that by accident: ctest, via
 * WORKING_DIRECTORY, and the #162 single-process CI step, via
 * `working-directory: modules/gsvg`. Measured from the repository root
 * instead, 63 of 164 tests failed; from outside the repository, the same.
 *
 * The same defect took main red in modules/ai first (QA-B-162), where the
 * #162 step ran the ai binary from THIS module's directory and nothing
 * resolved. The rule that came out of it: a test must pass under ctest AND
 * when its binary is run directly from anywhere -- ctest checks the
 * registered environment, a direct run checks that no environment is needed.
 *
 * A header rather than four copies of the same constant: the next test file
 * gets the path by including this, so the relative form has no natural place
 * to come back.
 */

#ifndef XPE_GSVG_TEST_DATA_DIR
#error "XPE_GSVG_TEST_DATA_DIR must be defined by the build (modules/gsvg/CMakeLists.txt)"
#endif
#ifndef XPE_GSVG_PRODUCT_DATA_DIR
#error "XPE_GSVG_PRODUCT_DATA_DIR must be defined by the build (modules/gsvg/CMakeLists.txt)"
#endif
#ifndef XPE_GSVG_MCSIM_PHANTOM_DIR
#error "XPE_GSVG_MCSIM_PHANTOM_DIR must be defined by the build (modules/gsvg/CMakeLists.txt)"
#endif

#include <string>

namespace xpe_gsvg_test {

/** modules/gsvg/tests/data -- absolute. */
inline const std::string& DataDir() {
    static const std::string d = XPE_GSVG_TEST_DATA_DIR;
    return d;
}

/** A file under tests/data, absolute. */
inline std::string Data(const std::string& rel) {
    return DataDir() + "/" + rel;
}

/**
 * modules/gsvg/data -- absolute. The SHIPPED table, not a fixture.
 *
 * A second directory because it is a different kind of thing: tests/data holds
 * invented numbers, data/ holds the table the product loads. The tests name it
 * so that a change to the real table is caught, so they must reach the real
 * one -- not a copy that could drift.
 */
inline std::string ProductData(const std::string& rel) {
    static const std::string d = XPE_GSVG_PRODUCT_DATA_DIR;
    return d + "/" + rel;
}

/**
 * tools/mcsim/phantoms -- absolute.
 *
 * Kept separate because it is NOT under tests/: the 512^2 images are read
 * from the directory that produces them rather than copied, since four 1 MB
 * images per phantom would put 8 MB of duplicated binaries in the repository.
 */
inline const std::string& McsimPhantomDir() {
    static const std::string d = XPE_GSVG_MCSIM_PHANTOM_DIR;
    return d;
}

/**
 * Escape a path for embedding in a JSON string literal.
 *
 * CMake hands these back with forward slashes even on Windows, so today this
 * changes nothing. It is here because the failure mode if that ever stops
 * being true is silent: a stray backslash makes an invalid escape, the config
 * still parses far enough for the table to go unloaded, and the test reads
 * the result as a measurement rather than as a missing fixture.
 */
inline std::string JsonPath(const std::string& p) {
    std::string out;
    out.reserve(p.size());
    for (const char c : p) {
        if (c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

}  // namespace xpe_gsvg_test

#endif  // XPE_GSVG_TESTS_TEST_DATA_PATHS_H
