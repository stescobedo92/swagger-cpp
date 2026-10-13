#include "swaggercpp/reference_resolver.hpp"

#include "swaggercpp/mutable_walker.hpp"
#include "swaggercpp/reader.hpp"
#include "detail/yaml_utils.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <yaml-cpp/yaml.h>

namespace swaggercpp {
namespace {

using json = nlohmann::json;

std::string unescape_json_pointer(std::string_view pointer) {
    std::string result;
    for (std::size_t i = 0; i < pointer.size(); ++i) {
        if (pointer[i] != '~') result.push_back(pointer[i]);
        else if (++i < pointer.size() && (pointer[i] == '0' || pointer[i] == '1'))
            result.push_back(pointer[i] == '0' ? '~' : '/');
        else throw std::runtime_error("Invalid JSON Pointer escape");
    }
    return result;
}

std::string escape_json_pointer(std::string_view token) {
    std::string result;
    for (const char ch : token) {
        if (ch == '~') result += "~0";
        else if (ch == '/') result += "~1";
        else result += ch;
    }
    return result;
}

std::string percent_decode(std::string_view value) {
    auto hex = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    std::string decoded;
    for (std::size_t i = 0; i < value.size(); ++i) {
        char ch = value[i];
        if (ch == '%') {
            if (i + 2 >= value.size() || hex(value[i + 1]) < 0 || hex(value[i + 2]) < 0)
                throw std::runtime_error("Invalid percent encoding in reference");
            ch = static_cast<char>(hex(value[i + 1]) * 16 + hex(value[i + 2]));
            i += 2;
        }
        if (ch == '\0') throw std::runtime_error("NUL is not permitted in references");
        decoded += ch;
    }
    return decoded;
}

// Replacing a Schema Object with siblings would discard OAS 3.1 constraints.
bool reference_only(const Schema& s) {
    return s.ref && !s.type && !s.format && !s.title && !s.description && !s.default_value &&
        !s.example && !s.additional_properties && s.properties.empty() && s.required.empty() &&
        s.all_of.empty() && s.any_of.empty() && s.one_of.empty() && !s.items && !s.not_schema &&
        !s.nullable && !s.read_only && !s.write_only && !s.deprecated && s.extensions.empty() &&
        s.unknown_fields.empty() && !s.boolean_value && s.types.empty() && s.enum_values.empty() &&
        !s.enum_defined && !s.const_value && !s.minimum && !s.maximum && !s.multiple_of &&
        !s.exclusive_minimum && !s.exclusive_maximum && !s.min_length && !s.max_length &&
        !s.min_items && !s.max_items && !s.min_properties && !s.max_properties && !s.pattern &&
        !s.unique_items && !s.schema_dialect && !s.id && !s.anchor && s.defs.empty();
}

template <typename Map>
auto component_target(Map& entries, const std::string& reference, std::string_view prefix) {
    if (!reference.starts_with(prefix)) return entries.end();
    try {
        const auto token = percent_decode(std::string_view(reference).substr(prefix.size()));
        if (token.find('/') != std::string::npos) return entries.end();
        return entries.find(unescape_json_pointer(token));
    } catch (const std::exception&) { return entries.end(); }
}

class ResolverVisitor : public MutableDocumentVisitor {
    Document& document_;
    std::set<const Schema*> visited_;
    std::set<const Schema*> active_;
    std::set<const Schema*> scoped_;
    bool collect_scopes_ {true};

    void preserve_subtree(const SchemaPtr& schema) {
        if (!schema || !visited_.insert(schema.get()).second) return;
        scoped_.insert(schema.get());
        for (const auto& [name, child] : schema->properties) preserve_subtree(child);
        for (const auto& [name, child] : schema->defs) preserve_subtree(child);
        preserve_subtree(schema->items);
        preserve_subtree(schema->not_schema);
        for (const auto& child : schema->all_of) preserve_subtree(child);
        for (const auto& child : schema->any_of) preserve_subtree(child);
        for (const auto& child : schema->one_of) preserve_subtree(child);
    }

