#include "detail/yaml_utils.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace swaggercpp::detail {
namespace {

std::string pointer_token(std::string_view token) {
    std::string result;
    for (const char c : token) {
        if (c == '~') result += "~0";
        else if (c == '/') result += "~1";
        else result += c;
    }
    return result;
}

bool decimal_digits(const std::string_view value) {
    return !value.empty() && std::ranges::all_of(value, [](const char ch) { return ch >= '0' && ch <= '9'; });
}

std::optional<nlohmann::json> core_integer(const std::string_view scalar) {
    auto digits = scalar;
    bool negative = false;
    int base = 10;
    if (digits.starts_with("0x")) { base = 16; digits.remove_prefix(2); }
    else if (digits.starts_with("0o")) { base = 8; digits.remove_prefix(2); }
    else if (!digits.empty() && (digits.front() == '+' || digits.front() == '-')) {
        negative = digits.front() == '-';
        digits.remove_prefix(1);
    }
    const auto valid_digit = [base](const char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0' < base;
        return base == 16 && ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'));
    };
    if (digits.empty() || !std::ranges::all_of(digits, valid_digit)) return std::nullopt;
    std::uint64_t value = 0;
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value, base);
    if (parsed.ec != std::errc {}) throw std::runtime_error("YAML integer exceeds the supported 64-bit range");
    if (!negative) return nlohmann::json(value);
    constexpr auto minimum_magnitude = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1;
    if (value > minimum_magnitude) throw std::runtime_error("YAML integer exceeds the supported 64-bit range");
    if (value == minimum_magnitude) return nlohmann::json(std::numeric_limits<std::int64_t>::min());
    return nlohmann::json(-static_cast<std::int64_t>(value));
}

bool core_float_syntax(std::string_view scalar) {
    if (!scalar.empty() && (scalar.front() == '+' || scalar.front() == '-')) scalar.remove_prefix(1);
    const auto exponent = scalar.find_first_of("eE");
    const auto mantissa = scalar.substr(0, exponent);
    if (exponent != std::string_view::npos) {
        auto power = scalar.substr(exponent + 1);
        if (!power.empty() && (power.front() == '+' || power.front() == '-')) power.remove_prefix(1);
        if (!decimal_digits(power)) return false;
    }
    const auto point = mantissa.find('.');
    if (point == std::string_view::npos) return decimal_digits(mantissa);
    const auto whole = mantissa.substr(0, point);
    const auto fraction = mantissa.substr(point + 1);
    return (!whole.empty() || !fraction.empty()) && (whole.empty() || decimal_digits(whole))
        && (fraction.empty() || decimal_digits(fraction));
}

bool core_nonfinite(std::string_view scalar) {
    if (scalar == ".nan" || scalar == ".NaN" || scalar == ".NAN") return true;
    if (!scalar.empty() && (scalar.front() == '+' || scalar.front() == '-')) scalar.remove_prefix(1);
    return scalar == ".inf" || scalar == ".Inf" || scalar == ".INF";
}

nlohmann::json scalar_to_json(const YAML::Node& node) {
    const auto& scalar = node.Scalar();
    const auto& tag = node.Tag();
    if (tag == "!" || tag == "tag:yaml.org,2002:str") return scalar;
    const bool tagged_bool = tag == "tag:yaml.org,2002:bool";
    const bool tagged_int = tag == "tag:yaml.org,2002:int";
    const bool tagged_float = tag == "tag:yaml.org,2002:float";
    const bool tagged_null = tag == "tag:yaml.org,2002:null";
    if (tagged_null) {
        if (scalar.empty() || scalar == "~" || scalar == "null" || scalar == "Null" || scalar == "NULL") return nullptr;
        throw std::runtime_error("Invalid explicitly tagged YAML null");
    }
    if (!tagged_int && !tagged_float) {
        if (scalar == "true" || scalar == "True" || scalar == "TRUE") return true;
        if (scalar == "false" || scalar == "False" || scalar == "FALSE") return false;
        if (tagged_bool) throw std::runtime_error("Invalid explicitly tagged YAML boolean");
    }
    if (!tagged_float) {
        if (const auto integer = core_integer(scalar)) return *integer;
        if (tagged_int) throw std::runtime_error("Invalid explicitly tagged YAML integer");
    }
    if (core_nonfinite(scalar)) throw std::runtime_error("Non-finite numbers are not valid JSON values");
    if (core_float_syntax(scalar)) {
        std::string_view text(scalar);
        if (text.starts_with('+')) text.remove_prefix(1);
        double value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
        if (parsed.ec != std::errc {} || parsed.ptr != text.data() + text.size() || !std::isfinite(value)) {
            throw std::runtime_error("YAML floating-point value is outside the supported finite range");
        }
        return value;
    }
    if (tagged_float) throw std::runtime_error("Invalid explicitly tagged YAML float");
    return scalar;
}

