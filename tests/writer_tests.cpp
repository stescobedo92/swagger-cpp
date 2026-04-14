#include <gtest/gtest.h>

#include "swaggercpp/swaggercpp.hpp"

TEST(DocumentWriterTests, SerializesAndRoundTripsJson) {
    swaggercpp::Document document;
    document.version = swaggercpp::SpecificationVersion::openapi_3_1;
    document.openapi = "3.1.0";
    document.info.title = "Writer";
    document.info.version = "0.1.0";

    swaggercpp::SchemaPtr id_schema = std::make_shared<swaggercpp::Schema>();
    id_schema->type = "string";

    swaggercpp::SchemaPtr widget_schema = std::make_shared<swaggercpp::Schema>();
    widget_schema->type = "object";
    widget_schema->properties.emplace("id", id_schema);
    widget_schema->required.push_back("id");

    swaggercpp::Response response;
    response.description = "ok";
    response.content.emplace("application/json", swaggercpp::MediaType {.schema = widget_schema});

    swaggercpp::Operation operation;
    operation.responses.emplace("200", response);

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);

    document.paths.emplace("/widgets", path_item);
    document.components.schemas.emplace("Widget", widget_schema);

    const auto json_result = swaggercpp::DocumentWriter::write_json(document);
    ASSERT_TRUE(json_result.ok());
    EXPECT_NE(json_result.value().find("\"openapi\""), std::string::npos);

    const auto roundtrip = swaggercpp::DocumentWriter::roundtrip(json_result.value(), swaggercpp::DocumentFormat::yaml);
    ASSERT_TRUE(roundtrip.ok());
    EXPECT_EQ(roundtrip.value().info.title, "Writer");
    EXPECT_TRUE(roundtrip.value().paths.contains("/widgets"));
}

