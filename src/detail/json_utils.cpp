#include "detail/json_utils.hpp"

#include <algorithm>

namespace swaggercpp::detail {

bool is_extension_key(const std::string_view key) noexcept {
    return key.starts_with("x-");
}

void collect_extensions(
    const json& source,
    ExtensionMap& target,
    const std::initializer_list<std::string_view> known_keys) {
    for (const auto& [key, value] : source.items()) {
        const bool is_known = std::ranges::find(known_keys, key) != known_keys.end();
        if (!is_known && is_extension_key(key)) {
            target.emplace(key, value);
        }
    }
}

void add_extension_properties(const ExtensionMap& extensions, json& target) {
    for (const auto& [key, value] : extensions) {
        target[key] = value;
    }
}

void add_missing_required_issue(
    std::vector<Issue>& issues,
    const std::string_view json_path,
    const std::string_view field_name) {
    issues.push_back(Issue {
        .severity = IssueSeverity::error,
        .path = std::string(json_path),
        .message = "Missing required field '" + std::string(field_name) + "'"
    });
}

} // namespace swaggercpp::detail

