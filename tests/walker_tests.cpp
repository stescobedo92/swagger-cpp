#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "swaggercpp/swaggercpp.hpp"
#include "swaggercpp/mutable_walker.hpp"

namespace {

class ConstCollector : public swaggercpp::DocumentVisitor {
public:
    std::vector<std::string> paths;
    std::size_t parameters {0};
    std::size_t responses {0};
    std::size_t request_bodies {0};
    std::size_t entered_paths {0};
    std::size_t left_paths {0};
    std::size_t entered_operations {0};
    std::size_t left_operations {0};

    void visit_schema(const swaggercpp::Schema&, const std::string_view path) override { paths.emplace_back(path); }
    void visit_parameter(const swaggercpp::Parameter&) override { ++parameters; }
    void visit_response(std::string_view, const swaggercpp::Response&) override { ++responses; }
    void visit_request_body(const swaggercpp::RequestBody&) override { ++request_bodies; }
    void enter_path_item(std::string_view, const swaggercpp::PathItem&) override { ++entered_paths; }
    void leave_path_item(std::string_view, const swaggercpp::PathItem&) override { ++left_paths; }
    void enter_operation(std::string_view, swaggercpp::HttpMethod, const swaggercpp::Operation&) override { ++entered_operations; }
    void leave_operation(std::string_view, swaggercpp::HttpMethod, const swaggercpp::Operation&) override { ++left_operations; }
};

class MutableCollector : public swaggercpp::MutableDocumentVisitor {
public:
    std::vector<std::string> paths;
    void visit_schema(swaggercpp::SchemaPtr&, const std::string_view path) override { paths.emplace_back(path); }
};

} // namespace

TEST(DocumentWalkerTests, TraversesTypedSchemasWithMatchingEscapedPointers) {
    auto parsed = swaggercpp::DocumentReader::read(R"({
        "openapi":"3.1.1", "info":{"title":"Walk","version":"1"},
        "paths":{"/a~b":{
            "parameters":[{"name":"path", "in":"query", "content":{"application/json":{"schema":true}}}],
            "get":{
                "parameters":[{"name":"first", "in":"query", "schema":false}],
                "requestBody":{"content":{"application/json":{"schema":true}}},
                "responses":{"200":{"description":"ok", "content":{"application/json":{"schema":false}}}}
            }
        }},
        "webhooks":{"event/name":{"post":{"responses":{"204":{"description":"ok", "content":{"application/json":{"schema":true}}}}}}},
        "components":{
            "schemas":{"A/B~":{"properties":{"x/y":true},"$defs":{"local~":false},"items":true,
                "not":false,"allOf":[true],"anyOf":[false],"oneOf":[true]}},
            "parameters":{"Filter":{"name":"filter","in":"query","content":{"application/json":{"schema":true}}}},
            "requestBodies":{"Body":{"content":{"application/json":{"schema":false}}}},
            "responses":{"Reply":{"description":"ok","content":{"application/json":{"schema":true}}}}
        }
    })");
    ASSERT_TRUE(parsed.ok());
    ConstCollector const_visitor;
    MutableCollector mutable_visitor;
    swaggercpp::DocumentWalker::walk(parsed.value(), const_visitor);
    swaggercpp::MutableDocumentWalker::walk(parsed.value(), mutable_visitor);
    EXPECT_EQ(const_visitor.paths, mutable_visitor.paths);
    const std::set<std::string> actual(const_visitor.paths.begin(), const_visitor.paths.end());
    const std::set<std::string> expected {
        "/paths/~1a~0b/parameters/0/content/application~1json/schema",
        "/paths/~1a~0b/get/parameters/0/schema",
        "/paths/~1a~0b/get/requestBody/content/application~1json/schema",
        "/paths/~1a~0b/get/responses/200/content/application~1json/schema",
        "/webhooks/event~1name/post/responses/204/content/application~1json/schema",
        "/components/schemas/A~1B~0",
        "/components/schemas/A~1B~0/properties/x~1y",
        "/components/schemas/A~1B~0/$defs/local~0",
        "/components/schemas/A~1B~0/items",
        "/components/schemas/A~1B~0/not",
        "/components/schemas/A~1B~0/allOf/0",
        "/components/schemas/A~1B~0/anyOf/0",
        "/components/schemas/A~1B~0/oneOf/0",
        "/components/parameters/Filter/content/application~1json/schema",
        "/components/requestBodies/Body/content/application~1json/schema",
        "/components/responses/Reply/content/application~1json/schema"
    };
    EXPECT_EQ(actual, expected);
    EXPECT_EQ(const_visitor.parameters, 3U);
    EXPECT_EQ(const_visitor.responses, 3U);
    EXPECT_EQ(const_visitor.request_bodies, 2U);
    EXPECT_EQ(const_visitor.entered_paths, 2U);
    EXPECT_EQ(const_visitor.entered_paths, const_visitor.left_paths);
    EXPECT_EQ(const_visitor.entered_operations, 2U);
    EXPECT_EQ(const_visitor.entered_operations, const_visitor.left_operations);

    const auto serialized = swaggercpp::DocumentWriter::write_json(parsed.value());
    ASSERT_TRUE(serialized.ok());
    const auto document_json = nlohmann::json::parse(serialized.value());
    for (const auto& path : actual) EXPECT_TRUE(document_json.contains(nlohmann::json::json_pointer(path))) << path;
}

