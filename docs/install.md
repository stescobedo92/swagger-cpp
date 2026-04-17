# Installation

`swaggercpp` targets **C++23** and is distributed through multiple channels so you can pick whichever matches your project's toolchain.

## Requirements

- A C++23-capable compiler: MSVC 19.38+ (VS 2022 17.8), GCC 13+, Clang 17+
- CMake 3.26 or newer
- Transitive dependencies (pulled automatically by your package manager):
  - `nlohmann-json` — JSON DOM
  - `yaml-cpp` — YAML parsing/emission
  - `cpp-httplib` — embedded Swagger UI server

::: tip
Tests and benchmarks additionally require `gtest` and `benchmark`, but they are off by default. Consumers never build them.
:::

## Option 1: vcpkg (recommended)

### Manifest mode

Add a `vcpkg.json` next to your top-level `CMakeLists.txt`:

```json
{
  "name": "my-app",
  "version": "0.1.0",
  "dependencies": [ "swaggercpp" ],
  "builtin-baseline": "<recent-sha-from-microsoft/vcpkg>"
}
```

Obtain a baseline SHA that already contains `swaggercpp`:

```powershell
git -C $env:VCPKG_ROOT rev-parse HEAD
```

Then point CMake at the vcpkg toolchain in `CMakePresets.json`:

```json
"cacheVariables": {
  "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
}
```

CMake will install `swaggercpp` and its dependencies into `out/build/<preset>/vcpkg_installed/` on the first `configure`.

### Classic mode

```powershell
vcpkg install swaggercpp
```

This installs into `<vcpkg-root>/installed/<triplet>/` and is visible to any project that wires in the vcpkg toolchain file **without** carrying a `vcpkg.json`.

::: warning Do not mix modes
If your project has a `vcpkg.json`, manifest mode activates automatically and the classic global install is ignored. In that case, `swaggercpp` **must** be declared in `dependencies` or `find_package(swaggercpp CONFIG REQUIRED)` will fail.
:::

## Option 2: Conan

A Conan recipe ships under `packaging/conan/`. To consume directly:

```ini
# conanfile.txt
[requires]
swaggercpp/0.2.1

[generators]
CMakeDeps
CMakeToolchain
```

And build:

```powershell
conan install . --output-folder=build --build=missing -s compiler.cppstd=23
cmake --preset conan-default
cmake --build --preset conan-release
```

See the [Conan guide](/packaging/conan) for registry setup and local `conan create` validation.

## Option 3: CMake `find_package`

If you installed the library manually (for example through vcpkg or `cmake --install`), consume it with:

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_app LANGUAGES CXX)

find_package(swaggercpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

## Option 4: FetchContent

For fully hermetic builds that pin a specific tag:

```cmake
include(FetchContent)

FetchContent_Declare(
  swaggercpp
  GIT_REPOSITORY https://github.com/stescobedo92/swagger-cpp.git
  GIT_TAG v0.2.1
)

FetchContent_MakeAvailable(swaggercpp)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

::: warning
With `FetchContent`, you must resolve `nlohmann_json`, `yaml-cpp` and `httplib` yourself — typically through vcpkg or Conan alongside.
:::

## Verifying the installation

Create a minimal program:

```cpp
// main.cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    auto doc = swaggercpp::DocumentReader::read(R"(
openapi: 3.1.0
info:
  title: Smoke
  version: 0.0.1
paths: {}
)");
    std::cout << (doc ? "ok" : "fail") << '\n';
}
```

Build and run — you should see `ok`.

## Next steps

- [Quick start](/quick-start) — 5-minute tutorial
- [Guide](/guide) — deeper narrative walkthrough
- [API reference](/api/) — full type and function listings
