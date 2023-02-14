# Aos core cpp libraries

## Configure

Configuration shall be done once for the desired options and targets.

```sh
mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
cmake ${PATH_TO_SOURCES} -D${VAR1}=${VAL1} -D{VAR2}=${VAL2} ...
```

Supported options:

| Option | Description |
| --- | --- |
| `WITH_TEST` | creates unit tests target |
| `WITH_COVERAGE` | creates coverage calculation target |
| `WITH_DOC` | creates documentation target |

Options should be set to `ON` or `OFF` value.

Supported variables:

| Variable | Description |
| --- | --- |
| `CMAKE_BUILD_TYPE` | `Release`, `Debug`, `RelWithDebInfo`, `MinSizeRel` |
| `CMAKE_INSTALL_PREFIX` | overrides default install path |

## Build libraries

Configure example:

```sh
cd ${BUILD_DIR}
cmake ../
```

Build:

```sh
cd ${BUILD_DIR}
make
```

## Run unit tests

Configure example:

```sh
cd ${BUILD_DIR}
cmake ../ -DWITH_TEST=ON
```

Build and run:

```sh
cd ${BUILD_DIR}
make ; make test
```

## Check coverage

`lcov` utility shall be installed on your host to run this target:

```sh
sudo apt install lcov
```

The coverage target is required test option to be `ON` as well as `Debug` build type.

Configure example:

```sh
cd ${BUILD_DIR}
cmake ../ -DWITH_TEST=ON -DWITH_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
```

Build and run:

```sh
cd ${BUILD_DIR}
make ; make coverage
```

The overall coverage rate will be displayed at the end of the coverage target output:

```sh
...
Overall coverage rate:
  lines......: 94.7% (72 of 76 lines)
  functions..: 100.0% (39 of 39 functions)
```

Detailed coverage information can be find by viewing `./coverage/index.html` file in your browser.

## Generate documentation

`doxygen` package should be installed before generation the documentaions:

```sh
sudo apt install doxygen
```

Configure example:

```sh
cd ${BUILD_DIR}
cmake ../ -DWITH_DOC=ON
```

Generate documentation:

```sh
cd ${BUILD_DIR}
make doc
```

The result documentation is located in `${BUILD_DIR}/doc folder`. And it can be viewed by opening
`./doc/html/index.html` file in your browser.

## Install libraries

The default install path can be overridden by setting `CMAKE_INSTALL_PREFIX` variable.

Configure example:

```sh
cd ${BUILD_DIR}
cmake ../ -DCMAKE_INSTALL_PREFIX=/my/location
```

Install:

```sh
cd ${BUILD_DIR}
make  install
```

## Development tools

The following tools are used for code formatting and analyzing:

| Tool | Description | Configuration | Link
| --- | --- | --- | --- |
| `clang-format` | used for source code formatting | .clang-format | <https://clang.llvm.org/docs/ClangFormat.html> |
| `cmake-format` | used for formatting cmake files | .cmake-format | <https://github.com/cheshirekow/cmake_format> |
| `cppcheck` | used for static code analyzing | | <https://cppcheck.sourceforge.io/> |
