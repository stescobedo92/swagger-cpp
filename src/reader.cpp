#include "swaggercpp/reader.hpp"

#include <cctype>
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

#include "detail/json_utils.hpp"
#include "detail/yaml_utils.hpp"

namespace swaggercpp {
namespace {

using detail::json;

class ParseError : public std::runtime_error {
public:
    std::string path;
    ParseError(std::string location, const std::string& message)
        : std::runtime_error(message), path(std::move(location)) {}
};

std::string pointer_path(const std::string_view path, const std::string_view key) {
    std::string escaped;
    for (const char ch : key) {
        if (ch == '~') escaped += "~0";
        else if (ch == '/') escaped += "~1";
        else escaped += ch;
    }
    return std::string(path) + "/" + escaped;
}

void check_fields(const json& node, const std::string_view path,
    const std::initializer_list<std::string_view> objects = {},
    const std::initializer_list<std::string_view> arrays = {},
    const std::initializer_list<std::string_view> strings = {},
    const std::initializer_list<std::string_view> booleans = {},
    const std::initializer_list<std::string_view> numbers = {}) {
    if (!node.is_object()) {
        throw ParseError(std::string(path), "Expected an object");
    }
    auto check = [&](const auto keys, const auto predicate, const char* expected) {
        for (const auto key : keys) {
            if (const auto it = node.find(std::string(key)); it != node.end() && !predicate(*it)) {
                throw ParseError(pointer_path(path, key), std::string("Expected ") + expected);
            }
        }
    };
    check(objects, [](const json& value) { return value.is_object(); }, "an object");
    check(arrays, [](const json& value) { return value.is_array(); }, "an array");
    check(strings, [](const json& value) { return value.is_string(); }, "a string");
    check(booleans, [](const json& value) { return value.is_boolean(); }, "a boolean");
    check(numbers, [](const json& value) { return value.is_number(); }, "a number");
}

std::vector<std::string> parse_string_array(const json& node, const std::string_view path) {
    if (!node.is_array()) {
        throw ParseError(std::string(path), "Expected an array of strings");
    }
    std::vector<std::string> values;
    values.reserve(node.size());
    for (std::size_t index = 0; index < node.size(); ++index) {
        if (!node[index].is_string()) {
            throw ParseError(pointer_path(path, std::to_string(index)), "Expected a string");
        }
        values.push_back(node[index].get<std::string>());
    }
    return values;
}

auto parse_security(const json& node, const std::string_view path) {
    std::vector<std::map<std::string, std::vector<std::string>>> security;
    if (!node.is_array()) {
        throw ParseError(std::string(path), "Expected an array of security requirements");
    }
    for (std::size_t index = 0; index < node.size(); ++index) {
        const auto item_path = pointer_path(path, std::to_string(index));
        check_fields(node[index], item_path);
        std::map<std::string, std::vector<std::string>> requirement;
        for (const auto& [name, scopes] : node[index].items()) {
            requirement.emplace(name, parse_string_array(scopes, pointer_path(item_path, name)));
        }
        security.push_back(std::move(requirement));
    }
    return security;
}

std::string load_text_file(const std::filesystem::path& path, const std::size_t max_bytes) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open file: " + path.string());
    }

    std::string content;
    std::array<char, 8192> buffer {};
    while (stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || stream.gcount() != 0) {
        const auto count = static_cast<std::size_t>(stream.gcount());
        if (count > max_bytes - content.size()) throw std::runtime_error("Document exceeds max_document_bytes");
        content.append(buffer.data(), count);
    }
    if (stream.bad()) throw std::runtime_error("Unable to read file: " + path.string());
    return content;
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

Contact parse_contact(const json& node, const std::string_view path, std::vector<Issue>&, const ReaderOptions&) {
    check_fields(node, path, {}, {}, {"name", "url", "email"});
    Contact contact;
    contact.name = detail::get_optional_scalar<std::string>(node, "name");
    contact.url = detail::get_optional_scalar<std::string>(node, "url");
    contact.email = detail::get_optional_scalar<std::string>(node, "email");
    detail::collect_extensions(node, contact.extensions, {"name", "url", "email"});
    detail::collect_unknown_fields(node, contact.unknown_fields, {"name", "url", "email"});
    return contact;
}

License parse_license(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {}, {}, {"name", "url", "identifier"});
    License license;
    license.name = required_string(node, "name", path, issues, options).value_or("");
    license.url = detail::get_optional_scalar<std::string>(node, "url");
    license.identifier = detail::get_optional_scalar<std::string>(node, "identifier");
    detail::collect_extensions(node, license.extensions, {"name", "url", "identifier"});
    detail::collect_unknown_fields(node, license.unknown_fields, {"name", "url", "identifier"});
    return license;
}

