# Validate a document

Read a spec, validate it semantically, and render issues sorted by severity.

## Code

```cpp
#include <iostream>
#include <algorithm>
#include "swaggercpp/swaggercpp.hpp"

static std::string_view level(swaggercpp::IssueSeverity s) {
    return s == swaggercpp::IssueSeverity::error ? "error  " : "warning";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: validate <openapi.yaml|.json>\n";
        return 2;
    }

    auto r = swaggercpp::DocumentReader::read_file(argv[1]);
    if (!r) {
        for (const auto& issue : r.issues())
            std::cerr << level(issue.severity) << " " << issue.path << ": " << issue.message << '\n';
        return 1;
    }

    swaggercpp::DocumentValidator validator;
    auto report = validator.validate(r.value());

    // Merge reader warnings + validator issues
    auto issues = r.issues();
    issues.insert(issues.end(), report.issues.begin(), report.issues.end());

    std::ranges::sort(issues, [](const auto& a, const auto& b) {
        if (a.severity != b.severity) return a.severity > b.severity;  // errors first
        return a.path < b.path;
    });

    for (const auto& i : issues)
        std::cerr << level(i.severity) << " " << i.path << ": " << i.message << '\n';

    return report.ok() ? 0 : 3;
}
```

## Sample spec with issues

```yaml
openapi: 3.1.0
info:
  title: ""
  version: 1.0.0
paths:
  /items:
    get:
      responses:
        "2XX":
          description: ok
    post:
      operationId: getItems   # duplicate id below
      responses:
        "201": { description: created }
  /items/{id}:
    get:
      operationId: getItems
      responses:
        "200": { description: ok }
```

## Expected output

```
error   /info/title: info.title must be non-empty
error   /paths/~1items/get/responses/2XX: response code must match \d{3} or "default"
error   /paths/~1items~1{id}/get/operationId: duplicate operationId "getItems"
warning /paths/~1items/get/operationId: operation has no operationId
```

## See also

- [DocumentValidator API](/api/validator)
- [Result and Issues](/api/result)
