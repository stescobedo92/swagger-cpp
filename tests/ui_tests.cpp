#include <gtest/gtest.h>

#include <httplib.h>

#include "swaggercpp/swaggercpp.hpp"

TEST(SwaggerUiServerTests, ServesUiAndSpecificationLocally) {
    swaggercpp::Document document;
    document.version = swaggercpp::SpecificationVersion::openapi_3_1;
    document.openapi = "3.1.0";
    document.info.title = "UI Test";
    document.info.version = "1.0.0";

    swaggercpp::Operation operation;
    operation.operation_id = "getStatus";
    operation.responses.emplace("200", swaggercpp::Response {.description = "ok"});

    swaggercpp::PathItem path_item;
    path_item.operations.emplace(swaggercpp::HttpMethod::get, operation);
    document.paths.emplace("/status", path_item);

    auto session_result = swaggercpp::SwaggerUiServer::start(document, {
        .open_browser = false,
        .title = "UI Test",
    });

    ASSERT_TRUE(session_result.ok());
    auto session = std::move(session_result.value());
    ASSERT_TRUE(session.running());

    httplib::Client client(session.host(), session.port());

    const auto ui_response = client.Get("/");
    ASSERT_TRUE(ui_response);
    EXPECT_EQ(ui_response->status, 200);
    EXPECT_NE(ui_response->body.find("SwaggerUIBundle"), std::string::npos);
    EXPECT_NE(ui_response->body.find("/openapi.json"), std::string::npos);

    const auto spec_response = client.Get("/openapi.json");
    ASSERT_TRUE(spec_response);
    EXPECT_EQ(spec_response->status, 200);
    EXPECT_NE(spec_response->body.find("\"openapi\": \"3.1.0\""), std::string::npos);
    EXPECT_NE(spec_response->body.find("\"/status\""), std::string::npos);

    session.stop();
}
