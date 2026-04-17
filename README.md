# swaggercpp

[![vcpkg port](https://img.shields.io/badge/vcpkg-swaggercpp-0b7dbb?logo=microsoft&logoColor=white)](https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp)
[![Conan Center](https://img.shields.io/badge/Conan-recipe-6699cc?logo=conan&logoColor=white)](https://github.com/stescobedo92/swagger-cpp/tree/master/packaging/conan)

`swaggercpp` is a modern C++23 library for reading, writing, validating, traversing, and packaging Swagger/OpenAPI documents with a professional distribution setup.

## What it includes

- Typed document model inspired by the ergonomics of Microsoft.OpenApi for .NET
- JSON and YAML parsing/serialization
- Validation pipeline with actionable issues
- Visitor/walker API for tooling and transformations
- Embedded Swagger UI server that can open the browser when your binary starts
- Unit tests with GoogleTest
- Microbenchmarks with Google Benchmark
- Consumer-facing CMake package exports
- Conan recipe and test package
- vcpkg overlay/custom-registry packaging assets

## Architecture

- `include/swaggercpp`: public API and document model
- `src`: parser, writer, validator, traversal, and private helpers
- `tests`: unit tests
- `benchmarks`: performance measurements
- `examples`: usage samples
- `packaging/conan`: Conan consumer validation package
- `packaging/vcpkg`: overlay/custom-registry assets and helper scripts
- `.github/workflows`: CI/CD pipelines

## Embedded Swagger UI

If you want an application built with `swaggercpp` to open a browser and show Swagger UI at runtime, use `SwaggerUiServer::start(...)`:

```cpp
#include "swaggercpp/swaggercpp.hpp"

int main() {
    swaggercpp::Document document;
    document.openapi = "3.1.0";
    document.info.title = "Customer API";
    document.info.version = "1.0.0";

    swaggercpp::Operation operation;
    operation.operation_id = "listCustomers";
    operation.responses.emplace("200", swaggercpp::Response {.description = "ok"});

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);
    document.paths.emplace("/customers", path_item);

    auto session = swaggercpp::SwaggerUiServer::start(document, {
        .title = "Customer API Docs",
    });

    if (!session) {
        return 1;
    }

    session.value().wait();
    return 0;
}
```

This starts a local HTTP server, serves Swagger UI plus `/openapi.json`, opens the default browser automatically, and keeps the process alive until the app exits.

## Usage examples

### Parse and validate a document

```cpp
#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    const auto result = swaggercpp::DocumentReader::read(R"(
openapi: 3.1.0
info:
  title: Inventory API
  version: 1.0.0
paths:
  /items:
    get:
      operationId: listItems
      responses:
        "200":
          description: ok
)");

    if (!result) {
        return 1;
    }

    swaggercpp::DocumentValidator validator;
    const auto report = validator.validate(result.value());
    std::cout << "valid=" << std::boolalpha << report.ok() << '\n';
    return report.ok() ? 0 : 2;
}
```

### Generate OpenAPI JSON from code

```cpp
#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    swaggercpp::Document document;
    document.openapi = "3.1.0";
    document.info.title = "Billing API";
    document.info.version = "2026.04";

    swaggercpp::Operation operation;
    operation.operation_id = "listInvoices";
    operation.responses.emplace("200", swaggercpp::Response {.description = "ok"});

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);
    document.paths.emplace("/invoices", path_item);

    const auto json = swaggercpp::DocumentWriter::write_json(document);
    if (!json) {
        return 1;
    }

    std::cout << json.value() << '\n';
    return 0;
}
```

### Serve Swagger UI directly from your app

```cpp
#include "swaggercpp/swaggercpp.hpp"

int main() {
    auto session = swaggercpp::SwaggerUiServer::start_file("openapi.yaml", {
        .title = "My API Docs",
        .open_browser = true,
    });

    if (!session) {
        return 1;
    }

    session.value().wait();
    return 0;
}
```

### Consume with CMake

```cmake
find_package(swaggercpp CONFIG REQUIRED)

add_executable(my_api_docs main.cpp)
target_link_libraries(my_api_docs PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_api_docs PRIVATE cxx_std_23)
```

### Consume with FetchContent from GitHub

```cmake
include(FetchContent)

FetchContent_Declare(
  swaggercpp
  GIT_REPOSITORY https://github.com/stescobedo92/swagger-cpp.git
  GIT_TAG v0.2.1
)

FetchContent_MakeAvailable(swaggercpp)

add_executable(my_api_docs main.cpp)
target_link_libraries(my_api_docs PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_api_docs PRIVATE cxx_std_23)
```

If you use `FetchContent`, make sure `nlohmann_json`, `yaml-cpp`, and `httplib` are available in your toolchain, for example through vcpkg.

## Conan

Create and validate the package:

```powershell
conan create . --test-folder=packaging/conan/test_package -s compiler.cppstd=23
```

## vcpkg

`swaggercpp` is published in the official [microsoft/vcpkg](https://github.com/microsoft/vcpkg/tree/master/ports/swaggercpp) registry. The `vcpkg.io` search page may take 1–2 weeks to index a freshly merged port; the Git path above is the authoritative source.

### Classic mode (simplest)

Install globally and consume without a manifest:

```powershell
vcpkg install swaggercpp
```

In `CMakePresets.json`, point CMake at the vcpkg toolchain:

```json
"CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
```

Then `find_package(swaggercpp CONFIG REQUIRED)` resolves against the global install.

### Manifest mode (recommended for teams and CI)

Add a `vcpkg.json` to the consumer project:

```json
{
  "name": "my-app",
  "version": "0.1.0",
  "dependencies": [ "swaggercpp" ],
  "builtin-baseline": "<recent-sha-from-microsoft/vcpkg>"
}
```

Get a baseline SHA that already contains `swaggercpp` with:

```powershell
git -C $env:VCPKG_ROOT rev-parse HEAD
```

CMake will auto-install dependencies into `out/build/<preset>/vcpkg_installed/` on configure. **Do not mix modes:** if your project has a `vcpkg.json`, the classic global install is ignored, and `swaggercpp` **must** be listed in `dependencies` or `find_package` will fail.

### Local overlay (for contributors)

```powershell
vcpkg install swaggercpp --overlay-ports=packaging\vcpkg\ports --classic
```

### Custom registry

If you prefer to pin `swaggercpp` from this repository directly (e.g. pre-release or fork consumption), point `vcpkg-configuration.json` at the custom registry shipped here:

```json
{
  "default-registry": {
    "kind": "builtin",
    "baseline": "b21ff8f3cadbd8e0b175b49be2dd9202f1f208f4"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/stescobedo92/swagger-cpp.git",
      "reference": "v0.2.1",
      "baseline": "<commit-sha-that-contains-packaging/vcpkg/registry/baseline.json>",
      "packages": [ "swaggercpp" ]
    }
  ]
}
```

### CI/CD for vcpkg

- `vcpkg-registry.yml` validates the overlay port on changes to packaging or core library files.
- `vcpkg-publish.yml` validates the port, refreshes the custom registry metadata, verifies the registry is in sync, and publishes a zipped registry bundle as a workflow artifact and GitHub release asset on tags.

## Examples

- `examples/load_validate.cpp`
- `examples/build_petstore.cpp`
- `examples/mutate_document.cpp`
- `examples/serve_ui.cpp`

## Roadmap

- Increase Swagger 2.0 coverage
- Expand security flows and callbacks/webhooks
- Add schema/reference resolution layers
- Add richer diagnostics and source locations
