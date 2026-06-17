#include "swaggercpp/mutable_walker.hpp"
#include <set>

namespace swaggercpp {
namespace {

void walk_schema(SchemaPtr& schema, MutableDocumentVisitor& visitor, const std::string_view path, std::set<const Schema*>& visited) {
    if (!schema) {
        return;
    }
    
    visitor.visit_schema(schema, path);
    
    if (!schema || visited.count(schema.get())) {
        return;
    }
    visited.insert(schema.get());

    for (auto& [name, child_schema] : schema->properties) {
        walk_schema(child_schema, visitor, std::string(path) + "/properties/" + name, visited);
    }
    walk_schema(schema->items, visitor, std::string(path) + "/items", visited);
    walk_schema(schema->not_schema, visitor, std::string(path) + "/not", visited);

    auto walk_schema_list = [&](std::vector<SchemaPtr>& schemas, const std::string_view key) {
        std::size_t index = 0;
        for (auto& item : schemas) {
            walk_schema(item, visitor, std::string(path) + "/" + std::string(key) + "/" + std::to_string(index), visited);
            ++index;
        }
    };

    walk_schema_list(schema->all_of, "allOf");
    walk_schema_list(schema->any_of, "anyOf");
    walk_schema_list(schema->one_of, "oneOf");
}

} // namespace

void MutableDocumentWalker::walk(Document& document, MutableDocumentVisitor& visitor) {
    visitor.enter_document(document);

    std::set<const Schema*> visited_schemas;

    for (auto& [path_name, path_item] : document.paths) {
        visitor.enter_path_item(path_name, path_item);

        for (auto& parameter : path_item.parameters) {
            visitor.visit_parameter(parameter);
            if (parameter.schema) {
                walk_schema(parameter.schema, visitor, "/paths/" + path_name + "/parameters/" + parameter.name + "/schema", visited_schemas);
            }
        }

        for (auto& [method, operation] : path_item.operations) {
            visitor.enter_operation(path_name, method, operation);
            for (auto& parameter : operation.parameters) {
                visitor.visit_parameter(parameter);
                if (parameter.schema) {
                    walk_schema(parameter.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/parameters/" + parameter.name + "/schema", visited_schemas);
                }
            }
            if (operation.request_body) {
                visitor.visit_request_body(*operation.request_body);
                for (auto& [media_type_name, media_type] : operation.request_body->content) {
                    if (media_type.schema) {
                        walk_schema(media_type.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/requestBody/content/" + media_type_name + "/schema", visited_schemas);
                    }
                }
            }
            for (auto& [status_code, response] : operation.responses) {
                visitor.visit_response(status_code, response);
                for (auto& [media_type_name, media_type] : response.content) {
                    if (media_type.schema) {
                        walk_schema(media_type.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/responses/" + status_code + "/content/" + media_type_name + "/schema", visited_schemas);
                    }
                }
            }
            visitor.leave_operation(path_name, method, operation);
        }

        visitor.leave_path_item(path_name, path_item);
    }

    for (auto& [name, schema] : document.components.schemas) {
        walk_schema(schema, visitor, "/components/schemas/" + name, visited_schemas);
    }
    
    for (auto& [name, parameter] : document.components.parameters) {
        visitor.visit_parameter(parameter);
        if (parameter.schema) {
            walk_schema(parameter.schema, visitor, "/components/parameters/" + name + "/schema", visited_schemas);
        }
    }
    for (auto& [name, request_body] : document.components.request_bodies) {
        visitor.visit_request_body(request_body);
        for (auto& [media_type_name, media_type] : request_body.content) {
            if (media_type.schema) {
                walk_schema(media_type.schema, visitor, "/components/requestBodies/" + name + "/content/" + media_type_name + "/schema", visited_schemas);
            }
        }
    }
    for (auto& [name, response] : document.components.responses) {
        visitor.visit_response(name, response);
        for (auto& [media_type_name, media_type] : response.content) {
            if (media_type.schema) {
                walk_schema(media_type.schema, visitor, "/components/responses/" + name + "/content/" + media_type_name + "/schema", visited_schemas);
            }
        }
    }

    visitor.leave_document(document);
}

} // namespace swaggercpp
