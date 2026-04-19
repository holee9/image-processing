add_executable(xpe_preprocess_smoke
    test_preprocess_smoke.cpp
)

target_link_libraries(xpe_preprocess_smoke
    PRIVATE xpe_preprocess
            GTest::gtest_main
)

gtest_discover_tests(xpe_preprocess_smoke)
