#pragma once

#include <string>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include "swaggercpp/document.hpp"

namespace swaggercpp::detail {

inline std::string child_pointer(const std::string_view parent, const std::string_view token) {
    std::string result(parent);
    result += '/';
    for (const char ch : token) {
        if (ch == '~') result += "~0";
        else if (ch == '/') result += "~1";
        else result += ch;
    }
    return result;
}

// Both public walkers use the same traversal so their coverage and paths agree.
// The active set tracks ancestors, allowing shared schemas at different paths.
template <bool Mutable, typename Visitor>
class DocumentTraversal {
    template <typename T>
    using Qualified = std::conditional_t<Mutable, T, const T>;

    Visitor& visitor_;
    std::unordered_set<const Schema*> active_schemas_;
    std::size_t schema_visits_ {0};

    void schema(Qualified<SchemaPtr>& pointer, const std::string_view path) {
        if (!pointer) return;
        if (++schema_visits_ > 100000) {
            throw std::runtime_error("Document traversal exceeds the limit of 100000 schema visits");
        }
        if constexpr (Mutable) visitor_.visit_schema(pointer, path);
        else visitor_.visit_schema(*pointer, path);
        if (!pointer || active_schemas_.contains(pointer.get())) return;
        if (active_schemas_.size() >= 256) {
            throw std::runtime_error("Document traversal exceeds the schema depth limit of 256");
        }

        // A mutable visitor may replace a pointer. Keep the visited node alive
        // while traversing it, and check the replacement for cycles.
        const auto current = pointer;
        active_schemas_.insert(current.get());
        Qualified<Schema>& value = *current;
        for (auto& [name, child] : value.properties)
            schema(child, child_pointer(child_pointer(path, "properties"), name));
        for (auto& [name, child] : value.defs)
            schema(child, child_pointer(child_pointer(path, "$defs"), name));
        schema(value.items, child_pointer(path, "items"));
        schema(value.not_schema, child_pointer(path, "not"));
        auto sequence = [&](auto& entries, const std::string_view keyword) {
            for (std::size_t index = 0; index < entries.size(); ++index)
                schema(entries[index], child_pointer(child_pointer(path, keyword), std::to_string(index)));
        };
        sequence(value.all_of, "allOf");
        sequence(value.any_of, "anyOf");
        sequence(value.one_of, "oneOf");
        active_schemas_.erase(current.get());
    }

    void content(Qualified<std::map<std::string, MediaType>>& entries, const std::string_view path) {
        for (auto& [name, media_type] : entries)
            schema(media_type.schema, child_pointer(child_pointer(path, name), "schema"));
    }

    void parameter(Qualified<Parameter>& value, const std::string_view path) {
        visitor_.visit_parameter(value);
        schema(value.schema, child_pointer(path, "schema"));
        content(value.content, child_pointer(path, "content"));
    }

    void parameters(Qualified<std::vector<Parameter>>& entries, const std::string_view path) {
        for (std::size_t index = 0; index < entries.size(); ++index)
            parameter(entries[index], child_pointer(path, std::to_string(index)));
    }

    void request_body(Qualified<RequestBody>& value, const std::string_view path) {
        visitor_.visit_request_body(value);
        content(value.content, child_pointer(path, "content"));
    }

    void response(Qualified<Response>& value, const std::string_view name, const std::string_view path) {
        visitor_.visit_response(name, value);
        content(value.content, child_pointer(path, "content"));
    }

    void path_items(Qualified<std::map<std::string, PathItem>>& entries, const std::string_view path) {
        for (auto& [name, item] : entries) {
            const auto item_path = child_pointer(path, name);
            visitor_.enter_path_item(name, item);
            parameters(item.parameters, child_pointer(item_path, "parameters"));
            for (auto& [method, operation] : item.operations) {
                const auto operation_path = child_pointer(item_path, to_string(method));
                visitor_.enter_operation(name, method, operation);
                parameters(operation.parameters, child_pointer(operation_path, "parameters"));
                if (operation.request_body)
                    request_body(*operation.request_body, child_pointer(operation_path, "requestBody"));
                for (auto& [status, value] : operation.responses)
                    response(value, status, child_pointer(child_pointer(operation_path, "responses"), status));
                visitor_.leave_operation(name, method, operation);
            }
            visitor_.leave_path_item(name, item);
        }
    }

public:
    explicit DocumentTraversal(Visitor& visitor) : visitor_(visitor) {}

    void walk(Qualified<Document>& document) {
        visitor_.enter_document(document);
        path_items(document.paths, "/paths");
        path_items(document.webhooks, "/webhooks");
        for (auto& [name, value] : document.components.schemas)
            schema(value, child_pointer("/components/schemas", name));
        for (auto& [name, value] : document.components.parameters)
            parameter(value, child_pointer("/components/parameters", name));
        for (auto& [name, value] : document.components.request_bodies)
            request_body(value, child_pointer("/components/requestBodies", name));
        for (auto& [name, value] : document.components.responses)
            response(value, name, child_pointer("/components/responses", name));
        visitor_.leave_document(document);
    }
};

} // namespace swaggercpp::detail