    void resolve_schema(SchemaPtr& schema) {
        if (!schema || visited_.contains(schema.get()) || active_.contains(schema.get())) return;
        const auto original = schema;
        active_.insert(original.get());
        if (reference_only(*schema)) {
            const auto it = component_target(document_.components.schemas, *schema->ref, "#/components/schemas/");
            if (it != document_.components.schemas.end() && it->second && !active_.contains(it->second.get()) && !scoped_.contains(it->second.get())) {
                auto target = it->second;
                resolve_schema(target);
                if (!active_.contains(target.get())) schema = std::move(target);
            }
        }
        for (auto& [name, child] : schema->properties) resolve_schema(child);
        for (auto& [name, child] : schema->defs) resolve_schema(child);
        resolve_schema(schema->items);
        resolve_schema(schema->not_schema);
        for (auto& child : schema->all_of) resolve_schema(child);
        for (auto& child : schema->any_of) resolve_schema(child);
        for (auto& child : schema->one_of) resolve_schema(child);
        active_.erase(original.get());
        visited_.insert(original.get());
        visited_.insert(schema.get());
    }

    template <typename Value, typename Map>
    void resolve_value(Value& value, Map& entries, std::string_view prefix) {
        if (!value.ref || !value.extensions.empty() || !value.unknown_fields.empty()) return;
        auto has_description = [](const Value& item) {
            if constexpr (requires { item.description.has_value(); }) return item.description.has_value();
            else if constexpr (requires { item.description_defined; }) return item.description_defined || !item.description.empty();
            else return !item.description.empty();
        };
        auto replacement = value;
        auto description = value.description;
        bool description_defined = has_description(value);
        std::set<std::string> visited;
        while (replacement.ref) {
            if (!replacement.extensions.empty() || !replacement.unknown_fields.empty()) return;
            if (!visited.insert(*replacement.ref).second) return;
            const auto it = component_target(entries, *replacement.ref, prefix);
            if (it == entries.end()) return;
            replacement = it->second;
            if (!description_defined && has_description(replacement)) {
                description = replacement.description;
                description_defined = true;
            }
        }
        replacement.description = std::move(description);
        if constexpr (requires { replacement.description_defined; }) replacement.description_defined = description_defined;
        value = std::move(replacement);
    }

public:
    explicit ResolverVisitor(Document& document) : document_(document) {}
    void begin_resolution() { collect_scopes_ = false; }
    void visit_schema(SchemaPtr& schema, std::string_view) override {
        if (collect_scopes_) {
            if (schema && (schema->id || schema->schema_dialect || document_.unknown_fields.contains("jsonSchemaDialect")))
                preserve_subtree(schema);
        } else resolve_schema(schema);
    }
    void visit_parameter(Parameter& parameter) override {
        if (!collect_scopes_) resolve_value(parameter, document_.components.parameters, "#/components/parameters/");
    }
    void visit_response(std::string_view, Response& response) override {
        if (!collect_scopes_) resolve_value(response, document_.components.responses, "#/components/responses/");
    }
    void visit_request_body(RequestBody& body) override {
        if (!collect_scopes_) resolve_value(body, document_.components.request_bodies, "#/components/requestBodies/");
    }
};

enum class NodeKind { document, path_item, operation, schema, parameter, response, request_body,
    media_type, header, encoding, callback, link, example, security_scheme };

class BundleFailure : public std::runtime_error {
public:
    std::string location;
    BundleFailure(std::string where, std::string message)
        : std::runtime_error(std::move(message)), location(std::move(where)) {}
};

class Bundler {
    ResolverOptions options_;
    std::optional<std::filesystem::path> allowed_root_;
    std::filesystem::path root_file_;
    std::map<std::filesystem::path, json> documents_;
    std::map<std::tuple<std::filesystem::path, std::string, NodeKind>, std::string> references_;
    json bundled_ = json::object();
    std::string extension_name_ = "x-swaggercpp-bundled";
    std::size_t total_bytes_ {0};
    std::size_t copied_bytes_ {0};
    std::size_t yaml_expanded_bytes_ {0};
    std::size_t nodes_ {0};

    void check_node(std::size_t depth, const std::string& location) {
        if (depth > options_.max_depth) throw BundleFailure(location, "Reference or document nesting exceeds max_depth");
        if (++nodes_ > options_.max_nodes) throw BundleFailure(location, "Bundle exceeds max_nodes");
    }

