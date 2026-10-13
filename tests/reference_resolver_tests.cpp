#include <gtest/gtest.h>
#include "swaggercpp/document.hpp"
#include "swaggercpp/reference_resolver.hpp"
#include "swaggercpp/reader.hpp"
#include "swaggercpp/writer.hpp"

#include <filesystem>
#include <fstream>
#include <limits>
#include <random>

using namespace swaggercpp;

TEST(ReferenceResolverTest, ResolvesSchemaReferences) {
    Document doc;
    doc.components.schemas["Pet"] = std::make_shared<Schema>();
    doc.components.schemas["Pet"]->type = "object";
    doc.components.schemas["Pet"]->title = "Pet Schema";

    PathItem path_item;
    Operation get_op;
    Response ok_resp;
    MediaType json_media;
    
    auto ref_schema = std::make_shared<Schema>();
    ref_schema->ref = "#/components/schemas/Pet";
    json_media.schema = ref_schema;
    
    ok_resp.content["application/json"] = json_media;
    get_op.responses["200"] = ok_resp;
    path_item.operations[HttpMethod::get] = get_op;
    doc.paths["/pets"] = path_item;

    ReferenceResolver::resolve(doc);

    auto resolved_schema = doc.paths["/pets"].operations[HttpMethod::get].responses["200"].content["application/json"].schema;
    EXPECT_NE(resolved_schema, nullptr);
    EXPECT_EQ(resolved_schema, doc.components.schemas["Pet"]);
    EXPECT_EQ(resolved_schema->type, "object");
    EXPECT_EQ(resolved_schema->title, "Pet Schema");
}

TEST(ReferenceResolverTest, ResolvesUnescapedPointers) {
    Document doc;
    doc.components.schemas["Pet~Name/Alias"] = std::make_shared<Schema>();
    doc.components.schemas["Pet~Name/Alias"]->type = "string";

    auto ref_schema = std::make_shared<Schema>();
    ref_schema->ref = "#/components/schemas/Pet~0Name~1Alias";
    
    doc.components.schemas["Ref"] = ref_schema;

    ReferenceResolver::resolve(doc);

    auto resolved_schema = doc.components.schemas["Ref"];
    EXPECT_NE(resolved_schema, nullptr);
    EXPECT_EQ(resolved_schema, doc.components.schemas["Pet~Name/Alias"]);
    EXPECT_EQ(resolved_schema->type, "string");
}

TEST(ReferenceResolverTest, ResolvesParameterReferences) {
    Document doc;
    Parameter comp_param;
    comp_param.name = "skip";
    comp_param.in = ParameterLocation::query;
    comp_param.description = "number of items to skip";
    doc.components.parameters["SkipParam"] = comp_param;

    PathItem path_item;
    Parameter ref_param;
    ref_param.ref = "#/components/parameters/SkipParam";
    path_item.parameters.push_back(ref_param);
    doc.paths["/pets"] = path_item;

    ReferenceResolver::resolve(doc);

    auto resolved_param = doc.paths["/pets"].parameters[0];
    EXPECT_EQ(resolved_param.name, "skip");
    EXPECT_EQ(resolved_param.in, ParameterLocation::query);
    EXPECT_EQ(resolved_param.description, "number of items to skip");
}

TEST(ReferenceResolverTest, HandlesCyclicSchemas) {
    Document doc;
    
    auto node_schema = std::make_shared<Schema>();
    node_schema->type = "object";
    
    auto ref_schema = std::make_shared<Schema>();
    ref_schema->ref = "#/components/schemas/Node";
    
    node_schema->properties["next"] = ref_schema;
    doc.components.schemas["Node"] = node_schema;

    PathItem path_item;
    Operation get_op;
    Response ok_resp;
    MediaType json_media;
    
    auto entry_schema = std::make_shared<Schema>();
    entry_schema->ref = "#/components/schemas/Node";
    json_media.schema = entry_schema;
    
    ok_resp.content["application/json"] = json_media;
    get_op.responses["200"] = ok_resp;
    path_item.operations[HttpMethod::get] = get_op;
    doc.paths["/nodes"] = path_item;

    ReferenceResolver::resolve(doc);

    auto resolved_schema = doc.paths["/nodes"].operations[HttpMethod::get].responses["200"].content["application/json"].schema;
    EXPECT_NE(resolved_schema, nullptr);
    EXPECT_EQ(resolved_schema, node_schema);
    
    auto next_schema = resolved_schema->properties["next"];
    EXPECT_NE(next_schema, nullptr);
    EXPECT_NE(next_schema, node_schema);
    ASSERT_TRUE(next_schema->ref.has_value());
    EXPECT_EQ(next_schema->ref, "#/components/schemas/Node");
    EXPECT_TRUE(DocumentWriter::write_json(doc).ok());
}

