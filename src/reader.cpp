#include "swaggercpp/reader.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

#include "detail/json_utils.hpp"
#include "detail/yaml_utils.hpp"

namespace swaggercpp {
namespace {

using detail::json;

std::string load_text_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool looks_like_json(const std::string_view text) {
    for (const auto ch : text) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            return ch == '{' || ch == '[';
        }
    }
    return false;
}

std::optional<std::string> required_string(
    const json& node,
    const std::string_view key,
    const std::string_view path,
    std::vector<Issue>& issues,
    const ReaderOptions& options) {
    if (const auto value = detail::get_optional_scalar<std::string>(node, key)) {
        return value;
    }

    if (options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, key);
    }
    return std::nullopt;
}

Contact parse_contact(const json& node, const std::string_view, std::vector<Issue>&, const ReaderOptions&) {
    Contact contact;
    contact.name = detail::get_optional_scalar<std::string>(node, "name");
    contact.url = detail::get_optional_scalar<std::string>(node, "url");
    contact.email = detail::get_optional_scalar<std::string>(node, "email");
    detail::collect_extensions(node, contact.extensions, {"name", "url", "email"});
    return contact;
}

License parse_license(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    License license;
    license.name = required_string(node, "name", path, issues, options).value_or("");
    license.url = detail::get_optional_scalar<std::string>(node, "url");
    license.identifier = detail::get_optional_scalar<std::string>(node, "identifier");
    detail::collect_extensions(node, license.extensions, {"name", "url", "identifier"});
    return license;
}

Info parse_info(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Info info;
    info.title = required_string(node, "title", path, issues, options).value_or("");
    info.version = required_string(node, "version", path, issues, options).value_or("");
    info.summary = detail::get_optional_scalar<std::string>(node, "summary");
    info.description = detail::get_optional_scalar<std::string>(node, "description");
    info.terms_of_service = detail::get_optional_scalar<std::string>(node, "termsOfService");

    if (const auto it = node.find("contact"); it != node.end() && it->is_object()) {
        info.contact = parse_contact(*it, std::string(path) + "/contact", issues, options);
    }
    if (const auto it = node.find("license"); it != node.end() && it->is_object()) {
        info.license = parse_license(*it, std::string(path) + "/license", issues, options);
    }

    detail::collect_extensions(node, info.extensions, {"title", "version", "summary", "description", "termsOfService", "contact", "license"});
    return info;
}

ServerVariable parse_server_variable(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    ServerVariable variable;
    variable.default_value = required_string(node, "default", path, issues, options).value_or("");
    variable.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("enum"); it != node.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (item.is_string()) {
                variable.enum_values.push_back(item.get<std::string>());
            }
        }
    }
    detail::collect_extensions(node, variable.extensions, {"default", "description", "enum"});
    return variable;
}

Server parse_server(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Server server;
    server.url = required_string(node, "url", path, issues, options).value_or("");
    server.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("variables"); it != node.end() && it->is_object()) {
        for (const auto& [name, item] : it->items()) {
            server.variables.emplace(name, parse_server_variable(item, std::string(path) + "/variables/" + name, issues, options));
        }
    }
    detail::collect_extensions(node, server.extensions, {"url", "description", "variables"});
    return server;
}

ExternalDocumentation parse_external_docs(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    ExternalDocumentation external_docs;
    external_docs.url = required_string(node, "url", path, issues, options).value_or("");
    external_docs.description = detail::get_optional_scalar<std::string>(node, "description");
    detail::collect_extensions(node, external_docs.extensions, {"url", "description"});
    return external_docs;
}

Tag parse_tag(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Tag tag;
    tag.name = required_string(node, "name", path, issues, options).value_or("");
    tag.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("externalDocs"); it != node.end() && it->is_object()) {
        tag.external_docs = parse_external_docs(*it, std::string(path) + "/externalDocs", issues, options);
    }
    detail::collect_extensions(node, tag.extensions, {"name", "description", "externalDocs"});
    return tag;
}

SchemaPtr parse_schema(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options);

MediaType parse_media_type(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    MediaType media_type;
    if (const auto it = node.find("schema"); it != node.end() && it->is_object()) {
        media_type.schema = parse_schema(*it, std::string(path) + "/schema", issues, options);
    }
    if (const auto it = node.find("example"); it != node.end()) {
        media_type.example = *it;
    }
    if (const auto it = node.find("examples"); it != node.end() && it->is_object()) {
        for (const auto& [name, item] : it->items()) {
            media_type.examples.emplace(name, item);
        }
    }
    detail::collect_extensions(node, media_type.extensions, {"schema", "example", "examples"});
    return media_type;
}

