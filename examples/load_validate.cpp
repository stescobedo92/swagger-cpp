#include <iostream>

#include "swaggercpp/swaggercpp.hpp"

int main() {
    const auto document = swaggercpp::DocumentReader::read(R"(
openapi: 3.1.0
info:
  title: Customer API
  version: 1.0.0
paths:
  /customers:
    get:
      operationId: listCustomers
      responses:
        "200":
          description: ok
)");

    if (!document) {
        std::cerr << "Unable to read document\n";
        return 1;
    }

    swaggercpp::DocumentValidator validator;
    const auto report = validator.validate(document.value());
    std::cout << "Title: " << document.value().info.title << "\n";
    std::cout << "Valid: " << std::boolalpha << report.ok() << "\n";
    return report.ok() ? 0 : 2;
}

