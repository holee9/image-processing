add_library(xpe_preprocess
    src/preprocess.cpp
)

target_include_directories(xpe_preprocess
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_preprocess
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(xpe_preprocess PROPERTIES
    OUTPUT_NAME "xpe_preprocess"
    VERSION ${PROJECT_VERSION}
)