TEST(DocumentWalkerTests, StopsCyclesButVisitsEachSharedOccurrence) {
    swaggercpp::Document document;
    auto recursive = std::make_shared<swaggercpp::Schema>();
    recursive->properties.emplace("self", recursive);
    recursive->items = std::make_shared<swaggercpp::Schema>();
    document.components.schemas.emplace("A", recursive);
    document.components.schemas.emplace("B", recursive);
    ConstCollector const_visitor;
    MutableCollector mutable_visitor;
    swaggercpp::DocumentWalker::walk(document, const_visitor);
    swaggercpp::MutableDocumentWalker::walk(document, mutable_visitor);
    recursive->properties.clear();
    const std::vector<std::string> expected {
        "/components/schemas/A", "/components/schemas/A/properties/self", "/components/schemas/A/items",
        "/components/schemas/B", "/components/schemas/B/properties/self", "/components/schemas/B/items"
    };
    EXPECT_EQ(const_visitor.paths, expected);
    EXPECT_EQ(mutable_visitor.paths, expected);
}

TEST(MutableDocumentWalkerTests, ChecksReplacementPointersForCycles) {
    class ReplacingVisitor : public swaggercpp::MutableDocumentVisitor {
    public:
        swaggercpp::SchemaPtr root;
        std::size_t visits {0};
        void visit_schema(swaggercpp::SchemaPtr& schema, const std::string_view path) override {
            ++visits;
            if (path.ends_with("/properties/child")) schema = root;
        }
    } visitor;
    swaggercpp::Document document;
    visitor.root = std::make_shared<swaggercpp::Schema>();
    visitor.root->properties.emplace("child", std::make_shared<swaggercpp::Schema>());
    document.components.schemas.emplace("Root", visitor.root);
    swaggercpp::MutableDocumentWalker::walk(document, visitor);
    const auto replaced = visitor.root->properties.at("child") == visitor.root;
    visitor.root->properties.clear();
    EXPECT_TRUE(replaced);
    EXPECT_EQ(visitor.visits, 2U);
}

TEST(MutableDocumentWalkerTests, AllowsVisitorToRemoveSchemaPointer) {
    class RemovingVisitor : public swaggercpp::MutableDocumentVisitor {
    public:
        void visit_schema(swaggercpp::SchemaPtr& schema, std::string_view) override { schema.reset(); }
    } visitor;
    swaggercpp::Document document;
    document.components.schemas.emplace("Root", std::make_shared<swaggercpp::Schema>());
    EXPECT_NO_THROW(swaggercpp::MutableDocumentWalker::walk(document, visitor));
    EXPECT_FALSE(document.components.schemas.at("Root"));
}

TEST(DocumentWalkerTests, BoundsWorkForSharedAcyclicSchemaGraphs) {
    class CountingVisitor : public swaggercpp::DocumentVisitor {
    public:
        std::size_t visits {0};
        void visit_schema(const swaggercpp::Schema&, std::string_view) override { ++visits; }
    } const_visitor;
    class MutableCountingVisitor : public swaggercpp::MutableDocumentVisitor {
    public:
        std::size_t visits {0};
        void visit_schema(swaggercpp::SchemaPtr&, std::string_view) override { ++visits; }
    } mutable_visitor;
    swaggercpp::Document document;
    auto graph = std::make_shared<swaggercpp::Schema>();
    for (std::size_t level = 0; level < 40; ++level) {
        auto parent = std::make_shared<swaggercpp::Schema>();
        parent->properties.emplace("a", graph);
        parent->properties.emplace("b", graph);
        graph = std::move(parent);
    }
    document.components.schemas.emplace("Graph", graph);
    EXPECT_THROW(swaggercpp::DocumentWalker::walk(document, const_visitor), std::runtime_error);
    EXPECT_THROW(swaggercpp::MutableDocumentWalker::walk(document, mutable_visitor), std::runtime_error);
    EXPECT_EQ(const_visitor.visits, 100000U);
    EXPECT_EQ(mutable_visitor.visits, 100000U);
}

TEST(DocumentWalkerTests, BoundsSchemaDepth) {
    swaggercpp::Document document;
    auto graph = std::make_shared<swaggercpp::Schema>();
    for (std::size_t level = 0; level < 256; ++level) {
        auto parent = std::make_shared<swaggercpp::Schema>();
        parent->items = graph;
        graph = std::move(parent);
    }
    document.components.schemas.emplace("Deep", graph);
    swaggercpp::DocumentVisitor const_visitor;
    swaggercpp::MutableDocumentVisitor mutable_visitor;
    EXPECT_THROW(swaggercpp::DocumentWalker::walk(document, const_visitor), std::runtime_error);
    EXPECT_THROW(swaggercpp::MutableDocumentWalker::walk(document, mutable_visitor), std::runtime_error);
}