SchemaPtr parse_schema(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    auto schema = std::make_shared<Schema>();
    schema->ref = detail::get_optional_scalar<std::string>(node, "$ref");
    schema->type = detail::get_optional_scalar<std::string>(node, "type");
    schema->format = detail::get_optional_scalar<std::string>(node, "format");
    schema->title = detail::get_optional_scalar<std::string>(node, "title");
    schema->description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("default"); it != node.end()) {
        schema->default_value = *it;
    }
    if (const auto it = node.find("example"); it != node.end()) {
        schema->example = *it;
    }
    if (const auto it = node.find("additionalProperties"); it != node.end()) {
        schema->additional_properties = *it;
    }
    schema->nullable = detail::get_value_or<bool>(node, "nullable", false);
    schema->read_only = detail::get_value_or<bool>(node, "readOnly", false);
    schema->write_only = detail::get_value_or<bool>(node, "writeOnly", false);
    schema->deprecated = detail::get_value_or<bool>(node, "deprecated", false);

    if (const auto it = node.find("required"); it != node.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (item.is_string()) {
                schema->required.push_back(item.get<std::string>());
            }
        }
    }

    if (const auto it = node.find("properties"); it != node.end() && it->is_object()) {
        for (const auto& [name, property_schema] : it->items()) {
            schema->properties.emplace(name, parse_schema(property_schema, std::string(path) + "/properties/" + name, issues, options));
        }
    }

    if (const auto it = node.find("items"); it != node.end() && it->is_object()) {
        schema->items = parse_schema(*it, std::string(path) + "/items", issues, options);
    }
    if (const auto it = node.find("not"); it != node.end() && it->is_object()) {
        schema->not_schema = parse_schema(*it, std::string(path) + "/not", issues, options);
    }

    auto parse_schema_array = [&](const std::string_view key, std::vector<SchemaPtr>& target) {
        if (const auto it = node.find(std::string(key)); it != node.end() && it->is_array()) {
            std::size_t index = 0;
            for (const auto& item : *it) {
                if (item.is_object()) {
                    target.push_back(parse_schema(item, std::string(path) + "/" + std::string(key) + "/" + std::to_string(index), issues, options));
                }
                ++index;
            }
        }
    };

    parse_schema_array("allOf", schema->all_of);
    parse_schema_array("anyOf", schema->any_of);
    parse_schema_array("oneOf", schema->one_of);

    detail::collect_extensions(node, schema->extensions, {"$ref", "type", "format", "title", "description", "default", "example", "additionalProperties", "properties", "required", "allOf", "anyOf", "oneOf", "items", "not", "nullable", "readOnly", "writeOnly", "deprecated"});
    return schema;
}

Parameter parse_parameter(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Parameter parameter;
    parameter.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    parameter.name = detail::get_optional_scalar<std::string>(node, "name").value_or("");

    if (const auto location = detail::get_optional_scalar<std::string>(node, "in")) {
        parameter.in = parameter_location_from_string(*location).value_or(ParameterLocation::query);
    } else if (options.strict_required_fields && !parameter.ref) {
        detail::add_missing_required_issue(issues, path, "in");
    }

    parameter.description = detail::get_optional_scalar<std::string>(node, "description");
    parameter.required = detail::get_value_or<bool>(node, "required", false);
    parameter.deprecated = detail::get_value_or<bool>(node, "deprecated", false);
    parameter.allow_empty_value = detail::get_value_or<bool>(node, "allowEmptyValue", false);

    if (const auto it = node.find("schema"); it != node.end() && it->is_object()) {
        parameter.schema = parse_schema(*it, std::string(path) + "/schema", issues, options);
    }
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            parameter.content.emplace(media_type_name, parse_media_type(media_type_node, std::string(path) + "/content/" + media_type_name, issues, options));
        }
    }

    detail::collect_extensions(node, parameter.extensions, {"$ref", "name", "in", "description", "required", "deprecated", "allowEmptyValue", "schema", "content"});
    return parameter;
}

RequestBody parse_request_body(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    RequestBody request_body;
    request_body.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    request_body.description = detail::get_optional_scalar<std::string>(node, "description");
    request_body.required = detail::get_value_or<bool>(node, "required", false);
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            request_body.content.emplace(media_type_name, parse_media_type(media_type_node, std::string(path) + "/content/" + media_type_name, issues, options));
        }
    } else if (!request_body.ref && options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "content");
    }
    detail::collect_extensions(node, request_body.extensions, {"$ref", "description", "required", "content"});
    return request_body;
}