    std::filesystem::path canonical_file(const std::filesystem::path& path) const {
        std::error_code error;
        const auto result = std::filesystem::canonical(path, error);
        if (error) throw BundleFailure(path.string(), "Unable to resolve local file: " + error.message());
        if (allowed_root_) {
            const auto relative = result.lexically_relative(*allowed_root_);
            if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
                throw BundleFailure(path.string(), "Reference escapes allowed_root");
        }
        if (!std::filesystem::is_regular_file(result, error) || error)
            throw BundleFailure(path.string(), "Reference must identify a local regular file");
        return result;
    }

    json yaml_value(const YAML::Node& node, std::size_t depth, const std::string& location) {
        check_node(depth, location);
        if (!node || node.IsNull()) return nullptr;
        if (node.IsScalar()) {
            if (node.Scalar().size() > options_.max_total_bytes - yaml_expanded_bytes_)
                throw BundleFailure(location, "Expanded YAML scalar content exceeds max_total_bytes");
            yaml_expanded_bytes_ += node.Scalar().size();
            return detail::yaml_to_json(node);
        }
        if (node.IsSequence()) {
            auto result = json::array();
            for (const auto& child : node) result.push_back(yaml_value(child, depth + 1, location));
            return result;
        }
        auto result = json::object();
        for (const auto& entry : node) {
            if (!entry.first.IsScalar()) throw BundleFailure(location, "YAML mapping keys must be strings");
            const auto key = entry.first.Scalar();
            if (key.size() > options_.max_total_bytes - yaml_expanded_bytes_)
                throw BundleFailure(location, "Expanded YAML mapping content exceeds max_total_bytes");
            yaml_expanded_bytes_ += key.size();
            if (result.contains(key)) throw BundleFailure(location, "Duplicate YAML mapping key: " + key);
            result[key] = yaml_value(entry.second, depth + 1, location + "/" + escape_json_pointer(key));
        }
        return result;
    }

    const json& load(const std::filesystem::path& file) {
        if (const auto it = documents_.find(file); it != documents_.end()) return it->second;
        if (documents_.size() >= options_.max_documents) throw BundleFailure(file.string(), "Bundle exceeds max_documents");
        std::ifstream stream(file, std::ios::binary);
        if (!stream) throw BundleFailure(file.string(), "Unable to open referenced file");
        std::string content;
        std::array<char, 8192> buffer {};
        while (stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || stream.gcount() != 0) {
            const auto count = static_cast<std::size_t>(stream.gcount());
            if (count > options_.max_total_bytes - total_bytes_)
                throw BundleFailure(file.string(), "Bundle input exceeds max_total_bytes");
            total_bytes_ += count;
            content.append(buffer.data(), count);
        }
        if (stream.bad()) throw BundleFailure(file.string(), "Unable to read referenced file");
        try {
            const auto first = content.find_first_not_of(" \t\r\n");
            json value;
            if (first != std::string::npos && (content[first] == '{' || content[first] == '[')) {
                try {
                    value = json::parse(content, [this, &file](int depth, json::parse_event_t event, json&) {
                        if (event == json::parse_event_t::object_start || event == json::parse_event_t::array_start ||
                            event == json::parse_event_t::value) check_node(static_cast<std::size_t>(depth), file.string());
                        return true;
                    });
                } catch (const json::parse_error&) {
                    // Flow-style YAML can start with the same braces as JSON.
                    value = yaml_value(YAML::Load(content), 0, file.string());
                }
            } else value = yaml_value(YAML::Load(content), 0, file.string());
            return documents_.emplace(file, std::move(value)).first->second;
        } catch (const BundleFailure&) { throw; }
        catch (const std::exception& error) { throw BundleFailure(file.string(), "Unable to parse reference document: " + std::string(error.what())); }
    }

    static void reject_schema_identifiers(const json& node, const std::string& location) {
        for (const auto* key : {"$id", "$anchor", "$dynamicRef", "$dynamicAnchor", "$recursiveRef", "$recursiveAnchor", "$vocabulary"}) {
            if (node.contains(key))
                throw BundleFailure(location + "/" + key, "Bundling schemas with " + std::string(key) + " is not supported; URI scope must be preserved");
        }
    }

