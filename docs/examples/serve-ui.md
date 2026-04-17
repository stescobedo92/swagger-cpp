# Serve Swagger UI

Embed an HTTP server that hosts Swagger UI against your live document.

## Code

```cpp
#include <iostream>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    swaggercpp::Document doc;
    doc.version = swaggercpp::SpecificationVersion::openapi_3_1;
    doc.openapi = "3.1.0";
    doc.info.title   = "Embedded Swagger UI";
    doc.info.version = "1.0.0";

    swaggercpp::Operation op;
    op.operation_id = "listCustomers";
    op.responses.emplace("200", swaggercpp::Response { .description = "ok" });

    swaggercpp::PathItem item;
    item.operations.emplace(swaggercpp::HttpMethod::get, op);
    doc.paths.emplace("/customers", item);

    auto session = swaggercpp::SwaggerUiServer::start(doc, {
        .title = "Embedded Swagger UI",
    });

    if (!session) {
        for (const auto& issue : session.issues())
            std::cerr << issue.path << ": " << issue.message << '\n';
        return 1;
    }

    std::cout << "Swagger UI: " << session.value().ui_url()   << '\n'
              << "Spec JSON:  " << session.value().spec_url() << '\n';

    session.value().wait();
}
```

On startup:

- The default browser opens at `session.value().ui_url()`.
- `GET /` serves the Swagger UI HTML shell.
- `GET /openapi.json` returns the document serialised to JSON.

## Custom options

### LAN-accessible, fixed port, no auto-launch

```cpp
auto session = swaggercpp::SwaggerUiServer::start(doc, {
    .host = "0.0.0.0",
    .port = 8080,
    .open_browser = false,
    .title = "Staging",
    .doc_expansion = "none",
});
```

### Start from a file without building a `Document`

```cpp
auto session = swaggercpp::SwaggerUiServer::start_file("openapi.yaml");
```

### Stop from another thread

```cpp
auto s = swaggercpp::SwaggerUiServer::start(doc);
if (!s) return 1;

std::thread stopper([&]{
    std::this_thread::sleep_for(std::chrono::seconds(30));
    s.value().stop();
});

s.value().wait();
stopper.join();
```

## Security note

::: warning
The embedded server has no authentication and no TLS. Use it for local development or internal tooling only — not as a production documentation host.
:::

## See also

- [SwaggerUiServer API](/api/ui)
