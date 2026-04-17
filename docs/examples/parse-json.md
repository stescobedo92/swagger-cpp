# Parse JSON

Same shape as [Parse YAML](/examples/parse-yaml) but with a JSON payload.

## Code

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    constexpr std::string_view json = R"({
      "openapi": "3.1.0",
      "info": {
        "title": "Catalog",
        "version": "1.0.0"
      },
      "paths": {
        "/products": {
          "get": {
            "operationId": "listProducts",
            "responses": {
              "200": { "description": "ok" }
            }
          }
        }
      }
    })";

    const auto result = swaggercpp::DocumentReader::read(json);

    if (!result) {
        for (const auto& issue : result.issues())
            std::cerr << issue.path << ": " << issue.message << '\n';
        return 1;
    }

    const auto& doc = result.value();
    std::cout << doc.info.title << " " << doc.info.version << '\n';
    std::cout << doc.paths.size() << " path(s)\n";
}
```

::: tip
`DocumentReader::read` detects JSON vs YAML from the payload — same API for both. The `allow_yaml` option exists only to **restrict** to JSON-only input.
:::

## Expected output

```
Catalog 1.0.0
1 path(s)
```

## See also

- [DocumentReader API](/api/reader)
- [Parse YAML](/examples/parse-yaml)
