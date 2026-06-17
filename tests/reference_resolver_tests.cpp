#include <gtest/gtest.h>
#include "swaggercpp/document.hpp"
#include "swaggercpp/reference_resolver.hpp"

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
    EXPECT_EQ(next_schema, node_schema);
}
