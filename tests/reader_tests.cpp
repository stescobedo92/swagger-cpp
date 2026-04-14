#include <gtest/gtest.h>

#include "swaggercpp/swaggercpp.hpp"

namespace {

constexpr auto kSampleDocument = R"(
openapi: 3.1.0
info:
  title: Inventory API
  version: 1.2.0
paths:
  /items:
    get:
      operationId: listItems
      responses:
        "200":
          description: Success
          content:
            application/json:
              schema:
                type: array
                items:
                  $ref: '#/components/schemas/Item'
components:
  schemas:
    Item:
      type: object
      required: [id, name]
      properties:
        id:
          type: string
        name:
          type: string
)";

} // namespace

TEST(DocumentReaderTests, ParsesYamlIntoTypedDocument) {
    const auto result = swaggercpp::DocumentReader::read(kSampleDocument);

    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().info.title, "Inventory API");
    EXPECT_EQ(result.value().version, swaggercpp::SpecificationVersion::openapi_3_1);
    ASSERT_TRUE(result.value().paths.contains("/items"));
    const auto& operation = result.value().paths.at("/items").operations.at(swaggercpp::HttpMethod::get);
    EXPECT_EQ(operation.operation_id.value_or(""), "listItems");
    ASSERT_TRUE(result.value().components.schemas.contains("Item"));
}

