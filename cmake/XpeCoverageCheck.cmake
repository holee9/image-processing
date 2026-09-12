# XpeCoverageCheck.cmake — script mode (cmake -P). Reads a Cobertura report and
# fails when the overall line-rate is below XPE_COVERAGE_MIN, then reports the
# same figure broken down per module.
#
# Inputs: -DXPE_COVERAGE_XML=<path> -DXPE_COVERAGE_MIN=<0..1>
#         -DXPE_COVERAGE_MODULE_MIN=<0..1>   optional; when set, a module below it fails
#         -DXPE_COVERAGE_REQUIRED_MODULE=<name>  optional; module the requirement names
#
# Why the per-module pass exists (2026-09-12, #120): REQ-P0-006 asks for 85% on
# **xpe_common.dll**, but the `coverage` preset measures xpe_common and
# xpe_preprocess together. An aggregate above the threshold does not establish
# that each module is above it — a small module can sit well under while a large
# one carries the average. The aggregate was about to be reported as satisfying
# the requirement; splitting it is what makes the claim attributable.

if(NOT EXISTS "${XPE_COVERAGE_XML}")
    # lcov builds do not produce coverage.xml; treat as "no data" rather than pass.
    message(FATAL_ERROR "Coverage check: report not found: ${XPE_COVERAGE_XML}")
endif()

file(READ "${XPE_COVERAGE_XML}" _head LIMIT 4096)
# Cobertura root element carries the aggregate: <coverage line-rate="0.87" ...>
string(REGEX MATCH "<coverage[^>]*line-rate=\"([0-9.]+)\"" _m "${_head}")
if(NOT _m)
    message(FATAL_ERROR "Coverage check: could not read line-rate from ${XPE_COVERAGE_XML}")
endif()
set(_rate "${CMAKE_MATCH_1}")

# ---------------------------------------------------------------------------
# Per-module breakdown. Cobertura <class filename="..."> entries carry the source
# path, so every counted line can be attributed to the module directory it lives
# in. Lines are counted rather than rates averaged: averaging per-file rates
# weights a 3-line file like a 300-line one.
# ---------------------------------------------------------------------------
file(READ "${XPE_COVERAGE_XML}" _xml)

set(_modules "")
string(REGEX MATCHALL "<class[^>]*filename=\"[^\"]*\"[^>]*>" _classes "${_xml}")

# Split the document at each <class ...> so a class's <line .../> elements can be
# scanned without a full XML parser. The tail after the last class is dropped
# with it, which is correct: it holds no line elements of its own.
string(REGEX REPLACE "<class[ \t]" ";<class " _chunks "${_xml}")

foreach(_chunk IN LISTS _chunks)
    if(NOT _chunk MATCHES "^<class ")
        continue()
    endif()
    string(REGEX MATCH "filename=\"([^\"]*)\"" _fm "${_chunk}")
    if(NOT _fm)
        continue()
    endif()
    set(_file "${CMAKE_MATCH_1}")
    string(REPLACE "\\" "/" _file "${_file}")
    # modules/<name>/... anywhere in the path; everything else is grouped as "other".
    if(_file MATCHES "modules/([A-Za-z0-9_]+)/")
        set(_mod "${CMAKE_MATCH_1}")
    else()
        set(_mod "other")
    endif()

    string(REGEX MATCHALL "<line [^>]*hits=\"[0-9]+\"" _lines "${_chunk}")
    foreach(_l IN LISTS _lines)
        string(REGEX MATCH "hits=\"([0-9]+)\"" _hm "${_l}")
        if(NOT DEFINED _tot_${_mod})
            set(_tot_${_mod} 0)
            set(_hit_${_mod} 0)
            list(APPEND _modules "${_mod}")
        endif()
        math(EXPR _tot_${_mod} "${_tot_${_mod}} + 1")
        if(NOT CMAKE_MATCH_1 STREQUAL "0")
            math(EXPR _hit_${_mod} "${_hit_${_mod}} + 1")
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES _modules)
list(SORT _modules)

set(_below "")
if(_modules)
    message(STATUS "Coverage by module (lines hit / lines instrumented):")
    foreach(_mod IN LISTS _modules)
        set(_t "${_tot_${_mod}}")
        set(_h "${_hit_${_mod}}")
        if(_t GREATER 0)
            # CMake has no float division; scale to per-mille and format.
            math(EXPR _permille "(${_h} * 1000) / ${_t}")
            math(EXPR _whole "${_permille} / 1000")
            math(EXPR _frac "${_permille} % 1000")
            string(LENGTH "${_frac}" _flen)
            while(_flen LESS 3)
                set(_frac "0${_frac}")
                string(LENGTH "${_frac}" _flen)
            endwhile()
            message(STATUS "  ${_mod}: ${_whole}.${_frac}  (${_h}/${_t})")
            if(DEFINED XPE_COVERAGE_MODULE_MIN AND NOT XPE_COVERAGE_MODULE_MIN STREQUAL "")
                math(EXPR _min_permille "0")
                # Compare in per-mille to stay in integer arithmetic.
                string(REGEX REPLACE "^0\\." "" _minfrac "${XPE_COVERAGE_MODULE_MIN}")
                string(SUBSTRING "${_minfrac}000" 0 3 _minfrac)
                if(_permille LESS _minfrac)
                    list(APPEND _below "${_mod}")
                endif()
            endif()
        endif()
    endforeach()
else()
    message(WARNING "Coverage check: no per-class line data found; only the aggregate is known. "
                    "A per-module claim cannot be made from this report.")
endif()

if(_rate LESS XPE_COVERAGE_MIN)
    message(FATAL_ERROR "Coverage check FAILED: line-rate ${_rate} < required ${XPE_COVERAGE_MIN} (REQ-P0-006)")
endif()
message(STATUS "Coverage check PASSED: line-rate ${_rate} >= ${XPE_COVERAGE_MIN}")

if(_below)
    string(REPLACE ";" ", " _below_str "${_below}")
    if(DEFINED XPE_COVERAGE_REQUIRED_MODULE AND NOT XPE_COVERAGE_REQUIRED_MODULE STREQUAL "")
        list(FIND _below "${XPE_COVERAGE_REQUIRED_MODULE}" _idx)
        if(NOT _idx EQUAL -1)
            message(FATAL_ERROR
                "Coverage check FAILED: module ${XPE_COVERAGE_REQUIRED_MODULE} is below "
                "${XPE_COVERAGE_MODULE_MIN} even though the aggregate (${_rate}) passes. "
                "REQ-P0-006 names that module, not the aggregate.")
        endif()
    endif()
    message(WARNING "Coverage: below ${XPE_COVERAGE_MODULE_MIN} per module: ${_below_str}. "
                    "The aggregate passes; these modules do not.")
endif()
