#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/reader.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp {

struct SwaggerUiOptions {
    std::string host {"127.0.0.1"};
    int port {0};
    bool open_browser {true};
    std::string ui_path {"/"};
    std::string spec_path {"/openapi.json"};
    std::string title {"swaggercpp"};
    bool deep_linking {true};
    bool display_operation_id {true};
    bool display_request_duration {true};
    bool show_extensions {true};
    bool show_common_extensions {true};
    bool try_it_out_enabled {true};
    std::string doc_expansion {"list"};
};

class SWAGGERCPP_EXPORT SwaggerUiSession {
public:
    SwaggerUiSession() = default;
    SwaggerUiSession(const SwaggerUiSession&) = delete;
    SwaggerUiSession& operator=(const SwaggerUiSession&) = delete;
    SwaggerUiSession(SwaggerUiSession&&) noexcept;
    SwaggerUiSession& operator=(SwaggerUiSession&&) noexcept;
    ~SwaggerUiSession();

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] int port() const noexcept;
    [[nodiscard]] std::string host() const;
    [[nodiscard]] std::string base_url() const;
    [[nodiscard]] std::string ui_url() const;
    [[nodiscard]] std::string spec_url() const;

    void stop();
    void wait();

private:
    struct Impl;

    explicit SwaggerUiSession(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;

    friend class SwaggerUiServer;
};

class SWAGGERCPP_EXPORT SwaggerUiServer {
public:
    [[nodiscard]] static Result<SwaggerUiSession> start(const Document& document, SwaggerUiOptions options = {});
    [[nodiscard]] static Result<SwaggerUiSession> start(
        std::string_view content,
        SwaggerUiOptions options = {},
        ReaderOptions reader_options = {});
    [[nodiscard]] static Result<SwaggerUiSession> start_file(
        const std::filesystem::path& path,
        SwaggerUiOptions options = {},
        ReaderOptions reader_options = {});
};

} // namespace swaggercpp
