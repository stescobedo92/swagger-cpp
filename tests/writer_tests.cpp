#include <gtest/gtest.h>

#include "swaggercpp/swaggercpp.hpp"

TEST(DocumentWriterTests, BoundsExpansionOfSharedAcyclicSchemas) {
    swaggercpp::Document document;
    auto node = std::make_shared<swaggercpp::Schema>();
    for (int i = 0; i < 17; ++i) {
        auto parent = std::make_shared<swaggercpp::Schema>();
        parent->all_of = {node, node};
        node = std::move(parent);
    }
    document.components.schemas["Shared"] = node;
    const auto written = swaggercpp::DocumentWriter::write_json(document);
    ASSERT_FALSE(written);
    EXPECT_NE(written.issues().front().message.find("work limit"), std::string::npos);
}

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

TEST(DocumentWriterTests, PreservesContractFieldsThroughJsonAndYaml) {
    const auto original = nlohmann::json::parse(R"({
        "openapi": "3.1.1", "info": {"title": "Contract", "version": "1", "customInfo": {"enabled": true}},
        "jsonSchemaDialect": "https://json-schema.org/draft/2020-12/schema",
        "security": [{"auth": ["read"]}],
        "paths": {
            "x-owner": "payments",
            "/values": {
                "servers": [{"url": "https://path.example.test", "customServer": 1}],
                "get": {
                    "security": [], "servers": [{"url": "https://operation.example.test"}],
                    "externalDocs": {"url": "https://docs.example.test"},
                    "parameters": [{"name": "filter", "in": "query", "style": "deepObject", "explode": false, "allowReserved": false,
                        "schema": true, "example": {"key": 1}}],
                    "callbacks": {"done": {"{$request.body#/callback}": {"post": {"responses": {"200": {"description": "ok"}}}}}},
                    "responses": {
                        "x-owner": {"department": "api"},
                        "200": {"description": "", "headers": {"X-Id": {"schema": {"type": "string"}}},
                            "links": {"next": {"operationId": "nextValue"}},
                            "content": {"application/json": {"schema": false, "encoding": {"part": {"contentType": "text/plain"}}}}}
                    }
                }
            }
        },
        "webhooks": {"event": {"post": {"responses": {"204": {"description": "received"}}}}},
        "components": {
            "schemas": {"Value": {"type": ["integer", "null"], "enum": [null, 9007199254740993], "const": null,
                "minimum": 9007199254740993, "maximum": 18446744073709551615, "multipleOf": 2,
                "exclusiveMinimum": 1, "exclusiveMaximum": 99, "minLength": 0, "maxLength": 20,
                "minItems": 0, "maxItems": 4, "minProperties": 0, "maxProperties": 5, "pattern": "^[a-z]+$", "uniqueItems": false,
                "$schema": "https://json-schema.org/draft/2020-12/schema", "$id": "https://example.test/value", "$anchor": "value",
                "$defs": {"Accept": true, "Reject": false}, "items": false, "not": false, "allOf": [true],
                "properties": {"blocked": false}, "additionalProperties": false, "unevaluatedProperties": false,
                "if": {"type": "string"}, "then": {"maxLength": 5}, "x-owner": "data"}},
            "securitySchemes": {"auth": {"type": "oauth2", "flows": {"clientCredentials": {"tokenUrl": "/token", "scopes": {"read": "Read"}}}}},
            "headers": {"Trace": {"schema": {"type": "string"}}}
        }
    })");
    const auto parsed = swaggercpp::DocumentReader::read(original.dump());
    ASSERT_TRUE(parsed.ok());
    for (const auto format : {swaggercpp::DocumentFormat::json, swaggercpp::DocumentFormat::yaml}) {
        const auto written = swaggercpp::DocumentWriter::write(parsed.value(), {.format = format});
        ASSERT_TRUE(written.ok());
        const auto reread = swaggercpp::DocumentReader::read(written.value());
        ASSERT_TRUE(reread.ok());
        const auto final_json = swaggercpp::DocumentWriter::write_json(reread.value());
        ASSERT_TRUE(final_json.ok());
        EXPECT_EQ(nlohmann::json::parse(final_json.value()), original);
    }
}