    static void check_dialect(const json& node, std::string_view key, const std::string& location) {
        if (const auto it = node.find(key); it != node.end()) {
            if (!it->is_string() || (*it != "https://json-schema.org/draft/2020-12/schema" &&
                    *it != "https://spec.openapis.org/oas/3.1/dialect/base"))
                throw BundleFailure(location + "/" + std::string(key), "Only the OpenAPI 3.1 and JSON Schema 2020-12 dialects are supported for bundling");
        }
    }

    std::string reference(const std::filesystem::path& source, const std::string& ref,
        NodeKind kind, std::size_t depth, const std::string& location) {
        const auto hash = ref.find('#');
        const auto resource = std::string_view(ref).substr(0, hash);
        const auto slash = resource.find('/');
        const auto colon = resource.find(':');
        if (resource.starts_with("//") || resource.starts_with('/') || resource.find('\\') != std::string_view::npos ||
            (colon != std::string_view::npos && (slash == std::string_view::npos || colon < slash)) || resource.find('?') != std::string_view::npos)
            throw BundleFailure(location, "Only relative local file references are supported (no URI schemes, network paths, absolute paths, or queries)");
        const auto decoded_resource = percent_decode(resource);
        if (decoded_resource.find('\\') != std::string::npos || decoded_resource.find(':') != std::string::npos ||
            std::filesystem::path(decoded_resource).is_absolute())
            throw BundleFailure(location, "Reference is not a relative local file URI");
        const auto relative_path = std::filesystem::path(std::u8string(decoded_resource.begin(), decoded_resource.end()));
        const auto target_file = resource.empty() ? source : canonical_file(source.parent_path() / relative_path);
        const auto pointer_text = hash == std::string::npos ? std::string{} : percent_decode(std::string_view(ref).substr(hash + 1));
        if (!pointer_text.empty() && pointer_text.front() != '/')
            throw BundleFailure(location, "Only JSON Pointer fragments are supported; named anchors are not supported");
        const auto key = std::make_tuple(target_file, pointer_text, kind);
        if (const auto cached = references_.find(key); cached != references_.end()) return cached->second;
        const auto& document = load(target_file);
        const json* target;
        try {
            const json::json_pointer pointer(pointer_text);
            target = &document.at(pointer);
            if (kind == NodeKind::schema) {
                auto parent = pointer;
                while (!parent.empty()) {
                    parent = parent.parent_pointer();
                    const auto& ancestor = document.at(parent);
                    if (ancestor.is_object() && ancestor.contains("$id") && ancestor.at("$id").is_string())
                        throw BundleFailure(location, "Referenced schema inherits an unsupported $id scope");
                }
            }
        } catch (const json::exception& error) {
            throw BundleFailure(location, "Unresolved reference '" + ref + "': " + error.what());
        }
        if (!target->is_object() && !(kind == NodeKind::schema && target->is_boolean()))
            throw BundleFailure(location, "Reference target must be an object (or a boolean schema)");
        const auto copy_size = target->dump().size();
        if (copy_size > options_.max_total_bytes - copied_bytes_)
            throw BundleFailure(location, "Bundled reference content exceeds max_total_bytes");
        copied_bytes_ += copy_size;
        const auto name = "ref-" + std::to_string(references_.size());
        const auto result = "#/" + extension_name_ + "/" + name;
        references_.emplace(key, result);
        auto copy = *target;
        visit(copy, target_file, pointer_text, kind, depth + 1);
        bundled_[name] = std::move(copy);
        return result;
    }

