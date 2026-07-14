<!-- markdownlint-disable-next-line MD041 -->
| Branch | CI | Coverage | Quality Gate |
|--------|----|----------|--------------|
| main | [![ci][ci-main]][ci-main-link] | [![coverage][cov-main]][cov-main-link] | [![Quality Gate][qg-main]][qg-main-link] |
| develop | [![ci][ci-dev]][ci-dev-link] | [![coverage][cov-dev]][cov-dev-link] | [![Quality Gate][qg-dev]][qg-dev-link] |

[ci-main]: https://github.com/aosedge/aos_core_lib_cpp/actions/workflows/build-test.yaml/badge.svg?branch=main
[ci-main-link]: https://github.com/aosedge/aos_core_lib_cpp/actions/workflows/build-test.yaml?query=branch%3Amain
[cov-main]: https://sonarcloud.io/api/project_badges/measure?project=aosedge_aos_core_lib_cpp&metric=coverage&branch=main
[cov-main-link]: https://sonarcloud.io/summary/new_code?id=aosedge_aos_core_lib_cpp&branch=main
[qg-main]: https://sonarcloud.io/api/project_badges/measure?project=aosedge_aos_core_lib_cpp&metric=alert_status&branch=main
[qg-main-link]: https://sonarcloud.io/summary/new_code?id=aosedge_aos_core_lib_cpp&branch=main
[ci-dev]: https://github.com/aosedge/aos_core_lib_cpp/actions/workflows/build-test.yaml/badge.svg?branch=develop
[ci-dev-link]: https://github.com/aosedge/aos_core_lib_cpp/actions/workflows/build-test.yaml?query=branch%3Adevelop
[cov-dev]: https://sonarcloud.io/api/project_badges/measure?project=aosedge_aos_core_lib_cpp&metric=coverage&branch=develop
[cov-dev-link]: https://sonarcloud.io/summary/new_code?id=aosedge_aos_core_lib_cpp&branch=develop
[qg-dev]: https://sonarcloud.io/api/project_badges/measure?project=aosedge_aos_core_lib_cpp&metric=alert_status&branch=develop
[qg-dev-link]: https://sonarcloud.io/summary/new_code?id=aosedge_aos_core_lib_cpp&branch=develop

# AosCore cpp libraries

This document describes two ways to build the project from scratch:

