add_executable(xpe_common_smoke
    test_common_smoke.cpp
)

target_link_libraries(xpe_common_smoke
    PRIVATE xpe_common
            GTest::gtest_main
)

gtest_discover_tests(xpe_common_smoke)
