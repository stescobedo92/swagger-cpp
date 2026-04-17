# CMake package

`swaggercpp` installs a standard CMake package (`swaggercppConfig.cmake`, `swaggercppTargets*.cmake`). Any CMake consumer can pick it up via `find_package` once the install prefix is on `CMAKE_PREFIX_PATH`.

## Installing from source

```powershell
git clone https://github.com/stescobedo92/swagger-cpp
cmake -S swagger-cpp -B swagger-cpp/build `
    -DCMAKE_INSTALL_PREFIX=C:/opt/swaggercpp `
    -DSWAGGERCPP_BUILD_TESTS=OFF `
    -DSWAGGERCPP_BUILD_BENCHMARKS=OFF `
    -DSWAGGERCPP_BUILD_EXAMPLES=OFF
cmake --build swagger-cpp/build --config Release
cmake --install swagger-cpp/build --config Release
```

::: info
You must resolve `nlohmann_json`, `yaml-cpp` and `cpp-httplib` yourself before configuring — either through vcpkg, Conan, or a manual install. The config file lists them as transitive `find_dependency` calls.
:::

## Consuming

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_app LANGUAGES CXX)

list(APPEND CMAKE_PREFIX_PATH "C:/opt/swaggercpp")

find_package(swaggercpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

## What the config exports

- Target: `swaggercpp::swaggercpp`
- Headers: `${CMAKE_INSTALL_PREFIX}/include/swaggercpp/*.hpp`
- Library: `${CMAKE_INSTALL_PREFIX}/lib/swaggercpp.{lib,a,so,dylib}`
- CMake package files: `${CMAKE_INSTALL_PREFIX}/lib/cmake/swaggercpp/`

## pkg-config

A `.pc` file is also installed:

```powershell
pkg-config --cflags --libs swaggercpp
```

## See also

- [Installation](/install)
- [FetchContent](/packaging/fetchcontent)
