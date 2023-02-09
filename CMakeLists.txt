cmake_minimum_required(VERSION 3.14)

project("aos_core_lib_cpp")

# ######################################################################################################################
# Options
# ######################################################################################################################

option(WITH_TEST "build with test" OFF)

message(STATUS)
message(STATUS "${CMAKE_PROJECT_NAME} configuration:")
message(STATUS "CMAKE_BUILD_TYPE              = ${CMAKE_BUILD_TYPE}")
message(STATUS "CMAKE_INSTALL_PREFIX          = ${CMAKE_INSTALL_PREFIX}")
message(STATUS)
message(STATUS "WITH_TEST                     = ${WITH_TEST}")
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

# ######################################################################################################################
# Includes
# ######################################################################################################################

include_directories(src/common)

# ######################################################################################################################
# Targets
# ######################################################################################################################

add_subdirectory(src/sm)
add_subdirectory(src/iam)