TEST(DocumentWriterTests, PreservesReferenceObjectsWithoutInventingFields) {
    const auto original = nlohmann::json::parse(R"({
        "openapi":"3.1.0", "info":{"title":"Refs","version":"1"}, "paths":{},
        "components": {
            "parameters":{"Id":{"$ref":"#/components/parameters/Other"}},
            "responses":{"Failure":{"$ref":"#/components/responses/Other"}},
            "securitySchemes":{"Auth":{"$ref":"#/components/securitySchemes/Other"}}
        }
    })");
    const auto parsed = swaggercpp::DocumentReader::read(original.dump());
    ASSERT_TRUE(parsed.ok());
    const auto written = swaggercpp::DocumentWriter::write_json(parsed.value());
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(nlohmann::json::parse(written.value()), original);
}

TEST(DocumentWriterTests, RejectsUnrepresentablePointerCyclesWithoutRecursingForever) {
    swaggercpp::Document document;
    document.info.title = "Cycle";
    document.info.version = "1";
    auto cyclic = std::make_shared<swaggercpp::Schema>();
    cyclic->properties.emplace("self", cyclic);
    document.components.schemas.emplace("Cyclic", cyclic);
    const auto json = swaggercpp::DocumentWriter::write_json(document);
    const auto yaml = swaggercpp::DocumentWriter::write_yaml(document);
    cyclic->properties.clear(); // Release the test's intentionally cyclic ownership graph.
    EXPECT_FALSE(json.ok());
    EXPECT_FALSE(yaml.ok());
    ASSERT_FALSE(json.issues().empty());
    EXPECT_NE(json.issues().front().message.find("cycle"), std::string::npos);
}

TEST(DocumentWriterTests, EmitsExistingReferenceForRecursivePointerCycles) {
    swaggercpp::Document document;
    document.info.title = "Cycle";
    document.info.version = "1";
    auto cyclic = std::make_shared<swaggercpp::Schema>();
    cyclic->ref = "#/components/schemas/Cyclic";
    cyclic->properties.emplace("self", cyclic);
    document.components.schemas.emplace("Cyclic", cyclic);
    const auto result = swaggercpp::DocumentWriter::write_json(document);
    cyclic->properties.clear();
    ASSERT_TRUE(result.ok());
    const auto output = nlohmann::json::parse(result.value());
    EXPECT_EQ(output["components"]["schemas"]["Cyclic"]["properties"]["self"],
        (nlohmann::json {{"$ref", "#/components/schemas/Cyclic"}}));
}

TEST(DocumentWriterTests, TypedFieldsOverridePreservedFieldsAndExtensions) {
    swaggercpp::Document document;
    document.info.title = "Current";
    document.info.version = "1";
    document.info.unknown_fields["title"] = "Stale";
    document.info.extensions["title"] = "Invalid extension";
    const auto result = swaggercpp::DocumentWriter::write_json(document);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(nlohmann::json::parse(result.value())["info"]["title"], "Current");
}

TEST(DocumentWriterTests, RetainsExplicitEmptyOverridesAndRequiredMaps) {
    const auto original = nlohmann::json::parse(R"({
        "openapi":"3.1.0", "info":{"title":"Empty","version":"1"},
        "servers":[{"url":"https://example.test"}], "security":[{"auth":[]}],
        "paths":{"/value":{"servers":[], "get":{
            "servers":[], "security":[], "responses":{}, "requestBody":{"content":{}}
        }}}
    })");
    const auto parsed = swaggercpp::DocumentReader::read(original.dump());
    ASSERT_TRUE(parsed.ok());
    const auto written = swaggercpp::DocumentWriter::write_json(parsed.value());
    ASSERT_TRUE(written.ok());
    EXPECT_EQ(nlohmann::json::parse(written.value()), original);
    EXPECT_TRUE(swaggercpp::DocumentReader::read(written.value()).ok());
}

