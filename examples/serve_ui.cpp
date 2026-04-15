#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    swaggercpp::Document document;
    document.version = swaggercpp::SpecificationVersion::openapi_3_1;
    document.openapi = "3.1.0";
    document.info.title = "Embedded Swagger UI";
    document.info.version = "1.0.0";

    swaggercpp::Response response;
    response.description = "ok";

    swaggercpp::Operation operation;
    operation.operation_id = "listCustomers";
    operation.responses.emplace("200", response);

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);

    document.paths.emplace("/customers", path_item);

    auto session = swaggercpp::SwaggerUiServer::start(document, {
        .title = "Embedded Swagger UI",
    });

    if (!session) {
        for (const auto& issue : session.issues()) {
            std::cerr << issue.path << ": " << issue.message << '\n';
        }
        return 1;
    }

    std::cout << "Swagger UI available at " << session.value().ui_url() << '\n';
    session.value().wait();
    return 0;
}
