#include "detail/yaml_utils.hpp"

namespace swaggercpp::detail {

nlohmann::json yaml_to_json(const YAML::Node& node) {
    if (!node || node.IsNull()) {
        return nullptr;
    }

    if (node.IsScalar()) {
        const auto scalar = node.Scalar();
        if (scalar == "true") {
            return true;
        }
        if (scalar == "false") {
            return false;
        }
        try {
            const auto int_value = node.as<std::int64_t>();
            if (std::to_string(int_value) == scalar) {
                return int_value;
            }
        } catch (...) {
        }
        try {
            const auto double_value = node.as<double>();
            if (std::to_string(double_value).starts_with(scalar)) {
                return double_value;
            }
        } catch (...) {
        }
        return scalar;
    }

    if (node.IsSequence()) {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& item : node) {
            result.push_back(yaml_to_json(item));
        }
        return result;
    }

    nlohmann::json result = nlohmann::json::object();
    for (const auto& item : node) {
        result[item.first.as<std::string>()] = yaml_to_json(item.second);
    }
    return result;
}

YAML::Node json_to_yaml(const nlohmann::json& value) {
    if (value.is_null()) {
        return YAML::Node(YAML::NodeType::Null);
    }
    if (value.is_boolean()) {
        return YAML::Node(value.get<bool>());
    }
    if (value.is_number_integer()) {
        return YAML::Node(value.get<std::int64_t>());
    }
    if (value.is_number_unsigned()) {
        return YAML::Node(value.get<std::uint64_t>());
    }
    if (value.is_number_float()) {
        return YAML::Node(value.get<double>());
    }
    if (value.is_string()) {
        return YAML::Node(value.get<std::string>());
    }
    if (value.is_array()) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& item : value) {
            node.push_back(json_to_yaml(item));
        }
        return node;
    }

    YAML::Node node(YAML::NodeType::Map);
    for (const auto& [key, item] : value.items()) {
        node[key] = json_to_yaml(item);
    }
    return node;
}

std::string emit_yaml(const nlohmann::json& value) {
    YAML::Emitter emitter;
    emitter << json_to_yaml(value);
    return std::string(emitter.c_str());
}

} // namespace swaggercpp::detail

