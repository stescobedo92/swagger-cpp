#pragma once

#include <string>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace swaggercpp::detail {

[[nodiscard]] nlohmann::json yaml_to_json(const YAML::Node& node);
[[nodiscard]] YAML::Node json_to_yaml(const nlohmann::json& value);
[[nodiscard]] std::string emit_yaml(const nlohmann::json& value);

} // namespace swaggercpp::detail

