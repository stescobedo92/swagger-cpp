#include "swaggercpp/ui.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "swaggercpp/writer.hpp"

#if defined(_WIN32)
#    define NOMINMAX
#    include <Windows.h>
#    include <shellapi.h>
#endif

namespace swaggercpp {
namespace {

using nlohmann::json;

std::string normalize_route(std::string_view route, std::string_view fallback) {
    std::string normalized = route.empty() ? std::string(fallback) : std::string(route);
    if (normalized.front() != '/') {
        normalized.insert(normalized.begin(), '/');
    }
    if (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

std::string html_escape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const auto ch : value) {
        switch (ch) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&#39;";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::string build_swagger_ui_html(const SwaggerUiOptions& options) {
    const auto escaped_title = html_escape(options.title);
    const auto spec_url_literal = json(options.spec_path).dump();
    const auto doc_expansion_literal = json(options.doc_expansion).dump();

    return "<!DOCTYPE html>\n"
           "<html lang=\"en\">\n"
           "  <head>\n"
           "    <meta charset=\"utf-8\" />\n"
           "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
           "    <meta name=\"description\" content=\"SwaggerUI\" />\n"
           "    <title>" +
        escaped_title +
        "</title>\n"
        "    <link rel=\"stylesheet\" href=\"https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui.css\" />\n"
        "    <style>\n"
        "      html, body { margin: 0; padding: 0; background: #fafafa; }\n"
        "      #swagger-ui { min-height: 100vh; }\n"
        "    </style>\n"
        "  </head>\n"
        "  <body>\n"
        "    <div id=\"swagger-ui\"></div>\n"
        "    <script src=\"https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui-bundle.js\" crossorigin></script>\n"
        "    <script src=\"https://unpkg.com/swagger-ui-dist@5.11.0/swagger-ui-standalone-preset.js\" crossorigin></script>\n"
        "    <script>\n"
        "      window.onload = () => {\n"
        "        window.ui = SwaggerUIBundle({\n"
        "          url: " +
        spec_url_literal +
        ",\n"
        "          dom_id: '#swagger-ui',\n"
        "          presets: [SwaggerUIBundle.presets.apis, SwaggerUIStandalonePreset],\n"
        "          layout: 'StandaloneLayout',\n"
        "          deepLinking: " +
        std::string(options.deep_linking ? "true" : "false") +
        ",\n"
        "          displayOperationId: " +
        std::string(options.display_operation_id ? "true" : "false") +
        ",\n"
        "          displayRequestDuration: " +
        std::string(options.display_request_duration ? "true" : "false") +
        ",\n"
        "          showExtensions: " +
        std::string(options.show_extensions ? "true" : "false") +
        ",\n"
        "          showCommonExtensions: " +
        std::string(options.show_common_extensions ? "true" : "false") +
        ",\n"
        "          tryItOutEnabled: " +
        std::string(options.try_it_out_enabled ? "true" : "false") +
        ",\n"
        "          docExpansion: " +
        doc_expansion_literal +
        ",\n"
        "          validatorUrl: null\n"
        "        });\n"
        "      };\n"
        "    </script>\n"
        "  </body>\n"
        "</html>\n";
}

bool open_browser(std::string_view url) {
#if defined(_WIN32)
    const auto result =
        reinterpret_cast<std::intptr_t>(ShellExecuteA(nullptr, "open", std::string(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return result > 32;
#elif defined(__APPLE__)
    const auto command = "open \"" + std::string(url) + "\"";
    return std::system(command.c_str()) == 0;
#else
    const auto command = "xdg-open \"" + std::string(url) + "\"";
    return std::system(command.c_str()) == 0;
#endif
}

} // namespace

struct SwaggerUiSession::Impl {
    std::unique_ptr<httplib::Server> server;
    std::thread server_thread;
    std::string host;
    std::string ui_path;
    std::string spec_path;
    int port {0};
    std::atomic<bool> running {false};
    std::mutex wait_mutex;
};

SwaggerUiSession::SwaggerUiSession(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

SwaggerUiSession::SwaggerUiSession(SwaggerUiSession&&) noexcept = default;

SwaggerUiSession& SwaggerUiSession::operator=(SwaggerUiSession&&) noexcept = default;

SwaggerUiSession::~SwaggerUiSession() {
    stop();
}

bool SwaggerUiSession::running() const noexcept {
    return impl_ && impl_->running.load();
}

int SwaggerUiSession::port() const noexcept {
    return impl_ ? impl_->port : 0;
}

std::string SwaggerUiSession::host() const {
    return impl_ ? impl_->host : std::string {};
}

std::string SwaggerUiSession::base_url() const {
    return impl_ ? "http://" + impl_->host + ":" + std::to_string(impl_->port) : std::string {};
}

std::string SwaggerUiSession::ui_url() const {
    return base_url() + (impl_ ? impl_->ui_path : std::string {});
}

std::string SwaggerUiSession::spec_url() const {
    return base_url() + (impl_ ? impl_->spec_path : std::string {});
}

void SwaggerUiSession::stop() {
    if (!impl_) {
        return;
    }
    if (impl_->server) {
        impl_->server->stop();
    }
    impl_->running.store(false);
    if (impl_->server_thread.joinable()) {
        impl_->server_thread.join();
    }
}

void SwaggerUiSession::wait() {
    if (impl_ && impl_->server_thread.joinable()) {
        impl_->server_thread.join();
        impl_->running.store(false);
    }
}

Result<SwaggerUiSession> SwaggerUiServer::start(const Document& document, SwaggerUiOptions options) {
    std::vector<Issue> issues;

    options.ui_path = normalize_route(options.ui_path, "/");
    options.spec_path = normalize_route(options.spec_path, "/openapi.json");

    if (options.ui_path == options.spec_path) {
        return Result<SwaggerUiSession>::failure(
            {{.severity = IssueSeverity::error, .path = "/ui", .message = "UI path and spec path must be different"}});
    }

    const auto json_result = DocumentWriter::write_json(document, true, 2);
    if (!json_result) {
        return Result<SwaggerUiSession>::failure(json_result.issues());
    }

    auto impl = std::make_unique<SwaggerUiSession::Impl>();
    impl->server = std::make_unique<httplib::Server>();
    impl->host = options.host;
    impl->ui_path = options.ui_path;
    impl->spec_path = options.spec_path;

    const auto spec_json = json_result.value();
    const auto html = build_swagger_ui_html(options);

    impl->server->Get(options.spec_path, [spec_json](const httplib::Request&, httplib::Response& response) {
        response.set_content(spec_json, "application/json; charset=utf-8");
    });

    impl->server->Get(options.ui_path, [html](const httplib::Request&, httplib::Response& response) {
        response.set_content(html, "text/html; charset=utf-8");
    });

    if (options.ui_path == "/") {
        impl->server->Get("/index.html", [html](const httplib::Request&, httplib::Response& response) {
            response.set_content(html, "text/html; charset=utf-8");
        });
    } else {
        const auto redirect_target = options.ui_path;
        impl->server->Get("/", [redirect_target](const httplib::Request&, httplib::Response& response) {
            response.set_redirect(redirect_target);
        });
    }

    impl->server->Get("/healthz", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("ok", "text/plain; charset=utf-8");
    });

    if (options.port > 0) {
        if (!impl->server->bind_to_port(options.host, options.port)) {
            return Result<SwaggerUiSession>::failure({{
                .severity = IssueSeverity::error,
                .path = "/ui",
                .message = "Unable to bind the embedded Swagger UI server to the requested port",
            }});
        }
        impl->port = options.port;
    } else {
        impl->port = impl->server->bind_to_any_port(options.host);
        if (impl->port <= 0) {
            return Result<SwaggerUiSession>::failure({{
                .severity = IssueSeverity::error,
                .path = "/ui",
                .message = "Unable to bind the embedded Swagger UI server to an available port",
            }});
        }
    }

    impl->running.store(true);
    auto* server = impl->server.get();
    auto* running = &impl->running;
    impl->server_thread = std::thread([server, running]() {
        server->listen_after_bind();
        running->store(false);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(75));

    SwaggerUiSession session(std::move(impl));

    if (options.open_browser && !open_browser(session.ui_url())) {
        issues.push_back(Issue {
            .severity = IssueSeverity::warning,
            .path = session.ui_url(),
            .message = "The embedded Swagger UI server started, but the browser could not be opened automatically",
        });
    }

    return Result<SwaggerUiSession>::success(std::move(session), std::move(issues));
}

Result<SwaggerUiSession> SwaggerUiServer::start(
    std::string_view content,
    SwaggerUiOptions options,
    const ReaderOptions reader_options) {
    const auto read_result = DocumentReader::read(content, reader_options);
    if (!read_result) {
        return Result<SwaggerUiSession>::failure(read_result.issues());
    }

    auto start_result = start(read_result.value(), std::move(options));
    if (!start_result) {
        auto issues = read_result.issues();
        const auto& start_issues = start_result.issues();
        issues.insert(issues.end(), start_issues.begin(), start_issues.end());
        return Result<SwaggerUiSession>::failure(std::move(issues));
    }

    auto issues = read_result.issues();
    const auto& start_issues = start_result.issues();
    issues.insert(issues.end(), start_issues.begin(), start_issues.end());
    return Result<SwaggerUiSession>::success(std::move(start_result.value()), std::move(issues));
}

Result<SwaggerUiSession> SwaggerUiServer::start_file(
    const std::filesystem::path& path,
    SwaggerUiOptions options,
    const ReaderOptions reader_options) {
    const auto read_result = DocumentReader::read_file(path, reader_options);
    if (!read_result) {
        return Result<SwaggerUiSession>::failure(read_result.issues());
    }

    auto start_result = start(read_result.value(), std::move(options));
    if (!start_result) {
        auto issues = read_result.issues();
        const auto& start_issues = start_result.issues();
        issues.insert(issues.end(), start_issues.begin(), start_issues.end());
        return Result<SwaggerUiSession>::failure(std::move(issues));
    }

    auto issues = read_result.issues();
    const auto& start_issues = start_result.issues();
    issues.insert(issues.end(), start_issues.begin(), start_issues.end());
    return Result<SwaggerUiSession>::success(std::move(start_result.value()), std::move(issues));
}

} // namespace swaggercpp
