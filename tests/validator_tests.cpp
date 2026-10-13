#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <tuple>

#include "swaggercpp/swaggercpp.hpp"

namespace {

swaggercpp::Document valid_document() {
    swaggercpp::Document document;
    document.openapi = "3.1.0";
    document.info.title = "Validation";
    document.info.version = "1.0.0";
    swaggercpp::Operation operation;
    operation.responses.emplace("200", swaggercpp::Response {.description = "Success"});
    document.paths["/items"].operations.emplace(swaggercpp::HttpMethod::get, std::move(operation));
    return document;
}

swaggercpp::Operation& operation(swaggercpp::Document& document) {
    return document.paths.at("/items").operations.at(swaggercpp::HttpMethod::get);
}

std::size_t rule_count(const swaggercpp::ValidationReport& report, const std::string_view rule_id) {
    return static_cast<std::size_t>(std::ranges::count_if(report.issues, [&](const swaggercpp::Issue& issue) { return issue.rule_id == rule_id; }));
}

swaggercpp::Parameter path_parameter(const std::string& name) {
    swaggercpp::Parameter parameter;
    parameter.name = name;
    parameter.in = swaggercpp::ParameterLocation::path;
    parameter.required = true;
    parameter.schema = std::make_shared<swaggercpp::Schema>();
    parameter.schema->type = "string";
    return parameter;
}

} // namespace

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

TEST(DocumentValidatorTests, AcceptsEmptyPathsAndSchemasWithoutSpuriousWarnings) {
    auto document = valid_document();
    document.paths.clear();
    document.components.schemas["Anything"] = std::make_shared<swaggercpp::Schema>();
    const auto report = swaggercpp::DocumentValidator().validate(document);
    EXPECT_TRUE(report.ok());
    EXPECT_TRUE(report.issues.empty());
}

TEST(DocumentValidatorTests, ValidatesWebhookOnlyDocumentsWithoutTreatingNamesAsPaths) {
    auto document = valid_document();
    document.webhooks["order.created"] = document.paths.at("/items");
    document.paths.clear();
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    document.openapi = "3.0.3";
    document.version = swaggercpp::SpecificationVersion::openapi_3_0;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "document.webhooks.version"), 1U);
}

TEST(DocumentValidatorTests, ChecksSupportedVersionAndModelConsistency) {
    auto document = valid_document();
    for (const auto* version : {"3.2.0", "3.1", "3.1.bad", "4.0.0"}) {
        document.openapi = version;
        EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "document.version"), 1U);
    }
    document.openapi = "3.0.4";
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "document.version"), 1U);
    document.version = swaggercpp::SpecificationVersion::openapi_3_0;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).ok());
    document.swagger = "2.0";
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "document.version"), 1U);
}

TEST(DocumentValidatorTests, SupportsOpenApiResponseRangesAndRejectsInvalidCodes) {
    auto document = valid_document();
    auto& responses = operation(document).responses;
    responses.clear();
    for (const auto* status : {"100", "599", "1XX", "2XX", "3XX", "4XX", "5XX", "default"}) responses.emplace(status, swaggercpp::Response {.description = "Expected"});
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    for (const auto* status : {"099", "600", "20X", "2xx", "2000", ""}) responses.emplace(status, swaggercpp::Response {.description = "Invalid"});
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "response.status"), 6U);
}

TEST(DocumentValidatorTests, DoesNotAcceptOpenApiResponseRangesInSwagger2) {
    auto document = valid_document();
    document.openapi.reset();
    document.swagger = "2.0";
    document.version = swaggercpp::SpecificationVersion::swagger_2_0;
    operation(document).responses.emplace("2XX", swaggercpp::Response {.description = "Success"});
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "response.status"), 1U);
}

TEST(DocumentValidatorTests, AllowsOperationParametersToOverridePathParameters) {
    auto document = valid_document();
    auto path = document.paths.extract("/items");
    path.key() = "/items/{id}";
    path.mapped().parameters.push_back(path_parameter("id"));
    path.mapped().operations.at(swaggercpp::HttpMethod::get).parameters.push_back(path_parameter("id"));
    document.paths.insert(std::move(path));
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    document.paths.at("/items/{id}").operations.at(swaggercpp::HttpMethod::get).parameters.push_back(path_parameter("id"));
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "parameter.unique"), 1U);
}

TEST(DocumentValidatorTests, DetectsMissingAndExtraneousTemplateParameters) {
    auto document = valid_document();
    auto path = document.paths.extract("/items");
    path.key() = "/items/{id}";
    path.mapped().parameters.push_back(path_parameter("other"));
    document.paths.insert(std::move(path));
    const auto report = swaggercpp::DocumentValidator().validate(document);
    EXPECT_EQ(rule_count(report, "path.parameter"), 2U);
    EXPECT_TRUE(std::ranges::all_of(report.issues, [](const swaggercpp::Issue& issue) { return issue.path.starts_with("/paths/~1items~1{id}/get/"); }));
}

