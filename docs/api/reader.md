# DocumentReader

Header: `swaggercpp/reader.hpp`

Parses OpenAPI content into a `Document`. Autodetects JSON vs YAML. Never throws on malformed input — returns a `Result<Document>` with a collected issue list.

## Options

```cpp
struct ReaderOptions {
    bool allow_yaml { true };                // set false to reject YAML payloads
    bool strict_required_fields { true };    // missing info.title/version -> error; if false, warnings
    bool track_source_locations { false };   // collect one-based source positions
    std::size_t max_document_bytes { 16 * 1024 * 1024 };
    std::size_t max_nesting_depth { 128 };
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
info: { title: Ex, version: "1" }
paths: {}
)");
```

### `read_file`

Reads the file's bytes and delegates to `read`:

```cpp
auto result = swaggercpp::DocumentReader::read_file("openapi.yaml");
```

Content sniffing decides the input format. File reads are bounded by `max_document_bytes`; JSON/YAML nesting is bounded by `max_nesting_depth`. YAML conversion also limits alias expansion to one million nodes and `max_document_bytes` scalar/key bytes, and rejects recursive aliases and duplicate mapping keys.

Enable `track_source_locations` to populate `Document::source_locations`. The validator uses these positions for semantic diagnostics, falling back to the closest containing object for missing fields. `read_file` also populates `source_file`. Location tracking is opt-in to avoid its extra memory and parsing cost for ordinary library consumers.

The reader accepts Swagger 2.0 (partial compatibility), OpenAPI 3.0.x and 3.1.x. Unsupported or conflicting version declarations fail explicitly. Unmodeled fields are retained in `unknown_fields`; preserving a field does not imply semantic validation of that field. See the [document model](/api/document).

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
