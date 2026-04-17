# Parse YAML

Load a YAML OpenAPI document from memory and print basic information.

## Code

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    constexpr std::string_view yaml = R"(
openapi: 3.1.0
info:
  title: Orders API
  version: 1.0.0
  description: Sample spec for the swaggercpp tutorial.
paths:
  /orders:
    get:
      operationId: listOrders
      responses:
        "200":
          description: A list of orders.
    post:
      operationId: createOrder
      responses:
        "201":
          description: Created.
)";

    const auto result = swaggercpp::DocumentReader::read(yaml);

    if (!result) {
        for (const auto& issue : result.issues())
            std::cerr << issue.path << ": " << issue.message << '\n';
        return 1;
    }

    const auto& doc = result.value();
    std::cout << "Title:       " << doc.info.title << '\n'
              << "Version:     " << doc.info.version << '\n'
              << "Description: " << doc.info.description.value_or("(none)") << '\n'
              << "Paths:       " << doc.paths.size() << '\n';

    for (const auto& [path, item] : doc.paths) {
        for (const auto& [method, op] : item.operations) {
            std::cout << "  " << swaggercpp::to_string(method) << ' ' << path
                      << " → " << op.operation_id.value_or("(anonymous)") << '\n';
        }
    }
}
```

## Expected output

```
Title:       Orders API
Version:     1.0.0
Description: Sample spec for the swaggercpp tutorial.
Paths:       1
  get /orders → listOrders
  post /orders → createOrder
```

## Variations

### From a file on disk

```cpp
const auto result = swaggercpp::DocumentReader::read_file("openapi.yaml");
```

### Reject YAML input

```cpp
swaggercpp::ReaderOptions opts { .allow_yaml = false };
const auto result = swaggercpp::DocumentReader::read(yaml, opts);
// result.ok() == false; issue reports "YAML input disallowed"
```

### Loose parsing for drafts

```cpp
swaggercpp::ReaderOptions opts { .strict_required_fields = false };
const auto result = swaggercpp::DocumentReader::read(draft_yaml, opts);
```

## See also

- [DocumentReader API](/api/reader)
- [Parse JSON](/examples/parse-json)
