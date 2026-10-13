#include "swaggercpp/writer.hpp"

#include <fstream>
#include <stdexcept>
#include <unordered_set>

#include "detail/json_utils.hpp"
#include "detail/yaml_utils.hpp"
#include "swaggercpp/reader.hpp"

namespace swaggercpp {
namespace {

using detail::json;

template <typename Value>
void add_if_has_value(json& target, const std::string_view key, const std::optional<Value>& value) {
    if (value) {
        target[std::string(key)] = *value;
    }
}

class Serializer {
    std::unordered_set<const Schema*> active_schemas_;
    std::size_t schema_visits_ {0};

public:
    json to_json(const Contact& contact) {
        json node = contact.unknown_fields;
        add_if_has_value(node, "name", contact.name);
        add_if_has_value(node, "url", contact.url);
        add_if_has_value(node, "email", contact.email);
        detail::add_extension_properties(contact.extensions, node);
        return node;
    }

    json to_json(const License& license) {
        json node = license.unknown_fields;
        node["name"] = license.name;
        add_if_has_value(node, "url", license.url);
        add_if_has_value(node, "identifier", license.identifier);
        detail::add_extension_properties(license.extensions, node);
        return node;
    }

    json to_json(const Info& info) {
        json node = info.unknown_fields;
        node["title"] = info.title;
        node["version"] = info.version;
        add_if_has_value(node, "summary", info.summary);
        add_if_has_value(node, "description", info.description);
        add_if_has_value(node, "termsOfService", info.terms_of_service);
        if (info.contact) {
            node["contact"] = to_json(*info.contact);
        }
        if (info.license) {
            node["license"] = to_json(*info.license);
        }
        detail::add_extension_properties(info.extensions, node);
        return node;
    }

    json to_json(const ServerVariable& variable) {
        json node = variable.unknown_fields;
        node["default"] = variable.default_value;
        if (!variable.enum_values.empty()) {
            node["enum"] = variable.enum_values;
        }
        add_if_has_value(node, "description", variable.description);
        detail::add_extension_properties(variable.extensions, node);
        return node;
    }

    json to_json(const Server& server) {
        json node = server.unknown_fields;
        node["url"] = server.url;
        add_if_has_value(node, "description", server.description);
        if (!server.variables.empty()) {
            json variables = json::object();
            for (const auto& [name, variable] : server.variables) {
                variables[name] = to_json(variable);
            }
            node["variables"] = std::move(variables);
        }
        detail::add_extension_properties(server.extensions, node);
        return node;
    }

    json to_json(const ExternalDocumentation& external_docs) {
        json node = external_docs.unknown_fields;
        node["url"] = external_docs.url;
        add_if_has_value(node, "description", external_docs.description);
        detail::add_extension_properties(external_docs.extensions, node);
        return node;
    }

    json to_json(const Tag& tag) {
        json node = tag.unknown_fields;
        node["name"] = tag.name;
        add_if_has_value(node, "description", tag.description);
        if (tag.external_docs) {
            node["externalDocs"] = to_json(*tag.external_docs);
        }
        detail::add_extension_properties(tag.extensions, node);
        return node;
    }

