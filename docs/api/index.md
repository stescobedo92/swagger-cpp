# API reference

The `swaggercpp` public API is organised around a typed **document model** and five **subsystems** that operate on it. All symbols live in the `swaggercpp` namespace.

## Subsystems at a glance

| Subsystem              | Header                       | Entry point                         |
|------------------------|------------------------------|-------------------------------------|
| [Document model](/api/document)   | `swaggercpp/document.hpp`   | `Document`, `Info`, `PathItem`, ... |
| [DocumentReader](/api/reader)     | `swaggercpp/reader.hpp`     | `DocumentReader::read(_file)`       |
| [DocumentWriter](/api/writer)     | `swaggercpp/writer.hpp`     | `DocumentWriter::write_{yaml,json}` |
| [DocumentValidator](/api/validator) | `swaggercpp/validator.hpp` | `DocumentValidator::validate`      |
| [DocumentWalker](/api/walker)     | `swaggercpp/walker.hpp`     | `DocumentWalker::walk`              |
| [SwaggerUiServer](/api/ui)        | `swaggercpp/ui.hpp`         | `SwaggerUiServer::start(_file)`     |
| [Result and Issues](/api/result)  | `swaggercpp/result.hpp`     | `Result<T>`, `Issue`, `ValidationReport` |

## Namespace summary

All public symbols are under `swaggercpp`:

```cpp
namespace swaggercpp {
    // Enums
    enum class SpecificationVersion { swagger_2_0, openapi_3_0, openapi_3_1 };
    enum class DocumentFormat       { json, yaml };
    enum class ParameterLocation    { query, header, path, cookie };
    enum class HttpMethod           { get, put, post, delete_, options, head, patch, trace };
    enum class IssueSeverity        { warning, error };

    // Strong types
    struct Document;
    struct Info; struct Contact; struct License;
    struct Server; struct ServerVariable;
    struct Tag; struct ExternalDocumentation;
    struct Schema;  using SchemaPtr = std::shared_ptr<Schema>;
    struct MediaType; struct Parameter; struct RequestBody; struct Response;
    struct Operation; struct PathItem;
    struct SecurityScheme; struct Components;

    template <typename T> class Result;
    struct Issue;
    struct ValidationReport;

    // Options
    struct ReaderOptions;
    struct WriterOptions;
    struct SwaggerUiOptions;

    // Services
    class DocumentReader;
    class DocumentWriter;
    class DocumentValidator;
    class DocumentVisitor;
    class DocumentWalker;
    class SwaggerUiServer;
    class SwaggerUiSession;

    // Helpers
    std::string_view to_string(HttpMethod);
    std::string_view to_string(ParameterLocation);
    std::string_view to_string(SpecificationVersion);
    std::optional<HttpMethod> http_method_from_string(std::string_view);
    std::optional<ParameterLocation> parameter_location_from_string(std::string_view);
}
```

## Umbrella header

Prefer the umbrella header for most cases:

```cpp
#include "swaggercpp/swaggercpp.hpp"
```

It transitively includes every public header.

## Error-handling convention

- **No exceptions for expected errors.** Parsing and validation return `Result<T>` / `ValidationReport`.
- **Exceptions for programmer errors.** `Result<T>::value()` throws `std::logic_error` if no value is present — treat it as a precondition, always guard with `operator bool()`.

See [Result and Issues](/api/result).
