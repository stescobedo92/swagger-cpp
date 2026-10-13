#pragma once

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "swaggercpp/document.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp::detail {

using json = nlohmann::json;

// Throws on schema pointer cycles that cannot be represented as references.
[[nodiscard]] json document_to_json(const Document& document);

[[nodiscard]] bool is_extension_key(std::string_view key) noexcept;
void collect_extensions(const json& source, ExtensionMap& target, std::initializer_list<std::string_view> known_keys);
void collect_unknown_fields(const json& source, ExtensionMap& target, std::initializer_list<std::string_view> known_keys);
void add_extension_properties(const ExtensionMap& extensions, json& target);

template <typename T>
std::optional<T> get_optional_scalar(const json& node, std::string_view key) {
    const auto it = node.find(std::string(key));
    if (it == node.end()) {
        return std::nullopt;
    }
    return it->template get<T>();
}

template <typename T>
T get_value_or(const json& node, std::string_view key, T fallback) {
    const auto it = node.find(std::string(key));
    if (it == node.end()) {
        return fallback;
    }
    return it->template get<T>();
}

void add_missing_required_issue(
    std::vector<Issue>& issues,
    std::string_view json_path,
    std::string_view field_name);

} // namespace swaggercpp::detail

