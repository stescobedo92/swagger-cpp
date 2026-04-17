# SwaggerUiServer

Header: `swaggercpp/ui.hpp`

Embeds an HTTP server (cpp-httplib) that serves a Swagger UI shell and your document at `/openapi.json`. Designed to be a one-liner during development.

## Options

```cpp
struct SwaggerUiOptions {
    std::string host        { "127.0.0.1" };  // use "0.0.0.0" for LAN
    int         port        { 0 };            // 0 = auto-select free port
    bool        open_browser{ true };
    std::string ui_path     { "/" };
    std::string spec_path   { "/openapi.json" };
    std::string title       { "swaggercpp" };
    bool        deep_linking              { true };
    bool        display_operation_id      { true };
    bool        display_request_duration  { true };
    bool        show_extensions           { true };
    bool        show_common_extensions    { true };
    bool        try_it_out_enabled        { true };
    std::string doc_expansion             { "list" };  // "list" | "full" | "none"
};
```

## `SwaggerUiServer`

```cpp
class SwaggerUiServer {
public:
    [[nodiscard]] static Result<SwaggerUiSession> start(const Document&, SwaggerUiOptions = {});
    [[nodiscard]] static Result<SwaggerUiSession> start(std::string_view content,
                                                        SwaggerUiOptions = {},
                                                        ReaderOptions    = {});
    [[nodiscard]] static Result<SwaggerUiSession> start_file(const std::filesystem::path&,
                                                             SwaggerUiOptions = {},
                                                             ReaderOptions    = {});
};
```

Three overloads for three input sources: a pre-built `Document`, raw content, or a file on disk.

## `SwaggerUiSession`

Move-only RAII handle to a running server:

```cpp
class SwaggerUiSession {
public:
    [[nodiscard]] bool        running() const noexcept;
    [[nodiscard]] int         port() const noexcept;
    [[nodiscard]] std::string host() const;
    [[nodiscard]] std::string base_url() const;   // e.g. "http://127.0.0.1:54321"
    [[nodiscard]] std::string ui_url() const;     // base_url + ui_path
    [[nodiscard]] std::string spec_url() const;   // base_url + spec_path

    void stop();
    void wait();  // blocks until stop() or destruction
};
```

`stop()` is idempotent; the destructor calls it.

## Examples

### Minimal

```cpp
auto s = swaggercpp::SwaggerUiServer::start(doc);
if (!s) return 1;
s.value().wait();
```

### Fixed port, no browser, LAN-accessible

```cpp
swaggercpp::SwaggerUiOptions opts {
    .host = "0.0.0.0",
    .port = 8080,
    .open_browser = false,
    .title = "Staging Spec",
    .doc_expansion = "none",
};

auto s = swaggercpp::SwaggerUiServer::start(doc, opts);
```

### Direct from file

```cpp
auto s = swaggercpp::SwaggerUiServer::start_file("openapi.yaml");
if (!s) return 1;
std::cout << "Open " << s.value().ui_url() << '\n';
s.value().wait();
```

### Graceful shutdown from another thread

```cpp
auto s = swaggercpp::SwaggerUiServer::start(doc);
if (!s) return 1;

std::thread shutdown([&]{
    std::this_thread::sleep_for(std::chrono::seconds(30));
    s.value().stop();
});

s.value().wait();
shutdown.join();
```

## Security considerations

::: warning
The embedded server is intended for local development. It has **no authentication, no TLS, and no rate limiting**. Do not expose it on the public Internet. For production documentation hosting, publish the spec artefact and use a proper web server or CDN.
:::

## See also

- [Serve UI example](/examples/serve-ui)
- [DocumentReader](/api/reader)
