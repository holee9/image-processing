# XPE AI Module
# AI-based image processing (ONNX Runtime)

cmake_minimum_required(VERSION 3.20)

# Module name
set(MODULE_NAME xpe_ai)

# Shared library
add_library(${MODULE_NAME} SHARED
    src/ai.cpp
)

# C++ standard (inherited from root)
target_compile_features(${MODULE_NAME} PUBLIC cxx_std_17)

# Include directories
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Dependencies (to be added: ONNX Runtime)
target_link_libraries(${MODULE_NAME}
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

# Export symbols
target_compile_definitions(${MODULE_NAME} PRIVATE XPE_DLL_EXPORT)

# Set version properties
set_target_properties(${MODULE_NAME} PROPERTIES
    OUTPUT_NAME "xpe_ai"
    VERSION ${PROJECT_VERSION}
)
