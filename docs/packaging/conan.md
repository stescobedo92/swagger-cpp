# Conan

`swaggercpp` ships a Conan recipe under `packaging/conan/`. You can consume it from ConanCenter once published, or validate/build locally from the recipe in this repository.

## `conanfile.txt` for consumers

```ini
[requires]
swaggercpp/0.2.1

[generators]
CMakeDeps
CMakeToolchain

[options]
&:shared = False
```

Then:

```powershell
conan install . --output-folder=build --build=missing -s compiler.cppstd=23
cmake --preset conan-default
cmake --build --preset conan-release
```

## CMake glue

The recipe declares CMake targets that `find_package` picks up:

```cmake
find_package(swaggercpp CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

## Local validation

To build and test the recipe against a checkout of the source tree:

```powershell
conan create . --test-folder=packaging/conan/test_package -s compiler.cppstd=23
```

This creates the package in your local Conan cache and runs the test package (a minimal C++ program that links against `swaggercpp::swaggercpp`).

## `conanfile.py` highlights

The recipe in `packaging/conan/conanfile.py`:

- Declares the three runtime dependencies — `nlohmann_json`, `yaml-cpp`, `httplib` — and pins compatible versions.
- Exposes options to toggle tests/benchmarks/examples, all **off by default** for consumers.
- Emits a CMake package config named `swaggercpp` with a namespaced target `swaggercpp::swaggercpp`.
- Marks minimum `compiler.cppstd=23`.

## Typical consumer profile

```ini
# ~/.conan2/profiles/default
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=23
compiler.runtime=dynamic
compiler.version=194
os=Windows
```

## See also

- [Installation](/install)
- [vcpkg alternative](/packaging/vcpkg)
- Local recipe: [`packaging/conan/`](https://github.com/stescobedo92/swagger-cpp/tree/master/packaging/conan)