TEST(ReferenceResolverTest, RecursiveReferencesDoNotCreateOwnershipCycles) {
    std::weak_ptr<Schema> observer;
    {
        Document doc;
        auto first = std::make_shared<Schema>();
        auto second = std::make_shared<Schema>();
        first->properties["other"] = std::make_shared<Schema>();
        first->properties["other"]->ref = "#/components/schemas/B";
        second->properties["other"] = std::make_shared<Schema>();
        second->properties["other"]->ref = "#/components/schemas/A";
        doc.components.schemas["A"] = first;
        doc.components.schemas["B"] = second;
        observer = first;
        ReferenceResolver::resolve(doc);
        ReferenceResolver::resolve(doc);
        EXPECT_TRUE(DocumentWriter::write_json(doc).ok());
    }
    EXPECT_TRUE(observer.expired());
}

TEST(ReferenceResolverTest, KeepsSchemaReferenceSiblingConstraints) {
    Document doc;
    doc.components.schemas["Base"] = std::make_shared<Schema>();
    doc.components.schemas["Base"]->type = "string";
    auto constrained = std::make_shared<Schema>();
    constrained->ref = "#/components/schemas/Base";
    constrained->max_length = 12;
    doc.components.schemas["Constrained"] = constrained;
    ReferenceResolver::resolve(doc);
    EXPECT_EQ(doc.components.schemas["Constrained"], constrained);
    EXPECT_EQ(constrained->max_length, 12U);
    EXPECT_TRUE(constrained->ref.has_value());
}

TEST(ReferenceResolverTest, ResolvesAliasChainsAndLeavesMissingReferences) {
    Document doc;
    for (const auto* name : {"A", "B", "C", "Missing"}) doc.components.schemas[name] = std::make_shared<Schema>();
    doc.components.schemas["A"]->ref = "#/components/schemas/B";
    doc.components.schemas["B"]->ref = "#/components/schemas/C";
    doc.components.schemas["C"]->type = "integer";
    doc.components.schemas["Missing"]->ref = "#/components/schemas/Unknown";
    ReferenceResolver::resolve(doc);
    EXPECT_EQ(doc.components.schemas["A"], doc.components.schemas["C"]);
    EXPECT_EQ(doc.components.schemas["Missing"]->ref, "#/components/schemas/Unknown");
}

TEST(ReferenceResolverTest, PreservesExplicitEmptyResponseDescriptionOverride) {
    auto result = DocumentReader::read(R"({
        "openapi":"3.1.1", "info":{"title":"References", "version":"1"},
        "paths":{"/items":{"get":{"responses":{"200":{"$ref":"#/components/responses/A","description":""}}}}},
        "components":{"responses":{
            "A":{"$ref":"#/components/responses/B","description":"intermediate"},
            "B":{"description":"target"}
        }}
    })");
    ASSERT_TRUE(result.ok());
    ReferenceResolver::resolve(result.value());
    const auto& response = result.value().paths.at("/items").operations.at(HttpMethod::get).responses.at("200");
    EXPECT_FALSE(response.ref.has_value());
    EXPECT_EQ(response.description, "");
    EXPECT_EQ(result.value().components.responses.at("A").description, "intermediate");
}

TEST(ReferenceResolverTest, PreservesReferencesInsideSchemaUriScopes) {
    Document doc;
    auto scoped = std::make_shared<Schema>();
    scoped->id = "other.json";
    auto nested = std::make_shared<Schema>();
    nested->ref = "#/components/schemas/Target";
    scoped->properties["value"] = nested;
    doc.components.schemas["Scoped"] = scoped;
    doc.components.schemas["Target"] = std::make_shared<Schema>();
    doc.components.schemas["Target"]->type = "string";
    ReferenceResolver::resolve(doc);
    EXPECT_EQ(scoped->properties["value"], nested);
    EXPECT_EQ(nested->ref, "#/components/schemas/Target");
}

namespace {
using json = nlohmann::json;

class ReferenceBundlerTest : public testing::Test {
protected:
    std::filesystem::path directory;

