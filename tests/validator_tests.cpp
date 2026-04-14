#include <gtest/gtest.h>

#include "swaggercpp/swaggercpp.hpp"

TEST(DocumentValidatorTests, DetectsDuplicateOperationIds) {
    swaggercpp::Document document;
    document.openapi = "3.1.0";
    document.info.title = "Validation";
    document.info.version = "0.1.0";

    swaggercpp::Operation first_operation;
    first_operation.operation_id = "duplicate";
    first_operation.responses.emplace("200", swaggercpp::Response {.description = "ok"});

    swaggercpp::Operation second_operation;
    second_operation.operation_id = "duplicate";
    second_operation.responses.emplace("200", swaggercpp::Response {.description = "ok"});

    swaggercpp::PathItem first_path;
    first_path.operations.emplace(swaggercpp::HttpMethod::get, first_operation);

    swaggercpp::PathItem second_path;
    second_path.operations.emplace(swaggercpp::HttpMethod::post, second_operation);

    document.paths.emplace("/first", first_path);
    document.paths.emplace("/second", second_path);

    swaggercpp::DocumentValidator validator;
    const auto report = validator.validate(document);

    EXPECT_FALSE(report.ok());
    EXPECT_TRUE(std::ranges::any_of(report.issues, [](const swaggercpp::Issue& issue) {
        return issue.message.find("operationId must be unique") != std::string::npos;
    }));
}

