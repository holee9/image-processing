if(BUILD_DICOM)
    find_package(DCMTK CONFIG REQUIRED)
    find_package(OpenJPEG CONFIG REQUIRED)
else()
    message(FATAL_ERROR "BUILD_DICOM option is required for xpe_dicom module")
endif()

add_library(xpe_dicom SHARED
    src/dicom.cpp
    src/DicomReader.cpp
    src/DicomWriter.cpp
    src/DicomValidator.cpp
    src/DicomNetworkSCU.cpp
)

target_include_directories(xpe_dicom
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xpe_dicom
    PUBLIC  xpe_common
    PRIVATE spdlog::spdlog
            fmt::fmt
            DCMTK::dcmnet
            DCMTK::dcmdata
            DCMTK::ofstd
            DCMTK::oflog
            openjp2
)

set_target_properties(xpe_dicom PROPERTIES
    OUTPUT_NAME "xpe_dicom"
    VERSION ${PROJECT_VERSION}
)

if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)

    add_executable(test_xpe_dicom
        tests/test_dicom_reader.cpp
        tests/test_dicom_writer.cpp
        tests/test_dicom_validator.cpp
        tests/test_dicom_network_scu.cpp
    )

    target_include_directories(test_xpe_dicom
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(test_xpe_dicom
        PRIVATE xpe_dicom
                GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(test_xpe_dicom)
endif()