* directly on the host — see [Prepare build environment](#prepare-build-environment) and
  [Build with build.sh](#build-with-buildsh);
* inside a Docker container, without installing anything on the host — see
  [Build inside a Docker container](#build-inside-a-docker-container).

If you only want a quick build with the default options, use `./build.sh build`. If you need a custom configuration
(for example a `Release` build, or OpenSSL instead of MbedTLS), use the
[manual build with individual CMake options](#custom-build-with-individual-cmake-options).

## Get the sources

```console
git clone https://github.com/aosedge/aos_core_lib_cpp.git
cd aos_core_lib_cpp
```

The MbedTLS crypto backend is fetched automatically at configure time (via CMake `FetchContent`), so `git` must be
available on the host.

## Prepare build environment

The instructions below target Ubuntu. Other distributions provide the same packages under similar names.

* Install the build tools and libraries:

```console
sudo apt install build-essential git python3-pip python3-jinja2 softhsm2
```

| Package | Why it is needed |
| --- | --- |
| `build-essential` | C/C++ compiler and `make` to build the project and the Conan dependencies |
| `git` | clone the repository and fetch MbedTLS at configure time |
| `python3-jinja2` | required by the MbedTLS code generator during configuration |
| `softhsm2` | provides `libsofthsm2`, required by the crypto unit tests (`WITH_TEST=ON`) |

* Install `conan` (package manager for the external dependencies, e.g. OpenSSL and GoogleTest):

```console
pip install conan
```

* Install `cmake` (version 3.23 or greater is required):

```console
pip install cmake
```

The `cmake` shipped with Ubuntu 22.04 (3.22) is too old. On Ubuntu 24.04 the distribution `cmake` (3.28) is new enough,
so you may use `sudo apt install cmake` instead. The [Kitware APT repository](https://apt.kitware.com/) is another
option for an up-to-date `cmake`.

* Install `lcov`:

```console
sudo apt install lcov
```

`lcov` is required to generate the code coverage report. It is also required to **configure** the project with
`./build.sh build`, because that script always enables the coverage target. Without `lcov` the configuration step
fails with `lcov not found`. Version 2.0 or greater is required. The version shipped with Ubuntu 22.04 (1.x) is
too old; in that case download and install it manually:

```console
wget https://launchpad.net/ubuntu/+source/lcov/2.0-4ubuntu2/+build/27959742/+files/lcov_2.0-4ubuntu2_all.deb
sudo dpkg -i lcov_2.0-4ubuntu2_all.deb
sudo apt-get install -f
```

The last `apt-get install -f` step pulls in the Perl dependencies that the `.deb` requires.

## Build with build.sh

`build.sh` is the recommended entry point. To make a build, run:

```console
./build.sh build
```

It installs all external dependencies via Conan, fetches and builds MbedTLS, creates the `./build` directory and builds
the AosCore libraries together with the unit test and coverage targets. The configuration used by `build.sh build` is
fixed: `WITH_TEST=ON`, `WITH_COVERAGE=ON`, `WITH_MBEDTLS=ON` and `WITH_OPENSSL=ON`.

`build.sh` accepts the following commands:

| Command | Description |
| --- | --- |
| `build` | configures and builds the project |
| `test` | runs the unit tests (see [Run unit tests](#run-unit-tests)) |
| `coverage` | runs the tests and collects coverage (see [Check coverage](#check-coverage)) |
| `lint` | runs static analysis with `cppcheck` |
| `doc` | generates the documentation (see [Generate documentation](#generate-documentation)) |

And the following options:

| Option | Description |
| --- | --- |
| `--clean` | removes the `./build` directory before building |
| `--ci` | builds through `build-wrapper` for CI / SonarQube analysis |
| `--parallel <N>` | number of parallel build jobs (default: all available cores) |
| `--build-type <type>` | `Debug` (default), `Release`, `RelWithDebInfo` or `MinSizeRel` |

Example — a clean `Release` build on 8 jobs:

```console
./build.sh build --clean --build-type Release --parallel 8
```

## Custom build with individual CMake options

If you need options that `build.sh` does not expose (for example building only the OpenSSL backend, or building without
tests/coverage), run Conan and CMake manually. From the repository root:

```console
mkdir -p build
cd build
conan install ../conan/ --output-folder . --settings=build_type=Debug --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake -DWITH_TEST=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel
```

`conan install` generates `conan_toolchain.cmake` and provides the external dependencies; the `cmake` configure step
must therefore be run with that toolchain file.

CMake options:

| Option | Value | Default | Description |
| --- | --- | --- | --- |
| `WITH_TEST` | `ON`, `OFF` | `OFF` | creates the unit tests target (requires `softhsm2`) |
| `WITH_COVERAGE` | `ON`, `OFF` | `OFF` | creates the coverage calculation target (requires `lcov`) |
| `WITH_DOC` | `ON`, `OFF` | `OFF` | creates the documentation target (requires `doxygen`) |
| `WITH_MBEDTLS` | `ON`, `OFF` | `ON` | builds the MbedTLS crypto provider |
| `WITH_OPENSSL` | `ON`, `OFF` | `OFF` | builds the OpenSSL crypto provider |

At least one of `WITH_MBEDTLS` or `WITH_OPENSSL` must be enabled. Enabling `WITH_TEST` pulls in the crypto unit tests,
which need `softhsm2`; enabling `WITH_COVERAGE` needs `lcov`.

CMake variables:

| Variable | Description |
| --- | --- |
| `CMAKE_BUILD_TYPE` | `Release`, `Debug`, `RelWithDebInfo`, `MinSizeRel` |
| `CMAKE_INSTALL_PREFIX` | overrides the default install path |

## Run unit tests

Build and run:

```console
./build.sh test
```

## Check coverage

`lcov` shall be installed on your host to run this target. See [Prepare build environment](#prepare-build-environment).

Build and run:

```console
./build.sh coverage
```

The overall coverage rate will be displayed at the end of the coverage target output:

```console
...
Overall coverage rate:
  lines......: 94.7% (72 of 76 lines)
  functions..: 100.0% (39 of 39 functions)
```

Detailed coverage information can be find by viewing `./coverage/index.html` file in your browser.

## Generate documentation

`doxygen` package should be installed before generation the documentations:

```console
sudo apt install doxygen
```

Generate documentation:

```console
./build.sh doc
```

The result documentation is located in `build/doc` folder. And it can be viewed by opening `build/doc/html/index.html`
file in your browser.

## Install libraries

The default install path can be overridden by setting `CMAKE_INSTALL_PREFIX` variable.

Configure example:

```sh
cd build/
cmake ../ -DCMAKE_INSTALL_PREFIX=/my/location
```

Install:

```sh
cd build/
make  install
```

## Build inside a Docker container

The provided container already contains every host dependency (compiler, `cmake`, `conan`, `lcov`, `softhsm2`, ...) and
a pre-populated Conan cache, so it is the easiest way to build without changing your host.

Build the container image (run from the repository root):

```console
docker build -t aos-core-build:latest -f docker/Dockerfile .
```

Start a container with the sources mounted into it:

```console
docker run -v ${PWD}:/opt/aos_core_lib_cpp -w /opt/aos_core_lib_cpp -it --rm aos-core-build:latest
```

Inside the container all the commands described above are available. For the default build run:

```console
./build.sh build
```

Or, for a custom configuration, the manual flow works the same way:

```console
mkdir -p build && cd build
conan install ../conan/ --output-folder . --settings=build_type=Release --build=missing
cmake .. -DCMAKE_TOOLCHAIN_FILE=./conan_toolchain.cmake -DWITH_OPENSSL=ON -DWITH_MBEDTLS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

You can also run a one-off build without an interactive shell:

```console
docker run -v ${PWD}:/opt/aos_core_lib_cpp -w /opt/aos_core_lib_cpp --rm aos-core-build:latest ./build.sh build
```

## Development tools

The following tools are used for code formatting and analyzing:

| Tool | Description | Configuration | Link |
| --- | --- | --- | --- |
| `clang-format` | used for source code formatting | .clang-format | <https://clang.llvm.org/docs/ClangFormat.html> |
| `cmake-format` | used for formatting cmake files | .cmake-format | <https://github.com/cheshirekow/cmake_format> |
| `cppcheck` | used for static code analyzing | | <https://cppcheck.sourceforge.io/> |
