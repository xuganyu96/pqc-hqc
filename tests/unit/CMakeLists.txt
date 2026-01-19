cmake_minimum_required(VERSION 3.21)
project(hqc_unit_tests C CXX)

enable_testing()

if(NOT DEFINED HQC_ARCH)
    message(FATAL_ERROR "HQC_ARCH must be set (ref or x86_64)")
endif()
if(HQC_ARCH STREQUAL "x86_64" AND NOT DEFINED HQC_X86_IMPL)
    message(FATAL_ERROR "HQC_X86_IMPL must be set when HQC_ARCH=x86_64")
endif()
if(NOT DEFINED VARIANTS)
    message(FATAL_ERROR "VARIANTS (e.g. \"hqc-1;hqc-3;hqc-5\") must be set by tests/CMakeLists.txt")
endif()

option(HQC_ENABLE_NTL_TESTS "Enable NTL verification tests" ON)

set(TESTS_DIR     ${CMAKE_SOURCE_DIR}/tests/unit)
set(GF_TEST_SRC   ${TESTS_DIR}/test_gf.c)
set(RS_TEST_SRC   ${TESTS_DIR}/test_reed_solomon.c)
set(VEC_TEST_SRC  ${TESTS_DIR}/test_vector.c)
set(MAIN_TEST_SRC ${TESTS_DIR}/tests_main.c)
set(GF2X_NTL_SRC  ${TESTS_DIR}/test_vect_mul.cpp)

set(COMMON_INCLUDES
        ${CMAKE_SOURCE_DIR}/src/common
        ${CMAKE_SOURCE_DIR}/lib/fips202
        ${CMAKE_SOURCE_DIR}/tests/external/munit
        ${CMAKE_SOURCE_DIR}/tests
)

set(HQC_NTL_FOUND OFF)
if(HQC_ENABLE_NTL_TESTS)
    find_path(NTL_INCLUDE_DIR NAMES NTL/GF2X.h)
    find_library(NTL_LIBRARY NAMES ntl)
    find_library(GMP_LIBRARY NAMES gmp)

    if(NOT NTL_INCLUDE_DIR OR NOT NTL_LIBRARY OR NOT GMP_LIBRARY)
        message(WARNING "NTL/GMP not found. NTL-based tests will be skipped. Install NTL/GMP or set HQC_ENABLE_NTL_TESTS=OFF to silence this.")
        set(HQC_NTL_FOUND OFF)
    else()
        set(HQC_NTL_FOUND ON)
    endif()
endif()

foreach(param_variant IN LISTS VARIANTS)
    string(REPLACE "-" "_" safe_param ${param_variant})

    if(HQC_ARCH STREQUAL "ref")
        set(lib_target    ${safe_param}_ref)
        set(exe_name      test_unit_${safe_param}_ref)

        set(PARAM_INCLUDES
                ${CMAKE_SOURCE_DIR}/src/common/${param_variant}
                ${CMAKE_SOURCE_DIR}/src/ref
                ${CMAKE_SOURCE_DIR}/src/ref/${param_variant}
        )
    else()
        set(lib_target    ${safe_param}_${HQC_ARCH})
        set(exe_name      test_unit_${safe_param}_${HQC_X86_IMPL})

        set(PARAM_INCLUDES
                ${CMAKE_SOURCE_DIR}/src/common/${param_variant}
                ${CMAKE_SOURCE_DIR}/src/x86_64/common
                ${CMAKE_SOURCE_DIR}/src/x86_64/common/${param_variant}
                ${CMAKE_SOURCE_DIR}/src/x86_64/${HQC_X86_IMPL}
        )
    endif()

    set(unit_sources
            ${GF_TEST_SRC}
            ${RS_TEST_SRC}
            ${VEC_TEST_SRC}
            ${MAIN_TEST_SRC}
    )
    if(HQC_NTL_FOUND)
        list(APPEND unit_sources ${GF2X_NTL_SRC})
    endif()

    add_executable(${exe_name} ${unit_sources})

    target_link_libraries(${exe_name} PRIVATE
            munit
            ${lib_target}
            fips202
    )

    target_compile_definitions(${exe_name} PRIVATE
            $<$<STREQUAL:${HQC_ARCH},x86_64>:AVX2_MODE>
            $<$<NOT:$<STREQUAL:${HQC_ARCH},x86_64>>:REF_MODE>
            $<$<STREQUAL:${HQC_ARCH},x86_64>:HQC_X86_IMPL=${HQC_X86_IMPL}>
    )

    target_include_directories(${exe_name} PRIVATE
            ${COMMON_INCLUDES}
            ${PARAM_INCLUDES}
    )

    if(HQC_NTL_FOUND)
        target_include_directories(${exe_name} PRIVATE
                ${NTL_INCLUDE_DIR}
        )
        target_link_libraries(${exe_name} PRIVATE
                ${NTL_LIBRARY}
                ${GMP_LIBRARY}
        )
        target_compile_definitions(${exe_name} PRIVATE
                HQC_HAVE_NTL=1
        )
        set_target_properties(${exe_name} PROPERTIES
                CXX_STANDARD 17
                CXX_STANDARD_REQUIRED ON
                CXX_EXTENSIONS OFF
        )
        if(HQC_ARCH STREQUAL "x86_64")
            target_compile_options(${exe_name} PRIVATE
                    -mavx -mavx2 -mbmi -mpclmul
            )
        endif()
    endif()

    add_test(NAME ${exe_name} COMMAND ${exe_name})
    set_tests_properties(${exe_name} PROPERTIES LABELS "unit")
endforeach()