    void visit(json& node, const std::filesystem::path& source, const std::string& pointer,
        NodeKind kind, std::size_t depth) {
        const auto location = source.string() + "#" + pointer;
        check_node(depth, location);
        if (!node.is_object()) return;
        if (kind == NodeKind::schema) {
            reject_schema_identifiers(node, location);
            check_dialect(node, "$schema", location);
        }
        if (kind != NodeKind::document && kind != NodeKind::operation && kind != NodeKind::media_type && kind != NodeKind::encoding) {
            if (auto it = node.find("$ref"); it != node.end()) {
                if (!it->is_string()) throw BundleFailure(location + "/$ref", "$ref must be a string");
                *it = reference(source, it->get<std::string>(), kind, depth, location + "/$ref");
            }
        }
        auto child = [&](std::string_view key, NodeKind child_kind) {
            if (auto it = node.find(key); it != node.end())
                visit(*it, source, pointer + "/" + escape_json_pointer(key), child_kind, depth + 1);
        };
        auto mapping = [&](std::string_view key, NodeKind child_kind, bool skip_extensions = false) {
            if (auto it = node.find(key); it != node.end() && it->is_object()) {
                for (auto& [name, item] : it->items()) {
                    if (skip_extensions && name.starts_with("x-")) continue;
                    visit(item, source, pointer + "/" + escape_json_pointer(key) + "/" + escape_json_pointer(name), child_kind, depth + 1);
                }
            }
        };
        auto sequence = [&](std::string_view key, NodeKind child_kind) {
            if (auto it = node.find(key); it != node.end() && it->is_array()) {
                for (std::size_t i = 0; i < it->size(); ++i)
                    visit((*it)[i], source, pointer + "/" + escape_json_pointer(key) + "/" + std::to_string(i), child_kind, depth + 1);
            }
        };
        switch (kind) {
        case NodeKind::document:
            check_dialect(node, "jsonSchemaDialect", location);
            mapping("paths", NodeKind::path_item, true);
            mapping("webhooks", NodeKind::path_item);
            mapping("definitions", NodeKind::schema);
            mapping("parameters", NodeKind::parameter);
            mapping("responses", NodeKind::response);
            mapping("securityDefinitions", NodeKind::security_scheme);
            if (auto it = node.find("components"); it != node.end() && it->is_object()) {
                const std::pair<std::string_view, NodeKind> groups[] = {
                    {"schemas", NodeKind::schema}, {"parameters", NodeKind::parameter}, {"responses", NodeKind::response},
                    {"requestBodies", NodeKind::request_body}, {"headers", NodeKind::header}, {"callbacks", NodeKind::callback},
                    {"links", NodeKind::link}, {"examples", NodeKind::example}, {"securitySchemes", NodeKind::security_scheme},
                    {"pathItems", NodeKind::path_item}};
                for (const auto& [key, child_kind] : groups) {
                    if (auto group = it->find(key); group != it->end() && group->is_object())
                        for (auto& [name, item] : group->items())
                            visit(item, source, pointer + "/components/" + std::string(key) + "/" + escape_json_pointer(name), child_kind, depth + 1);
                }
            }
            break;
        case NodeKind::path_item:
            for (const auto* method : {"get", "put", "post", "delete", "options", "head", "patch", "trace"}) child(method, NodeKind::operation);
            sequence("parameters", NodeKind::parameter);
            break;
        case NodeKind::operation:
            sequence("parameters", NodeKind::parameter);
            mapping("responses", NodeKind::response, true);
            mapping("callbacks", NodeKind::callback);
            child("requestBody", NodeKind::request_body);
            break;
        case NodeKind::schema:
            for (const auto* key : {"properties", "patternProperties", "$defs", "definitions", "dependentSchemas", "dependencies"}) mapping(key, NodeKind::schema);
            for (const auto* key : {"items", "additionalItems", "additionalProperties", "unevaluatedProperties", "unevaluatedItems", "contains", "propertyNames", "not", "if", "then", "else", "contentSchema"}) child(key, NodeKind::schema);
            for (const auto* key : {"allOf", "anyOf", "oneOf", "prefixItems", "items"}) sequence(key, NodeKind::schema);
            break;
        case NodeKind::parameter:
        case NodeKind::header:
            child("schema", NodeKind::schema);
            child("items", NodeKind::schema);
            mapping("content", NodeKind::media_type);
            mapping("examples", NodeKind::example);
            break;
        case NodeKind::response:
            child("schema", NodeKind::schema);
            mapping("content", NodeKind::media_type);
            mapping("headers", NodeKind::header);
            mapping("links", NodeKind::link);
            break;
        case NodeKind::request_body:
            mapping("content", NodeKind::media_type);
            break;
        case NodeKind::media_type:
            child("schema", NodeKind::schema);
            mapping("examples", NodeKind::example);
            mapping("encoding", NodeKind::encoding);
            break;
        case NodeKind::encoding:
            mapping("headers", NodeKind::header);
            break;
        case NodeKind::callback:
            for (auto& [name, item] : node.items())
                if (name != "$ref" && !name.starts_with("x-"))
                    visit(item, source, pointer + "/" + escape_json_pointer(name), NodeKind::path_item, depth + 1);
            break;
        case NodeKind::link:
            if (auto it = node.find("operationRef"); it != node.end()) {
                if (!it->is_string() || source != root_file_ || !it->get_ref<const std::string&>().starts_with('#'))
                    throw BundleFailure(location + "/operationRef", "External operationRef is not supported; its operation must retain path and server context");
                try {
                    const auto fragment = percent_decode(std::string_view(it->get_ref<const std::string&>()).substr(1));
                    if (!load(source).at(json::json_pointer(fragment)).is_object())
                        throw BundleFailure(location + "/operationRef", "operationRef must identify an Operation Object");
                } catch (const json::exception& error) {
                    throw BundleFailure(location + "/operationRef", "Unresolved operationRef: " + std::string(error.what()));
                }
            }
            break;
        case NodeKind::example:
        case NodeKind::security_scheme:
            break;
        }
    }

public:
    explicit Bundler(ResolverOptions options) : options_(std::move(options)) {
        if (options_.allowed_root) {
            std::error_code error;
            allowed_root_ = std::filesystem::canonical(*options_.allowed_root, error);
            if (error || !std::filesystem::is_directory(*allowed_root_))
                throw BundleFailure(options_.allowed_root->string(), "allowed_root must be an existing directory");
        }
    }

