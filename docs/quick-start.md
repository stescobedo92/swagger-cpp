# Quick start

This page takes you from zero to a working program that parses, validates and traverses an OpenAPI document in under five minutes.

## 1. Scaffold the project

```
my-app/
├── CMakeLists.txt
├── vcpkg.json
├── CMakePresets.json
└── main.cpp
```

`vcpkg.json`:

```json
{
  "name": "my-app",
  "version": "0.1.0",
  "dependencies": [ "swaggercpp" ],
  "builtin-baseline": "<sha-from-vcpkg-repo>"
}
```

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.26)
project(my_app LANGUAGES CXX)

find_package(swaggercpp CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE swaggercpp::swaggercpp)
target_compile_features(my_app PRIVATE cxx_std_23)
```

`CMakePresets.json`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "default",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ]
}
```

## 2. Parse a YAML document

`main.cpp`:

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    const auto result = swaggercpp::DocumentReader::read(R"(
openapi: 3.1.0
info:
  title: Orders API
  version: 1.0.0
paths:
  /orders:
    get:
      operationId: listOrders
      responses:
        "200":
          description: ok
)");

    if (!result) {
        for (const auto& issue : result.issues())
            std::cerr << issue.path << ": " << issue.message << '\n';
        return 1;
    }

    const auto& doc = result.value();
    std::cout << "Title:   " << doc.info.title << '\n';
    std::cout << "Version: " << doc.info.version << '\n';
    std::cout << "Paths:   " << doc.paths.size() << '\n';
}
```

Build and run:

```powershell
cmake --preset default
cmake --build out/build/default
.\out\build\default\my_app.exe
```

Expected output:

```
Title:   Orders API
Version: 1.0.0
Paths:   1
```

## 3. Add validation

```cpp
swaggercpp::DocumentValidator validator;
const auto report = validator.validate(doc);

if (!report.ok()) {
    for (const auto& issue : report.issues) {
        std::cerr << "[" << (issue.severity == swaggercpp::IssueSeverity::error ? "error" : "warning")
                  << "] " << issue.path << ": " << issue.message << '\n';
    }
}
```

## 4. Walk every operation

```cpp
struct CountingVisitor : swaggercpp::DocumentVisitor {
    int operations = 0;
    void enter_operation(std::string_view, swaggercpp::HttpMethod, const swaggercpp::Operation&) override {
        ++operations;
    }
};

CountingVisitor visitor;
swaggercpp::DocumentWalker::walk(doc, visitor);
std::cout << "Found " << visitor.operations << " operations\n";
```

## 5. Serve Swagger UI from your binary

```cpp
auto session = swaggercpp::SwaggerUiServer::start(doc, {
    .title = "Orders API Docs",
});

if (!session) return 1;
std::cout << "Open " << session.value().ui_url() << '\n';
session.value().wait();
```

::: tip
`SwaggerUiServer::start` opens the default browser unless you pass `open_browser = false`. The server stays alive until `stop()` or destruction of the session.
:::

## Next

- [Guide](/guide) — narrative walkthrough of every subsystem
- [Examples](/examples/) — copy-paste recipes from simple to elaborate
- [API reference](/api/) — type-by-type documentation
