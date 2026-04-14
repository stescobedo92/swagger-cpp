#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    auto result = swaggercpp::DocumentReader::read(R"({
      "openapi": "3.1.0",
      "info": { "title": "Catalog", "version": "1.0.0" },
      "paths": {
        "/products": {
          "get": {
            "operationId": "listProducts",
            "responses": {
              "200": { "description": "ok" }
            }
          }
        }
      }
    })");

    if (!result) {
        return 1;
    }

    result.value().info.version = "1.1.0";
    result.value().paths["/products"].operations[swaggercpp::HttpMethod::post].responses["201"] =
        swaggercpp::Response {.description = "created"};

    const auto json = swaggercpp::DocumentWriter::write_json(result.value());
    if (!json) {
        return 2;
    }

    std::cout << json.value() << '\n';
    return 0;
}