Info parse_info(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"contact", "license"}, {}, {"title", "version", "summary", "description", "termsOfService"});
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
    detail::collect_unknown_fields(node, info.unknown_fields, {"title", "version", "summary", "description", "termsOfService", "contact", "license"});
    return info;
}

ServerVariable parse_server_variable(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {}, {"enum"}, {"default", "description"});
    ServerVariable variable;
    variable.default_value = required_string(node, "default", path, issues, options).value_or("");
    variable.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("enum"); it != node.end() && it->is_array()) {
        variable.enum_values = parse_string_array(*it, pointer_path(path, "enum"));
    }
    detail::collect_extensions(node, variable.extensions, {"default", "description", "enum"});
    detail::collect_unknown_fields(node, variable.unknown_fields, {"default", "description", "enum"});
    return variable;
}

Server parse_server(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"variables"}, {}, {"url", "description"});
    Server server;
    server.url = required_string(node, "url", path, issues, options).value_or("");
    server.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("variables"); it != node.end() && it->is_object()) {
        for (const auto& [name, item] : it->items()) {
            server.variables.emplace(name, parse_server_variable(item, pointer_path(pointer_path(path, "variables"), name), issues, options));
        }
    }
    detail::collect_extensions(node, server.extensions, {"url", "description", "variables"});
    detail::collect_unknown_fields(node, server.unknown_fields, {"url", "description", "variables"});
    return server;
}

ExternalDocumentation parse_external_docs(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {}, {}, {"url", "description"});
    ExternalDocumentation external_docs;
    external_docs.url = required_string(node, "url", path, issues, options).value_or("");
    external_docs.description = detail::get_optional_scalar<std::string>(node, "description");
    detail::collect_extensions(node, external_docs.extensions, {"url", "description"});
    detail::collect_unknown_fields(node, external_docs.unknown_fields, {"url", "description"});
    return external_docs;
}

Tag parse_tag(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"externalDocs"}, {}, {"name", "description"});
    Tag tag;
    tag.name = required_string(node, "name", path, issues, options).value_or("");
    tag.description = detail::get_optional_scalar<std::string>(node, "description");
    if (const auto it = node.find("externalDocs"); it != node.end() && it->is_object()) {
        tag.external_docs = parse_external_docs(*it, std::string(path) + "/externalDocs", issues, options);
    }
    detail::collect_extensions(node, tag.extensions, {"name", "description", "externalDocs"});
    detail::collect_unknown_fields(node, tag.unknown_fields, {"name", "description", "externalDocs"});
    return tag;
}

SchemaPtr parse_schema(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options);

MediaType parse_media_type(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"examples"});
    MediaType media_type;
    if (const auto it = node.find("schema"); it != node.end()) {
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
    detail::collect_unknown_fields(node, media_type.unknown_fields, {"schema", "example", "examples"});
    return media_type;
}

