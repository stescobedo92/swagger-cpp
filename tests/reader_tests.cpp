#include <gtest/gtest.h>

#include <chrono>
#include <fstream>

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

TEST(DocumentReaderTests, ReadsSchemaConstraintsAndBooleanSchemas) {
    const auto result = swaggercpp::DocumentReader::read(R"({
        "openapi": "3.1.1", "info": {"title": "Schemas", "version": "1"},
        "components": {"schemas": {
            "Never": false,
            "Value": {
                "type": ["integer", "null"], "enum": [null, 9007199254740993], "const": null,
                "minimum": 9007199254740993, "maximum": 18446744073709551615, "multipleOf": 2,
                "exclusiveMinimum": 3, "exclusiveMaximum": 11,
                "minLength": 0, "maxLength": 40, "pattern": "^[a-z]+$",
                "minItems": 0, "maxItems": 5, "uniqueItems": false,
                "minProperties": 1, "maxProperties": 4,
                "$schema": "https://json-schema.org/draft/2020-12/schema",
                "$id": "https://example.test/value", "$anchor": "value", "$defs": {"Anything": true},
                "items": false, "not": false, "allOf": [true], "properties": {"blocked": false},
                "unevaluatedProperties": false, "x-owner": "team"
            }
        }}
    })");
    ASSERT_TRUE(result.ok());
    const auto& schema = *result.value().components.schemas.at("Value");
    EXPECT_FALSE(result.value().components.schemas.at("Never")->boolean_value.value());
    EXPECT_EQ(schema.types, (std::vector<std::string> {"integer", "null"}));
    EXPECT_FALSE(schema.type.has_value());
    ASSERT_TRUE(schema.const_value.has_value());
    EXPECT_TRUE(schema.const_value->is_null());
    EXPECT_EQ(*schema.minimum, nlohmann::json(std::uint64_t {9007199254740993}));
    EXPECT_EQ(*schema.maximum, nlohmann::json(std::uint64_t {18446744073709551615ULL}));
    EXPECT_EQ(schema.enum_values.size(), 2U);
    EXPECT_TRUE(schema.defs.at("Anything")->boolean_value.value());
    EXPECT_FALSE(schema.items->boolean_value.value());
    EXPECT_FALSE(schema.properties.at("blocked")->boolean_value.value());
    EXPECT_EQ(schema.unknown_fields.at("unevaluatedProperties"), false);
    EXPECT_FALSE(schema.unknown_fields.contains("x-owner"));
    EXPECT_EQ(schema.extensions.at("x-owner"), "team");
}

TEST(DocumentReaderTests, DistinguishesInheritedAndExplicitlyPublicSecurity) {
    const auto result = swaggercpp::DocumentReader::read(R"({
        "openapi": "3.1.0", "info": {"title": "Security", "version": "1"},
        "security": [{"bearer": []}], "paths": {
            "/public": {"get": {"security": [], "responses": {"200": {"description": ""}}}},
            "/private": {"get": {"responses": {"200": {"description": "ok"}}}}
        }
    })");
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.value().security_defined);
    EXPECT_EQ(result.value().security.size(), 1U);
    const auto& public_op = result.value().paths.at("/public").operations.at(swaggercpp::HttpMethod::get);
    EXPECT_TRUE(public_op.security_defined);
    EXPECT_TRUE(public_op.security.empty());
    EXPECT_FALSE(result.value().paths.at("/private").operations.at(swaggercpp::HttpMethod::get).security_defined);
}

TEST(DocumentReaderTests, RejectsMalformedModeledValuesWithoutThrowing) {
    const std::vector<std::string> documents {
        "[]", "true", "null", "{}",
        R"({"openapi":"3.2.0","info":{"title":"a","version":"1"}})",
        R"({"openapi":"3.10.0","info":{"title":"a","version":"1"}})",
        R"({"openapi":"3.1.0","swagger":"2.0","info":{"title":"a","version":"1"}})",
        R"({"openapi":"3.1.0","info":{"title":1,"version":"1"}})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"servers":{}})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"security":[{"key":"read"}]})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"components":{"schemas":{"a":{"type":["string",4]}}}})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"components":{"schemas":{"a":{"items":null}}}})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"components":{"schemas":{"a":{"minItems":-1}}}})",
        R"({"openapi":"3.1.0","info":{"title":"a","version":"1"},"paths":{"/x":{"get":{"parameters":[{"name":"x","in":"bad"}],"responses":{}}}}})"
    };
    for (const auto& document : documents) {
        SCOPED_TRACE(document);
        EXPECT_NO_THROW({
            const auto result = swaggercpp::DocumentReader::read(document);
            EXPECT_FALSE(result.ok());
            EXPECT_FALSE(result.issues().empty());
        });
    }
}

