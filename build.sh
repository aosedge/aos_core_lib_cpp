#!/bin/bash

set +x
set -euo pipefail

print_next_step() {
    echo
    echo "====================================="
    echo "  $1"
    echo "====================================="
    echo
}

print_usage() {
    echo
    echo "Usage: ./build.sh <command> [options]"
    echo
    echo "Commands:"
    echo "  build                      builds target"
    echo "  test                       runs tests only"
    echo "  coverage                   runs tests with coverage"
    echo "  lint                       runs static analysis (cppcheck)"
    echo "  doc                        generates documentation"
    echo
    echo "Options:"
    echo "  --clean                    cleans build artifacts"
    echo "  --aos-service <services>   specifies services (e.g., sm,mp,iam)"
    echo "  --ci                       uses build-wrapper for CI analysis (SonarQube)"
    echo
}

error_with_usage() {
    echo >&2 "ERROR: $1"
    echo

    print_usage

    exit 1
}

clean_build() {
    print_next_step "Clean artifacts"

    rm -rf ./build/
    conan remove 'gtest*' -c
    conan remove 'grpc*' -c
    conan remove 'poco*' -c
    conan remove 'libcu*' -c
    conan remove 'opens*' -c
    conan remove 'pkcs11provider*' -c
}

conan_setup() {
    print_next_step "Setting up conan default profile"

    conan profile detect --force

    print_next_step "Generate conan toolchain"

    conan install ./conan/ --output-folder build --settings=build_type=Debug --build=missing
}

build_project() {
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

    if [ "$ARG_CI_FLAG" == "true" ]; then
        print_next_step "Run build-wrapper and build (CI mode)"

        build-wrapper-linux-x86-64 --out-dir "$BUILD_WRAPPER_OUT_DIR" cmake --build ./build/ --config Debug --parallel
    else
        print_next_step "Run build"

        cmake --build ./build/ --config Debug --parallel
    fi

    echo
    echo "Build succeeded!"
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
        --clean)
            ARG_CLEAN_FLAG=true
            shift
            ;;

        --ci)
            ARG_CI_FLAG=true
            shift
            ;;

        *)
            error_with_usage "Unknown option: $1"
            ;;
        esac
    done
}

build_target() {
    if [ "$ARG_CLEAN_FLAG" == "true" ]; then
        clean_build

        return
    fi

    conan_setup
    build_project
}

run_tests() {
    print_next_step "Run tests"

    cd ./build
    make test
    echo
    echo "Tests completed!"
}

run_coverage() {
    print_next_step "Run tests with coverage"

    cd ./build
    make coverage
    echo
    echo "Coverage completed!"
}

run_lint() {
    print_next_step "Run static analysis (cppcheck)"

    cppcheck --enable=all --inline-suppr --std=c++17 --error-exitcode=1 \
        --suppressions-list=./suppressions.txt --project=build/compile_commands.json --file-filter='src/*' \
        --file-filter='tests/*' --file-filter='include/*'

    echo
    echo "Static analysis completed!"
}

build_doc() {
    print_next_step "Build documentation"

    cd ./build

    cmake -DWITH_DOC=ON ../
    make doc

    echo
    echo "Documentation generated!"
}

#=======================================================================================================================

if [ $# -lt 1 ]; then
    error_with_usage "Missing command"
fi

command="$1"
shift

ARG_CLEAN_FLAG=false
ARG_AOS_SERVICES=""
ARG_CI_FLAG=false

case "$command" in
build)
    parse_arguments "$@"
    build_target
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

doc)
    build_doc
    ;;

*)
    error_with_usage "Unknown command: $command"
    ;;
esac