SchemaPtr parse_schema(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    auto schema = std::make_shared<Schema>();
    if (node.is_boolean()) {
        schema->boolean_value = node.get<bool>();
        return schema;
    }
    if (std::ranges::count(path, '/') > 512) {
        throw ParseError(std::string(path), "Schema nesting exceeds the supported depth");
    }
    check_fields(node, path, {"properties", "$defs"}, {"required", "allOf", "anyOf", "oneOf", "enum"},
        {"$ref", "format", "title", "description", "pattern", "$schema", "$id", "$anchor"},
        {"nullable", "readOnly", "writeOnly", "deprecated", "uniqueItems"}, {"minimum", "maximum", "multipleOf"});
    schema->ref = detail::get_optional_scalar<std::string>(node, "$ref");
    if (const auto it = node.find("type"); it != node.end()) {
        if (it->is_array()) {
            schema->types = parse_string_array(*it, pointer_path(path, "type"));
            if (schema->types.empty()) {
                throw ParseError(pointer_path(path, "type"), "A type array must not be empty");
            }
        } else if (it->is_string()) {
            schema->type = it->get<std::string>();
        } else {
            throw ParseError(pointer_path(path, "type"), "Expected a type string or an array of strings");
        }
    }
    if (const auto it = node.find("enum"); it != node.end()) {
        schema->enum_values = it->get<std::vector<json>>();
        schema->enum_defined = true;
    }
    if (const auto it = node.find("const"); it != node.end()) schema->const_value = *it;
    schema->minimum = detail::get_optional_scalar<json>(node, "minimum");
    schema->maximum = detail::get_optional_scalar<json>(node, "maximum");
    schema->multiple_of = detail::get_optional_scalar<json>(node, "multipleOf");
    auto exclusive_bound = [&](const char* key) -> std::optional<json> {
        if (const auto it = node.find(key); it != node.end()) {
            if (!it->is_number() && !it->is_boolean()) {
                throw ParseError(pointer_path(path, key), "Expected a numeric or boolean exclusive bound");
            }
            return std::optional<json>(std::in_place, *it);
        }
        return std::nullopt;
    };
    schema->exclusive_minimum = exclusive_bound("exclusiveMinimum");
    schema->exclusive_maximum = exclusive_bound("exclusiveMaximum");
    auto count_bound = [&](const char* key) -> std::optional<std::uint64_t> {
        if (const auto it = node.find(key); it != node.end()) {
            if (!it->is_number_integer() || (!it->is_number_unsigned() && it->get<std::int64_t>() < 0)) {
                throw ParseError(pointer_path(path, key), "Expected a nonnegative integer");
            }
            return it->get<std::uint64_t>();
        }
        return std::nullopt;
    };
    schema->min_length = count_bound("minLength");
    schema->max_length = count_bound("maxLength");
    schema->min_items = count_bound("minItems");
    schema->max_items = count_bound("maxItems");
    schema->min_properties = count_bound("minProperties");
    schema->max_properties = count_bound("maxProperties");
    schema->pattern = detail::get_optional_scalar<std::string>(node, "pattern");
    schema->schema_dialect = detail::get_optional_scalar<std::string>(node, "$schema");
    schema->id = detail::get_optional_scalar<std::string>(node, "$id");
    schema->anchor = detail::get_optional_scalar<std::string>(node, "$anchor");
    schema->unique_items = detail::get_optional_scalar<bool>(node, "uniqueItems");
    if (const auto it = node.find("$defs"); it != node.end()) {
        for (const auto& [name, child] : it->items()) {
            schema->defs.emplace(name, parse_schema(child, pointer_path(pointer_path(path, "$defs"), name), issues, options));
        }
    }
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
        if (!it->is_boolean() && !it->is_object()) {
            throw ParseError(pointer_path(path, "additionalProperties"), "Expected a schema object or boolean");
        }
        schema->additional_properties = *it;
    }
    schema->nullable = detail::get_value_or<bool>(node, "nullable", false);
    schema->read_only = detail::get_value_or<bool>(node, "readOnly", false);
    schema->write_only = detail::get_value_or<bool>(node, "writeOnly", false);
    schema->deprecated = detail::get_value_or<bool>(node, "deprecated", false);

    if (const auto it = node.find("required"); it != node.end()) {
        schema->required = parse_string_array(*it, pointer_path(path, "required"));
    }

    if (const auto it = node.find("properties"); it != node.end() && it->is_object()) {
        for (const auto& [name, property_schema] : it->items()) {
            schema->properties.emplace(name, parse_schema(property_schema, pointer_path(pointer_path(path, "properties"), name), issues, options));
        }
    }

    if (const auto it = node.find("items"); it != node.end()) {
        schema->items = parse_schema(*it, std::string(path) + "/items", issues, options);
    }
    if (const auto it = node.find("not"); it != node.end()) {
        schema->not_schema = parse_schema(*it, std::string(path) + "/not", issues, options);
    }

    auto parse_schema_array = [&](const std::string_view key, std::vector<SchemaPtr>& target) {
        if (const auto it = node.find(std::string(key)); it != node.end() && it->is_array()) {
            if (it->empty()) {
                throw ParseError(pointer_path(path, key), "A schema composition array must not be empty");
            }
            std::size_t index = 0;
            for (const auto& item : *it) {
                target.push_back(parse_schema(item, std::string(path) + "/" + std::string(key) + "/" + std::to_string(index), issues, options));
                ++index;
            }
        }
    };

    parse_schema_array("allOf", schema->all_of);
    parse_schema_array("anyOf", schema->any_of);
    parse_schema_array("oneOf", schema->one_of);

    detail::collect_extensions(node, schema->extensions, {"$ref", "type", "format", "title", "description", "default", "example", "additionalProperties", "properties", "required", "allOf", "anyOf", "oneOf", "items", "not", "nullable", "readOnly", "writeOnly", "deprecated", "$schema", "$id", "$anchor", "$defs", "enum", "const", "minimum", "maximum", "multipleOf", "exclusiveMinimum", "exclusiveMaximum", "minLength", "maxLength", "minItems", "maxItems", "minProperties", "maxProperties", "pattern", "uniqueItems"});
    detail::collect_unknown_fields(node, schema->unknown_fields, {"$ref", "type", "format", "title", "description", "default", "example", "additionalProperties", "properties", "required", "allOf", "anyOf", "oneOf", "items", "not", "nullable", "readOnly", "writeOnly", "deprecated", "$schema", "$id", "$anchor", "$defs", "enum", "const", "minimum", "maximum", "multipleOf", "exclusiveMinimum", "exclusiveMaximum", "minLength", "maxLength", "minItems", "maxItems", "minProperties", "maxProperties", "pattern", "uniqueItems"});
    return schema;
}