TEST(DocumentValidatorTests, ResolvesParameterReferencesForDuplicatesAndPathMatching) {
    auto document = valid_document();
    document.components.parameters["Item/Id"] = path_parameter("id");
    auto path = document.paths.extract("/items");
    path.key() = "/items/{id}";
    path.mapped().parameters.push_back(swaggercpp::Parameter {.ref = "#/components/parameters/Item~1Id"});
    document.paths.insert(std::move(path));
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    document.paths.at("/items/{id}").parameters.push_back(path_parameter("id"));
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "parameter.unique"), 1U);
}

TEST(DocumentValidatorTests, DefersUnknownExternalParameterReferences) {
    auto document = valid_document();
    auto path = document.paths.extract("/items");
    path.key() = "/items/{id}";
    path.mapped().parameters.push_back(swaggercpp::Parameter {.ref = "parameters.yaml#/id"});
    document.paths.insert(std::move(path));
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
}

TEST(DocumentValidatorTests, EnforcesExclusiveParameterSchemaOrContent) {
    auto document = valid_document();
    swaggercpp::Parameter parameter;
    parameter.name = "query";
    operation(document).parameters.push_back(parameter);
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "parameter.schema_or_content"), 1U);
    auto& stored = operation(document).parameters.front();
    stored.schema = std::make_shared<swaggercpp::Schema>();
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    stored.content["application/json"].schema = std::make_shared<swaggercpp::Schema>();
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "parameter.schema_or_content"), 1U);
}

TEST(DocumentValidatorTests, SupportsConfigurableLintSeverityAndDisablingRules) {
    auto document = valid_document();
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    swaggercpp::ValidatorOptions options;
    options.require_operation_id = true;
    options.require_operation_description = true;
    options.require_error_response = true;
    options.rules["operation.id.required"].severity = swaggercpp::IssueSeverity::error;
    options.rules["operation.description.required"].enabled = false;
    const auto report = swaggercpp::DocumentValidator(options).validate(document);
    EXPECT_FALSE(report.ok());
    ASSERT_EQ(report.issues.size(), 2U);
    EXPECT_EQ(rule_count(report, "operation.id.required"), 1U);
    EXPECT_EQ(rule_count(report, "operation.error_response.required"), 1U);
    EXPECT_TRUE(std::ranges::any_of(report.issues, [](const swaggercpp::Issue& issue) { return issue.rule_id == "operation.id.required" && issue.severity == swaggercpp::IssueSeverity::error; }));
    options.rules["operation.id.required"].enabled = false;
    operation(document).responses.emplace("default", swaggercpp::Response {.description = "Failure"});
    EXPECT_TRUE(swaggercpp::DocumentValidator(options).validate(document).issues.empty());
}

TEST(DocumentValidatorTests, RuleMapCanEnableLintWithoutConvenienceFlags) {
    swaggercpp::ValidatorOptions options;
    options.rules["operation.id.required"].severity = swaggercpp::IssueSeverity::error;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator(options).validate(valid_document()), "operation.id.required"), 1U);
}

TEST(DocumentValidatorTests, CustomRulesUseRegisteredIdsSeverityAndDeterministicOrder) {
    swaggercpp::ValidatorOptions options;
    options.rules["team.version"].severity = swaggercpp::IssueSeverity::warning;
    swaggercpp::DocumentValidator validator(options);
    validator.add_rule("team.version", [](const swaggercpp::Document&) {
        return std::vector<swaggercpp::Issue> {
            {.path = "/z", .message = "Second"}, {.path = "/a", .message = "First"}};
    });
    const auto report = validator.validate(valid_document());
    EXPECT_TRUE(report.ok());
    ASSERT_EQ(report.issues.size(), 2U);
    EXPECT_EQ(report.issues[0].path, "/a");
    EXPECT_EQ(report.issues[0].rule_id, "team.version");
    EXPECT_EQ(report.issues[1].path, "/z");
    EXPECT_THROW(validator.add_rule("team.version", [](const auto&) { return std::vector<swaggercpp::Issue>(); }), std::invalid_argument);
    EXPECT_THROW(validator.add_rule("", [](const auto&) { return std::vector<swaggercpp::Issue>(); }), std::invalid_argument);
    EXPECT_THROW(validator.add_rule("team.empty", {}), std::invalid_argument);
}

TEST(DocumentValidatorTests, DisabledCustomRuleIsNotInvoked) {
    swaggercpp::ValidatorOptions options;
    options.rules["team.skip"].enabled = false;
    swaggercpp::DocumentValidator validator(options);
    validator.add_rule("team.skip", [](const auto&) -> std::vector<swaggercpp::Issue> { throw std::runtime_error("Must not run"); });
    EXPECT_TRUE(validator.validate(valid_document()).ok());
}

