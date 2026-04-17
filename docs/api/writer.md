# DocumentWriter

Header: `swaggercpp/writer.hpp`

Serialises `Document` back to JSON or YAML and offers a round-trip helper for format conversion.

## Options

```cpp
struct WriterOptions {
    DocumentFormat format { DocumentFormat::yaml };
    bool pretty  { true };
    int  indent  { 2 };
};
```

## API

```cpp
class DocumentWriter {
public:
    [[nodiscard]] static Result<std::string> write(const Document&, WriterOptions = {});
    [[nodiscard]] static Result<std::string> write_json(const Document&, bool pretty = true, int indent = 2);
    [[nodiscard]] static Result<std::string> write_yaml(const Document&);
    [[nodiscard]] static Result<Document>    roundtrip(std::string_view content, DocumentFormat output_format);
    static              Result<std::string>  write_file(const Document&, const std::filesystem::path&, WriterOptions = {});
};
```

### `write`

Generic entry point that honours `WriterOptions`:

```cpp
auto out = swaggercpp::DocumentWriter::write(doc, {
    .format = swaggercpp::DocumentFormat::json,
    .pretty = true,
    .indent = 4,
});
```

### `write_json` / `write_yaml`

Shorthand for the common case:

```cpp
auto json = swaggercpp::DocumentWriter::write_json(doc);
auto yaml = swaggercpp::DocumentWriter::write_yaml(doc);
```

### `roundtrip`

Parse content and re-serialise in the target format, without exposing the intermediate `Document`:

```cpp
auto as_json = swaggercpp::DocumentWriter::roundtrip(
    yaml_text, swaggercpp::DocumentFormat::json);
```

This is the fastest path when you only need format conversion.

### `write_file`

Serialises and writes to disk. Returns the produced content on success (same as `write`), so you can log or diff:

```cpp
auto r = swaggercpp::DocumentWriter::write_file(doc, "out.yaml");
if (!r) return 1;
std::clog << "wrote " << r.value().size() << " bytes\n";
```

## Error cases

- Writing succeeds for any well-formed `Document`. If the model has structural inconsistencies (e.g. cyclic `SchemaPtr`), the writer reports `IssueSeverity::error` entries and `result.ok()` is `false`.
- `write_file` additionally fails on filesystem errors (permission denied, missing directory, etc.).

## See also

- [DocumentReader](/api/reader)
- [Roundtrip example](/examples/roundtrip)
- [Mutate and write example](/examples/mutate-document)