    void SetUp() override {
        std::random_device random;
        for (int attempt = 0; attempt < 10; ++attempt) {
            auto candidate = std::filesystem::temp_directory_path() /
                ("swaggercpp-references-" + std::to_string(random()) + "-" + std::to_string(random()));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::move(candidate);
                return;
            }
        }
        FAIL() << "Unable to create test directory";
    }

    void TearDown() override {
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    }

    void write(const std::string& name, const std::string& content) {
        const auto file = directory / name;
        std::filesystem::create_directories(file.parent_path());
        std::ofstream stream(file, std::ios::binary);
        stream << content;
        ASSERT_TRUE(stream.good());
    }

    static json document(const std::string& ref) {
        return {{"openapi", "3.1.1"}, {"info", {{"title", "Bundled"}, {"version", "1"}}},
            {"paths", json::object()}, {"components", {{"schemas", {{"Entry", {{"$ref", ref}}}}}}}};
    }

    static const json& target(const json& bundle, const json& reference) {
        return bundle.at(json::json_pointer(reference.at("$ref").get<std::string>().substr(1)));
    }
};
} // namespace

TEST_F(ReferenceBundlerTest, BundlesMultiFileYamlCyclesAsFiniteLocalReferences) {
    write("api.json", document("models/a.yaml").dump());
    write("models/a.yaml", "type: object\nproperties:\n  b:\n    $ref: b.yaml\n");
    write("models/b.yaml", "type: object\nproperties:\n  a:\n    $ref: a.yaml\n");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok()) << (result.issues().empty() ? "" : result.issues()[0].message);
    const auto& bundle = result.value();
    const auto& entry = bundle["components"]["schemas"]["Entry"];
    const auto& a = target(bundle, entry);
    const auto& b = target(bundle, a["properties"]["b"]);
    EXPECT_EQ(b["properties"]["a"]["$ref"], entry["$ref"]);
    EXPECT_EQ(bundle["x-swaggercpp-bundled"].size(), 2U);
    const auto typed = ReferenceResolver::bundle_file(directory / "api.json");
    ASSERT_TRUE(typed.ok());
    EXPECT_TRUE(DocumentWriter::write_json(typed.value()).ok());
}

TEST_F(ReferenceBundlerTest, DecodesFileNamesAndJsonPointerEscapes) {
    write("api.json", document("models%20one.json#/names/A~1B~0%20C").dump());
    write("models one.json", R"({"names":{"A/B~ C":{"type":"string"}}})");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(target(result.value(), result.value()["components"]["schemas"]["Entry"])["type"], "string");
}

TEST_F(ReferenceBundlerTest, CachesCanonicalFileAndReferenceTargets) {
    auto source = document("model.json");
    source["components"]["schemas"]["Other"] = {{"$ref", "./model.json#"}};
    write("api.json", source.dump());
    write("model.json", R"({"type":"number"})");
    ResolverOptions options;
    options.max_documents = 2;
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json", options);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value()["x-swaggercpp-bundled"].size(), 1U);
    EXPECT_EQ(result.value()["components"]["schemas"]["Entry"]["$ref"], result.value()["components"]["schemas"]["Other"]["$ref"]);
}

TEST_F(ReferenceBundlerTest, PreservesLiteralReferencesInDataAndSchemaSiblings) {
    auto source = document("model.json");
    const json literal = {{"$ref", "https://example.invalid/not-a-reference"}, {"$id", "literal"}};
    source["components"]["schemas"]["Entry"]["maxLength"] = 5;
    source["components"]["schemas"]["Entry"]["default"] = literal;
    source["components"]["schemas"]["Entry"]["example"] = literal;
    source["components"]["schemas"]["Entry"]["enum"] = json::array({literal});
    source["components"]["schemas"]["Entry"]["x-data"] = literal;
    source["components"]["examples"]["Literal"] = {{"value", literal}};
    source["x-data"] = literal;
    write("api.json", source.dump());
    write("model.json", R"({"type":"string"})");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    const auto& schema = result.value()["components"]["schemas"]["Entry"];
    EXPECT_EQ(schema["maxLength"], 5);
    EXPECT_EQ(schema["default"], literal);
    EXPECT_EQ(schema["example"], literal);
    EXPECT_EQ(schema["enum"][0], literal);
    EXPECT_EQ(schema["x-data"], literal);
    EXPECT_EQ(result.value()["components"]["examples"]["Literal"]["value"], literal);
    EXPECT_EQ(result.value()["x-data"], literal);
}

