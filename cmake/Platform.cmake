# Platform detection and SIMD flags

include(CheckCXXCompilerFlag)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2 AND NOT MSVC)
        add_compile_options(-mavx2)
    elseif(MSVC)
        add_compile_options(/arch:AVX2)
    endif()
    message(STATUS "Platform: x86-64, AVX2 enabled")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64")
    message(STATUS "Platform: ARM64, NEON enabled by default")
endif()
