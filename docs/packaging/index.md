# Packaging

`swaggercpp` is distributed through multiple channels. Pick the one that matches your toolchain.

## Channels

| Channel                                                   | Recommended for                              |
|-----------------------------------------------------------|----------------------------------------------|
| [vcpkg](/packaging/vcpkg)                                 | Most C++ projects; manifest or classic mode  |
| [Conan](/packaging/conan)                                 | Teams already on Conan profiles              |
| [CMake package](/packaging/cmake-package)                 | Consumers that install into a fixed prefix   |
| [FetchContent](/packaging/fetchcontent)                   | Hermetic, fully source-built pipelines       |

## At a glance

### vcpkg manifest

```json
// vcpkg.json
{ "dependencies": [ "swaggercpp" ], "builtin-baseline": "<sha>" }
```

### Conan

```ini
# conanfile.txt
[requires]
swaggercpp/0.2.1
```

### CMake `find_package`

```cmake
find_package(swaggercpp CONFIG REQUIRED)
target_link_libraries(app PRIVATE swaggercpp::swaggercpp)
```

### FetchContent

```cmake
FetchContent_Declare(swaggercpp
  GIT_REPOSITORY https://github.com/stescobedo92/swagger-cpp.git
  GIT_TAG v0.2.1)
FetchContent_MakeAvailable(swaggercpp)
```

See each page for full setup, trade-offs, and gotchas.
