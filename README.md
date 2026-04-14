# swaggercpp

`swaggercpp` is a modern C++23 library for reading, writing, validating, traversing, and packaging Swagger/OpenAPI documents with a professional distribution setup.

## What it includes

- Typed document model inspired by the ergonomics of Microsoft.OpenApi for .NET
- JSON and YAML parsing/serialization
- Validation pipeline with actionable issues
- Visitor/walker API for tooling and transformations
- Unit tests with GoogleTest
- Microbenchmarks with Google Benchmark
- Consumer-facing CMake package exports
- Conan recipe and test package
- vcpkg overlay/custom-registry packaging assets
- GitHub Actions for CI, packaging, and release automation

## Architecture

- `include/swaggercpp`: public API and document model
- `src`: parser, writer, validator, traversal, and private helpers
- `tests`: unit tests
- `benchmarks`: performance measurements
- `examples`: usage samples
- `packaging/conan`: Conan consumer validation package
- `packaging/vcpkg`: overlay/custom-registry assets and helper scripts
- `.github/workflows`: CI/CD pipelines

The parser focuses on OpenAPI 3.0/3.1 first, with partial Swagger 2.0 compatibility surfaced as warnings so the library stays honest about current fidelity.

## Context7-backed decisions

This project was shaped with updated guidance gathered through Context7 for:

- `GoogleTest`: modern CMake usage via `find_package(GTest CONFIG REQUIRED)`, `GTest::gtest_main`, and `gtest_discover_tests`
- `Google Benchmark`: `find_package(benchmark CONFIG REQUIRED)` with `benchmark::benchmark` and `benchmark::benchmark_main`
- `nlohmann/json`: safe custom serialization patterns with explicit field handling
- `yaml-cpp`: node-type-aware parsing and YAML emission

## Build

On this workstation the project is configured through `CMakePresets.json` and the local vcpkg installation:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" && "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --preset default'
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" && "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build --preset default --parallel'
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" && ctest --test-dir build\default --output-on-failure'
```

## Benchmark

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" && build\default\swaggercpp-benchmarks.exe --benchmark_min_time=0.02s'
```

Measured locally in this environment:

- `BM_ParseDocument`: about `17.4 us` CPU per iteration on the current sample payload

## Conan

Create and validate the package:

```powershell
conan create . --test-folder=packaging/conan/test_package -s compiler.cppstd=23
```

## vcpkg

Use the local overlay port:

```powershell
C:\Users\stesc\cpp-packages\vcpkg\vcpkg.exe install swaggercpp --overlay-ports=packaging\vcpkg\ports --classic
```

For a registry-based flow, point `vcpkg-configuration.json` to `packaging/vcpkg/registry`.

## Examples

- `examples/load_validate.cpp`
- `examples/build_petstore.cpp`
- `examples/mutate_document.cpp`

## Roadmap

- Increase Swagger 2.0 coverage
- Expand security flows and callbacks/webhooks
- Add schema/reference resolution layers
- Add richer diagnostics and source locations
