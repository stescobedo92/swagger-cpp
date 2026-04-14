#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace swaggercpp {

using ExtensionMap = std::map<std::string, nlohmann::json>;

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

[[nodiscard]] std::string_view to_string(HttpMethod method) noexcept;
[[nodiscard]] std::string_view to_string(ParameterLocation location) noexcept;
[[nodiscard]] std::string_view to_string(SpecificationVersion version) noexcept;

[[nodiscard]] std::optional<HttpMethod> http_method_from_string(std::string_view value) noexcept;
[[nodiscard]] std::optional<ParameterLocation> parameter_location_from_string(std::string_view value) noexcept;

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

struct ExternalDocumentation {
    std::string url;
    std::optional<std::string> description;
    ExtensionMap extensions;
};

struct Tag {
    std::string name;
    std::optional<std::string> description;
    std::optional<ExternalDocumentation> external_docs;
    ExtensionMap extensions;
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
};

struct MediaType {
    SchemaPtr schema;
    std::optional<nlohmann::json> example;
    std::map<std::string, nlohmann::json> examples;
    ExtensionMap extensions;
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
};

struct RequestBody {
    std::optional<std::string> ref;
    std::optional<std::string> description;
    std::map<std::string, MediaType> content;
    bool required {false};
    ExtensionMap extensions;
};

struct Response {
    std::optional<std::string> ref;
    std::string description;
    std::map<std::string, MediaType> content;
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
    bool deprecated {false};
    std::vector<std::map<std::string, std::vector<std::string>>> security;
    ExtensionMap extensions;
};

struct PathItem {
    std::optional<std::string> ref;
    std::optional<std::string> summary;
    std::optional<std::string> description;
    std::vector<Parameter> parameters;
    std::map<HttpMethod, Operation> operations;
    ExtensionMap extensions;
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
};

struct Components {
    std::map<std::string, SchemaPtr> schemas;
    std::map<std::string, Response> responses;
    std::map<std::string, Parameter> parameters;
    std::map<std::string, RequestBody> request_bodies;
    std::map<std::string, SecurityScheme> security_schemes;
    ExtensionMap extensions;
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
};

} // namespace swaggercpp

