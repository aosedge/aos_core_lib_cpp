#!/bin/bash

set +x
set -euo pipefail

print_next_step()
{
    echo
    echo "====================================="
    echo "  $1"
    echo "====================================="
    echo
}

print_usage()
{
    echo
    echo "Usage: ./build.sh <command> [options]"
    echo
    echo "Commands:"
    echo
    echo "  build                      build target"
    echo "  test                       run tests only"
    echo "  coverage                   run tests with coverage"
    echo "  lint                       run static analysis (cppcheck)"
    echo "Options:"
    echo "  --clean                    clean build artifacts"
    echo "  --aos-service <services>   specify services (e.g., sm,mp,iam)"
    echo "  --ci                       use build-wrapper for CI analysis (SonarQube)"
    echo
}

error_with_usage()
{
    echo >&2 "ERROR: $1"
    echo

    print_usage

    exit 1
}

clean_build()
{
    print_next_step "Clean artifacts"

    rm -rf ./build/
    conan remove 'gtest*' -c
    conan remove 'grpc*' -c
    conan remove 'poco*' -c
    conan remove 'libcu*' -c
    conan remove 'opens*' -c
    conan remove 'pkcs11provider*' -c
}

conan_setup()
{
    print_next_step "Setting up conan default profile"
    conan profile detect --force

    print_next_step "Generate conan toolchain"
    conan install ./conan/ --output-folder build --settings=build_type=Debug --build=missing
}

build_project()
{
    local ci_flag="$1"

    print_next_step "Run cmake configure"
    cmake -S . -B build \
        -DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake \
        -DCMAKE_BUILD_TYPE=Debug \
        -G "Unix Makefiles" \
        -DCoverage_SONARQUBE=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DWITH_TEST=ON \
        -DWITH_COVERAGE=ON \
        -DWITH_MBEDTLS=ON \
        -DWITH_OPENSSL=ON

    if [ "$ci_flag" == "true" ]; then
        print_next_step "Run build-wrapper and build (CI mode)"
        build-wrapper-linux-x86-64 --out-dir "$BUILD_WRAPPER_OUT_DIR" cmake --build build/ --config Debug --parallel
    else
        print_next_step "Run build"
        cmake --build ./build/ --config Debug --parallel
    fi

    echo
    echo "Build succeeded!"
}

parse_arguments()
{
    local clean_flag=false
    local ci_flag=false

    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_flag=true
                shift
                ;;

            --ci)
                ci_flag=true
                shift
                ;;

            *)
                error_with_usage "Unknown option: $1"
                ;;
        esac
    done

    echo "$clean_flag:$ci_flag"
}

build_target()
{
    local clean_flag="$1"
    local ci_flag="$2"

    if [ "$clean_flag" == "true" ]; then
        clean_build

        return
    fi

    conan_setup
    build_project "$ci_flag"
}

run_tests()
{
    print_next_step "Run tests"
    cd ./build
    make test
    echo
    echo "Tests completed!"
}

run_coverage()
{
    print_next_step "Run tests with coverage"
    cd ./build
    make coverage
    echo
    echo "Coverage completed!"
}

run_lint()
{
    print_next_step "Run static analysis (cppcheck)"

    cppcheck --enable=all --inline-suppr --std=c++17 --error-exitcode=1 \
        --suppressions-list=./suppressions.txt --project=build/compile_commands.json --file-filter='src/*' \
        --file-filter='tests/*' --file-filter='include/*'

    echo
    echo "Static analysis completed!"
}

#=======================================================================================================================

if [ $# -lt 1 ]; then
    error_with_usage "Missing command"
fi

command="$1"
shift

case "$command" in
    build)
        args_result=$(parse_arguments "$@")
        clean_flag=$(echo "$args_result" | cut -d: -f1)
        ci_flag=$(echo "$args_result" | cut -d: -f2)
        build_target "$clean_flag" "$ci_flag"
        ;;

    test)
        run_tests
        ;;

    coverage)
        run_coverage
        ;;

    lint)
        run_lint
        ;;

    *)
        error_with_usage "Unknown command: $command"
        ;;
esac
