#include "swaggercpp/validator.hpp"

#include <set>

namespace swaggercpp {
namespace {

void validate_schema(const SchemaPtr& schema, const std::string_view path, std::vector<Issue>& issues);

void validate_parameter(const Parameter& parameter, const std::string_view path, std::vector<Issue>& issues) {
    if (!parameter.ref) {
        if (parameter.name.empty()) {
            issues.push_back(Issue {.severity = IssueSeverity::error, .path = std::string(path), .message = "Parameter name must not be empty"});
        }
        if (parameter.in == ParameterLocation::path && !parameter.required) {
            issues.push_back(Issue {.severity = IssueSeverity::error, .path = std::string(path), .message = "Path parameters must be required"});
        }
        if (!parameter.schema && parameter.content.empty()) {
            issues.push_back(Issue {.severity = IssueSeverity::error, .path = std::string(path), .message = "Parameter must define either schema or content"});
        }
    }

    if (parameter.schema) {
        validate_schema(parameter.schema, std::string(path) + "/schema", issues);
    }
    for (const auto& [media_type_name, media_type] : parameter.content) {
        if (media_type.schema) {
            validate_schema(media_type.schema, std::string(path) + "/content/" + media_type_name + "/schema", issues);
        }
    }
}

void validate_schema(const SchemaPtr& schema, const std::string_view path, std::vector<Issue>& issues) {
    if (!schema) {
        return;
    }
    if (!schema->ref && !schema->type && schema->properties.empty() && !schema->items && schema->all_of.empty() && schema->any_of.empty() && schema->one_of.empty()) {
        issues.push_back(Issue {.severity = IssueSeverity::warning, .path = std::string(path), .message = "Schema is very sparse; consider providing a type or composition"});
    }

    for (const auto& [name, property] : schema->properties) {
        validate_schema(property, std::string(path) + "/properties/" + name, issues);
    }
    validate_schema(schema->items, std::string(path) + "/items", issues);
    validate_schema(schema->not_schema, std::string(path) + "/not", issues);

    auto validate_schema_list = [&](const std::vector<SchemaPtr>& schemas, const std::string_view key) {
        std::size_t index = 0;
        for (const auto& item : schemas) {
            validate_schema(item, std::string(path) + "/" + std::string(key) + "/" + std::to_string(index), issues);
            ++index;
        }
    };

    validate_schema_list(schema->all_of, "allOf");
    validate_schema_list(schema->any_of, "anyOf");
    validate_schema_list(schema->one_of, "oneOf");
}

} // namespace

ValidationReport DocumentValidator::validate(const Document& document) const {
    ValidationReport report;

    if (document.info.title.empty()) {
        report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/info/title", .message = "Document title must not be empty"});
    }
    if (document.info.version.empty()) {
        report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/info/version", .message = "Document version must not be empty"});
    }
    if (document.paths.empty()) {
        report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/paths", .message = "Document must expose at least one path"});
    }

    std::set<std::string> operation_ids;
    for (const auto& [path_name, path_item] : document.paths) {
        if (path_name.empty() || path_name.front() != '/') {
            report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = "/paths/" + path_name, .message = "Path keys must start with '/'"});
        }

        std::size_t parameter_index = 0;
        for (const auto& parameter : path_item.parameters) {
            validate_parameter(parameter, "/paths/" + path_name + "/parameters/" + std::to_string(parameter_index), report.issues);
            ++parameter_index;
        }

        for (const auto& [method, operation] : path_item.operations) {
            const auto operation_path = "/paths/" + path_name + "/" + std::string(to_string(method));
            if (operation.responses.empty()) {
                report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = operation_path, .message = "Operation must declare at least one response"});
            }
            if (operation.operation_id) {
                const auto [_, inserted] = operation_ids.insert(*operation.operation_id);
                if (!inserted) {
                    report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = operation_path + "/operationId", .message = "operationId must be unique"});
                }
            }

            std::size_t op_parameter_index = 0;
            for (const auto& parameter : operation.parameters) {
                validate_parameter(parameter, operation_path + "/parameters/" + std::to_string(op_parameter_index), report.issues);
                ++op_parameter_index;
            }
            if (operation.request_body) {
                for (const auto& [media_type_name, media_type] : operation.request_body->content) {
                    if (media_type.schema) {
                        validate_schema(media_type.schema, operation_path + "/requestBody/content/" + media_type_name + "/schema", report.issues);
                    }
                }
            }
            for (const auto& [status_code, response] : operation.responses) {
                if (!response.ref && response.description.empty()) {
                    report.issues.push_back(Issue {.severity = IssueSeverity::error, .path = operation_path + "/responses/" + status_code, .message = "Response description must not be empty"});
                }
                for (const auto& [media_type_name, media_type] : response.content) {
                    if (media_type.schema) {
                        validate_schema(media_type.schema, operation_path + "/responses/" + status_code + "/content/" + media_type_name + "/schema", report.issues);
                    }
                }
            }
        }
    }

    for (const auto& [name, schema] : document.components.schemas) {
        validate_schema(schema, "/components/schemas/" + name, report.issues);
    }

    return report;
}

} // namespace swaggercpp

