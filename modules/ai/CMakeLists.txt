add_library(xpe_ai
    src/ai.cpp
)

target_include_directories(xpe_ai
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_ai
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(xpe_ai PROPERTIES
    OUTPUT_NAME "xpe_ai"
    VERSION ${PROJECT_VERSION}
)
