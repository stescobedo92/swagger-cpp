# FetchContent

For fully hermetic builds, pull `swaggercpp` directly from GitHub with CMake's `FetchContent` module.

## Declaration

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_app LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
  swaggercpp
  GIT_REPOSITORY https://github.com/stescobedo92/swagger-cpp.git
  GIT_TAG        v0.2.1
)

# Silence tests/benchmarks/examples in the fetched build
set(SWAGGERCPP_BUILD_TESTS      OFF CACHE BOOL "" FORCE)
set(SWAGGERCPP_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(SWAGGERCPP_BUILD_EXAMPLES   OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(swaggercpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

## Transitive dependencies

`swaggercpp` itself does **not** fetch its dependencies. You must provide `nlohmann_json`, `yaml-cpp`, and `httplib` — typically via vcpkg or Conan alongside the FetchContent call.

```cmake
find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp      CONFIG REQUIRED)
find_package(httplib       CONFIG REQUIRED)
```

::: tip
A common pattern is to use vcpkg **manifest mode** for the three dependencies and FetchContent for `swaggercpp` itself, pinning a specific release tag to bypass the vcpkg baseline.
:::

## Pinning strategy

Pin by tag (`v0.2.1`), not by branch. Tags are immutable; branches move.

```cmake
GIT_TAG v0.2.1
```

## Pros and cons

| Pros                                           | Cons                                                 |
|------------------------------------------------|------------------------------------------------------|
| No package manager dependency                  | First configure pulls & compiles from source         |
| Exact version pinned in CMake                  | You still need the three runtime deps elsewhere      |
| Works in air-gapped environments (with mirror) | Longer clean builds than binary package reuse        |

## See also

- [Installation](/install)
- [vcpkg](/packaging/vcpkg)
- [CMake package](/packaging/cmake-package)