Parameter parse_parameter(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"content", "examples"}, {}, {"$ref", "name", "in", "description", "style"}, {"required", "deprecated", "allowEmptyValue", "explode", "allowReserved"});
    Parameter parameter;
    parameter.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    parameter.name = detail::get_optional_scalar<std::string>(node, "name").value_or("");

    if (const auto location = detail::get_optional_scalar<std::string>(node, "in")) {
        const auto parsed_location = parameter_location_from_string(*location);
        if (!parsed_location) {
            throw ParseError(pointer_path(path, "in"), "Unknown parameter location");
        }
        parameter.in = *parsed_location;
    } else if (options.strict_required_fields && !parameter.ref) {
        detail::add_missing_required_issue(issues, path, "in");
    }
    if (!parameter.ref && !node.contains("name") && options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "name");
    }

    parameter.description = detail::get_optional_scalar<std::string>(node, "description");
    parameter.required = detail::get_value_or<bool>(node, "required", false);
    parameter.deprecated = detail::get_value_or<bool>(node, "deprecated", false);
    parameter.allow_empty_value = detail::get_value_or<bool>(node, "allowEmptyValue", false);
    parameter.style = detail::get_optional_scalar<std::string>(node, "style");
    parameter.explode = detail::get_optional_scalar<bool>(node, "explode");
    parameter.allow_reserved = detail::get_optional_scalar<bool>(node, "allowReserved");

    if (const auto it = node.find("schema"); it != node.end()) {
        parameter.schema = parse_schema(*it, std::string(path) + "/schema", issues, options);
    }
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            parameter.content.emplace(media_type_name, parse_media_type(media_type_node, pointer_path(pointer_path(path, "content"), media_type_name), issues, options));
        }
    }

    detail::collect_extensions(node, parameter.extensions, {"$ref", "name", "in", "description", "required", "deprecated", "allowEmptyValue", "schema", "content", "style", "explode", "allowReserved"});
    detail::collect_unknown_fields(node, parameter.unknown_fields, {"$ref", "name", "in", "description", "required", "deprecated", "allowEmptyValue", "schema", "content", "style", "explode", "allowReserved"});
    return parameter;
}

RequestBody parse_request_body(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"content"}, {}, {"$ref", "description"}, {"required"});
    RequestBody request_body;
    request_body.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    request_body.description = detail::get_optional_scalar<std::string>(node, "description");
    request_body.required = detail::get_value_or<bool>(node, "required", false);
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        request_body.content_defined = true;
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            request_body.content.emplace(media_type_name, parse_media_type(media_type_node, pointer_path(pointer_path(path, "content"), media_type_name), issues, options));
        }
    } else if (!request_body.ref && options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "content");
    }
    detail::collect_extensions(node, request_body.extensions, {"$ref", "description", "required", "content"});
    detail::collect_unknown_fields(node, request_body.unknown_fields, {"$ref", "description", "required", "content"});
    return request_body;
}

