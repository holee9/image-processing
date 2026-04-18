add_library(gsvg
    src/gsvg.cpp
)

target_include_directories(gsvg
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(gsvg
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(gsvg PROPERTIES
    OUTPUT_NAME "gsvg"
    VERSION ${PROJECT_VERSION}
)