nlohmann::json convert_yaml(const YAML::Node& node, std::vector<YAML::Node>& ancestors,
    std::size_t& count, std::map<std::string, SourceLocation>* locations, const std::string& path,
    const std::size_t max_depth, std::size_t& remaining_bytes) {
    if (++count > 1000000 || ancestors.size() > max_depth) {
        throw std::runtime_error("YAML exceeds the depth or expanded node limit");
    }
    if (locations && !node.Mark().is_null()) {
        const auto mark = node.Mark();
        locations->insert_or_assign(path, SourceLocation {
            static_cast<std::size_t>(mark.line) + 1, static_cast<std::size_t>(mark.column) + 1});
    }
    if (!node || node.IsNull()) {
        return nullptr;
    }

    if (node.IsScalar()) {
        const auto& scalar = node.Scalar();
        if (scalar.size() > remaining_bytes) throw std::runtime_error("Expanded YAML exceeds the byte limit");
        remaining_bytes -= scalar.size();
        return scalar_to_json(node);
    }

    for (const auto& ancestor : ancestors) {
        if (node.is(ancestor)) throw std::runtime_error("Recursive YAML aliases cannot be represented as JSON");
    }
    ancestors.push_back(node);

    if (node.IsSequence()) {
        nlohmann::json result = nlohmann::json::array();
        std::size_t index = 0;
        for (const auto& item : node) {
            result.push_back(convert_yaml(item, ancestors, count, locations, path + "/" + std::to_string(index++), max_depth, remaining_bytes));
        }
        ancestors.pop_back();
        return result;
    }

    nlohmann::json result = nlohmann::json::object();
    for (const auto& item : node) {
        if (!item.first.IsScalar()) throw std::runtime_error("OpenAPI mapping keys must be scalar strings");
        const auto key = item.first.Scalar();
        if (key.size() > remaining_bytes) throw std::runtime_error("Expanded YAML exceeds the byte limit");
        remaining_bytes -= key.size();
        if (result.contains(key)) throw std::runtime_error("Duplicate YAML mapping key: " + key);
        result[key] = convert_yaml(item.second, ancestors, count, locations, path + "/" + pointer_token(key), max_depth, remaining_bytes);
    }
    ancestors.pop_back();
    return result;
}

void emit_value(YAML::Emitter& emitter, const nlohmann::json& value) {
    if (value.is_object()) {
        emitter << YAML::BeginMap;
        for (const auto& [key, item] : value.items()) {
            emitter << YAML::Key << YAML::DoubleQuoted << key << YAML::Value;
            emit_value(emitter, item);
        }
        emitter << YAML::EndMap;
    } else if (value.is_array()) {
        emitter << YAML::BeginSeq;
        for (const auto& item : value) emit_value(emitter, item);
        emitter << YAML::EndSeq;
    } else if (value.is_string()) {
        emitter << YAML::DoubleQuoted << value.get_ref<const std::string&>();
    } else if (value.is_null()) {
        emitter << YAML::Null;
    } else if (value.is_boolean()) {
        emitter << value.get<bool>();
    } else if (value.is_number_unsigned()) {
        emitter << value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        emitter << value.get<std::int64_t>();
    } else {
        const double number = value.get<double>();
        if (!std::isfinite(number)) throw std::runtime_error("Non-finite numbers are not valid JSON values");
        // yaml-cpp emits integral doubles without a decimal point, including -0.
        // An explicit float tag preserves their type and the sign of zero.
        if (std::trunc(number) == number) emitter << YAML::SecondaryTag("float");
        emitter << number;
    }
}
} // namespace

nlohmann::json yaml_to_json(const YAML::Node& node, std::map<std::string, SourceLocation>* locations,
    const std::size_t max_depth, std::size_t max_expanded_bytes) {
    std::vector<YAML::Node> ancestors;
    std::size_t count = 0;
    return convert_yaml(node, ancestors, count, locations, "", max_depth, max_expanded_bytes);
}

YAML::Node json_to_yaml(const nlohmann::json& value) {
    if (value.is_null()) {
        return YAML::Node(YAML::NodeType::Null);
    }
    if (value.is_boolean()) {
        return YAML::Node(value.get<bool>());
    }
    if (value.is_number_unsigned()) {
        return YAML::Node(value.get<std::uint64_t>());
    }
    if (value.is_number_integer()) {
        return YAML::Node(value.get<std::int64_t>());
    }
    if (value.is_number_float()) {
        return YAML::Node(value.get<double>());
    }
    if (value.is_string()) {
        YAML::Node node(value.get<std::string>());
        node.SetTag("tag:yaml.org,2002:str");
        return node;
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
    emit_value(emitter, value);
    if (!emitter.good()) throw std::runtime_error(emitter.GetLastError());
    return std::string(emitter.c_str());
}

} // namespace swaggercpp::detail

