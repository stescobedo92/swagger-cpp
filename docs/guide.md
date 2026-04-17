# Guide

This guide walks through `swaggercpp` from the perspective of a tooling author: how the library is organised, how to compose its pieces, and the idioms that make code concise.

## Library layout

Everything is in the `swaggercpp` namespace and reachable from a single umbrella header:

```cpp
#include "swaggercpp/swaggercpp.hpp"
```

The umbrella includes:

| Header                    | Purpose                                           |
|---------------------------|---------------------------------------------------|
| `document.hpp`            | Data model (Document, Info, PathItem, Schema...)  |
| `reader.hpp`              | `DocumentReader::read` and `read_file`            |
| `writer.hpp`              | `DocumentWriter::write_json`, `write_yaml`, etc.  |
| `validator.hpp`           | `DocumentValidator::validate`                     |
| `walker.hpp`              | `DocumentVisitor` + `DocumentWalker::walk`        |
| `ui.hpp`                  | Embedded Swagger UI server                        |
| `result.hpp`              | `Result<T>`, `Issue`, `ValidationReport`          |

You can include individual headers if you want finer control over compile times.

## The `Result<T>` contract

`swaggercpp` does not throw exceptions for expected failures. Every fallible operation returns `Result<T>`:

```cpp
auto result = swaggercpp::DocumentReader::read(content);

if (!result) {
    for (const auto& issue : result.issues()) {
        std::cerr << issue.path << ": " << issue.message << '\n';
    }
    return 1;
}

auto& doc = result.value();  // safe, only after the check
```

Rules:

- `if (!result)` → something went wrong **or** the value is absent.
- `result.issues()` always returns the collected `Issue` list, even on success — warnings included.
- `result.value()` throws `std::logic_error` if no value was stored; guard it with `operator bool()`.

See [Result and Issues](/api/result) for the full contract.

## Reading documents

### From a string

```cpp
auto r = swaggercpp::DocumentReader::read(yaml_or_json_text);
```

The parser detects JSON vs YAML automatically. You can disable YAML if you only accept JSON:

```cpp
swaggercpp::ReaderOptions options { .allow_yaml = false };
auto r = swaggercpp::DocumentReader::read(text, options);
```

### From a file

```cpp
auto r = swaggercpp::DocumentReader::read_file("openapi.yaml");
```

`read_file` uses the file extension as a hint but still autodetects content.

### Loose vs strict parsing

```cpp
swaggercpp::ReaderOptions relaxed { .strict_required_fields = false };
```

With `strict_required_fields = false`, missing `info.title` or `info.version` degrade to warnings instead of errors. Useful for work-in-progress specs.

## Writing documents

Three entry points cover the common cases:

```cpp
auto yaml = swaggercpp::DocumentWriter::write_yaml(doc);
auto json = swaggercpp::DocumentWriter::write_json(doc, /*pretty=*/true, /*indent=*/2);
auto out  = swaggercpp::DocumentWriter::write(doc, {
    .format = swaggercpp::DocumentFormat::json,
    .pretty = true,
    .indent = 4,
});
```

To persist directly to disk:

```cpp
auto ok = swaggercpp::DocumentWriter::write_file(doc, "out.yaml");
if (!ok) return 1;
```

### Round-trip helpers

When you need to translate between formats without exposing the intermediate `Document`:

```cpp
auto as_json = swaggercpp::DocumentWriter::roundtrip(
    yaml_text, swaggercpp::DocumentFormat::json);
```

## Validating semantics

The validator is decoupled from parsing — you can build a `Document` by hand and still validate it:

```cpp
swaggercpp::DocumentValidator validator;
auto report = validator.validate(doc);

if (!report.ok()) { /* one or more errors */ }
for (const auto& issue : report.issues) { /* warnings included */ }
```

What it checks today:

- Required top-level fields (`info.title`, `info.version`, a valid `paths` or `components`)
- Operation identifiers uniqueness
- Parameter `in`/`name` consistency
- Response codes are numeric strings or `default`
- Schema references resolve (for in-document `$ref`)

## Traversing documents

Deterministic, read-only walking via the visitor pattern:

```cpp
struct MethodCounter : swaggercpp::DocumentVisitor {
    std::map<swaggercpp::HttpMethod, int> counts;

    void enter_operation(std::string_view,
                         swaggercpp::HttpMethod method,
                         const swaggercpp::Operation&) override {
        ++counts[method];
    }
};

MethodCounter visitor;
swaggercpp::DocumentWalker::walk(doc, visitor);
```

The walker enters each `PathItem`, then each `Operation`, then parameters and responses. Override only the callbacks you care about; defaults are no-ops.

## Mutating documents

The data model is made of plain structs. Mutation is just field assignment:

```cpp
doc.info.version = "2.0.0";
doc.paths["/pets"].operations[swaggercpp::HttpMethod::post].responses["201"] =
    swaggercpp::Response { .description = "Created" };
```

Shared ownership of schemas uses `std::shared_ptr<Schema>` so deeply-nested structures compose without copies:

```cpp
auto pet = std::make_shared<swaggercpp::Schema>();
pet->type = "object";
pet->required = {"id", "name"};
// ... populate properties
doc.components.schemas.emplace("Pet", pet);
```

## Serving Swagger UI

One call, one browser tab:

```cpp
auto session = swaggercpp::SwaggerUiServer::start(doc);
if (!session) return 1;

session.value().wait();  // blocks until stop() or shutdown
```

The server serves:

- `GET /` → Swagger UI HTML shell
- `GET /openapi.json` → the document in JSON form

Customise with `SwaggerUiOptions`:

```cpp
swaggercpp::SwaggerUiOptions opts {
    .host = "0.0.0.0",
    .port = 8080,
    .open_browser = false,
    .title = "Orders API Docs",
    .try_it_out_enabled = true,
};
auto session = swaggercpp::SwaggerUiServer::start(doc, opts);
```

Or skip the `Document` altogether and feed raw content:

```cpp
auto session = swaggercpp::SwaggerUiServer::start_file("openapi.yaml");
```

## Idioms

### Prefer structured bindings when iterating paths

```cpp
for (const auto& [path, item] : doc.paths) {
    for (const auto& [method, op] : item.operations) {
        // ...
    }
}
```

### Use `std::optional` everywhere the spec says "optional"

Fields like `Info::description`, `License::identifier`, `Contact::email` are `std::optional<std::string>`. Always test before dereferencing:

```cpp
if (doc.info.description) { std::cout << *doc.info.description; }
```

### Extension fields go through `ExtensionMap`

Every spec node exposes `extensions`, a `std::map<std::string, nlohmann::json>`. Vendor fields like `x-rate-limit` live there:

```cpp
operation.extensions["x-rate-limit"] = 100;
```

## Where to go next

- [Examples](/examples/) — concrete recipes
- [API reference](/api/) — every type, every function
- [Architecture](/architecture) — why the library is shaped the way it is