Response parse_response(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"content", "headers", "links"}, {}, {"$ref", "description"});
    Response response;
    response.description_defined = node.contains("description");
    response.ref = detail::get_optional_scalar<std::string>(node, "$ref");
    response.description = detail::get_optional_scalar<std::string>(node, "description").value_or("");
    if (!response.ref && !node.contains("description") && options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "description");
    }
    if (const auto it = node.find("content"); it != node.end() && it->is_object()) {
        for (const auto& [media_type_name, media_type_node] : it->items()) {
            response.content.emplace(media_type_name, parse_media_type(media_type_node, pointer_path(pointer_path(path, "content"), media_type_name), issues, options));
        }
    }
    if (const auto it = node.find("headers"); it != node.end()) {
        response.headers = it->get<std::map<std::string, json>>();
    }
    if (const auto it = node.find("links"); it != node.end()) {
        response.links = it->get<std::map<std::string, json>>();
    }
    detail::collect_extensions(node, response.extensions, {"$ref", "description", "content", "headers", "links"});
    detail::collect_unknown_fields(node, response.unknown_fields, {"$ref", "description", "content", "headers", "links"});
    return response;
}

Operation parse_operation(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"requestBody", "responses", "callbacks"}, {"tags", "parameters", "security", "servers"}, {"summary", "description", "operationId"}, {"deprecated"});
    Operation operation;
    operation.summary = detail::get_optional_scalar<std::string>(node, "summary");
    operation.description = detail::get_optional_scalar<std::string>(node, "description");
    operation.operation_id = detail::get_optional_scalar<std::string>(node, "operationId");
    operation.deprecated = detail::get_value_or<bool>(node, "deprecated", false);

    if (const auto it = node.find("tags"); it != node.end() && it->is_array()) {
        operation.tags = parse_string_array(*it, pointer_path(path, "tags"));
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
        operation.responses_defined = true;
        for (const auto& [code, response_node] : it->items()) {
            if (detail::is_extension_key(code)) {
                operation.responses_extensions.emplace(code, response_node);
            } else {
                operation.responses.emplace(code, parse_response(response_node, pointer_path(pointer_path(path, "responses"), code), issues, options));
            }
        }
    } else if (options.strict_required_fields) {
        detail::add_missing_required_issue(issues, path, "responses");
    }

    if (const auto it = node.find("security"); it != node.end()) {
        operation.security = parse_security(*it, pointer_path(path, "security"));
        operation.security_defined = true;
    }
    if (const auto it = node.find("callbacks"); it != node.end()) {
        operation.callbacks = it->get<std::map<std::string, json>>();
    }
    if (const auto it = node.find("servers"); it != node.end()) {
        operation.servers_defined = true;
        for (std::size_t index = 0; index < it->size(); ++index) {
            operation.servers.push_back(parse_server((*it)[index], pointer_path(pointer_path(path, "servers"), std::to_string(index)), issues, options));
        }
    }

    detail::collect_extensions(node, operation.extensions, {"summary", "description", "operationId", "deprecated", "tags", "parameters", "requestBody", "responses", "security", "servers", "callbacks"});
    detail::collect_unknown_fields(node, operation.unknown_fields, {"summary", "description", "operationId", "deprecated", "tags", "parameters", "requestBody", "responses", "security", "servers", "callbacks"});
    return operation;
}

PathItem parse_path_item(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"get", "put", "post", "delete", "options", "head", "patch", "trace"}, {"parameters", "servers"}, {"$ref", "summary", "description"});
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

    if (const auto it = node.find("servers"); it != node.end()) {
        path_item.servers_defined = true;
        for (std::size_t index = 0; index < it->size(); ++index) {
            path_item.servers.push_back(parse_server((*it)[index], pointer_path(pointer_path(path, "servers"), std::to_string(index)), issues, options));
        }
    }
    detail::collect_extensions(node, path_item.extensions, {"$ref", "summary", "description", "parameters", "get", "put", "post", "delete", "options", "head", "patch", "trace", "servers"});
    detail::collect_unknown_fields(node, path_item.unknown_fields, {"$ref", "summary", "description", "parameters", "get", "put", "post", "delete", "options", "head", "patch", "trace", "servers"});
    return path_item;
}

SecurityScheme parse_security_scheme(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"flows"}, {}, {"$ref", "type", "description", "name", "in", "scheme", "bearerFormat", "openIdConnectUrl"});
    SecurityScheme scheme;
    scheme.type = node.contains("$ref") ? "" : required_string(node, "type", path, issues, options).value_or("");
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
    detail::collect_unknown_fields(node, scheme.unknown_fields, {"type", "description", "name", "in", "scheme", "bearerFormat", "openIdConnectUrl", "flows"});
    return scheme;
}

