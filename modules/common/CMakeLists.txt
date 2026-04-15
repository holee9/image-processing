add_library(xpe_common SHARED
    src/xpe_common.cpp
    src/xpe_memory.cpp
)

target_compile_definitions(xpe_common
    PRIVATE XPE_DLL_EXPORT
)

target_include_directories(xpe_common
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_common
    PRIVATE spdlog::spdlog
            nlohmann_json::nlohmann_json
            fmt::fmt
)

set_target_properties(xpe_common PROPERTIES
    OUTPUT_NAME "xpe_common"
    VERSION ${PROJECT_VERSION}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
