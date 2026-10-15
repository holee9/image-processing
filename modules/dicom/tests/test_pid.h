// QA-B-211 (#257): a per-process suffix for the scratch directories of the dicom tests.
//
// `gtest_discover_tests` registers every TEST as its own ctest test, i.e. its own PROCESS, and each process runs the fixture's
// SetUpTestSuite / TearDownTestSuite. A directory named by a fixed string was therefore created, filled and `remove_all`-ed by
// several processes at once under `ctest -j` (measured: `remove_all: ... being used by another process`, files gone under the
// feet of another test). The process id makes the directory this process's own; it is stable for the life of the process, so the
// fixture still shares one directory among its own tests.
#pragma once

#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace xpe_test {
inline std::string pid_suffix() {
#ifdef _WIN32
    return "_" + std::to_string(_getpid());
#else
    return "_" + std::to_string(static_cast<long>(getpid()));
#endif
}
}  // namespace xpe_test