    json to_json(const Schema& schema) {
        if (schema.boolean_value) {
            return *schema.boolean_value;
        }
        json node = schema.unknown_fields;
        add_if_has_value(node, "$ref", schema.ref);
        add_if_has_value(node, "type", schema.type);
        if (!schema.types.empty()) {
            node["type"] = schema.types;
        }
        if (schema.enum_defined || !schema.enum_values.empty()) {
            node["enum"] = schema.enum_values;
        }
        add_if_has_value(node, "const", schema.const_value);
        add_if_has_value(node, "minimum", schema.minimum);
        add_if_has_value(node, "maximum", schema.maximum);
        add_if_has_value(node, "multipleOf", schema.multiple_of);
        add_if_has_value(node, "exclusiveMinimum", schema.exclusive_minimum);
        add_if_has_value(node, "exclusiveMaximum", schema.exclusive_maximum);
        add_if_has_value(node, "minLength", schema.min_length);
        add_if_has_value(node, "maxLength", schema.max_length);
        add_if_has_value(node, "minItems", schema.min_items);
        add_if_has_value(node, "maxItems", schema.max_items);
        add_if_has_value(node, "minProperties", schema.min_properties);
        add_if_has_value(node, "maxProperties", schema.max_properties);
        add_if_has_value(node, "pattern", schema.pattern);
        add_if_has_value(node, "uniqueItems", schema.unique_items);
        add_if_has_value(node, "$schema", schema.schema_dialect);
        add_if_has_value(node, "$id", schema.id);
        add_if_has_value(node, "$anchor", schema.anchor);
        if (!schema.defs.empty()) {
            node["$defs"] = json::object();
            for (const auto& [name, child_schema] : schema.defs) {
                node["$defs"][name] = to_json(child_schema);
            }
        }
        add_if_has_value(node, "format", schema.format);
        add_if_has_value(node, "title", schema.title);
        add_if_has_value(node, "description", schema.description);
        if (schema.default_value) {
            node["default"] = *schema.default_value;
        }
        if (schema.example) {
            node["example"] = *schema.example;
        }
        if (schema.additional_properties) {
            node["additionalProperties"] = *schema.additional_properties;
        }
        if (!schema.properties.empty()) {
            json properties = json::object();
            for (const auto& [name, child_schema] : schema.properties) {
                properties[name] = to_json(child_schema);
            }
            node["properties"] = std::move(properties);
        }
        if (!schema.required.empty()) {
            node["required"] = schema.required;
        }

        auto emit_schema_array = [&](const std::string_view key, const std::vector<SchemaPtr>& schemas) {
            if (!schemas.empty()) {
                json array = json::array();
                for (const auto& item : schemas) {
                    array.push_back(to_json(item));
                }
                node[std::string(key)] = std::move(array);
            }
        };

        emit_schema_array("allOf", schema.all_of);
        emit_schema_array("anyOf", schema.any_of);
        emit_schema_array("oneOf", schema.one_of);

        if (schema.items) {
            node["items"] = to_json(schema.items);
        }
        if (schema.not_schema) {
            node["not"] = to_json(schema.not_schema);
        }
        if (schema.nullable) {
            node["nullable"] = true;
        }
        if (schema.read_only) {
            node["readOnly"] = true;
        }
        if (schema.write_only) {
            node["writeOnly"] = true;
        }
        if (schema.deprecated) {
            node["deprecated"] = true;
        }
        detail::add_extension_properties(schema.extensions, node);
        return node;
    }

    json to_json(const SchemaPtr& schema_ptr) {
        if (!schema_ptr) {
            return json::object();
        }
        if (++schema_visits_ > 100000) {
            throw std::runtime_error("Schema serialization exceeds the work limit of 100000 visits");
        }
        if (active_schemas_.contains(schema_ptr.get())) {
            if (schema_ptr->ref) {
                return json {{"$ref", *schema_ptr->ref}};
            }
            throw std::runtime_error("Cannot serialize a schema pointer cycle without a $ref");
        }
        if (active_schemas_.size() >= 256) {
            throw std::runtime_error("Schema nesting exceeds the supported depth of 256");
        }
        active_schemas_.insert(schema_ptr.get());
        auto result = to_json(*schema_ptr);
        active_schemas_.erase(schema_ptr.get());
        return result;
    }

    json to_json(const MediaType& media_type) {
        json node = media_type.unknown_fields;
        if (media_type.schema) {
            node["schema"] = to_json(media_type.schema);
        }
        if (media_type.example) {
            node["example"] = *media_type.example;
        }
        if (!media_type.examples.empty()) {
            node["examples"] = media_type.examples;
        }
        detail::add_extension_properties(media_type.extensions, node);
        return node;
    }

    json to_json(const Parameter& parameter) {
        json node = parameter.unknown_fields;
        add_if_has_value(node, "$ref", parameter.ref);
        if (!parameter.name.empty()) {
            node["name"] = parameter.name;
        }
        if (!parameter.ref || !parameter.name.empty()) {
            node["in"] = std::string(to_string(parameter.in));
        }
        add_if_has_value(node, "style", parameter.style);
        add_if_has_value(node, "explode", parameter.explode);
        add_if_has_value(node, "allowReserved", parameter.allow_reserved);
        add_if_has_value(node, "description", parameter.description);
        if (parameter.required) {
            node["required"] = true;
        }
        if (parameter.deprecated) {
            node["deprecated"] = true;
        }
        if (parameter.allow_empty_value) {
            node["allowEmptyValue"] = true;
        }
        if (parameter.schema) {
            node["schema"] = to_json(parameter.schema);
        }
        if (!parameter.content.empty()) {
            json content = json::object();
            for (const auto& [name, media_type] : parameter.content) {
                content[name] = to_json(media_type);
            }
            node["content"] = std::move(content);
        }
        detail::add_extension_properties(parameter.extensions, node);
        return node;
    }

