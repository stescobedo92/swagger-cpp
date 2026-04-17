# Examples

Task-oriented recipes. Each page is self-contained, compiles as a single `main.cpp`, and progresses from trivial usage to more elaborate pipelines.

## Simple

- [Parse YAML](/examples/parse-yaml) — load and inspect a YAML spec
- [Parse JSON](/examples/parse-json) — same for a JSON payload
- [Build from code](/examples/build-from-code) — construct a `Document` by hand
- [Validate a document](/examples/validate-document) — run `DocumentValidator` and render issues

## Intermediate

- [Mutate a document](/examples/mutate-document) — read, edit, write back
- [Round-trip formats](/examples/roundtrip) — YAML ↔ JSON without an intermediate variable
- [Traverse with a visitor](/examples/traverse-document) — walk every operation

## Elaborate

- [Serve Swagger UI](/examples/serve-ui) — embedded HTTP server with browser auto-open
- [Endpoint catalog](/examples/endpoint-catalog) — emit a Markdown catalog from any spec

## How to run the examples

Each recipe assumes the following layout:

```
example/
├── CMakeLists.txt
├── vcpkg.json
└── main.cpp
```

With the standard `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.26)
project(example LANGUAGES CXX)

find_package(swaggercpp CONFIG REQUIRED)

add_executable(example main.cpp)
target_link_libraries(example PRIVATE swaggercpp::swaggercpp)
target_compile_features(example PRIVATE cxx_std_23)
```

And this `vcpkg.json`:

```json
{
  "name": "example",
  "version": "0.1.0",
  "dependencies": [ "swaggercpp" ],
  "builtin-baseline": "<sha-from-vcpkg>"
}
```

Then build:

```powershell
cmake --preset default
cmake --build out/build/default
```

See [Installation](/install) for the full toolchain setup.
