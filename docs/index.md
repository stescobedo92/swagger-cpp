---
layout: home

hero:
  name: swaggercpp
  text: Modern C++23 toolkit for Swagger / OpenAPI.
  tagline: Parse, validate, traverse, write and serve OpenAPI documents — with a typed document model and zero surprises.
  actions:
    - theme: brand
      text: Quick start
      link: /quick-start
    - theme: alt
      text: View on GitHub
      link: https://github.com/stescobedo92/swagger-cpp
    - theme: alt
      text: Install
      link: /install

features:
  - icon: 📄
    title: Typed document model
    details: Document, Info, PathItem, Operation, Parameter, Response, Schema — every OpenAPI node as a real C++ struct, not stringly-typed maps.
  - icon: 🔄
    title: JSON and YAML, both ways
    details: Read, write and round-trip between formats with a single API. YAML detection is automatic; JSON output is configurable.
  - icon: ✅
    title: Validation with actionable issues
    details: Each Issue carries a severity, a JSON Pointer-style path, and a message. No exceptions for expected failures.
  - icon: 🧭
    title: Visitor-based traversal
    details: Implement DocumentVisitor once and walk every path, operation, parameter, response and schema deterministically.
  - icon: 🌐
    title: Embedded Swagger UI
    details: Spin up a local HTTP server with Swagger UI and /openapi.json in one call. Optional browser auto-launch.
  - icon: 📦
    title: First-class packaging
    details: Installable via vcpkg (upstream), Conan recipe, CMake package config, FetchContent, or custom registry.
---

## Why swaggercpp?

Most C++ OpenAPI libraries stop at "parse to a JSON tree". `swaggercpp` goes further: it exposes OpenAPI nodes as regular C++23 structures, so tooling authors can inspect and manipulate specifications with normal field access and standard containers.

```cpp
#include "swaggercpp/swaggercpp.hpp"

auto result = swaggercpp::DocumentReader::read_file("openapi.yaml");
if (!result) return 1;

swaggercpp::DocumentValidator validator;
const auto report = validator.validate(result.value());

for (const auto& op : result.value().paths) {
    for (const auto& [method, operation] : op.second.operations) {
        std::println("{} {} → {}", swaggercpp::to_string(method), op.first,
                     operation.operation_id.value_or("(anonymous)"));
    }
}
```

## What's in the box

- `DocumentReader` — format-detecting parser for JSON and YAML
- `DocumentWriter` — serialiser with pretty-printing and round-trip helpers
- `DocumentValidator` — semantic checks independent of syntax
- `DocumentWalker` — deterministic visitor traversal
- `SwaggerUiServer` — embedded HTTP server with Swagger UI shell
- `Result<T>` — non-throwing return type with issues attached

## License

Released under the [MIT License](https://github.com/stescobedo92/swagger-cpp/blob/master/LICENSE).
