# Document model

Header: `swaggercpp/document.hpp`

Every OpenAPI concept is represented as a plain C++ struct. There is no hidden state — struct layouts map one-to-one to spec nodes.

## Enums

```cpp
enum class SpecificationVersion { swagger_2_0, openapi_3_0, openapi_3_1 };
enum class DocumentFormat       { json, yaml };
enum class ParameterLocation    { query, header, path, cookie };
enum class HttpMethod           { get, put, post, delete_, options, head, patch, trace };
```

::: tip
`delete` is a reserved keyword in C++; the enumerator is spelled `delete_`. String conversions (`to_string`, `http_method_from_string`) handle the mapping to the lowercase spec token automatically.
:::

## String conversions

```cpp
std::string_view to_string(HttpMethod);              // "get", "put", "post", ...
std::string_view to_string(ParameterLocation);       // "query", "header", "path", "cookie"
std::string_view to_string(SpecificationVersion);    // "2.0", "3.0", "3.1"

std::optional<HttpMethod>        http_method_from_string(std::string_view);
std::optional<ParameterLocation> parameter_location_from_string(std::string_view);
```

Reverse parsing returns `std::nullopt` for unknown tokens — never throws.

## `Document`

Top-level node:

```cpp
struct Document {
    SpecificationVersion version { SpecificationVersion::openapi_3_1 };
    std::optional<std::string> swagger;   // "2.0" for Swagger 2 specs
    std::optional<std::string> openapi;   // "3.0.x" or "3.1.x" for OpenAPI
    Info info;
    std::vector<Server> servers;
    std::map<std::string, PathItem> paths;
    Components components;
    std::vector<Tag> tags;
    ExtensionMap extensions;              // x-* vendor fields
};
```

::: info
`paths` is a `std::map` (sorted by key). Iteration order is therefore stable and alphabetical — useful for deterministic output.
:::

## `Info`, `Contact`, `License`

```cpp
struct Info {
    std::string title;
    std::string version;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::optional<std::string> terms_of_service;
    std::optional<Contact> contact;
    std::optional<License> license;
    ExtensionMap extensions;
};

struct Contact {
    std::optional<std::string> name;
    std::optional<std::string> url;
    std::optional<std::string> email;
    ExtensionMap extensions;
};

struct License {
    std::string name;
    std::optional<std::string> url;
    std::optional<std::string> identifier;
    ExtensionMap extensions;
};
```

## `Server`, `ServerVariable`

```cpp
struct ServerVariable {
    std::string default_value;
    std::vector<std::string> enum_values;
    std::optional<std::string> description;
    ExtensionMap extensions;
};

struct Server {
    std::string url;
    std::optional<std::string> description;
    std::map<std::string, ServerVariable> variables;
    ExtensionMap extensions;
};
```

## `PathItem`, `Operation`

```cpp
struct PathItem {
    std::optional<std::string> ref;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::vector<Parameter> parameters;
    std::map<HttpMethod, Operation> operations;
    ExtensionMap extensions;
};

struct Operation {
    std::vector<std::string> tags;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::optional<std::string> operation_id;
    std::vector<Parameter> parameters;
    std::optional<RequestBody> request_body;
    std::map<std::string, Response> responses;
    bool deprecated { false };
    std::vector<std::map<std::string, std::vector<std::string>>> security;
    ExtensionMap extensions;
};
```

## `Parameter`, `RequestBody`, `Response`

```cpp
struct Parameter {
    std::optional<std::string> ref;
    std::string name;
    ParameterLocation in { ParameterLocation::query };
    std::optional<std::string> description;
    bool required { false };
    bool deprecated { false };
    bool allow_empty_value { false };
    SchemaPtr schema;
    std::map<std::string, MediaType> content;
    ExtensionMap extensions;
};

struct RequestBody {
    std::optional<std::string> ref;
    std::optional<std::string> description;
    std::map<std::string, MediaType> content;
    bool required { false };
    ExtensionMap extensions;
};

struct Response {
    std::optional<std::string> ref;
    std::string description;
    std::map<std::string, MediaType> content;
    ExtensionMap extensions;
};
```

## `Schema` and `SchemaPtr`

Schemas form a tree. Shared ownership is used so references compose without copying:

```cpp
struct Schema;
using SchemaPtr = std::shared_ptr<Schema>;

struct Schema {
    std::optional<std::string> ref;                    // "$ref": "#/components/schemas/Pet"
    std::optional<std::string> type;                   // "string", "integer", "object", ...
    std::optional<std::string> format;                 // "date-time", "uuid", ...
    std::optional<std::string> title;
    std::optional<std::string> description;
    std::optional<nlohmann::json> default_value;
    std::optional<nlohmann::json> example;
    std::optional<nlohmann::json> additional_properties;
    std::map<std::string, SchemaPtr> properties;
    std::vector<std::string> required;
    std::vector<SchemaPtr> all_of;
    std::vector<SchemaPtr> any_of;
    std::vector<SchemaPtr> one_of;
    SchemaPtr items;
    SchemaPtr not_schema;
    bool nullable   { false };
    bool read_only  { false };
    bool write_only { false };
    bool deprecated { false };
    ExtensionMap extensions;
};
```

## `Components` and security

```cpp
struct SecurityScheme {
    std::string type;                             // "http", "apiKey", "oauth2", "openIdConnect"
    std::optional<std::string> description;
    std::optional<std::string> name;
    std::optional<std::string> in;                // "header" / "query" / "cookie"
    std::optional<std::string> scheme;            // "bearer", "basic", ...
    std::optional<std::string> bearer_format;
    std::optional<std::string> open_id_connect_url;
    std::optional<nlohmann::json> flows;          // raw OAuth2 flows object
    ExtensionMap extensions;
};

struct Components {
    std::map<std::string, SchemaPtr>         schemas;
    std::map<std::string, Response>          responses;
    std::map<std::string, Parameter>         parameters;
    std::map<std::string, RequestBody>       request_bodies;
    std::map<std::string, SecurityScheme>    security_schemes;
    ExtensionMap extensions;
};
```

## Extension fields

Every struct exposes `ExtensionMap extensions`:

```cpp
using ExtensionMap = std::map<std::string, nlohmann::json>;
```

Vendor-specific `x-*` fields round-trip losslessly through `extensions`.

## Example: building a document in code

See [Build from code](/examples/build-from-code).
