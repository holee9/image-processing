# XpeCoverageRun.cmake — script mode (cmake -P). Runs the test suite under the
# coverage tool and writes the report even when some tests fail.
#
# Rationale (#120): a failing test must not suppress the coverage report. Test
# failures are reported by the regular test jobs; the coverage job's own gate is
# coverage_check (line-rate). OpenCppCoverage propagates the child's exit code,
# which turned a single timing assertion into "no coverage data at all".
#
# Inputs:
#   -DXPE_COV_CMD=<;-separated command list>   full tool invocation
#   -DXPE_COV_REPORT=<path>                     report that must exist afterwards
#   -DXPE_COV_WORKDIR=<dir>

if(NOT XPE_COV_CMD)
    message(FATAL_ERROR "XpeCoverageRun: XPE_COV_CMD not set")
endif()

execute_process(
    COMMAND ${XPE_COV_CMD}
    WORKING_DIRECTORY "${XPE_COV_WORKDIR}"
    RESULT_VARIABLE _rc)

if(NOT _rc EQUAL 0)
    message(WARNING "Coverage: test run under the coverage tool exited with ${_rc} "
                    "(test failures are reported by the test jobs; coverage continues).")
endif()

if(NOT EXISTS "${XPE_COV_REPORT}")
    message(FATAL_ERROR "Coverage: no report produced at ${XPE_COV_REPORT} (tool exit ${_rc})")
endif()
message(STATUS "Coverage: report written to ${XPE_COV_REPORT}")