    json run(const std::filesystem::path& path) {
        const auto source = canonical_file(path);
        root_file_ = source;
        auto document = load(source);
        if (!document.is_object()) throw BundleFailure(source.string(), "OpenAPI document must be an object");
        std::size_t suffix = 0;
        while (document.contains(extension_name_)) extension_name_ = "x-swaggercpp-bundled-" + std::to_string(++suffix);
        visit(document, source, "", NodeKind::document, 0);
        if (!bundled_.empty()) document[extension_name_] = std::move(bundled_);
        return document;
    }
};

} // namespace

void ReferenceResolver::resolve(Document& document) {
    ResolverVisitor visitor(document);
    // Record schema URI scopes before aliases can expose their children elsewhere.
    MutableDocumentWalker::walk(document, visitor);
    visitor.begin_resolution();
    MutableDocumentWalker::walk(document, visitor);
}

Result<json> ReferenceResolver::bundle_json_file(const std::filesystem::path& path, ResolverOptions options) {
    try {
        return Result<json>::success(Bundler(std::move(options)).run(path));
    } catch (const BundleFailure& error) {
        return Result<json>::failure({{IssueSeverity::error, error.location, error.what()}});
    } catch (const std::exception& error) {
        return Result<json>::failure({{IssueSeverity::error, path.string(), error.what()}});
    }
}

Result<Document> ReferenceResolver::bundle_file(const std::filesystem::path& path, ResolverOptions options) {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    // A copied target gains the extension object and its entry as ancestors.
    const auto output_depth = options.max_depth > maximum - 2 ? maximum : options.max_depth + 2;
    auto bundled = bundle_json_file(path, std::move(options));
    if (!bundled) return Result<Document>::failure(bundled.issues());
    try {
        const auto content = bundled.value().dump();
        auto document = DocumentReader::read(content, {.allow_yaml = false,
            .max_document_bytes = content.size(), .max_nesting_depth = output_depth});
        auto issues = document.issues();
        for (auto& issue : issues) issue.source_file = path.string();
        if (!document) return Result<Document>::failure(std::move(issues));
        document.value().source_file = path.string();
        return Result<Document>::success(std::move(document.value()), std::move(issues));
    } catch (const std::exception& error) {
        return Result<Document>::failure({Issue {.message = error.what(), .source_file = path.string()}});
    }
}

} // namespace swaggercpp