Components parse_components(const json& node, const std::string_view path, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(node, path, {"schemas", "responses", "parameters", "requestBodies", "securitySchemes"});
    Components components;

    auto parse_named_map = [&](const std::string_view key, auto& target, auto&& parser) {
        if (const auto it = node.find(std::string(key)); it != node.end() && it->is_object()) {
            for (const auto& [name, item] : it->items()) {
                target.emplace(name, parser(item, pointer_path(pointer_path(path, key), name), issues, options));
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
    detail::collect_unknown_fields(node, components.unknown_fields, {"schemas", "responses", "parameters", "requestBodies", "securitySchemes"});
    return components;
}

Document parse_document(const json& root, std::vector<Issue>& issues, const ReaderOptions& options) {
    check_fields(root, "", {"info", "paths", "components", "webhooks"}, {"servers", "tags", "security"}, {"swagger", "openapi"});
    Document document;
    document.swagger = detail::get_optional_scalar<std::string>(root, "swagger");
    document.openapi = detail::get_optional_scalar<std::string>(root, "openapi");

    if (document.swagger && document.openapi) {
        throw ParseError("/", "A document must declare exactly one of swagger or openapi");
    }
    auto matches_version = [](const std::string& version, const std::string_view prefix) {
        return version.starts_with(prefix) && version.size() > prefix.size()
            && std::all_of(version.begin() + static_cast<std::ptrdiff_t>(prefix.size()), version.end(),
                [](const unsigned char ch) { return ch >= '0' && ch <= '9'; });
    };
    if (document.swagger && *document.swagger == "2.0") {
        document.version = SpecificationVersion::swagger_2_0;
        issues.push_back(Issue {
            .severity = IssueSeverity::warning,
            .path = "/swagger",
            .message = "Swagger 2.0 compatibility is partial in this initial release; prefer OpenAPI 3.x for full fidelity."
        });
    } else if (document.openapi && matches_version(*document.openapi, "3.0.")) {
        document.version = SpecificationVersion::openapi_3_0;
    } else if (document.openapi && matches_version(*document.openapi, "3.1.")) {
        document.version = SpecificationVersion::openapi_3_1;
    } else {
        throw ParseError(document.swagger ? "/swagger" : "/openapi", "Missing or unsupported specification version; expected Swagger 2.0 or OpenAPI 3.0.x/3.1.x");
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
            if (detail::is_extension_key(path_name)) {
                document.paths_extensions.emplace(path_name, item);
            } else {
                document.paths.emplace(path_name, parse_path_item(item, pointer_path("/paths", path_name), issues, options));
            }
        }
    } else if (options.strict_required_fields && document.version != SpecificationVersion::openapi_3_1) {
        detail::add_missing_required_issue(issues, "/", "paths");
    }

    if (const auto it = root.find("components"); it != root.end() && it->is_object()) {
        document.components = parse_components(*it, "/components", issues, options);
    }
    if (const auto it = root.find("security"); it != root.end()) {
        document.security = parse_security(*it, "/security");
        document.security_defined = true;
    }
    if (const auto it = root.find("webhooks"); it != root.end()) {
        for (const auto& [name, item] : it->items()) {
            document.webhooks.emplace(name, parse_path_item(item, pointer_path("/webhooks", name), issues, options));
        }
    }

    if (const auto it = root.find("tags"); it != root.end() && it->is_array()) {
        std::size_t index = 0;
        for (const auto& item : *it) {
            document.tags.push_back(parse_tag(item, "/tags/" + std::to_string(index), issues, options));
            ++index;
        }
    }

    detail::collect_extensions(root, document.extensions, {"swagger", "openapi", "info", "servers", "paths", "components", "tags", "security", "webhooks"});
    detail::collect_unknown_fields(root, document.unknown_fields, {"swagger", "openapi", "info", "servers", "paths", "components", "tags", "security", "webhooks"});
    return document;
}

json load_content(std::string_view content, std::vector<Issue>& issues, const ReaderOptions& options,
    std::map<std::string, SourceLocation>& locations) {
    try {
        if (content.size() > options.max_document_bytes) {
            throw std::runtime_error("Document exceeds max_document_bytes");
        }
        if (looks_like_json(content)) {
            json root;
            try {
                root = json::parse(content, [&](int depth, json::parse_event_t, json&) {
                    if (static_cast<std::size_t>(depth) > options.max_nesting_depth) {
                        throw std::runtime_error("Document exceeds max_nesting_depth");
                    }
                    return true;
                });
            } catch (const json::parse_error&) {
                if (options.allow_yaml) {
                    // YAML flow mappings also start with '{'. Try YAML only
                    // after a syntax failure; resource-limit errors propagate.
                    YAML::Node yaml_root;
                    bool parsed_yaml = false;
                    try {
                        yaml_root = YAML::Load(std::string(content));
                        parsed_yaml = true;
                    } catch (const YAML::Exception&) {
                        // Retain the original JSON syntax diagnostic below.
                    }
                    const YAML::Node& candidate = yaml_root;
                    if (parsed_yaml && candidate.IsMap()
                        && ((candidate["openapi"] && candidate["openapi"].IsScalar())
                            || (candidate["swagger"] && candidate["swagger"].IsScalar()))) {
                        return detail::yaml_to_json(candidate, options.track_source_locations ? &locations : nullptr,
                            options.max_nesting_depth, options.max_document_bytes);
                    }
                }
                throw;
            }
            if (options.track_source_locations) {
                try {
                    (void)detail::yaml_to_json(YAML::Load(std::string(content)), &locations, options.max_nesting_depth, options.max_document_bytes);
                } catch (const std::exception&) {
                    locations.clear(); // Source metadata must not change JSON acceptance.
                }
            }
            return root;
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
        return detail::yaml_to_json(yaml_node, options.track_source_locations ? &locations : nullptr,
            options.max_nesting_depth, options.max_document_bytes);
    } catch (const YAML::Exception& ex) {
        Issue issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what()};
        if (!ex.mark.is_null()) {
            issue.line = static_cast<std::size_t>(ex.mark.line) + 1;
            issue.column = static_cast<std::size_t>(ex.mark.column) + 1;
        }
        issues.push_back(std::move(issue));
        return json::object();
    } catch (const json::parse_error& ex) {
        const auto offset = std::min(content.size(), ex.byte > 0 ? ex.byte - 1 : 0);
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < offset; ++i) {
            if (content[i] == '\n') { ++line; column = 1; }
            else ++column;
        }
        issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what(),
            .line = line, .column = column});
        return json::object();
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
    std::map<std::string, SourceLocation> locations;
    const auto root = load_content(content, issues, options, locations);
    auto attach_locations = [&]() {
        for (auto& issue : issues) {
            if (issue.line) continue;
            auto path = issue.path;
            while (true) {
                if (const auto it = locations.find(path); it != locations.end()) {
                    issue.line = it->second.line;
                    issue.column = it->second.column;
                    break;
                }
                if (path.empty()) break;
                const auto slash = path.rfind('/');
                path.resize(slash == std::string::npos ? 0 : slash);
            }
        }
    };
    for (const auto& issue : issues) {
        if (issue.severity == IssueSeverity::error) {
            return Result<Document>::failure(std::move(issues));
        }
    }
    try {
        auto document = parse_document(root, issues, options);
        attach_locations();
        if (std::ranges::any_of(issues, [](const Issue& issue) { return issue.severity == IssueSeverity::error; })) {
            return Result<Document>::failure(std::move(issues));
        }
        document.source_locations = std::move(locations);
        return Result<Document>::success(std::move(document), std::move(issues));
    } catch (const ParseError& ex) {
        issues.push_back(Issue {.severity = IssueSeverity::error, .path = ex.path.empty() ? "/" : ex.path, .message = ex.what()});
    } catch (const std::exception& ex) {
        issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what()});
    }
    attach_locations();
    return Result<Document>::failure(std::move(issues));
}

Result<Document> DocumentReader::read_file(const std::filesystem::path& path, const ReaderOptions options) {
    try {
        if (std::filesystem::file_size(path) > options.max_document_bytes) {
            throw std::runtime_error("Document exceeds max_document_bytes");
        }
        auto result = read(load_text_file(path, options.max_document_bytes), options);
        auto issues = result.issues();
        for (auto& issue : issues) issue.source_file = path.string();
        if (!result) return Result<Document>::failure(std::move(issues));
        result.value().source_file = path.string();
        return Result<Document>::success(std::move(result.value()), std::move(issues));
    } catch (const std::exception& ex) {
        return Result<Document>::failure({Issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what(),
            .source_file = path.string()}});
    }
}

} // namespace swaggercpp
