# XpeCoverageCheck.cmake — script mode (cmake -P). Reads a Cobertura report and
# fails when the overall line-rate is below XPE_COVERAGE_MIN (REQ-P0-006: 0.85).
#
# Inputs: -DXPE_COVERAGE_XML=<path> -DXPE_COVERAGE_MIN=<0..1>

if(NOT EXISTS "${XPE_COVERAGE_XML}")
    # lcov builds do not produce coverage.xml; treat as "no data" rather than pass.
    message(FATAL_ERROR "Coverage check: report not found: ${XPE_COVERAGE_XML}")
endif()

file(READ "${XPE_COVERAGE_XML}" _xml LIMIT 4096)
# Cobertura root element carries the aggregate: <coverage line-rate="0.87" ...>
string(REGEX MATCH "<coverage[^>]*line-rate=\"([0-9.]+)\"" _m "${_xml}")
if(NOT _m)
    message(FATAL_ERROR "Coverage check: could not read line-rate from ${XPE_COVERAGE_XML}")
endif()
set(_rate "${CMAKE_MATCH_1}")

if(_rate LESS XPE_COVERAGE_MIN)
    message(FATAL_ERROR "Coverage check FAILED: line-rate ${_rate} < required ${XPE_COVERAGE_MIN} (REQ-P0-006)")
endif()
message(STATUS "Coverage check PASSED: line-rate ${_rate} >= ${XPE_COVERAGE_MIN}")
