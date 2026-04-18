add_executable(xpe_common_unit
    test_xpe_common.cpp
)

target_link_libraries(xpe_common_unit
    PRIVATE xpe_common
            GTest::gtest_main
)

gtest_discover_tests(xpe_common_unit)