Response parse_response(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Response response;
    response.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    response.description = detail::get_optional_scalar<std::string>(node, "description").value_or("");
    if (!response.ref && response.description.empty() && options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "description");
    }
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            response.content.emplace(media_type_name, parse_media_type(media_type_node, std::string(path) + "/content/" + media_type_name, issues, options));
        }
    }
    detail::collect_extensions(node, response.extensions, {"$ref", "description", "content"});
    return response;
}

Operation parse_operation(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Operation operation;
    operation.summary = detail::get_optional_scalar<std::string>(node, "summary");
    operation.description = detail::get_optional_scalar<std::string>(node, "description");
    operation.operation_id = detail::get_optional_scalar<std::string>(node, "operationId");
    operation.deprecated = detail::get_value_or<bool>(node, "deprecated", false);

    if (const auto it = node.find("tags"); it != node.end() && it->is_array()) {
        for (const auto& item : *it) {
            if (item.is_string()) {
                operation.tags.push_back(item.get<std::string>());
            }
        }
    }
    if (const auto it = node.find("parameters"); it != node.end() && it->is_array()) {
        std::size_t index = 0;
        for (const auto& item : *it) {
            operation.parameters.push_back(parse_parameter(item, std::string(path) + "/parameters/" + std::to_string(index), issues, options));
            ++index;
        }
    }
    if (const auto it = node.find("requestBody"); it != node.end() && it->is_object()) {
        operation.request_body = parse_request_body(*it, std::string(path) + "/requestBody", issues, options);
    }
    if (const auto it = node.find("responses"); it != node.end() && it->is_object()) {
        for (const auto& [code, response_node] : it->items()) {
            operation.responses.emplace(code, parse_response(response_node, std::string(path) + "/responses/" + code, issues, options));
        }
    } else if (options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "responses");
    }

    if (const auto it = node.find("security"); it != node.end() && it->is_array()) {
        for (const auto& item : *it) {
            std::map<std::string, std::vector<std::string>> requirement;
            if (item.is_object()) {
                for (const auto& [name, values] : item.items()) {
                    std::vector<std::string> scopes;
                    if (values.is_array()) {
                        for (const auto& scope : values) {
                            if (scope.is_string()) {
                                scopes.push_back(scope.get<std::string>());
                            }
                        }
                    }
                    requirement.emplace(name, std::move(scopes));
                }
            }
            operation.security.push_back(std::move(requirement));
        }
    }

    detail::collect_extensions(node, operation.extensions, {"summary", "description", "operationId", "deprecated", "tags", "parameters", "requestBody", "responses", "security"});
    return operation;
}

PathItem parse_path_item(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    PathItem path_item;
    path_item.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    path_item.summary = detail::get_optional_scalar<std::string>(node, "summary");
    path_item.description = detail::get_optional_scalar<std::string>(node, "description");

    if (const auto it = node.find("parameters"); it != node.end() && it->is_array()) {
        std::size_t index = 0;
        for (const auto& item : *it) {
            path_item.parameters.push_back(parse_parameter(item, std::string(path) + "/parameters/" + std::to_string(index), issues, options));
            ++index;
        }
    }

    for (const auto& [key, value] : node.items()) {
        if (const auto method = http_method_from_string(key); method && value.is_object()) {
            path_item.operations.emplace(*method, parse_operation(value, std::string(path) + "/" + key, issues, options));
        }
    }

    detail::collect_extensions(node, path_item.extensions, {"$ref", "summary", "description", "parameters", "get", "put", "post", "delete", "options", "head", "patch", "trace"});
    return path_item;
}

SecurityScheme parse_security_scheme(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    SecurityScheme scheme;
    scheme.type = required_string(node, "type", path, issues, options).value_or("");
    scheme.description = detail::get_optional_scalar<std::string>(node, "description");
    scheme.name = detail::get_optional_scalar<std::string>(node, "name");
    scheme.in = detail::get_optional_scalar<std::string>(node, "in");
    scheme.scheme = detail::get_optional_scalar<std::string>(node, "scheme");
    scheme.bearer_format = detail::get_optional_scalar<std::string>(node, "bearerFormat");
    scheme.open_id_connect_url = detail::get_optional_scalar<std::string>(node, "openIdConnectUrl");
    if (const auto it = node.find("flows"); it != node.end()) {
        scheme.flows = *it;
    }
    detail::collect_extensions(node, scheme.extensions, {"type", "description", "name", "in", "scheme", "bearerFormat", "openIdConnectUrl", "flows"});
    return scheme;
}

