# GSVG Module
# Grid Suppression / Virtual Grid (Independent DLL)

cmake_minimum_required(VERSION 3.25)

# Module name
set(MODULE_NAME gsvg)

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
)

# Dependencies (to be added: FFTW3)
# target_link_libraries(${MODULE_NAME} PUBLIC ...)

# Export symbols
target_compile_definitions(${MODULE_NAME} PRIVATE GSVG_DLL_EXPORT)

# Placeholder version function
function(create_placeholder_version_function MODULE_NAME)
    set(VERSION_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/version.cpp")
    file(WRITE "${VERSION_SOURCE}"
"#include \"gsvg_api.h\"

extern \"C\" GSVG_API const char* ${MODULE_NAME}_version() {
    return \"0.1.0-placeholder\";
}
")
    target_sources(${MODULE_NAME} PRIVATE "${VERSION_SOURCE}")
endfunction()

create_placeholder_version_function(${MODULE_NAME})
