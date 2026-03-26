# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

aos_core_lib_cpp is a C++17 library for the Aos Edge platform, providing Control Manager (cm),
Service Manager (sm), Identity & Access Management (iam), and shared utilities (common).

## Build Commands

```bash
# Build (Debug by default)
./build.sh build
./build.sh build --build-type Release
./build.sh build --clean          # clean rebuild

# Run all tests
./build.sh test

# Run a single test binary directly
./build/bin/<test_name>
# Or with gtest filter:
./build/bin/<test_name> --gtest_filter="TestSuite.TestName"

# Code coverage (requires lcov 2.0+)
./build.sh coverage

# Static analysis
./build.sh lint

# Generate docs
./build.sh doc
```

## Build System

- CMake 3.23+ with Conan package manager for dependencies (GTest 1.14.0, OpenSSL 3.2.1)
- Key CMake options: `WITH_TEST`, `WITH_COVERAGE`, `WITH_DOC`
- Custom CMake functions in `cmake/AddModule.cmake`: `add_module()`, `add_test()`, `add_exec()`
- Compiler flags: `-Wall -Werror -Wextra -Wpedantic`

## Code Architecture

```text
src/core/
├── cm/       # Control Manager - cloud-side orchestration: alerts, communication,
│             #   imagemanager, launcher, monitoring, smcontroller, unitconfig,
│             #   updatemanager, fileserver, iamclient, nodeinfoprovider, storagestate
├── sm/       # Service Manager - node-side service lifecycle: launcher,
│             #   imagemanager, resourcemanager, networkmanager, nodeconfig,
│             #   smclient, logging, database
├── common/   # Shared abstractions and utilities: types, tools, crypto, ocispec,
│             #   monitoring, logging, alerts, networkmanager, nodeconfig,
│             #   iamclient, cloudconnection, downloader, spaceallocator,
│             #   instancestatusprovider, pkcs11, version
└── iam/      # Identity & Access Management
```

- **Interface-driven design**: Interfaces live in `*/itf/` subdirs and use `*Itf` suffix with pure virtual methods
- **Error handling**: Custom `Error` type, not exceptions.
  Pattern: `if (!err.IsNone()) { return err; }` with `AOS_ERROR_WRAP(err)` for context
- **Crypto abstraction**: Dual OpenSSL and mbed TLS backends via conditional compilation
- **Custom containers**: `StaticArray`, `StaticString`, `Array`, `String` instead of STL;
  `UniquePtr` for smart pointers
- **Networking**: `common/networkmanager` exposes `NetworkProviderItf` and
  `PendingUpdateHandlerItf`; node-side network lifecycle (Create/Start/Stop/Release,
  deferred firewall updates, OnConnect/SyncNetworkState) lives in `sm/networkmanager`
  with its own CNI interface. `cm/` no longer owns a networkmanager module
- **Launcher API**: Cloud and node launchers expose `RunInstances` (renamed from
  `UpdateInstances`); request payloads carry runtime and unit-state lifecycle deps

## Testing

- Google Test + Google Mock in `*/tests/` subdirectories within each module
- Mocks in `*/tests/mocks/` (use `MOCK_METHOD`), stubs in `*/tests/stubs/`
- Integration tests in `src/core/{cm,sm}/tests/`
- Test config constants in `src/core/testconfig.hpp`

## Coding Conventions

- **Formatting**: WebKit-based clang-format, 120 char line limit
- **Header guards**: `#ifndef AOS_CORE_<PATH>_HPP_` / `#define` / `#endif`
- **Namespaces**: nested `aos::sm::launcher`, `aos::core::cm`, etc.
- **Naming**: `m` prefix for members (`mGroups`), `c` prefix for constants (`cMaxNumGroups`),
  `Itf` suffix for interfaces, `Mock` suffix for mocks
- **License header**: All files require SPDX Apache-2.0 header with EPAM copyright
- **Module CMakeLists**: Follow the pattern using `add_module()` / `add_test()` from `cmake/AddModule.cmake`

## CI Checks

GitHub Actions runs on push/PR: Debug build + test, Release build,
clang-format + cmake-format + markdownlint checks, cppcheck lint, SonarQube analysis.
