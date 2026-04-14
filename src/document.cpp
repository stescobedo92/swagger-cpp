#include "swaggercpp/document.hpp"

namespace swaggercpp {

std::string_view to_string(const HttpMethod method) noexcept {
    switch (method) {
    case HttpMethod::get:
        return "get";
    case HttpMethod::put:
        return "put";
    case HttpMethod::post:
        return "post";
    case HttpMethod::delete_:
        return "delete";
    case HttpMethod::options:
        return "options";
    case HttpMethod::head:
        return "head";
    case HttpMethod::patch:
        return "patch";
    case HttpMethod::trace:
        return "trace";
    }
    return "get";
}

std::string_view to_string(const ParameterLocation location) noexcept {
    switch (location) {
    case ParameterLocation::query:
        return "query";
    case ParameterLocation::header:
        return "header";
    case ParameterLocation::path:
        return "path";
    case ParameterLocation::cookie:
        return "cookie";
    }
    return "query";
}

std::string_view to_string(const SpecificationVersion version) noexcept {
    switch (version) {
    case SpecificationVersion::swagger_2_0:
        return "2.0";
    case SpecificationVersion::openapi_3_0:
        return "3.0.3";
    case SpecificationVersion::openapi_3_1:
        return "3.1.0";
    }
    return "3.1.0";
}

std::optional<HttpMethod> http_method_from_string(const std::string_view value) noexcept {
    if (value == "get") {
        return HttpMethod::get;
    }
    if (value == "put") {
        return HttpMethod::put;
    }
    if (value == "post") {
        return HttpMethod::post;
    }
    if (value == "delete") {
        return HttpMethod::delete_;
    }
    if (value == "options") {
        return HttpMethod::options;
    }
    if (value == "head") {
        return HttpMethod::head;
    }
    if (value == "patch") {
        return HttpMethod::patch;
    }
    if (value == "trace") {
        return HttpMethod::trace;
    }
    return std::nullopt;
}

std::optional<ParameterLocation> parameter_location_from_string(const std::string_view value) noexcept {
    if (value == "query") {
        return ParameterLocation::query;
    }
    if (value == "header") {
        return ParameterLocation::header;
    }
    if (value == "path") {
        return ParameterLocation::path;
    }
    if (value == "cookie") {
        return ParameterLocation::cookie;
    }
    return std::nullopt;
}

} // namespace swaggercpp
