add_library(xpe_enhance_basic
    src/enhance_basic.cpp
)

target_include_directories(xpe_enhance_basic
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_enhance_basic
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(xpe_enhance_basic PROPERTIES
    OUTPUT_NAME "xpe_enhance_basic"
    VERSION ${PROJECT_VERSION}
)
