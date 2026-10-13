#include "swaggercpp/validator.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "detail/json_utils.hpp"

namespace swaggercpp {
namespace {

using json = nlohmann::json;

std::string escape_pointer(const std::string_view value) {
    std::string result;
    for (const char ch : value) {
        if (ch == '~') result += "~0";
        else if (ch == '/') result += "~1";
        else result += ch;
    }
    return result;
}

std::string child_path(const std::string& path, const std::string_view name) {
    return path + "/" + escape_pointer(name);
}

std::optional<std::string> decode_fragment(const std::string_view fragment) {
    auto hex = [](const char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    std::string result;
    for (std::size_t index = 0; index < fragment.size(); ++index) {
        if (fragment[index] != '%') {
            result += fragment[index];
            continue;
        }
        if (index + 2 >= fragment.size()) return std::nullopt;
        const int high = hex(fragment[index + 1]);
        const int low = hex(fragment[index + 2]);
        if (high < 0 || low < 0) return std::nullopt;
        result += static_cast<char>(high * 16 + low);
        index += 2;
    }
    return result;
}

bool valid_status(const std::string_view status, const bool swagger) {
    if (status == "default") return true;
    if (status.size() != 3 || status[0] < '1' || status[0] > '5') return false;
    if (!swagger && status.substr(1) == "XX") return true;
    return status[1] >= '0' && status[1] <= '9' && status[2] >= '0' && status[2] <= '9';
}

bool supported_openapi(const std::string_view version, const std::string_view prefix) {
    return version.starts_with(prefix) && version.size() > prefix.size()
        && std::ranges::all_of(version.substr(prefix.size()), [](const char ch) { return ch >= '0' && ch <= '9'; });
}

bool is_object_map(const std::string_view name) {
    return name == "properties" || name == "patternProperties" || name == "$defs" || name == "definitions"
        || name == "dependentSchemas" || name == "schemas" || name == "responses" || name == "headers"
        || name == "links" || name == "callbacks" || name == "securitySchemes" || name == "securityDefinitions"
        || name == "parameters" || name == "requestBodies" || name == "paths" || name == "webhooks"
        || name == "content" || name == "pathItems";
}

bool is_literal_keyword(const std::string_view name) {
    return name == "example" || name == "examples" || name == "default" || name == "enum"
        || name == "const" || name.starts_with("x-");
}

enum class JsonContext { object, named_objects, links, link };

JsonContext field_context(const std::string_view name) {
    if (name == "links") return JsonContext::links;
    return is_object_map(name) ? JsonContext::named_objects : JsonContext::object;
}

class ValidationContext {
public:
    ValidationContext(const Document& document, const ValidatorOptions& options, std::vector<Issue>& issues)
        : document_(document), options_(options), issues_(issues) {}

    bool enabled(const std::string& rule_id, const bool default_enabled = true) const {
        const auto found = options_.rules.find(rule_id);
        return found == options_.rules.end() ? default_enabled : found->second.enabled;
    }

    void emit(const std::string& rule_id, std::string path, std::string message,
              const IssueSeverity severity = IssueSeverity::error) {
        if (!enabled(rule_id)) return;
        const auto found = options_.rules.find(rule_id);
        issues_.push_back(Issue {
            .severity = found != options_.rules.end() ? found->second.severity.value_or(severity) : severity,
            .path = std::move(path), .message = std::move(message), .rule_id = rule_id});
    }

    void run() {
        validate_version();
        if (document_.info.title.empty()) emit("info.title", "/info/title", "Document title must not be empty");
        if (document_.info.version.empty()) emit("info.version", "/info/version", "Document version must not be empty");
        for (const auto& [name, item] : document_.paths) validate_path(item, child_path("/paths", name), name);
        for (const auto& [name, item] : document_.webhooks) validate_path(item, child_path("/webhooks", name), std::nullopt);
        if (!document_.webhooks.empty() && document_.version != SpecificationVersion::openapi_3_1) {
            emit("document.webhooks.version", "/webhooks", "Webhooks require OpenAPI 3.1");
        }
        for (const auto& [name, schema] : document_.components.schemas) validate_schema(schema, child_path("/components/schemas", name));
        for (const auto& [name, parameter] : document_.components.parameters) validate_parameter(parameter, child_path("/components/parameters", name));
        for (const auto& [name, response] : document_.components.responses) validate_response(response, child_path("/components/responses", name));
        for (const auto& [name, body] : document_.components.request_bodies) validate_body(body, child_path("/components/requestBodies", name));
        for (const auto& [name, scheme] : document_.components.security_schemes) {
            const auto path = child_path("/components/securitySchemes", name);
            targets_.insert(path);
            inspect_fields(scheme.unknown_fields, path);
        }
        validate_security(document_.security, "/security");
        inspect_fields(document_.components.unknown_fields, "/components");
        inspect_fields(document_.unknown_fields, "");
        check_references();
    }

private:
    struct Reference {
        std::string value;
        std::string path;
        std::string resource;
    };

    const Document& document_;
    const ValidatorOptions& options_;
    std::vector<Issue>& issues_;
    std::set<const Schema*> active_schemas_;
    std::set<std::string> targets_;
    std::set<std::pair<std::string, std::string>> anchors_;
    std::set<std::string> operation_ids_;
    std::vector<Reference> references_;
    std::size_t schema_visits_ {0};
    bool schema_limit_reached_ {false};

    void validate_version() {
        bool valid = false;
        if (document_.swagger && !document_.openapi) {
            valid = *document_.swagger == "2.0" && document_.version == SpecificationVersion::swagger_2_0;
        } else if (document_.openapi && !document_.swagger) {
            valid = (supported_openapi(*document_.openapi, "3.0.") && document_.version == SpecificationVersion::openapi_3_0)
                || (supported_openapi(*document_.openapi, "3.1.") && document_.version == SpecificationVersion::openapi_3_1);
        }
        if (!valid) emit("document.version", document_.swagger ? "/swagger" : "/openapi",
            "Declare one supported version matching the document model: Swagger 2.0, OpenAPI 3.0.x, or OpenAPI 3.1.x");
    }

    void reference(const std::optional<std::string>& value, const std::string& path, const std::string& resource = "") {
        if (value && value->starts_with('#')) references_.push_back({*value, path + "/$ref", resource});
    }

    // Example/default values are data: "$ref" inside them is not a reference.
    void inspect_json(const json& node, const std::string& path, std::string resource = "", const std::size_t depth = 0, const JsonContext context = JsonContext::object) {
        if (schema_limit_reached_) return;
        targets_.insert(path);
        if (depth > 256) {
            emit("document.depth", path, "Validation nesting limit exceeded");
            return;
        }
        if (node.is_object()) {
            const bool object_map = context == JsonContext::named_objects || context == JsonContext::links;
            if (!object_map) {
                if (const auto id = node.find("$id"); id != node.end() && id->is_string() && !id->get_ref<const std::string&>().empty()) resource = path;
                if (const auto anchor = node.find("$anchor"); anchor != node.end() && anchor->is_string()) anchors_.emplace(resource, anchor->get<std::string>());
                if (const auto ref = node.find("$ref"); ref != node.end() && ref->is_string()) reference(ref->get<std::string>(), path, resource);
            }
            if (context == JsonContext::link) return; // Link parameters/requestBody may contain arbitrary literal JSON.
            for (const auto& [name, value] : node.items()) {
                if (!object_map && is_literal_keyword(name)) continue;
                const auto next_context = context == JsonContext::links ? JsonContext::link
                    : object_map ? JsonContext::object : field_context(name);
                inspect_json(value, child_path(path, name), resource, depth + 1, next_context);
            }
        } else if (node.is_array()) {
            for (std::size_t index = 0; index < node.size(); ++index) inspect_json(node[index], child_path(path, std::to_string(index)), resource, depth + 1);
        }
    }

    void inspect_fields(const ExtensionMap& fields, const std::string& path, const std::string& resource = "") {
        for (const auto& [name, value] : fields) {
            if (!is_literal_keyword(name)) inspect_json(value, child_path(path, name), resource, 0, field_context(name));
        }
        if (const auto ref = fields.find("$ref"); ref != fields.end() && ref->second.is_string()) reference(ref->second.get<std::string>(), path, resource);
    }

    void inspect_named_objects(const ExtensionMap& objects, const std::string& path, const JsonContext context = JsonContext::object) {
        for (const auto& [name, value] : objects) inspect_json(value, child_path(path, name), "", 0, context);
    }

    void validate_schema(const SchemaPtr& schema, const std::string& path, std::string resource = "") {
        if (!schema || schema_limit_reached_) return;
        if (schema_visits_ == 100000) {
            schema_limit_reached_ = true;
            emit("document.limit", path, "Validation exceeds the limit of 100000 schema visits; validation is incomplete");
            return;
        }
        ++schema_visits_;
        targets_.insert(path);
        if (!active_schemas_.insert(schema.get()).second) return;
        if (active_schemas_.size() > 256) {
            emit("document.depth", path, "Validation nesting limit exceeded");
            active_schemas_.erase(schema.get());
            return;
        }
        const bool modern = document_.version == SpecificationVersion::openapi_3_1;
        if (schema->boolean_value) {
            if (!modern) emit("schema.version", path, "Boolean schemas require OpenAPI 3.1");
            active_schemas_.erase(schema.get());
            return;
        }
        if (schema->id && !schema->id->empty()) resource = path;
        if (schema->anchor) anchors_.emplace(resource, *schema->anchor);
        reference(schema->ref, path, resource);
        if (!schema->types.empty() && !modern) emit("schema.version", path + "/type", "Multiple schema types require OpenAPI 3.1");
        const auto check_type = [&](const std::string& type, const std::string& type_path) {
            if (type != "object" && type != "array" && type != "string" && type != "number" && type != "integer" && type != "boolean" && !(modern && type == "null")) emit("schema.type", type_path, "Unsupported schema type: " + type);
        };
        if (schema->types.empty()) {
            if (schema->type) check_type(*schema->type, path + "/type");
        } else {
            std::set<std::string> unique_types;
            for (std::size_t index = 0; index < schema->types.size(); ++index) {
                check_type(schema->types[index], path + "/type/" + std::to_string(index));
                if (!unique_types.insert(schema->types[index]).second) emit("schema.type", path + "/type", "Schema type array must contain unique types");
            }
        }
        const auto check_number = [&](const std::optional<json>& value, const std::string_view key) {
            if (value && !value->is_number()) emit("schema.numeric", child_path(path, key), "Schema numeric constraints must contain numbers");
        };
        check_number(schema->minimum, "minimum");
        check_number(schema->maximum, "maximum");
        check_number(schema->multiple_of, "multipleOf");
        if (schema->multiple_of && schema->multiple_of->is_number() && *schema->multiple_of <= 0) emit("schema.multiple_of", path + "/multipleOf", "multipleOf must be greater than zero");
        const auto check_exclusive = [&](const std::optional<json>& bound, const std::string_view name) {
            if (bound && (modern ? !bound->is_number() : !bound->is_boolean())) emit("schema.version", child_path(path, name), modern ? "Exclusive bounds must be numeric in OpenAPI 3.1" : "Exclusive bounds must be boolean before OpenAPI 3.1");
        };
        check_exclusive(schema->exclusive_minimum, "exclusiveMinimum");
        check_exclusive(schema->exclusive_maximum, "exclusiveMaximum");
        std::set<std::string> required;
        for (const auto& name : schema->required) {
            if (!required.insert(name).second) emit("schema.required.unique", path + "/required", "Required property names must be unique");
        }
        for (const auto& [name, child] : schema->properties) validate_schema(child, child_path(path + "/properties", name), resource);
        for (const auto& [name, child] : schema->defs) validate_schema(child, child_path(path + "/$defs", name), resource);
        validate_schema(schema->items, path + "/items", resource);
        validate_schema(schema->not_schema, path + "/not", resource);
        const auto children = [&](const std::vector<SchemaPtr>& schemas, const std::string_view key) {
            for (std::size_t index = 0; index < schemas.size(); ++index) validate_schema(schemas[index], child_path(path, key) + "/" + std::to_string(index), resource);
        };
        children(schema->all_of, "allOf");
        children(schema->any_of, "anyOf");
        children(schema->one_of, "oneOf");
        if (schema->additional_properties) inspect_json(*schema->additional_properties, path + "/additionalProperties", resource);
        inspect_fields(schema->unknown_fields, path, resource);
        active_schemas_.erase(schema.get());
    }

    void validate_content(const std::map<std::string, MediaType>& content, const std::string& path) {
        for (const auto& [name, media_type] : content) {
            const auto media_path = child_path(path, name);
            targets_.insert(media_path);
            validate_schema(media_type.schema, media_path + "/schema");
        }
    }

    void validate_parameter(const Parameter& parameter, const std::string& path) {
        targets_.insert(path);
        reference(parameter.ref, path);
        if (!parameter.ref) {
            if (parameter.name.empty()) emit("parameter.name", path + "/name", "Parameter name must not be empty");
            if (parameter.in == ParameterLocation::path && !parameter.required) emit("parameter.path.required", path + "/required", "Path parameters must be required");
            const bool legacy_type = document_.version == SpecificationVersion::swagger_2_0 && parameter.unknown_fields.contains("type");
            if (!legacy_type && (static_cast<bool>(parameter.schema) == !parameter.content.empty())) emit("parameter.schema_or_content", path, "Parameter must define exactly one of schema or content");
            if (parameter.content.size() > 1) emit("parameter.content", path + "/content", "Parameter content must contain exactly one media type");
        }
        validate_schema(parameter.schema, path + "/schema");
        validate_content(parameter.content, path + "/content");
    }

    const Parameter* resolve_parameter(const Parameter& parameter) const {
        const Parameter* current = &parameter;
        std::set<const Parameter*> seen;
        while (current->ref) {
            if (!seen.insert(current).second || !current->ref->starts_with('#')) return nullptr;
            const auto decoded = decode_fragment(std::string_view(*current->ref).substr(1));
            if (!decoded || !decoded->starts_with("/components/parameters/")) return nullptr;
            try {
                const json::json_pointer pointer(*decoded);
                if (pointer.parent_pointer().to_string() != "/components/parameters") return nullptr;
                const auto target = document_.components.parameters.find(pointer.back());
                if (target == document_.components.parameters.end()) return nullptr;
                current = &target->second;
            } catch (const json::exception&) { return nullptr; }
        }
        return current;
    }

    void validate_parameters(const std::vector<Parameter>& parameters, const std::string& path) {
        std::set<std::pair<ParameterLocation, std::string>> unique;
        for (std::size_t index = 0; index < parameters.size(); ++index) {
            const auto parameter_path = child_path(path, std::to_string(index));
            validate_parameter(parameters[index], parameter_path);
            const auto* resolved = resolve_parameter(parameters[index]);
            if (resolved && !unique.emplace(resolved->in, resolved->name).second) emit("parameter.unique", parameter_path, "Parameter name and location must be unique within each parameter list");
        }
    }

    void validate_body(const RequestBody& body, const std::string& path) {
        targets_.insert(path);
        reference(body.ref, path);
        validate_content(body.content, path + "/content");
    }

    void validate_response(const Response& response, const std::string& path) {
        targets_.insert(path);
        reference(response.ref, path);
        if (!response.ref && response.description.empty() && !response.description_defined) emit("response.description", path + "/description", "Response must declare a description");
        validate_content(response.content, path + "/content");
        inspect_named_objects(response.headers, path + "/headers");
        inspect_named_objects(response.links, path + "/links", JsonContext::link);
        inspect_fields(response.unknown_fields, path);
    }

    void validate_security(const std::vector<std::map<std::string, std::vector<std::string>>>& security, const std::string& path) {
        for (std::size_t index = 0; index < security.size(); ++index) {
            for (const auto& [name, scopes] : security[index]) {
                (void)scopes;
                bool found = document_.components.security_schemes.contains(name);
                const auto legacy = document_.unknown_fields.find("securityDefinitions");
                if (document_.version == SpecificationVersion::swagger_2_0 && legacy != document_.unknown_fields.end()) found = legacy->second.is_object() && legacy->second.contains(name);
                if (!found) emit("security.scheme", child_path(path + "/" + std::to_string(index), name), "Security requirement references an undefined scheme: " + name);
            }
        }
    }

    void validate_path(const PathItem& item, const std::string& path, const std::optional<std::string_view> template_path) {
        targets_.insert(path);
        reference(item.ref, path);
        std::set<std::string> template_names;
        if (template_path) {
            if (template_path->empty() || template_path->front() != '/') emit("path.leading_slash", path, "Path keys must start with '/'");
            std::size_t position = 0;
            while ((position = template_path->find_first_of("{}", position)) != std::string_view::npos) {
                if ((*template_path)[position] == '}') {
                    emit("path.template", path, "Path template contains an invalid parameter expression");
                    break;
                }
                const auto end = template_path->find('}', position + 1);
                if (end == std::string_view::npos || end == position + 1 || template_path->substr(position + 1, end - position - 1).find('{') != std::string_view::npos) {
                    emit("path.template", path, "Path template contains an invalid parameter expression");
                    break;
                }
                template_names.emplace(template_path->substr(position + 1, end - position - 1));
                position = end + 1;
            }
        }
        validate_parameters(item.parameters, path + "/parameters");
        for (const auto& [method, operation] : item.operations) {
            const auto op_path = child_path(path, to_string(method));
            targets_.insert(op_path);
            if (operation.responses.empty()) emit("operation.responses", op_path + "/responses", "Operation must declare at least one response");
            if (operation.operation_id && !operation_ids_.insert(*operation.operation_id).second) emit("operation.id.unique", op_path + "/operationId", "operationId must be unique");
            if (enabled("operation.id.required", options_.require_operation_id) && (!operation.operation_id || operation.operation_id->empty())) emit("operation.id.required", op_path + "/operationId", "Operation must declare an operationId", IssueSeverity::warning);
            if (enabled("operation.description.required", options_.require_operation_description) && (!operation.description || operation.description->empty())) emit("operation.description.required", op_path + "/description", "Operation must provide a description", IssueSeverity::warning);
            bool has_error_response = false;
            for (const auto& [status, response] : operation.responses) {
                const auto response_path = child_path(op_path + "/responses", status);
                if (!valid_status(status, document_.version == SpecificationVersion::swagger_2_0)) emit("response.status", response_path, "Response key must be a status from 100 to 599, default, or an OpenAPI range 1XX through 5XX");
                else if (status == "default" || status.front() == '4' || status.front() == '5') has_error_response = true;
                validate_response(response, response_path);
            }
            if (enabled("operation.error_response.required", options_.require_error_response) && !has_error_response) emit("operation.error_response.required", op_path + "/responses", "Operation must document a 4xx, 5xx, or default response", IssueSeverity::warning);
            validate_parameters(operation.parameters, op_path + "/parameters");
            if (operation.request_body) validate_body(*operation.request_body, op_path + "/requestBody");
            validate_security(operation.security, op_path + "/security");
            inspect_named_objects(operation.callbacks, op_path + "/callbacks");
            if (template_path) {
                std::set<std::string> declared;
                bool unknown = item.ref.has_value();
                const auto collect = [&](const std::vector<Parameter>& parameters) {
                    for (const auto& parameter : parameters) {
                        const auto* resolved = resolve_parameter(parameter);
                        if (!resolved) { unknown = true; continue; }
                        if (resolved->in == ParameterLocation::path) declared.insert(resolved->name);
                    }
                };
                collect(item.parameters);
                collect(operation.parameters);
                for (const auto& name : declared) {
                    if (!template_names.contains(name)) emit("path.parameter", op_path + "/parameters", "Path parameter has no matching template expression: " + name);
                }
                if (!unknown) {
                    for (const auto& name : template_names) {
                        if (!declared.contains(name)) emit("path.parameter", op_path + "/parameters", "Path template parameter is not declared: " + name);
                    }
                }
            }
        }
    }

    void check_references() {
        if (schema_limit_reached_ || references_.empty() || !enabled("reference.unresolved")) return;
        std::optional<json> serialized;
        try { serialized = detail::document_to_json(document_); }
        catch (const std::runtime_error&) { /* In-memory cycles use the typed index. */ }
        for (const auto& ref : references_) {
            const auto fragment = decode_fragment(std::string_view(ref.value).substr(1));
            bool found = false;
            if (fragment) {
                if (fragment->empty()) found = true;
                else if (fragment->front() != '/') found = anchors_.contains({ref.resource, *fragment});
                else {
                    const auto target = ref.resource + *fragment;
                    try {
                        const json::json_pointer pointer(target);
                        found = serialized ? serialized->contains(pointer) : targets_.contains(target);
                    } catch (const json::exception&) { found = false; }
                }
            }
            if (!found) emit("reference.unresolved", ref.path, "Unresolved internal reference: " + ref.value);
        }
    }
};

} // namespace

DocumentValidator::DocumentValidator(ValidatorOptions options) : options_(std::move(options)) {}

std::span<const std::string_view> DocumentValidator::builtin_rule_ids() noexcept {
    static constexpr std::string_view ids[] = {
        "document.depth", "document.limit", "document.version", "document.webhooks.version", "info.title", "info.version",
        "operation.description.required", "operation.error_response.required", "operation.id.required",
        "operation.id.unique", "operation.responses", "parameter.content", "parameter.name",
        "parameter.path.required", "parameter.schema_or_content", "parameter.unique", "path.leading_slash",
        "path.parameter", "path.template", "reference.unresolved", "response.description", "response.status",
        "schema.multiple_of", "schema.numeric", "schema.required.unique", "schema.type", "schema.version",
        "security.scheme"};
    return ids;
}

void DocumentValidator::add_rule(std::string rule_id, ValidationRule rule) {
    if (rule_id.empty() || !rule) throw std::invalid_argument("A custom validation rule requires an ID and a callback");
    if (!custom_rules_.emplace(std::move(rule_id), std::move(rule)).second) throw std::invalid_argument("Custom validation rule IDs must be unique");
}

ValidationReport DocumentValidator::validate(const Document& document) const {
    ValidationReport report;
    ValidationContext context(document, options_, report.issues);
    context.run();
    for (const auto& [rule_id, rule] : custom_rules_) {
        if (!context.enabled(rule_id)) continue;
        for (auto issue : rule(document)) {
            const auto config = options_.rules.find(rule_id);
            if (config != options_.rules.end() && config->second.severity) issue.severity = *config->second.severity;
            issue.rule_id = rule_id;
            report.issues.push_back(std::move(issue));
        }
    }
    for (auto& issue : report.issues) {
        const bool different_source = !issue.source_file.empty() && issue.source_file != document.source_file;
        if (issue.source_file.empty()) issue.source_file = document.source_file;
        if (different_source || issue.line || issue.column) continue;
        auto path = issue.path;
        for (;;) {
            if (const auto location = document.source_locations.find(path); location != document.source_locations.end()) {
                issue.line = location->second.line;
                issue.column = location->second.column;
                break;
            }
            if (path.empty()) break;
            const auto separator = path.rfind('/');
            path = separator == std::string::npos ? "" : path.substr(0, separator);
        }
    }
    std::stable_sort(report.issues.begin(), report.issues.end(), [](const Issue& left, const Issue& right) {
        return std::tie(left.path, left.rule_id, left.message, left.severity) < std::tie(right.path, right.rule_id, right.message, right.severity);
    });
    return report;
}

} // namespace swaggercpp
