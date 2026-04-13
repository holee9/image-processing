add_executable(xpe_common_smoke
    test_common_smoke.cpp
)

target_link_libraries(xpe_common_smoke
    PRIVATE xpe_common
)

add_test(
    NAME xpe_common_smoke
    COMMAND xpe_common_smoke
)
