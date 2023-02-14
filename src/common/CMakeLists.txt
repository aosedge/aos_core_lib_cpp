set(TARGET aoscommoncpp)

# ######################################################################################################################
# Versioning
# ######################################################################################################################

find_package(Git)

if(NOT GIT_FOUND)
    set(GIT_EXECUTABLE git)
endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.hpp
    COMMAND
        ${CMAKE_COMMAND} -D GIT_EXECUTABLE=${GIT_EXECUTABLE} -D
        INPUT_FILE=${CMAKE_SOURCE_DIR}/src/common/version.hpp.in -D OUTPUT_FILE=${CMAKE_CURRENT_BINARY_DIR}/version.hpp
        -D GIT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_MODULE_PATH}/GenerateVersion.cmake
)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_library(${TARGET} INTERFACE)

target_sources(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/version.hpp)

# ######################################################################################################################
# Install
# ######################################################################################################################

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/version.hpp DESTINATION include/aoscore/common)

install(
    DIRECTORY .
    DESTINATION include/aoscore/common
    FILES_MATCHING
    PATTERN "*.hpp"
)

# ######################################################################################################################
# Test
# ######################################################################################################################

if(WITH_TEST)
    set(TEST_SOURCES utils/utils_test.cpp error/error_test.cpp log/log_test.cpp)

    add_executable(${TARGET}_test ${TEST_SOURCES})
    target_link_libraries(${TARGET}_test GTest::gtest_main)

    gtest_discover_tests(${TARGET}_test)
endif()
