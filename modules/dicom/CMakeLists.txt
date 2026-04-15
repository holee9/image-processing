add_library(xpe_dicom
    src/dicom.cpp
)

target_include_directories(xpe_dicom
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
)

target_link_libraries(xpe_dicom
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
)

set_target_properties(xpe_dicom PROPERTIES
    OUTPUT_NAME "xpe_dicom"
    VERSION ${PROJECT_VERSION}
)