TEST_F(ReferenceBundlerTest, BundlesReferencesInUnmodeledSchemaKeywordsAndResponseHeaders) {
    auto source = document("model.json");
    source["components"]["schemas"]["Extra"] = {{"if", {{"$ref", "model.json"}}},
        {"additionalProperties", {{"$ref", "model.json"}}}, {"prefixItems", json::array({{{"$ref", "model.json"}}})}};
    source["components"]["responses"]["Ok"] = {{"description", "OK"}, {"headers", {{"X-Value", {{"$ref", "header.json"}}}}}};
    write("api.json", source.dump());
    write("model.json", R"({"type":"string"})");
    write("header.json", R"({"schema":{"$ref":"model.json"}})");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    const auto& bundle = result.value();
    const auto& schema = bundle["components"]["schemas"]["Extra"];
    EXPECT_EQ(target(bundle, schema["if"])["type"], "string");
    EXPECT_EQ(target(bundle, schema["additionalProperties"])["type"], "string");
    EXPECT_EQ(target(bundle, schema["prefixItems"][0])["type"], "string");
    const auto& header = target(bundle, bundle["components"]["responses"]["Ok"]["headers"]["X-Value"]);
    EXPECT_EQ(target(bundle, header["schema"])["type"], "string");
}

TEST_F(ReferenceBundlerTest, ReportsUnresolvedPointerWithSourceLocation) {
    write("api.json", document("model.json#/missing").dump());
    write("model.json", R"({"type":"string"})");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_FALSE(result.ok());
    ASSERT_FALSE(result.issues().empty());
    EXPECT_NE(result.issues()[0].path.find("api.json#/components/schemas/Entry/$ref"), std::string::npos);
    EXPECT_NE(result.issues()[0].message.find("Unresolved reference"), std::string::npos);
}

TEST_F(ReferenceBundlerTest, RejectsUnsupportedUrisAndInvalidFragments) {
    for (const auto* ref : {"https://example.test/model.json", "file:///model.json", "//server/model.json",
            "model.json?version=1", "model.json#anchor", "model.json#/bad~2escape", "model%00.json", "%2F%2Fserver/model.json"}) {
        write("api.json", document(ref).dump());
        write("model.json", R"({"type":"string"})");
        EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json").ok()) << ref;
    }
}

TEST_F(ReferenceBundlerTest, EnforcesConfiguredRootAfterResolvingDotSegments) {
    write("allowed/api.json", document("../outside.json").dump());
    write("outside.json", R"({"type":"string"})");
    ResolverOptions options;
    options.allowed_root = directory / "allowed";
    const auto result = ReferenceResolver::bundle_json_file(directory / "allowed/api.json", options);
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.issues()[0].message.find("allowed_root"), std::string::npos);
    EXPECT_TRUE(ReferenceResolver::bundle_json_file(directory / "allowed/api.json").ok());
    write("allowed/api.json", document("%2E%2E/outside.json").dump());
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "allowed/api.json", options).ok());
}

TEST_F(ReferenceBundlerTest, EnforcesResourceAndDocumentLimits) {
    write("api.json", document("model.json").dump());
    write("model.json", R"({"type":"string"})");
    ResolverOptions options;
    options.max_documents = 1;
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json", options).ok());
    options = {};
    options.max_total_bytes = 8;
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json", options).ok());
    options = {};
    options.max_nodes = 2;
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json", options).ok());
    options = {};
    options.max_depth = 1;
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json", options).ok());
}

TEST_F(ReferenceBundlerTest, RejectsSchemaIdentifiersIncludingInheritedScope) {
    write("api.json", document("model.json").dump());
    for (const auto* keyword : {"$id", "$anchor", "$dynamicRef", "$dynamicAnchor"}) {
        write("model.json", json({{keyword, "value"}, {"type", "string"}}).dump());
        const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
        ASSERT_FALSE(result.ok());
        EXPECT_NE(result.issues()[0].message.find(keyword), std::string::npos);
    }
    write("api.json", document("model.json#/$defs/Value").dump());
    write("model.json", R"({"$id":"https://example.test/base/","$defs":{"Value":{"type":"string"}}})");
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json").ok());
}

TEST_F(ReferenceBundlerTest, HandlesBooleanSchemasAndExtensionNameCollisions) {
    auto source = document("model.yaml");
    source["x-swaggercpp-bundled"] = {{"original", true}};
    write("api.json", source.dump());
    write("model.yaml", "false\n");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value()["x-swaggercpp-bundled"], source["x-swaggercpp-bundled"]);
    EXPECT_EQ(target(result.value(), result.value()["components"]["schemas"]["Entry"]), false);
    EXPECT_TRUE(result.value().contains("x-swaggercpp-bundled-1"));
}

