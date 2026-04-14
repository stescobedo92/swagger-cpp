#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    swaggercpp::Document document;
    document.version = swaggercpp::SpecificationVersion::openapi_3_1;
    document.openapi = "3.1.0";
    document.info.title = "Petstore";
    document.info.version = "2026.04";
    document.servers.push_back(swaggercpp::Server {.url = "https://api.example.com"});

    auto pet_schema = std::make_shared<swaggercpp::Schema>();
    pet_schema->type = "object";
    pet_schema->required = {"id", "name"};
    pet_schema->properties.emplace("id", std::make_shared<swaggercpp::Schema>(swaggercpp::Schema {.type = "string"}));
    pet_schema->properties.emplace("name", std::make_shared<swaggercpp::Schema>(swaggercpp::Schema {.type = "string"}));

    swaggercpp::Response response;
    response.description = "Pet payload";
    response.content.emplace("application/json", swaggercpp::MediaType {.schema = pet_schema});

    swaggercpp::Operation operation;
    operation.operation_id = "getPet";
    operation.responses.emplace("200", response);

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);

    document.paths.emplace("/pets/{id}", path_item);
    document.components.schemas.emplace("Pet", pet_schema);

    const auto yaml = swaggercpp::DocumentWriter::write_yaml(document);
    if (!yaml) {
        return 1;
    }

    std::cout << yaml.value() << '\n';
    return 0;
}

