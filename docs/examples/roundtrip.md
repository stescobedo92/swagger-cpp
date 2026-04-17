# Round-trip formats

Convert YAML to pretty-printed JSON (or vice versa) without keeping the intermediate `Document` around.

## Code

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    constexpr std::string_view yaml = R"(
openapi: 3.1.0
info: { title: Tiny, version: 0.1 }
paths:
  /ping:
    get:
      operationId: ping
      responses:
        "200": { description: pong }
)";

    // YAML → Document → JSON, in one call
    auto doc = swaggercpp::DocumentWriter::roundtrip(yaml, swaggercpp::DocumentFormat::json);
    if (!doc) return 1;

    // roundtrip returns a Document; write it however you want
    const auto json = swaggercpp::DocumentWriter::write_json(doc.value());
    if (!json) return 2;

    std::cout << json.value();
}
```

## Expected output

```json
{
  "openapi": "3.1.0",
  "info": {
    "title": "Tiny",
    "version": "0.1"
  },
  "paths": {
    "/ping": {
      "get": {
        "operationId": "ping",
        "responses": {
          "200": { "description": "pong" }
        }
      }
    }
  }
}
```

## Reverse direction

```cpp
auto doc = swaggercpp::DocumentWriter::roundtrip(json_text, swaggercpp::DocumentFormat::yaml);
```

## When to use `roundtrip` vs manual pipeline

Use `roundtrip` when you **only** need format conversion. If you also need to validate, mutate or traverse the document, use the three-step pipeline:

```cpp
auto r = swaggercpp::DocumentReader::read(yaml);    // parse
// ... inspect / mutate r.value() ...
auto out = swaggercpp::DocumentWriter::write_json(r.value());  // serialise
```

## See also

- [DocumentWriter API](/api/writer)
- [Mutate a document](/examples/mutate-document)
