# Mutate a document

Read a JSON spec, edit the in-memory model, write it back as JSON.

## Code

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    auto r = swaggercpp::DocumentReader::read(R"({
      "openapi": "3.1.0",
      "info": { "title": "Catalog", "version": "1.0.0" },
      "paths": {
        "/products": {
          "get": {
            "operationId": "listProducts",
            "responses": { "200": { "description": "ok" } }
          }
        }
      }
    })");

    if (!r) return 1;

    // Bump the version
    r.value().info.version = "1.1.0";

    // Add a POST operation
    r.value().paths["/products"]
        .operations[swaggercpp::HttpMethod::post]
        .responses["201"] = swaggercpp::Response { .description = "created" };

    // Tag every operation
    for (auto& [_, item] : r.value().paths) {
        for (auto& [_, op] : item.operations) {
            if (op.tags.empty()) op.tags.push_back("catalog");
        }
    }

    const auto json = swaggercpp::DocumentWriter::write_json(r.value(), /*pretty=*/true, /*indent=*/2);
    if (!json) return 2;

    std::cout << json.value();
}
```

## Expected output

```json
{
  "openapi": "3.1.0",
  "info": {
    "title": "Catalog",
    "version": "1.1.0"
  },
  "paths": {
    "/products": {
      "get": {
        "operationId": "listProducts",
        "tags": ["catalog"],
        "responses": {
          "200": { "description": "ok" }
        }
      },
      "post": {
        "tags": ["catalog"],
        "responses": {
          "201": { "description": "created" }
        }
      }
    }
  }
}
```

## See also

- [Document model](/api/document)
- [DocumentWriter](/api/writer)
- [Build from code](/examples/build-from-code)
