#include "swaggercpp/walker.hpp"

namespace swaggercpp {
namespace {

void walk_schema(const SchemaPtr& schema, DocumentVisitor& visitor, const std::string_view path) {
    if (!schema) {
        return;
    }
    visitor.visit_schema(*schema, path);

    for (const auto& [name, child_schema] : schema->properties) {
        walk_schema(child_schema, visitor, std::string(path) + "/properties/" + name);
    }
    walk_schema(schema->items, visitor, std::string(path) + "/items");
    walk_schema(schema->not_schema, visitor, std::string(path) + "/not");

    auto walk_schema_list = [&](const std::vector<SchemaPtr>& schemas, const std::string_view key) {
        std::size_t index = 0;
        for (const auto& item : schemas) {
            walk_schema(item, visitor, std::string(path) + "/" + std::string(key) + "/" + std::to_string(index));
            ++index;
        }
    };

    walk_schema_list(schema->all_of, "allOf");
    walk_schema_list(schema->any_of, "anyOf");
    walk_schema_list(schema->one_of, "oneOf");
}

} // namespace

void DocumentWalker::walk(const Document& document, DocumentVisitor& visitor) {
    visitor.enter_document(document);

    for (const auto& [path_name, path_item] : document.paths) {
        visitor.enter_path_item(path_name, path_item);

        for (const auto& parameter : path_item.parameters) {
            visitor.visit_parameter(parameter);
            if (parameter.schema) {
                walk_schema(parameter.schema, visitor, "/paths/" + path_name + "/parameters/" + parameter.name + "/schema");
            }
        }

        for (const auto& [method, operation] : path_item.operations) {
            visitor.enter_operation(path_name, method, operation);
            for (const auto& parameter : operation.parameters) {
                visitor.visit_parameter(parameter);
                if (parameter.schema) {
                    walk_schema(parameter.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/parameters/" + parameter.name + "/schema");
                }
            }
            if (operation.request_body) {
                for (const auto& [media_type_name, media_type] : operation.request_body->content) {
                    if (media_type.schema) {
                        walk_schema(media_type.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/requestBody/content/" + media_type_name + "/schema");
                    }
                }
            }
            for (const auto& [status_code, response] : operation.responses) {
                visitor.visit_response(status_code, response);
                for (const auto& [media_type_name, media_type] : response.content) {
                    if (media_type.schema) {
                        walk_schema(media_type.schema, visitor, "/paths/" + path_name + "/" + std::string(to_string(method)) + "/responses/" + status_code + "/content/" + media_type_name + "/schema");
                    }
                }
            }
            visitor.leave_operation(path_name, method, operation);
        }

        visitor.leave_path_item(path_name, path_item);
    }

    for (const auto& [name, schema] : document.components.schemas) {
        walk_schema(schema, visitor, "/components/schemas/" + name);
    }

    visitor.leave_document(document);
}

} // namespace swaggercpp