TEST(DocumentReaderTests, ReportsEscapedPointerForMalformedField) {
    const auto result = swaggercpp::DocumentReader::read(R"({
        "openapi":"3.1.0", "info":{"title":"a","version":"1"},
        "paths":{"/x":{"get":{"deprecated":"yes", "responses":{}}}}
    })");
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.issues().empty());
    EXPECT_EQ(result.issues().back().path, "/paths/~1x/get/deprecated");
}

TEST(DocumentReaderTests, PreservesRequiredFieldIssues) {
    const auto result = swaggercpp::DocumentReader::read(R"({"openapi":"3.1.0", "info":{}})");
    EXPECT_FALSE(result.ok());
    EXPECT_GE(result.issues().size(), 2U);
}

TEST(DocumentReaderTests, ReadsFlowStyleYamlAndHonorsYamlDisabled) {
    constexpr auto flow = "{openapi: 3.1.1, info: {title: Flow, version: '1'}, paths: {}}";
    const auto result = swaggercpp::DocumentReader::read(flow, {.track_source_locations = true});
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value().info.title, "Flow");
    EXPECT_EQ(result.value().info.version, "1");
    const auto location = result.value().source_locations.find("/info/title");
    ASSERT_NE(location, result.value().source_locations.end());
    EXPECT_EQ(location->second.line, 1U);
    EXPECT_EQ(location->second.column, std::string_view(flow).find("Flow") + 1);

    const auto disabled = swaggercpp::DocumentReader::read(flow, {.allow_yaml = false});
    ASSERT_FALSE(disabled.ok());
    ASSERT_FALSE(disabled.issues().empty());
    EXPECT_EQ(disabled.issues().front().path, "/");
    EXPECT_EQ(disabled.issues().front().line, 1U);
    EXPECT_NE(disabled.issues().front().message.find("parse_error"), std::string::npos);
}

TEST(DocumentReaderTests, KeepsJsonSyntaxDiagnosticWhenYamlIsNotAnOpenApiDocument) {
    const std::vector<std::string> malformed {
        "{\n  \"openapi\": \"3.1.0\",\n  \"info\": ",
        "{unrelated: mapping}"
    };
    for (const auto& content : malformed) {
        SCOPED_TRACE(content);
        const auto result = swaggercpp::DocumentReader::read(content, {.track_source_locations = true});
        const auto json_only = swaggercpp::DocumentReader::read(content, {.allow_yaml = false});
        ASSERT_FALSE(result.ok());
        ASSERT_FALSE(json_only.ok());
        ASSERT_EQ(result.issues().size(), 1U);
        ASSERT_EQ(json_only.issues().size(), 1U);
        EXPECT_EQ(result.issues().front().message, json_only.issues().front().message);
        EXPECT_EQ(result.issues().front().line, json_only.issues().front().line);
        EXPECT_EQ(result.issues().front().column, json_only.issues().front().column);
        EXPECT_EQ(result.issues().front().path, "/");
    }
}

TEST(DocumentReaderTests, YamlFallbackDoesNotMaskResourceLimits) {
    const auto json = swaggercpp::DocumentReader::read(
        R"({"openapi":"3.1.0","info":{"title":"Depth","version":"1"},"paths":{}})",
        {.max_nesting_depth = 1});
    ASSERT_FALSE(json.ok());
    ASSERT_FALSE(json.issues().empty());
    EXPECT_NE(json.issues().front().message.find("max_nesting_depth"), std::string::npos);

    constexpr auto flow = "{openapi: 3.1.0, info: {title: Depth, version: '1'}, paths: {}}";
    const auto yaml = swaggercpp::DocumentReader::read(flow, {.max_nesting_depth = 1});
    ASSERT_FALSE(yaml.ok());
    ASSERT_FALSE(yaml.issues().empty());
    EXPECT_NE(yaml.issues().front().message.find("depth"), std::string::npos);
    EXPECT_EQ(yaml.issues().front().message.find("parse_error"), std::string::npos);

    const auto oversized = swaggercpp::DocumentReader::read(flow, {.max_document_bytes = 10});
    ASSERT_FALSE(oversized.ok());
    EXPECT_NE(oversized.issues().front().message.find("max_document_bytes"), std::string::npos);
}

TEST(DocumentReaderTests, ReportsFlowYamlSemanticErrorsWithFileAndPointerLocation) {
    struct TemporaryFile {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("swaggercpp-flow-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".yaml");
        ~TemporaryFile() {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
        }
    } file;
    constexpr auto content = "{openapi: 3.1.0, info: {title: 42, version: '1'}, paths: {}}";
    {
        std::ofstream stream(file.path);
        ASSERT_TRUE(stream.is_open());
        stream << content;
    }
    const auto result = swaggercpp::DocumentReader::read_file(file.path, {.track_source_locations = true});
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.issues().empty());
    const auto& issue = result.issues().front();
    EXPECT_EQ(issue.path, "/info/title");
    EXPECT_EQ(issue.source_file, file.path.string());
    EXPECT_EQ(issue.line, 1U);
    EXPECT_EQ(issue.column, std::string_view(content).find("42") + 1);
}