TEST_F(ReferenceBundlerTest, PreservesQuotedYamlScalarsAndRejectsAliasCycles) {
    write("api.json", document("model.yaml").dump());
    write("model.yaml", "type: string\nenum: ['true', '123']\n");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(target(result.value(), result.value()["components"]["schemas"]["Entry"])["enum"], json::array({"true", "123"}));
    write("model.yaml", "&recursive\nproperties:\n  self: *recursive\n");
    ResolverOptions options;
    options.max_depth = 16;
    EXPECT_FALSE(ReferenceResolver::bundle_json_file(directory / "api.json", options).ok());
}

TEST_F(ReferenceBundlerTest, PreservesYamlScalarTypesAndAcceptsFlowMaps) {
    write("api.json", document("model.yaml").dump());
    write("model.yaml", "{enum: [True, FALSE, !!int 12, !!float 1.25, 'false', 18446744073709551615]}\n");
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok()) << (result.issues().empty() ? "" : result.issues()[0].message);
    const auto& values = target(result.value(), result.value()["components"]["schemas"]["Entry"])["enum"];
    EXPECT_EQ(values, json::array({true, false, 12, 1.25, "false", std::uint64_t {18446744073709551615ULL}}));
}

TEST_F(ReferenceBundlerTest, LimitsExpandedYamlAliasBytes) {
    write("api.json", document("model.yaml").dump());
    write("model.yaml", "description: &text '" + std::string(300, 'x') + "'\nenum: [*text, *text, *text, *text]\n");
    ResolverOptions options;
    options.max_total_bytes = 900;
    const auto result = ReferenceResolver::bundle_json_file(directory / "api.json", options);
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.issues()[0].message.find("Expanded YAML"), std::string::npos);
}

TEST_F(ReferenceBundlerTest, RejectsSymlinksOutsideAllowedRoot) {
    write("allowed/api.json", document("linked.json").dump());
    write("outside.json", R"({"type":"string"})");
    std::error_code error;
    std::filesystem::create_symlink(directory / "outside.json", directory / "allowed/linked.json", error);
    if (error) GTEST_SKIP() << "Symlink creation is unavailable: " << error.message();
    ResolverOptions options;
    options.allowed_root = directory / "allowed";
    const auto result = ReferenceResolver::bundle_json_file(directory / "allowed/api.json", options);
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.issues()[0].message.find("allowed_root"), std::string::npos);
}

TEST_F(ReferenceBundlerTest, KeepsRootOperationLinksAndRejectsExternalOperationContext) {
    auto source = document("model.json");
    source["paths"]["/items"]["get"] = {{"responses", {{"200", {{"description", "OK"}}}}}};
    source["components"]["links"]["Items"] = {{"operationRef", "#/paths/~1items/get"}};
    write("api.json", source.dump());
    write("model.json", R"({"type":"string"})");
    auto result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.value()["components"]["links"]["Items"]["operationRef"], "#/paths/~1items/get");
    source["components"]["links"]["Items"] = {{"$ref", "link.json"}};
    write("api.json", source.dump());
    write("link.json", R"({"operationRef":"#/paths/~1items/get"})");
    result = ReferenceResolver::bundle_json_file(directory / "api.json");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.issues()[0].message.find("operationRef"), std::string::npos);
}

TEST_F(ReferenceBundlerTest, TypedBundleUsesConfiguredDepthWithoutInventingSourceLocations) {
    write("api.json", document("model.json").dump());
    json nested = "leaf";
    for (int depth = 0; depth < 140; ++depth) nested = json {{"child", std::move(nested)}};
    write("model.json", json({{"type", "object"}, {"example", std::move(nested)}}).dump());
    ResolverOptions options;
    options.max_depth = 160;
    const auto result = ReferenceResolver::bundle_file(directory / "api.json", options);
    ASSERT_TRUE(result.ok()) << (result.issues().empty() ? "" : result.issues()[0].message);
    EXPECT_EQ(result.value().source_file, (directory / "api.json").string());
    EXPECT_TRUE(result.value().source_locations.empty());
    options.max_depth = std::numeric_limits<std::size_t>::max();
    EXPECT_TRUE(ReferenceResolver::bundle_file(directory / "api.json", options).ok());
}
