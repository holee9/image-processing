add_library(xpe_display
    src/display.cpp
)

target_include_directories(xpe_display
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_display
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(xpe_display PROPERTIES
    OUTPUT_NAME "xpe_display"
    VERSION ${PROJECT_VERSION}
)
