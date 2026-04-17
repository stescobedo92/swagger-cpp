# DocumentReader

Header: `swaggercpp/reader.hpp`

Parses OpenAPI content into a `Document`. Autodetects JSON vs YAML. Never throws on malformed input — returns a `Result<Document>` with a collected issue list.

## Options

```cpp
struct ReaderOptions {
    bool allow_yaml { true };                // set false to reject YAML payloads
    bool strict_required_fields { true };    // missing info.title/version -> error; if false, warnings
};
```

## API

```cpp
class DocumentReader {
public:
    [[nodiscard]] static Result<Document> read(std::string_view content, ReaderOptions = {});
    [[nodiscard]] static Result<Document> read_file(const std::filesystem::path&, ReaderOptions = {});
};
```

### `read`

Parses in-memory content:

```cpp
auto result = swaggercpp::DocumentReader::read(R"(
openapi: 3.1.0
info: { title: Ex, version: 1 }
paths: {}
)");
```

### `read_file`

Reads the file's bytes and delegates to `read`:

```cpp
auto result = swaggercpp::DocumentReader::read_file("openapi.yaml");
```

The file extension is used as a first-pass hint; content sniffing still decides the final format.

## Return semantics

- On success: `result.ok() == true`, `result.value()` is the parsed `Document`, `result.issues()` may carry warnings (never errors).
- On failure: `result.ok() == false`, `result.value()` throws, `result.issues()` lists one or more `IssueSeverity::error` entries plus any warnings.

## Example

```cpp
swaggercpp::ReaderOptions opts { .strict_required_fields = false };
auto r = swaggercpp::DocumentReader::read_file("spec.yaml", opts);

if (!r) {
    for (const auto& issue : r.issues())
        std::cerr << issue.path << ": " << issue.message << '\n';
    return 1;
}

// Success path — warnings (if any) still present in r.issues()
for (const auto& issue : r.issues())
    std::clog << "[warn] " << issue.path << ": " << issue.message << '\n';

const auto& doc = r.value();
```

## See also

- [DocumentWriter](/api/writer) — the reverse direction
- [Result and Issues](/api/result) — how `Result<T>` and `Issue` behave
- [Parse YAML example](/examples/parse-yaml)
- [Parse JSON example](/examples/parse-json)