TEST(DocumentValidatorTests, ChecksInternalReferencesWithEscapingAndBundledTargets) {
    auto document = valid_document();
    document.components.schemas["A/B~ C"] = std::make_shared<swaggercpp::Schema>();
    auto ref = std::make_shared<swaggercpp::Schema>();
    ref->ref = "#/components/schemas/A~1B~0%20C";
    document.components.schemas["ValidRef"] = ref;
    document.extensions["x-swaggercpp-bundled"]["ref-0"] = {{"type", "string"}};
    auto bundled = std::make_shared<swaggercpp::Schema>();
    bundled->ref = "#/x-swaggercpp-bundled/ref-0";
    document.components.schemas["Bundled"] = bundled;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    ref->ref = "#/components/schemas/Missing";
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "reference.unresolved"), 1U);
    ref->ref = "#/components/schemas/Bad~2";
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "reference.unresolved"), 1U);
}

TEST(DocumentValidatorTests, ValidatesReferencesWithinSchemaResourcesAndAnchors) {
    auto document = valid_document();
    auto resource = std::make_shared<swaggercpp::Schema>();
    resource->id = "https://example.test/resource";
    resource->defs["Value"] = std::make_shared<swaggercpp::Schema>();
    resource->defs["Value"]->anchor = "value";
    resource->properties["pointer"] = std::make_shared<swaggercpp::Schema>();
    resource->properties["pointer"]->ref = "#/$defs/Value";
    resource->properties["anchor"] = std::make_shared<swaggercpp::Schema>();
    resource->properties["anchor"]->ref = "#value";
    document.components.schemas["Resource"] = resource;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    resource->properties["anchor"]->ref = "#missing";
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "reference.unresolved"), 1U);
}

TEST(DocumentValidatorTests, DoesNotInterpretReferencesInExamplePayloads) {
    auto document = valid_document();
    auto schema = std::make_shared<swaggercpp::Schema>();
    schema->example = nlohmann::json {{"$ref", "#/does-not-exist"}};
    schema->unknown_fields["examples"] = nlohmann::json::array({{{"$ref", "#/literal-reference"}}});
    schema->additional_properties = nlohmann::json {{"type", "object"}, {"default", {{"$ref", "#/also-does-not-exist"}}}};
    document.components.schemas["Example"] = schema;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
}

TEST(DocumentValidatorTests, AllowsPresentEmptyResponseDescriptions) {
    const auto read = swaggercpp::DocumentReader::read(R"({"openapi":"3.1.0","info":{"title":"API","version":"1"},"paths":{"/test":{"get":{"responses":{"200":{"description":""}}}}}})");
    ASSERT_TRUE(read.ok());
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(read.value()).issues.empty());
}

TEST(DocumentValidatorTests, LinkPayloadsAreLiteralWhileLinkReferencesAreChecked) {
    auto document = valid_document();
    operation(document).responses.at("200").links["next"] = {
        {"operationId", "nextOperation"},
        {"parameters", {{"body", {{"$ref", "#/literal"}}}}},
        {"requestBody", {{"$ref", "#/literal-body"}}}
    };
    document.components.unknown_fields["links"] = {
        {"next", {{"operationId", "nextOperation"}, {"requestBody", {{"$ref", "#/also-literal"}}}}}
    };
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    operation(document).responses.at("200").links["other"] = {{"$ref", "#/components/links/Missing"}};
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "reference.unresolved"), 1U);
}

TEST(DocumentValidatorTests, DoesNotConfuseSchemaPropertyNamesWithLiteralKeywords) {
    auto document = valid_document();
    auto schema = std::make_shared<swaggercpp::Schema>();
    schema->additional_properties = nlohmann::json {
        {"properties", {{"default", {{"$ref", "#/components/schemas/Missing"}}}}}
    };
    document.components.schemas["Container"] = schema;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "reference.unresolved"), 1U);
}

TEST(DocumentValidatorTests, DetectsUnknownDocumentAndOperationSecuritySchemes) {
    auto document = valid_document();
    document.security = {{{"Token", {}}}};
    operation(document).security = {{{"Missing", {}}}};
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "security.scheme"), 2U);
    document.components.security_schemes["Token"].type = "http";
    operation(document).security.clear();
    operation(document).security_defined = true;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
}

