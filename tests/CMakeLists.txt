include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS "test_*.cpp")

foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_FILE})
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_options(${TEST_NAME} PRIVATE -O3 -DNDEBUG)

    target_link_libraries(${TEST_NAME}
        PRIVATE gtest_main
        PRIVATE gtest
        PRIVATE implementation
    )

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    set_tests_properties(${TEST_NAME}
        PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
    )
endforeach()
