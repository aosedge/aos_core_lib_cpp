cmake_minimum_required(VERSION 3.14)

project("aos_core_lib_cpp")

# ######################################################################################################################
# Options
# ######################################################################################################################

option(WITH_TEST "build with test" OFF)
option(WITH_COVERAGE "build with coverage" OFF)
option(WITH_DOC "build with documenation" OFF)

message(STATUS)
message(STATUS "${CMAKE_PROJECT_NAME} configuration:")
message(STATUS "CMAKE_BUILD_TYPE              = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX          = ${CMAKE_INSTALL_PREFIX}")
message(STATUS)
message(STATUS "WITH_TEST                     = ${WITH_TEST}")
message(STATUS "WITH_COVERAGE                 = ${WITH_COVERAGE}")
message(STATUS "WITH_DOC                      = ${WITH_DOC}")
message(STATUS)

# ######################################################################################################################
# Compiler flags
# ######################################################################################################################

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror -Wextra -Wpedantic")
set(CMAKE_CXX_STANDARD 14)

# ######################################################################################################################
# Dependencies
# ######################################################################################################################

if(WITH_TEST)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)

    enable_testing()
endif()

if(WITH_COVERAGE)
    set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)

    include(CodeCoverage)

    append_coverage_compiler_flags()

    set(COVERAGE_EXCLUDES "build/*" "/usr/*" "*_test.cpp")

    setup_target_for_coverage_lcov(NAME coverage EXECUTABLE ctest)
endif()

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(src/common)

# ######################################################################################################################
# Targets
# ######################################################################################################################

add_subdirectory(src/sm)
add_subdirectory(src/iam)

# ######################################################################################################################
# Doc
# ######################################################################################################################

if(WITH_DOC)
    find_package(Doxygen)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doxygen.cfg ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg @ONLY)

    add_custom_target(
        doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doxygen.cfg
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
endif()
