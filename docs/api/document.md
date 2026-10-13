# Document model

Header: `swaggercpp/document.hpp`

The public model uses plain C++ structs for common OpenAPI objects. It provides typed fields for the features listed below and retains unmodeled JSON fields for serialization. This is not a complete OpenAPI or JSON Schema implementation. Swagger 2.0 compatibility remains partial.

## Enums and conversions

```cpp
enum class SpecificationVersion { swagger_2_0, openapi_3_0, openapi_3_1 };
enum class DocumentFormat { json, yaml };
enum class ParameterLocation { query, header, path, cookie };
enum class HttpMethod { get, put, post, delete_, options, head, patch, trace };

std::string_view to_string(HttpMethod) noexcept;
std::string_view to_string(ParameterLocation) noexcept;
std::string_view to_string(SpecificationVersion) noexcept;
std::optional<HttpMethod> http_method_from_string(std::string_view) noexcept;
std::optional<ParameterLocation> parameter_location_from_string(std::string_view) noexcept;
```

`delete_` maps to `delete`. Version conversion produces `2.0`, `3.0.3`, or `3.1.0`; a parsed document also retains its original version string in `swagger` or `openapi`. Reverse parsing returns `std::nullopt` for unknown tokens.

## Document and components

`Document` contains `info`, `servers`, `paths`, `components`, `tags`, `security`, and `webhooks`. `paths` and `webhooks` are both `std::map<std::string, PathItem>`; their names have different meanings: URL templates for paths and application-defined names for webhooks.

`Components` provides typed maps for `schemas`, `responses`, `parameters`, `request_bodies`, and `security_schemes`. Other component groups, including `headers`, `links`, `callbacks`, and `pathItems`, remain in `components.unknown_fields`.

Maps use sorted keys for deterministic iteration. New members are appended after the existing members so existing aggregate initialization can continue to omit them.

## Metadata and servers

| Struct | Main fields |
| --- | --- |
| `Info` | `title`, `version`, optional `summary`, `description`, `terms_of_service`, `contact`, `license` |
| `Contact` | Optional `name`, `url`, `email` |
| `License` | `name`, optional `url`, `identifier` |
| `Server` | `url`, optional `description`, named `variables` |
| `ServerVariable` | `default_value`, `enum_values`, optional `description` |
| `Tag` | `name`, optional `description`, `external_docs` |
| `ExternalDocumentation` | `url`, optional `description` |

Servers are available on the document, path item, and operation. `PathItem::servers_defined` and `Operation::servers_defined` retain explicit empty server arrays when reading and writing.

## Operations, parameters, and responses

`PathItem` contains optional `ref`, `summary`, and `description`, plus shared `parameters` and a map of HTTP methods to `Operation` values.

`Operation` contains tags, summary, description, operation ID, parameters, optional request body, responses, deprecation, security requirements, servers, and callbacks. `callbacks` is a `std::map<std::string, nlohmann::json>`: its contents are preserved as JSON rather than represented by recursive C++ callback types.

| Struct | Fields beyond common descriptions/references |
| --- | --- |
| `Parameter` | `name`, `in`, `required`, `deprecated`, `allow_empty_value`, `schema`, `content`; optional `style`, `explode`, `allow_reserved` |
| `RequestBody` | Media-type `content`, `required`, `content_defined` |
| `Response` | `description`, media-type `content`, raw JSON maps `headers` and `links`, `description_defined` |
| `MediaType` | Typed `schema`, optional JSON `example`, named JSON `examples` |

Optional serialization booleans retain explicit `false` values. Presence flags also preserve empty values whose absence has a different meaning: request body `content`, operation `responses`, and an explicitly empty response description alongside a `$ref`. Nonempty maps and descriptions are written without requiring these flags when constructing a document in C++.

## Security inheritance

Both `Document` and `Operation` expose:

```cpp
std::vector<std::map<std::string, std::vector<std::string>>> security;
bool security_defined {false};
```