Components parse_components(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    Components components;

    auto parse_named_map = [&](const std::string_view key, auto& target, auto&& parser) {
        if (const auto it = node.find(std::string(key)); it != node.end() && it->is_object()) {
            for (const auto& [name, item] : it->items()) {
                target.emplace(name, parser(item, std::string(path) + "/" + std::string(key) + "/" + name, issues, options));
            }
        }
    };

    parse_named_map("schemas", components.schemas, [](const auto& item, const auto& item_path, auto& item_issues, const auto& item_options) {
        return parse_schema(item, item_path, item_issues, item_options);
    });
    parse_named_map("responses", components.responses, parse_response);
    parse_named_map("parameters", components.parameters, parse_parameter);
    parse_named_map("requestBodies", components.request_bodies, parse_request_body);
    parse_named_map("securitySchemes", components.security_schemes, parse_security_scheme);
    detail::collect_extensions(node, components.extensions, {"schemas", "responses", "parameters", "requestBodies", "securitySchemes"});
    return components;
}

Document parse_document(const json& root, std::vector<Issue>& issues, const ReaderOptions& options) {
    Document document;
    document.swagger = detail::get_optional_scalar<std::string>(root, "swagger");
    document.openapi = detail::get_optional_scalar<std::string>(root, "openapi");

    if (document.swagger) {
        document.version = SpecificationVersion::swagger_2_0;
        issues.push_back(Issue {
            .severity = IssueSeverity::warning,
            .path = "/swagger",
            .message = "Swagger 2.0 compatibility is partial in this initial release; prefer OpenAPI 3.x for full fidelity."
        });
    } else if (document.openapi && document.openapi->starts_with("3.0")) {
        document.version = SpecificationVersion::openapi_3_0;
    } else {
        document.version = SpecificationVersion::openapi_3_1;
    }

    if (const auto it = root.find("info"); it != root.end() && it->is_object()) {
        document.info = parse_info(*it, "/info", issues, options);
    } else if (options.strict_required_fields) {
        detail::add_missing_required_issue(issues, "/", "info");
    }

    if (const auto it = root.find("servers"); it != root.end() && it->is_array()) {
        std::size_t index = 0;
        for (const auto& item : *it) {
            document.servers.push_back(parse_server(item, "/servers/" + std::to_string(index), issues, options));
            ++index;
        }
    }

    if (const auto it = root.find("paths"); it != root.end() && it->is_object()) {
        for (const auto& [path_name, item] : it->items()) {
            document.paths.emplace(path_name, parse_path_item(item, "/paths/" + path_name, issues, options));
        }
    } else if (options.strict_required_fields) {
        detail::add_missing_required_issue(issues, "/", "paths");
    }

    if (const auto it = root.find("components"); it != root.end() && it->is_object()) {
        document.components = parse_components(*it, "/components", issues, options);
    }

    if (const auto it = root.find("tags"); it != root.end() && it->is_array()) {
        std::size_t index = 0;
        for (const auto& item : *it) {
            document.tags.push_back(parse_tag(item, "/tags/" + std::to_string(index), issues, options));
            ++index;
        }
    }

    detail::collect_extensions(root, document.extensions, {"swagger", "openapi", "info", "servers", "paths", "components", "tags"});
    return document;
}

json load_content(std::string_view content, std::vector<Issue>& issues, const ReaderOptions& options) {
    try {
        if (looks_like_json(content)) {
            return json::parse(content);
        }

        if (!options.allow_yaml) {
            issues.push_back(Issue {
                .severity = IssueSeverity::error,
                .path = "/",
                .message = "Input does not look like JSON and YAML support is disabled"
            });
            return json::object();
        }

        const auto yaml_node = YAML::Load(std::string(content));
        return detail::yaml_to_json(yaml_node);
    } catch (const std::exception& ex) {
        issues.push_back(Issue {
            .severity = IssueSeverity::error,
            .path = "/",
            .message = ex.what()
        });
        return json::object();
    }
}

} // namespace

Result<Document> DocumentReader::read(const std::string_view content, const ReaderOptions options) {
    std::vector<Issue> issues;
    const auto root = load_content(content, issues, options);
    for (const auto& issue : issues) {
        if (issue.severity == IssueSeverity::error) {
            return Result<Document>::failure(std::move(issues));
        }
    }
    return Result<Document>::success(parse_document(root, issues, options), issues);
}

Result<Document> DocumentReader::read_file(const std::filesystem::path& path, const ReaderOptions options) {
    try {
        return read(load_text_file(path), options);
    } catch (const std::exception& ex) {
        return Result<Document>::failure({Issue {.severity = IssueSeverity::error, .path = path.string(), .message = ex.what()}});
    }
}

} // namespace swaggercpp
