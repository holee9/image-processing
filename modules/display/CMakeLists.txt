# XPE Display Module
# Display processing (Modality/VOI/Presentation LUT)

cmake_minimum_required(VERSION 3.20)

# Module name
set(MODULE_NAME xpe_display)

# Shared library
add_library(${MODULE_NAME} SHARED
    # Source files will be added here
)

# C++ standard (inherited from root)
target_compile_features(${MODULE_NAME} PUBLIC cxx_std_17)

# Include directories
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/modules/common/include
)

# Health-only dependency for XPE_API export macros and common runtime loading.
target_link_libraries(${MODULE_NAME} PUBLIC xpe_common)

# Export symbols
target_compile_definitions(${MODULE_NAME} PRIVATE XPE_DLL_EXPORT)

# Placeholder version function
function(create_placeholder_version_function MODULE_NAME)
    set(VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
    file(WRITE "${VERSION_SOURCE}"
"#include \"xpe/common/xpe_common_api.h\"

extern \"C\" XPE_API const char* ${MODULE_NAME}_version() {
    return \"0.1.0-placeholder\";
}
")
    target_sources(${MODULE_NAME} PRIVATE "${VERSION_SOURCE}")
endfunction()

create_placeholder_version_function(${MODULE_NAME})