    json to_json(const RequestBody& request_body) {
        json node = request_body.unknown_fields;
        add_if_has_value(node, "$ref", request_body.ref);
        add_if_has_value(node, "description", request_body.description);
        if (request_body.required) {
            node["required"] = true;
        }
        if (request_body.content_defined || !request_body.content.empty()) {
            json content = json::object();
            for (const auto& [name, media_type] : request_body.content) {
                content[name] = to_json(media_type);
            }
            node["content"] = std::move(content);
        }
        detail::add_extension_properties(request_body.extensions, node);
        return node;
    }

    json to_json(const Response& response) {
        json node = response.unknown_fields;
        add_if_has_value(node, "$ref", response.ref);
        if (!response.ref || response.description_defined || !response.description.empty()) {
            node["description"] = response.description;
        }
        if (!response.content.empty()) {
            json content = json::object();
            for (const auto& [name, media_type] : response.content) {
                content[name] = to_json(media_type);
            }
            node["content"] = std::move(content);
        }
        detail::add_extension_properties(response.extensions, node);
        if (!response.headers.empty()) {
            node["headers"] = response.headers;
        }
        if (!response.links.empty()) {
            node["links"] = response.links;
        }
        return node;
    }

    json to_json(const Operation& operation) {
        json node = operation.unknown_fields;
        if (!operation.tags.empty()) {
            node["tags"] = operation.tags;
        }
        add_if_has_value(node, "summary", operation.summary);
        add_if_has_value(node, "description", operation.description);
        add_if_has_value(node, "operationId", operation.operation_id);
        if (!operation.parameters.empty()) {
            json parameters = json::array();
            for (const auto& parameter : operation.parameters) {
                parameters.push_back(to_json(parameter));
            }
            node["parameters"] = std::move(parameters);
        }
        if (operation.request_body) {
            node["requestBody"] = to_json(*operation.request_body);
        }
        if (operation.responses_defined || !operation.responses.empty() || !operation.responses_extensions.empty()) {
            json responses = operation.responses_extensions;
            for (const auto& [status_code, response] : operation.responses) {
                responses[status_code] = to_json(response);
            }
            node["responses"] = std::move(responses);
        }
        if (operation.deprecated) {
            node["deprecated"] = true;
        }
        if (operation.security_defined || !operation.security.empty()) {
            node["security"] = operation.security;
        }
        if (operation.servers_defined || !operation.servers.empty()) {
            node["servers"] = json::array();
            for (const auto& server : operation.servers) {
                node["servers"].push_back(to_json(server));
            }
        }
        if (!operation.callbacks.empty()) {
            node["callbacks"] = operation.callbacks;
        }
        detail::add_extension_properties(operation.extensions, node);
        return node;
    }

    json to_json(const PathItem& path_item) {
        json node = path_item.unknown_fields;
        add_if_has_value(node, "$ref", path_item.ref);
        add_if_has_value(node, "summary", path_item.summary);
        add_if_has_value(node, "description", path_item.description);
        if (path_item.servers_defined || !path_item.servers.empty()) {
            node["servers"] = json::array();
            for (const auto& server : path_item.servers) {
                node["servers"].push_back(to_json(server));
            }
        }
        if (!path_item.parameters.empty()) {
            json parameters = json::array();
            for (const auto& parameter : path_item.parameters) {
                parameters.push_back(to_json(parameter));
            }
            node["parameters"] = std::move(parameters);
        }
        for (const auto& [method, operation] : path_item.operations) {
            node[std::string(to_string(method))] = to_json(operation);
        }
        detail::add_extension_properties(path_item.extensions, node);
        return node;
    }

    json to_json(const SecurityScheme& scheme) {
        json node = scheme.unknown_fields;
        if (!scheme.type.empty() || !node.contains("$ref")) {
            node["type"] = scheme.type;
        }
        add_if_has_value(node, "description", scheme.description);
        add_if_has_value(node, "name", scheme.name);
        add_if_has_value(node, "in", scheme.in);
        add_if_has_value(node, "scheme", scheme.scheme);
        add_if_has_value(node, "bearerFormat", scheme.bearer_format);
        add_if_has_value(node, "openIdConnectUrl", scheme.open_id_connect_url);
        if (scheme.flows) {
            node["flows"] = *scheme.flows;
        }
        detail::add_extension_properties(scheme.extensions, node);
        return node;
    }

