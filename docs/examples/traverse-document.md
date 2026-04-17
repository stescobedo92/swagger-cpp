# Traverse a document

Use a `DocumentVisitor` to count operations by method and list every response code.

## Code

```cpp
#include <iostream>
#include <map>
#include "swaggercpp/swaggercpp.hpp"

struct StatsVisitor : swaggercpp::DocumentVisitor {
    std::map<swaggercpp::HttpMethod, int> by_method;
    std::map<std::string, int>            by_status;

    void enter_operation(std::string_view,
                         swaggercpp::HttpMethod method,
                         const swaggercpp::Operation&) override {
        ++by_method[method];
    }

    void visit_response(std::string_view status,
                        const swaggercpp::Response&) override {
        ++by_status[std::string(status)];
    }
};

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: stats <spec>\n"; return 2; }

    auto r = swaggercpp::DocumentReader::read_file(argv[1]);
    if (!r) return 1;

    StatsVisitor v;
    swaggercpp::DocumentWalker::walk(r.value(), v);

    std::cout << "By method:\n";
    for (const auto& [m, n] : v.by_method)
        std::cout << "  " << swaggercpp::to_string(m) << " = " << n << '\n';

    std::cout << "By status:\n";
    for (const auto& [s, n] : v.by_status)
        std::cout << "  " << s << " = " << n << '\n';
}
```

## Expected output

For a spec with 12 operations spread across methods and standard response codes:

```
By method:
  get    = 5
  post   = 4
  put    = 2
  delete = 1
By status:
  200 = 5
  201 = 4
  204 = 2
  400 = 3
  404 = 2
  default = 6
```

## See also

- [DocumentWalker API](/api/walker)
- [Endpoint catalog](/examples/endpoint-catalog) — more elaborate traversal that emits Markdown