TEST(DocumentValidatorTests, SchemaValidationIsVersionAware) {
    auto document = valid_document();
    auto schema = std::make_shared<swaggercpp::Schema>();
    schema->types = {"string", "null"};
    schema->exclusive_minimum = 0;
    document.components.schemas["Union"] = schema;
    auto boolean = std::make_shared<swaggercpp::Schema>();
    boolean->boolean_value = false;
    document.components.schemas["Never"] = boolean;
    EXPECT_TRUE(swaggercpp::DocumentValidator().validate(document).issues.empty());
    document.openapi = "3.0.3";
    document.version = swaggercpp::SpecificationVersion::openapi_3_0;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "schema.version"), 3U);
}

TEST(DocumentValidatorTests, TraversesManualRecursiveSchemaGraphsSafely) {
    auto document = valid_document();
    auto recursive = std::make_shared<swaggercpp::Schema>();
    recursive->type = "object";
    recursive->properties["next"] = recursive;
    recursive->properties["invalid"] = std::make_shared<swaggercpp::Schema>();
    recursive->properties["invalid"]->type = "unsupported";
    document.components.schemas["Node"] = recursive;
    auto reference = std::make_shared<swaggercpp::Schema>();
    reference->ref = "#/components/schemas/Node";
    document.components.schemas["Reference"] = reference;
    const auto report = swaggercpp::DocumentValidator().validate(document);
    EXPECT_EQ(rule_count(report, "schema.type"), 1U);
    EXPECT_EQ(rule_count(report, "reference.unresolved"), 0U);
    recursive->properties.erase("next"); // Release the deliberately constructed shared_ptr cycle.
}

TEST(DocumentValidatorTests, BoundsRepeatedExpansionOfSharedSchemaGraphs) {
    auto document = valid_document();
    auto schema = std::make_shared<swaggercpp::Schema>();
    schema->type = "string";
    for (std::size_t level = 0; level < 30; ++level) {
        auto parent = std::make_shared<swaggercpp::Schema>();
        parent->type = "object";
        parent->properties["left"] = schema;
        parent->properties["right"] = schema;
        schema = std::move(parent);
    }
    document.components.schemas["Shared"] = schema;
    const auto report = swaggercpp::DocumentValidator().validate(document);
    EXPECT_FALSE(report.ok());
    ASSERT_EQ(report.issues.size(), 1U);
    EXPECT_EQ(report.issues[0].rule_id, "document.limit");
    EXPECT_TRUE(report.issues[0].path.starts_with("/components/schemas/Shared/properties/"));
}

TEST(DocumentValidatorTests, RejectsNonnumericProgrammaticSchemaBounds) {
    auto document = valid_document();
    auto schema = std::make_shared<swaggercpp::Schema>();
    schema->minimum = "not-a-number";
    schema->maximum = false;
    schema->multiple_of = "zero";
    document.components.schemas["Number"] = schema;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "schema.numeric"), 3U);
    schema->minimum = 0;
    schema->maximum = 100;
    schema->multiple_of = 0;
    EXPECT_EQ(rule_count(swaggercpp::DocumentValidator().validate(document), "schema.multiple_of"), 1U);
}

TEST(DocumentValidatorTests, AttachesRecordedSourceLocationsWithoutInventingCoordinates) {
    auto document = valid_document();
    document.info.title.clear();
    document.source_file = "api.yaml";
    const auto no_location = swaggercpp::DocumentValidator().validate(document);
    ASSERT_EQ(no_location.issues.size(), 1U);
    EXPECT_EQ(no_location.issues[0].source_file, "api.yaml");
    EXPECT_FALSE(no_location.issues[0].line.has_value());
    EXPECT_FALSE(no_location.issues[0].column.has_value());
    document.source_locations["/info"] = {.line = 4, .column = 1};
    const auto report = swaggercpp::DocumentValidator().validate(document);
    ASSERT_EQ(report.issues.size(), 1U);
    EXPECT_EQ(report.issues[0].line, 4U);
    EXPECT_EQ(report.issues[0].column, 1U);
    document.source_locations["/info/title"] = {.line = 5, .column = 3};
    EXPECT_EQ(swaggercpp::DocumentValidator().validate(document).issues[0].line, 5U);
}

TEST(DocumentValidatorTests, PreservesCustomRuleSourceCoordinates) {
    auto document = valid_document();
    document.source_file = "api.yaml";
    document.source_locations[""] = {.line = 1, .column = 1};
    swaggercpp::DocumentValidator validator;
    validator.add_rule("team.rule", [](const auto&) {
        return std::vector<swaggercpp::Issue> {{.path = "/info", .message = "Policy issue", .source_file = "policy.yaml", .line = 20, .column = 4}};
    });
    const auto report = validator.validate(document);
    ASSERT_EQ(report.issues.size(), 1U);
    EXPECT_EQ(report.issues[0].source_file, "policy.yaml");
    EXPECT_EQ(report.issues[0].line, 20U);
    EXPECT_EQ(report.issues[0].column, 4U);
}