    json to_json(const Components& components) {
        json node = components.unknown_fields;

        auto emit_named_map = [&](const std::string_view key, const auto& source, auto&& projector) {
            if (!source.empty()) {
                json child = json::object();
                for (const auto& [name, value] : source) {
                    child[name] = projector(value);
                }
                node[std::string(key)] = std::move(child);
            }
        };

        emit_named_map("schemas", components.schemas, [this](const auto& value) { return to_json(value); });
        emit_named_map("responses", components.responses, [this](const auto& value) { return to_json(value); });
        emit_named_map("parameters", components.parameters, [this](const auto& value) { return to_json(value); });
        emit_named_map("requestBodies", components.request_bodies, [this](const auto& value) { return to_json(value); });
        emit_named_map("securitySchemes", components.security_schemes, [this](const auto& value) { return to_json(value); });

        detail::add_extension_properties(components.extensions, node);
        return node;
    }

    json to_json(const Document& document) {
        json node = document.unknown_fields;
        if (document.version == SpecificationVersion::swagger_2_0) {
            node["swagger"] = document.swagger.value_or("2.0");
        } else {
            node["openapi"] = document.openapi.value_or(std::string(to_string(document.version)));
        }
        node["info"] = to_json(document.info);

        if (!document.servers.empty()) {
            json servers = json::array();
            for (const auto& server : document.servers) {
                servers.push_back(to_json(server));
            }
            node["servers"] = std::move(servers);
        }

        json paths = document.paths_extensions;
        for (const auto& [path_name, path_item] : document.paths) {
            paths[path_name] = to_json(path_item);
        }
        node["paths"] = std::move(paths);
        if (document.security_defined || !document.security.empty()) {
            node["security"] = document.security;
        }
        if (!document.webhooks.empty()) {
            node["webhooks"] = json::object();
            for (const auto& [name, webhook] : document.webhooks) {
                node["webhooks"][name] = to_json(webhook);
            }
        }

        const auto components = to_json(document.components);
        if (!components.empty()) {
            node["components"] = components;
        }

        if (!document.tags.empty()) {
            json tags = json::array();
            for (const auto& tag : document.tags) {
                tags.push_back(to_json(tag));
            }
            node["tags"] = std::move(tags);
        }

        detail::add_extension_properties(document.extensions, node);
        return node;
    }
};

} // namespace

detail::json detail::document_to_json(const Document& document) {
    return Serializer {}.to_json(document);
}

Result<std::string> DocumentWriter::write(const Document& document, const WriterOptions options) {
    return options.format == DocumentFormat::json ? write_json(document, options.pretty, options.indent) : write_yaml(document);
}

Result<std::string> DocumentWriter::write_json(const Document& document, const bool pretty, const int indent) {
    try {
        const auto content = detail::document_to_json(document).dump(pretty ? indent : -1);
        return Result<std::string>::success(content);
    } catch (const std::exception& ex) {
        return Result<std::string>::failure({Issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what()}});
    }
}

Result<std::string> DocumentWriter::write_yaml(const Document& document) {
    try {
        return Result<std::string>::success(detail::emit_yaml(detail::document_to_json(document)));
    } catch (const std::exception& ex) {
        return Result<std::string>::failure({Issue {.severity = IssueSeverity::error, .path = "/", .message = ex.what()}});
    }
}

Result<Document> DocumentWriter::roundtrip(const std::string_view content, const DocumentFormat output_format) {
    const auto read_result = DocumentReader::read(content);
    if (!read_result) {
        return read_result;
    }

    const auto write_result = write(read_result.value(), WriterOptions {.format = output_format});
    if (!write_result) {
        return Result<Document>::failure(write_result.issues());
    }

    return DocumentReader::read(write_result.value());
}

Result<std::string> DocumentWriter::write_file(
    const Document& document,
    const std::filesystem::path& path,
    const WriterOptions options) {
    const auto content_result = write(document, options);
    if (!content_result) {
        return content_result;
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return Result<std::string>::failure({Issue {.severity = IssueSeverity::error, .path = path.string(), .message = "Unable to open file for writing"}});
    }

    stream << content_result.value();
    stream.flush();
    if (!stream) {
        return Result<std::string>::failure({Issue {.severity = IssueSeverity::error, .path = path.string(), .message = "Unable to write complete document"}});
    }
    return content_result;
}

} // namespace swaggercpp
