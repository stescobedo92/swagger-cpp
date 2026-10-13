#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"

namespace swaggercpp::detail {

[[nodiscard]] nlohmann::json yaml_to_json(const YAML::Node& node,
    std::map<std::string, SourceLocation>* locations = nullptr, std::size_t max_depth = 128,
    std::size_t max_expanded_bytes = 16 * 1024 * 1024);
[[nodiscard]] YAML::Node json_to_yaml(const nlohmann::json& value);
// Exported for the CLI when the library is built as a Windows DLL.
[[nodiscard]] SWAGGERCPP_EXPORT std::string emit_yaml(const nlohmann::json& value);

} // namespace swaggercpp::detail