An omitted operation `security` inherits document security. An explicitly empty array removes that requirement. Multiple array entries are alternatives; multiple schemes inside one entry apply together. These semantics follow the [OpenAPI 3.1.1 Operation Object](https://spec.openapis.org/oas/v3.1.1.html#operation-object).

```cpp
swaggercpp::Operation public_operation;
public_operation.security_defined = true; // Writes security: []

swaggercpp::Operation authenticated_operation;
authenticated_operation.security = {{{"oauth", {"read:items"}}}};
```

`SecurityScheme` has typed `type`, `description`, `name`, `in`, `scheme`, `bearer_format`, and `open_id_connect_url`. OAuth2 `flows` remains `std::optional<nlohmann::json>` so flow details can round-trip without introducing a second, conflicting representation.

## Schema and SchemaPtr

```cpp
struct Schema;
using SchemaPtr = std::shared_ptr<Schema>;
```

Schemas can share nodes. Prefer `$ref` strings for recursive relationships; serialization returns a structured failure for a pointer cycle without a usable reference. Both document walkers stop descending into active schema nodes.

| C++ field | JSON keyword / behavior |
| --- | --- |
| `ref`, `type`, `format`, `title`, `description` | Optional string annotations and reference |
| `boolean_value` | Optional boolean schema; when present, it takes precedence over object fields |
| `types` | OpenAPI 3.1 array-valued `type`; a nonempty vector takes precedence over `type` |
| `enum_values`, `enum_defined` | JSON `enum` values; the presence flag retains an explicitly empty array |
| `const_value` | Optional JSON `const`, including an explicit JSON null |
| `minimum`, `maximum`, `multiple_of` | Optional JSON numbers, preserving signed/unsigned integer precision |
| `exclusive_minimum`, `exclusive_maximum` | OpenAPI 3.0 boolean bounds or OpenAPI 3.1 numeric bounds |
| `min_length`, `max_length` | Optional unsigned 64-bit `minLength` / `maxLength` |
| `min_items`, `max_items` | Optional unsigned 64-bit `minItems` / `maxItems` |
| `min_properties`, `max_properties` | Optional unsigned 64-bit `minProperties` / `maxProperties` |
| `pattern`, `unique_items` | Optional `pattern` string and `uniqueItems` boolean |
| `schema_dialect`, `id`, `anchor` | `$schema`, `$id`, `$anchor` |
| `properties`, `defs` | Named typed schema maps for `properties` and `$defs` |
| `items`, `not_schema` | Child schemas for `items` and `not` |
| `all_of`, `any_of`, `one_of` | Vectors of child schemas |
| `required` | Required property names |
| `default_value`, `example`, `additional_properties` | Optional raw JSON values |
| `nullable`, `read_only`, `write_only`, `deprecated` | Existing boolean fields |

The reader accepts boolean schemas anywhere it parses a schema, including properties, array items, composition arrays, parameters, and media types. The validator checks their OpenAPI version compatibility. In OpenAPI 3.1, represent nullable types with `types = {"string", "null"}`; `nullable` remains available for OpenAPI 3.0 documents. See the [OpenAPI 3.1.1 Schema Object](https://spec.openapis.org/oas/v3.1.1.html#schema-object).

```cpp
auto identifier = std::make_shared<swaggercpp::Schema>();
identifier->types = {"integer", "null"};
identifier->minimum = nlohmann::json(std::uint64_t {9007199254740993ULL});
identifier->const_value = nullptr; // Present JSON null, not an absent optional.

auto rejected = std::make_shared<swaggercpp::Schema>();
rejected->boolean_value = false;
```

Unmodeled vocabularies such as `if`, `then`, `prefixItems`, and `unevaluatedProperties` stay in `unknown_fields`. Retaining these fields does not imply that the validator implements their JSON Schema semantics or that the typed walkers traverse them. Instance/payload validation is not provided by this model.

## Extensions and unmodeled fields

Every modeled OpenAPI object has two maps:

```cpp
using ExtensionMap = std::map<std::string, nlohmann::json>;
ExtensionMap extensions;     // x-* keys
ExtensionMap unknown_fields; // Other keys without a dedicated C++ field
```

The reader separates these maps. The writer starts with preserved fields and writes populated typed fields over them; extensions cannot overwrite emitted fields. Use the typed member for a modeled keyword instead of inserting the same key into either map.

`Document::paths_extensions` and `Operation::responses_extensions` preserve extensions belonging to the enclosing Paths and Responses objects. JSON values survive conversion, but comments, source formatting, and mapping order are not retained. Optional fields with equivalent default semantics can be normalized during serialization.

## Source metadata

```cpp
struct SourceLocation {
    std::size_t line {0};
    std::size_t column {0};
};

// Appended Document members:
std::map<std::string, SourceLocation> source_locations;
std::string source_file;
```

Set `ReaderOptions::track_source_locations = true` to collect locations. Keys are escaped JSON Pointers; line and column are one-based when available. `read_file` records the source filename, and validator issues use the closest available location for their pointer. Metadata refers to the original input and does not automatically move when you edit the document. It is never serialized into the OpenAPI output.

## See also

- [Build from code](/examples/build-from-code)
- [DocumentWalker](/api/walker)
- [DocumentValidator](/api/validator)
