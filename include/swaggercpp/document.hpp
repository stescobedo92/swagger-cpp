#pragma once

#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include "swaggercpp/export.hpp"

namespace swaggercpp {

using ExtensionMap = std::map<std::string, nlohmann::json>;

struct SourceLocation {
    std::size_t line {0};
    std::size_t column {0};
};

enum class SpecificationVersion {
    swagger_2_0,
    openapi_3_0,
    openapi_3_1
};

enum class DocumentFormat {
    json,
    yaml
};

enum class ParameterLocation {
    query,
    header,
    path,
    cookie
};

enum class HttpMethod {
    get,
    put,
    post,
    delete_,
    options,
    head,
    patch,
    trace
};

[[nodiscard]] SWAGGERCPP_EXPORT std::string_view to_string(HttpMethod method) noexcept;
[[nodiscard]] SWAGGERCPP_EXPORT std::string_view to_string(ParameterLocation location) noexcept;
[[nodiscard]] SWAGGERCPP_EXPORT std::string_view to_string(SpecificationVersion version) noexcept;

[[nodiscard]] SWAGGERCPP_EXPORT std::optional<HttpMethod> http_method_from_string(std::string_view value) noexcept;
[[nodiscard]] SWAGGERCPP_EXPORT std::optional<ParameterLocation> parameter_location_from_string(std::string_view value) noexcept;

struct Contact {
    std::optional<std::string> name;
    std::optional<std::string> url;
    std::optional<std::string> email;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct License {
    std::string name;
    std::optional<std::string> url;
    std::optional<std::string> identifier;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Info {
    std::string title;
    std::string version;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::optional<std::string> terms_of_service;
    std::optional<Contact> contact;
    std::optional<License> license;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct ServerVariable {
    std::string default_value;
    std::vector<std::string> enum_values;
    std::optional<std::string> description;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Server {
    std::string url;
    std::optional<std::string> description;
    std::map<std::string, ServerVariable> variables;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct ExternalDocumentation {
    std::string url;
    std::optional<std::string> description;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Tag {
    std::string name;
    std::optional<std::string> description;
    std::optional<ExternalDocumentation> external_docs;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Schema;
using SchemaPtr = std::shared_ptr<Schema>;

struct Schema {
    std::optional<std::string> ref;
    std::optional<std::string> type;
    std::optional<std::string> format;
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
    bool nullable {false};
    bool read_only {false};
    bool write_only {false};
    bool deprecated {false};
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    // A boolean schema takes precedence over the object keywords below.
    std::optional<bool> boolean_value;
    // For OpenAPI 3.1 type arrays; nonempty types takes precedence over type.
    std::vector<std::string> types;
    std::vector<nlohmann::json> enum_values;
    bool enum_defined {false};
    std::optional<nlohmann::json> const_value;
    // JSON numeric storage preserves integral bounds beyond double precision.
    std::optional<nlohmann::json> minimum;
    std::optional<nlohmann::json> maximum;
    std::optional<nlohmann::json> multiple_of;
    // OpenAPI 3.0 uses booleans; OpenAPI 3.1 uses numeric bounds.
    std::optional<nlohmann::json> exclusive_minimum;
    std::optional<nlohmann::json> exclusive_maximum;
    std::optional<std::uint64_t> min_length;
    std::optional<std::uint64_t> max_length;
    std::optional<std::uint64_t> min_items;
    std::optional<std::uint64_t> max_items;
    std::optional<std::uint64_t> min_properties;
    std::optional<std::uint64_t> max_properties;
    std::optional<std::string> pattern;
    std::optional<bool> unique_items;
    std::optional<std::string> schema_dialect;
    std::optional<std::string> id;
    std::optional<std::string> anchor;
    std::map<std::string, SchemaPtr> defs;
};

struct MediaType {
    SchemaPtr schema;
    std::optional<nlohmann::json> example;
    std::map<std::string, nlohmann::json> examples;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Parameter {
    std::optional<std::string> ref;
    std::string name;
    ParameterLocation in {ParameterLocation::query};
    std::optional<std::string> description;
    bool required {false};
    bool deprecated {false};
    bool allow_empty_value {false};
    SchemaPtr schema;
    std::map<std::string, MediaType> content;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    std::optional<std::string> style;
    std::optional<bool> explode;
    std::optional<bool> allow_reserved;
};

struct RequestBody {
    std::optional<std::string> ref;
    std::optional<std::string> description;
    std::map<std::string, MediaType> content;
    bool required {false};
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    bool content_defined {false};
};

struct Response {
    std::optional<std::string> ref;
    std::string description;
    std::map<std::string, MediaType> content;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    std::map<std::string, nlohmann::json> headers;
    std::map<std::string, nlohmann::json> links;
    bool description_defined {false};
};

struct Operation {
    std::vector<std::string> tags;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::optional<std::string> operation_id;
    std::vector<Parameter> parameters;
    std::optional<RequestBody> request_body;
    std::map<std::string, Response> responses;
    bool deprecated {false};
    std::vector<std::map<std::string, std::vector<std::string>>> security;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    // An explicitly empty array disables document-level authentication.
    bool security_defined {false};
    std::vector<Server> servers;
    std::map<std::string, nlohmann::json> callbacks;
    ExtensionMap responses_extensions;
    bool servers_defined {false};
    bool responses_defined {false};
};

struct PathItem {
    std::optional<std::string> ref;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::vector<Parameter> parameters;
    std::map<HttpMethod, Operation> operations;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    std::vector<Server> servers;
    bool servers_defined {false};
};

struct SecurityScheme {
    std::string type;
    std::optional<std::string> description;
    std::optional<std::string> name;
    std::optional<std::string> in;
    std::optional<std::string> scheme;
    std::optional<std::string> bearer_format;
    std::optional<std::string> open_id_connect_url;
    std::optional<nlohmann::json> flows;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Components {
    std::map<std::string, SchemaPtr> schemas;
    std::map<std::string, Response> responses;
    std::map<std::string, Parameter> parameters;
    std::map<std::string, RequestBody> request_bodies;
    std::map<std::string, SecurityScheme> security_schemes;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
};

struct Document {
    SpecificationVersion version {SpecificationVersion::openapi_3_1};
    std::optional<std::string> swagger;
    std::optional<std::string> openapi;
    Info info;
    std::vector<Server> servers;
    std::map<std::string, PathItem> paths;
    Components components;
    std::vector<Tag> tags;
    ExtensionMap extensions;
    ExtensionMap unknown_fields;
    std::vector<std::map<std::string, std::vector<std::string>>> security;
    bool security_defined {false};
    std::map<std::string, PathItem> webhooks;
    ExtensionMap paths_extensions;
    std::map<std::string, SourceLocation> source_locations;
    std::string source_file;
};

} // namespace swaggercpp

